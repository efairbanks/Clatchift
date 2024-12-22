#include <Bounce2.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <EEPROM.h>

#define CHORD_POT_PIN 9 // pin for Chord pot
#define CHORD_CV_PIN 6 // pin for Chord CV 
#define ROOT_POT_PIN 7 // pin for Root Note pot
#define ROOT_CV_PIN 8 // pin for Root Note CV
#define RESET_BUTTON 8 // Reset button 
#define RESET_LED 11 // Reset LED indicator 
#define RESET_CV 9 // Reset pulse in / out
#define BANK_BUTTON 2 // Bank Button 
#define LED0 6
#define LED1 5
#define LED2 4
#define LED3 3

#define ADC_BITS 13
#define ADC_MAX_VAL 8192
#define CHANGE_TOLERANCE 64

#define CTRL_SAMPLE_RATE 48000
#define CTRL_TIMER_US (1000000/CTRL_SAMPLE_RATE)
#define CTRL_1MS (CTRL_SAMPLE_RATE/1000)

#define B32_1HZ_DELTA ((0xFFFFFFFF)/CTRL_SAMPLE_RATE)
#define B32_1MS_DELTA (B32_1HZ_DELTA/1000)

class ClockGenerator {
public:
  int phase = 0;
  int offset = 0;
  int interval = 0;
  void Reset() { phase = 0; }
  void SetOffset(int x) { offset = x; }
  void SetInterval(int x) { interval = x; }
  int GetOffsetPhase() {
    int offsetPhase = phase - ((interval*offset)>>13);
    while(offsetPhase < 0) offsetPhase += interval;
    return offsetPhase;
  }
  bool GetSquare() {
    int result = 0;
    int offsetPhase = phase - ((interval*offset)>>13);
    while(offsetPhase < 0) offsetPhase += interval;
    if(offsetPhase < (interval>>1)) result = 1;
    return result;
  }
  bool Process() {
    bool result = 0;
    if(interval > 0) {
      int offsetPhase = phase - ((interval*offset)>>13);
      while(offsetPhase < 0) offsetPhase += interval;
      if(offsetPhase == 0) result = 1;
      phase++;
      while(phase >= interval) phase -= interval;
    }
    return result;
  }
};

class TrigDetector {
  bool state = 0;
  bool lastState = 0;
  int lowToHighThreshold = 5000;
  int highToLowThreshold = 1000;
public:
  bool GetState() { return state; }
  bool Process(int x) {
    int result = 0;
    if(state == 0 && x > lowToHighThreshold) {
      state = 1;
    }
    if(state == 1 && x < highToLowThreshold) {
      state = 0;
    }
    if(state == 1 && state != lastState) result = 1;
    lastState = state;
    return result;
  }
};

class ClockRateDetector {
public:
  int samplesSinceLastClock;
  int lastIntervalInSamples;
  bool lastVal;
  ClockRateDetector() {
    samplesSinceLastClock = 0;
    lastIntervalInSamples = 0;
    lastVal = false;
  }
  bool isStale() {
    return samplesSinceLastClock > (CTRL_SAMPLE_RATE<<1);
  }
  int GetInterval() {
    return lastIntervalInSamples;
  }
  void Process(bool triggered) {
    if(triggered && lastVal != triggered) {
      if(isStale()) {
        lastIntervalInSamples = samplesSinceLastClock;
      } else {
        lastIntervalInSamples = (lastIntervalInSamples + samplesSinceLastClock) >> 1;
      }
      samplesSinceLastClock = 0;
    } else {
      samplesSinceLastClock++;
    }
    lastVal = triggered;
  }
};

class GateDelay {
  bool buffer[CTRL_SAMPLE_RATE];
  bool initialized = false;
  int index = 0;
  int delay = 0;
  int wrapIndex(int x) {
    while(x>=CTRL_SAMPLE_RATE) x -= CTRL_SAMPLE_RATE;
    while(x<0) x += CTRL_SAMPLE_RATE;
    return x;
  }
public:
  void Init() {
    for(int i=0;i<CTRL_SAMPLE_RATE;i++) buffer[i]=0;
    initialized = true;
  }
  void SetDelay(int delaySamples) {
    delay = delaySamples;
  }
  int GetIndex() { return index; }
  bool Process(int gate) {
    int result = buffer[wrapIndex(index-delay)];
    index = wrapIndex(index+1);
    buffer[index] = gate;
    return result;
  }
};

