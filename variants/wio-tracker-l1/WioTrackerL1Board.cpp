#include <Arduino.h>
#include <Wire.h>

#include "WioTrackerL1Board.h"

// [mishmesh] runtime battery ADC trim; 1.0 = no correction. The mishmesh UI sets
// this from the persisted calibration percent (at boot and when the user adjusts
// it) so on-device %, the phone-app battery report, and mesh telemetry all agree.
// Non-mishmesh envs never write it, so their behavior is unchanged.
float mishmeshBatteryCalFactor = 1.0f;
// [/mishmesh]

void WioTrackerL1Board::begin() {
  NRF52BoardDCDC::begin();
  btn_prev_state = HIGH;

  pinMode(PIN_VBAT_READ, INPUT); // VBAT ADC input
  // Set all button pins to INPUT_PULLUP
  pinMode(PIN_BUTTON1, INPUT_PULLUP);
  pinMode(PIN_BUTTON2, INPUT_PULLUP);
  pinMode(PIN_BUTTON3, INPUT_PULLUP);
  pinMode(PIN_BUTTON4, INPUT_PULLUP);
  pinMode(PIN_BUTTON5, INPUT_PULLUP);
  pinMode(PIN_BUTTON6, INPUT_PULLUP);
  

  #if defined(PIN_WIRE_SDA) && defined(PIN_WIRE_SCL)
    Wire.setPins(PIN_WIRE_SDA, PIN_WIRE_SCL);
  #endif

  Wire.begin();

  #ifdef P_LORA_TX_LED
    pinMode(P_LORA_TX_LED, OUTPUT);
    digitalWrite(P_LORA_TX_LED, LOW);
  #endif

  delay(10);   // give sx1262 some time to power up
}
