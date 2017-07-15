#include <elapsedMillis.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <string.h>

extern "C" {
#include "user_interface.h"
}

const int kShiftSER     = D1;
const int kShiftSRCLK   = D2;
const int kShiftRCLK    = D4;
const int kBuzzerPin    = D5;
const int kInterruptPin = D8;

const int kTicksPerCycle = 180;

os_timer_t MainTimer;
uint64_t Pattern[kTicksPerCycle];

elapsedMicros CurrentCycleStopwatch = 0;
const int kDebouncingThreshold = 5000; // for now

volatile int RealizedTicks = kTicksPerCycle;
volatile double whole_cycle = 20000;
volatile double realized_in = whole_cycle;
volatile double timer_period = whole_cycle / kTicksPerCycle;

void ICACHE_RAM_ATTR Interrupt() {
  double current_time = CurrentCycleStopwatch;
  if (current_time > kDebouncingThreshold) {
    whole_cycle = current_time;
    double real_timer_period;
    if (RealizedTicks == kTicksPerCycle) {
      real_timer_period = realized_in / kTicksPerCycle;
    } else {
      real_timer_period = whole_cycle / RealizedTicks;
    }
    double timer_period_correction = real_timer_period / timer_period;
    timer_period = timer_period_correction * (whole_cycle / kTicksPerCycle); 
    os_timer_disarm(&MainTimer);
    os_timer_arm_us(&MainTimer, (unsigned int)timer_period, true);
    RealizedTicks = 0;
    CurrentCycleStopwatch = 0;
  }
}

void MainTimerCallback(void *pArg) {
  if (RealizedTicks >= 0  && RealizedTicks < kTicksPerCycle) {
    LED_Set60BitShift(Pattern[RealizedTicks]);
    LED_Refresh();
    RealizedTicks++;
    if (RealizedTicks == kTicksPerCycle) realized_in = CurrentCycleStopwatch;
  }
}

////////////////////////////////////////////////////////////////////////////////
// WiFi
////////////////////////////////////////////////////////////////////////////////

const char* kWiFiSSID = "Temp";
const char* kWiFiPassword = "pajopajo95";
const int   kUdpServerPort = 12345;
WiFiUDP udp_server;

////////////////////////////////////////////////////////////////////////////////
//  Buzzer
////////////////////////////////////////////////////////////////////////////////

const int kNoteC7 = 2093;
const int kNoteD7 = 2349;
const int kNoteE7 = 2637;
const int kNoteF7 = 2794;
const int kNoteG7 = 3136;
const int kNoteA7 = 3520;
const int kNoteB7 = 5951;

inline void _Buzz(uint8_t pin, unsigned int frequency, unsigned long duration) {
  if (frequency) {
    analogWriteFreq(frequency);
    analogWrite(pin, 500);
  }
  delay(duration);
  analogWrite(pin, 0);
}

inline void BuzzPositive() {
  int d = 50;
  _Buzz(kBuzzerPin, kNoteC7, d);
  delay(d);
  _Buzz(kBuzzerPin, kNoteD7, d);
  delay(d);
  _Buzz(kBuzzerPin, kNoteE7, d);
  delay(d);
  _Buzz(kBuzzerPin, kNoteF7, d);
  delay(d);
  _Buzz(kBuzzerPin, kNoteG7, d);
  delay(d);
  _Buzz(kBuzzerPin, kNoteA7, d);
  delay(d);
  _Buzz(kBuzzerPin, kNoteB7, d);
  delay(d);
}

inline void BuzzNegative() {
  int d = 50;
  _Buzz(kBuzzerPin, kNoteB7, d);
  delay(d);
  _Buzz(kBuzzerPin, kNoteA7, d);
  delay(d);
  _Buzz(kBuzzerPin, kNoteG7, d);
  delay(d);
  _Buzz(kBuzzerPin, kNoteF7, d);
  delay(d);
  _Buzz(kBuzzerPin, kNoteE7, d);
  delay(d);
  _Buzz(kBuzzerPin, kNoteD7, d);
  delay(d);
  _Buzz(kBuzzerPin, kNoteC7, d);
  delay(d);
}

////////////////////////////////////////////////////////////////////////////////
// LED Control
////////////////////////////////////////////////////////////////////////////////

inline void _LED_ShiftOneBit(bool set) {
  digitalWrite(kShiftSER, set ? HIGH : LOW);
  digitalWrite(kShiftSRCLK, HIGH);
  digitalWrite(kShiftSRCLK, LOW);
}

inline void LED_Set60BitShift(uint64_t x) {
  for (int i = 0; i < 60; i++) {
    _LED_ShiftOneBit(x & 1);
    x >>= 1;
  }
  _LED_ShiftOneBit(false);
  _LED_ShiftOneBit(false);
  _LED_ShiftOneBit(false);
  _LED_ShiftOneBit(false);
}

inline void LED_Refresh() {
  digitalWrite(kShiftRCLK, LOW);
  digitalWrite(kShiftRCLK, HIGH);
}

////////////////////////////////////////////////////////////////////////////////
//  Main
////////////////////////////////////////////////////////////////////////////////

void setup() {
  // 1) Initialize pins.
  pinMode(kShiftSER, OUTPUT);
  pinMode(kShiftSRCLK, OUTPUT);
  pinMode(kShiftRCLK, OUTPUT);  
  pinMode(kBuzzerPin, OUTPUT);
  digitalWrite(kShiftSER, LOW);
  digitalWrite(kShiftSRCLK, LOW);
  digitalWrite(kShiftRCLK, LOW);
  digitalWrite(kBuzzerPin, LOW);
  // 2) Setup initial pattern.
  memset(Pattern, 0, sizeof(Pattern));
  // 3) Create the main timer.
  os_timer_setfn(&MainTimer, MainTimerCallback, NULL);
  // 4) Intialize the HW interrupt on each cycle pass.
  attachInterrupt(digitalPinToInterrupt(kInterruptPin), Interrupt, HIGH);
  // 5) Connect to WiFi.
  WiFi.begin(kWiFiSSID, kWiFiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    BuzzNegative();
    delay(500);
  }
  BuzzPositive();
  // 6) Run UDP server.
  while (!udp_server.begin(kUdpServerPort)) {}
  BuzzPositive();
  
}

void loop() {
  int k;
  if ((k = udp_server.parsePacket()) > 0) {
    if (k != 2) {
      udp_server.flush();
      return;
    }
    int circle = udp_server.read();
    int part = udp_server.read();
    Pattern[part] ^= 1llu << (59 - circle);
  }
}