TrigDetector buttonPressDetector;
TrigDetector trigDetector;
ClockRateDetector clockRateDetector;
ClockGenerator clockGenerator;
GateDelay gateDelay;

void writeIntToLED(int x) {
  digitalWrite(LED0, (x>>0)&1);
  digitalWrite(LED1, (x>>1)&1);
  digitalWrite(LED2, (x>>2)&1);
  digitalWrite(LED3, (x>>3)&1);
}

int scaleInterval(int interval, int factor) {
  bool shouldMul = factor > 0;
  int result = interval;
  factor = abs(factor);
  if((shouldMul)  && (factor > 0)) result = interval>>(factor-1);
  if((!shouldMul) && (factor > 0)) result = interval<<(factor-1);
  return result;
}

bool outputGate = 0;
bool lastSquareOutValue = 0;
int ledMask = 0;
int lastScaleFactor = 0;
int clockDividerIndex = 0;
int clockDividerLen = 1;
float analogOutVal = -1;

void ctrlLoop() {
    int buttonState = digitalRead(RESET_BUTTON);

    // Read pots + CVs
    int scaleFactorPot = -8+((analogRead(CHORD_POT_PIN)*17)>>13);
    int scaleFactorCV = ((analogRead(CHORD_CV_PIN)*9)>>13);
    int scaleFactor = scaleFactorPot + scaleFactorCV;

    writeIntToLED(abs(scaleFactorCV + scaleFactorPot) | ledMask);

    int offsetPot = analogRead(ROOT_POT_PIN);
    int clockInCV = analogRead(ROOT_CV_PIN);

    bool buttonPressed = buttonPressDetector.Process(buttonState ? 8191 : 0);
    bool triggered = trigDetector.Process(clockInCV);

    if(buttonPressed || triggered) {
      int expOffsetPot = (offsetPot*offsetPot)>>13;
      gateDelay.SetDelay((expOffsetPot*CTRL_SAMPLE_RATE)>>13);
    }

    clockRateDetector.Process(triggered);

    if(triggered) {

      if(clockDividerIndex == 0) {
        clockGenerator.Reset();
        clockGenerator.SetInterval(scaleInterval(clockRateDetector.GetInterval(), scaleFactor));
        clockGenerator.SetOffset(offsetPot);
      }

      if(scaleFactor != lastScaleFactor) {
        clockDividerIndex = 0;
        clockDividerLen = 1 << (scaleFactor < -1 ? (abs(scaleFactor)-1) : 0);
      }

      lastScaleFactor = scaleFactor;

      clockDividerIndex = (clockDividerIndex+1)%clockDividerLen;
    }

    bool delayedGateOut = gateDelay.Process(trigDetector.GetState() || buttonState);
    bool clockOut = clockGenerator.Process();
    bool pulseOut = clockGenerator.GetOffsetPhase() < CTRL_1MS*2;
    bool eighthPulseOut = clockGenerator.GetOffsetPhase() < (scaleInterval(clockRateDetector.GetInterval(), scaleFactor)>>3);
    bool squareOut = clockGenerator.GetSquare();
    bool fallingEdge = squareOut != lastSquareOutValue;
    lastSquareOutValue = squareOut;

    if(clockOut || !squareOut) outputGate = buttonState;

    ledMask = (pulseOut & eighthPulseOut) ? 0xF : 0x0;

    if(scaleFactor == 0) {
      digitalWrite(RESET_LED, delayedGateOut);
      digitalWrite(RESET_CV, delayedGateOut);
      analogWrite(A14, delayedGateOut);
    } else {
      digitalWrite(RESET_LED, squareOut && outputGate);
      digitalWrite(RESET_CV, squareOut && outputGate);
      analogWrite(A14, squareOut && outputGate);
    }
}

IntervalTimer ctrlTimer;

void setup(){
    pinMode(BANK_BUTTON,INPUT);
    pinMode(RESET_BUTTON, INPUT);
    pinMode(RESET_CV, INPUT); 
    pinMode(RESET_LED, OUTPUT);
    pinMode(LED0,OUTPUT);
    pinMode(LED1,OUTPUT);
    pinMode(LED2,OUTPUT);
    pinMode(LED3,OUTPUT);
    pinMode(A14,OUTPUT);
    analogReadRes(ADC_BITS);
    analogWriteRes(1);
    ctrlTimer.priority(200);
    ctrlTimer.begin(ctrlLoop, CTRL_TIMER_US);
    gateDelay.Init();
}

void loop() { }
