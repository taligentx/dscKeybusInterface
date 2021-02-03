/*
    DSC Keybus Interface

    https://github.com/taligentx/dscKeybusInterface

    This library is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dscKeypad.h"


#if defined(ESP32)
portMUX_TYPE dscKeypadInterface::timer0Mux = portMUX_INITIALIZER_UNLOCKED;

#if ESP_IDF_VERSION_MAJOR < 4
hw_timer_t * dscKeypadInterface::timer0 = NULL;

#else  // ESP-IDF 4+
esp_timer_handle_t timer2;
const esp_timer_create_args_t timer2Parameters = { .callback = reinterpret_cast<esp_timer_cb_t>(&dscKeypadInterface::dscClockInterrupt) };

#endif  // ESP_IDF_VERSION_MAJOR
#endif  // ESP32


dscKeypadInterface::dscKeypadInterface(byte setClockPin, byte setReadPin, byte setWritePin) {
  dscClockPin = setClockPin;
  dscReadPin = setReadPin;
  dscWritePin = setWritePin;
  alarmKeyData = 0xFF;
  commandReady = true;
  keyData = 0xFF;
  clockInterval = 57800;  // Sets AVR timer 1 to trigger an overflow interrupt every ~500us to generate a 1kHz clock signal
}


void dscKeypadInterface::begin(Stream &_stream) {
  pinMode(dscClockPin, OUTPUT);
  pinMode(dscReadPin, INPUT);
  pinMode(dscWritePin, OUTPUT);
  digitalWrite(dscClockPin, LOW);
  digitalWrite(dscWritePin, LOW);
  stream = &_stream;

  // Platform-specific timers setup the Keybus 1kHz clock signal

  // Arduino/AVR Timer1 calls ISR(TIMER1_OVF_vect)
  #if defined(__AVR__)
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = clockInterval;
  TCCR1B |= (1 << CS10);

  // esp8266 timer1 calls dscClockInterrupt()
  #elif defined(ESP8266)
  timer1_isr_init();
  timer1_attachInterrupt(dscClockInterrupt);
  timer1_write(2500);

  // esp32 timer0 calls dscClockInterrupt()
  #elif defined(ESP32)
  #if ESP_IDF_VERSION_MAJOR < 4
  timer0 = timerBegin(0, 80, true);
  timerStop(timer0);
  timerAttachInterrupt(timer0, &dscClockInterrupt, true);
  timerAlarmWrite(timer0, 500, true);
  timerAlarmEnable(timer0);
  #else  // IDF4+
  esp_timer_create(&timer2Parameters, &timer2);
  #endif  // ESP_IDF_VERSION_MAJOR
  #endif  // ESP32

  intervalStart = millis();
}


bool dscKeypadInterface::loop() {

  // Sets up the next panel command once the previous command is complete
  if (commandReady && millis() - intervalStart >= commandInterval) {
    commandReady = false;

    // Sets the startup command sequence
    if (startupCycle) {
      static byte startupCommand = 0x16;
      switch (startupCommand) {
        case 0x16: {
          for (byte i = 0; i < 5; i++) panelCommand[i] = panelCommand16[i];
          panelCommandByteTotal = 5;
          startupCommand = 0x5D;
          break;
        }
        case 0x5D: {
          delay(200);
          for (byte i = 0; i < 7; i++) panelCommand[i] = panelCommand5D[i];
          panelCommandByteTotal = 7;
          startupCommand = 0x4C;
          break;
        }
        case 0x4C: {
          for (byte i = 0; i < 12; i++) panelCommand[i] = panelCommand4C[i];
          panelCommandByteTotal = 12;
          startupCommand = 0xB1;
          break;
        }
        case 0xB1: {
          for (byte i = 0; i < 10; i++) panelCommand[i] = panelCommandB1[i];
          panelCommandByteTotal = 10;
          startupCommand = 0xA5;
          break;
        }
        case 0xA5: {
          for (byte i = 0; i < 8; i++) panelCommand[i] = panelCommandA5[i];
          panelCommandByteTotal = 8;
          startupCommand = 0x05;
          break;
        }
        case 0x05: {
          for (byte i = 0; i < 5; i++) panelCommand[i] = panelCommand05[i];
          panelCommandByteTotal = 5;
          startupCommand = 0xD5;
          break;
        }
        case 0xD5: {
          for (byte i = 0; i < 9; i++) panelCommand[i] = panelCommandD5[i];
          panelCommandByteTotal = 9;
          startupCommand = 0x27;
          break;
        }
        case 0x27: {
          for (byte i = 0; i < 7; i++) panelCommand[i] = panelCommand27[i];
          panelCommandByteTotal = 7;
          startupCycle = false;
          break;
        }
      }
    }

    // Sets the next panel command to 0x1C alarm key verification if an alarm key is pressed
    else if (alarmKeyDetected) {
      alarmKeyDetected = false;
      alarmKeyResponsePending = true;
      panelCommand[0] = 0x1C;
      panelCommandByteTotal = 1;
    }

    // Sets the next panel command
    else if (!alarmKeyResponsePending) {

      // Sets lights
      if (panelLights != previousLights) {
        previousLights = panelLights;
        panelCommand05[1] = panelLights;
        panelCommand27[1] = panelLights;
      }

      // Sets next panel command to 0xD5 keypad zone query on keypad zone notification
      if (panelCommand[0] == 0x05 && !bitRead(moduleData[5], 2)) {
        for (byte i = 0; i < 9; i++) panelCommand[i] = panelCommandD5[i];
        panelCommandByteTotal = 9;
      }

      // Sets next panel command to 0x27 zones 1-8 status if a zone changed
      else if (panelZones != previousZones) {
        previousZones = panelZones;
        panelCommand27[5] = panelZones;

        int dataSum = 0;
        for (byte panelByte = 0; panelByte < 6; panelByte++) dataSum += panelCommand27[panelByte];
        panelCommand27[6] = dataSum % 256;

        for (byte i = 0; i < 7; i++) panelCommand[i] = panelCommand27[i];
        panelCommandByteTotal = 7;
      }

      // Sets next panel command to 0x05 status command
      else {
        for (byte i = 0; i < 5; i++) panelCommand[i] = panelCommand05[i];
        panelCommandByteTotal = 5;
      }
    }
    clockCycleCount = 0;
    clockCycleTotal = (panelCommandByteTotal * 16) + 4;

    #if defined(__AVR__)
    TIMSK1 |= (1 << TOIE1);  // Enables AVR Timer 1 interrupt
    #elif defined(ESP8266)
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
    #elif defined(ESP32)
    #if ESP_IDF_VERSION_MAJOR < 4
    timerStart(timer0);
    #else  // IDF4+
    esp_timer_start_periodic(timer2, 500);
    #endif  // ESP_IDF_VERSION_MAJOR
    #endif
  }
  else if (!commandReady) intervalStart = millis();

  // Sets panel lights
  if (Ready) bitWrite(panelLights, 0, 1);
  else bitWrite(panelLights, 0, 0);
  if (Armed) bitWrite(panelLights, 1, 1);
  else bitWrite(panelLights, 1, 0);
  if (Memory) bitWrite(panelLights, 2, 1);
  else bitWrite(panelLights, 2, 0);
  if (Bypass) bitWrite(panelLights, 3, 1);
  else bitWrite(panelLights, 3, 0);
  if (Trouble) bitWrite(panelLights, 4, 1);
  else bitWrite(panelLights, 4, 0);
  if (Program) bitWrite(panelLights, 5, 1);
  else bitWrite(panelLights, 5, 0);
  if (Fire) bitWrite(panelLights, 6, 1);
  else bitWrite(panelLights, 6, 0);
  if (Backlight) bitWrite(panelLights, 7, 1);
  else bitWrite(panelLights, 7, 0);

  // Sets zone lights
  if (Zone1) bitWrite(panelZones, 0, 1);
  else bitWrite(panelZones, 0, 0);
  if (Zone2) bitWrite(panelZones, 1, 1);
  else bitWrite(panelZones, 1, 0);
  if (Zone3) bitWrite(panelZones, 2, 1);
  else bitWrite(panelZones, 2, 0);
  if (Zone4) bitWrite(panelZones, 3, 1);
  else bitWrite(panelZones, 3, 0);
  if (Zone5) bitWrite(panelZones, 4, 1);
  else bitWrite(panelZones, 4, 0);
  if (Zone6) bitWrite(panelZones, 5, 1);
  else bitWrite(panelZones, 5, 0);
  if (Zone7) bitWrite(panelZones, 6, 1);
  else bitWrite(panelZones, 6, 0);
  if (Zone8) bitWrite(panelZones, 7, 1);
  else bitWrite(panelZones, 7, 0);

  if (moduleDataCaptured) {
    moduleDataCaptured = false;
    if (alarmKeyData != 0xFF) {
      KeyAvailable = true;
      switch (alarmKeyData) {
        case 0xBB: Key = 0x0B; break;  // Fire alarm
        case 0xDD: Key = 0x0D; break;  // Aux alarm
        case 0xEE: Key = 0x0E; break;  // Panic alarm
        default: KeyAvailable = false; break;
      }
      alarmKeyData = 0xFF;
    }
    else if (keyData != 0xFF) {
      KeyAvailable = true;
      switch (keyData) {
        case 0x00: Key = 0x00; break;  // 0
        case 0x05: Key = 0x05; break;  // 1
        case 0x0A: Key = 0x0A; break;  // 2
        case 0x0F: Key = 0x0F; break;  // 3
        case 0x11: Key = 0x11; break;  // 4
        case 0x16: Key = 0x16; break;  // 5
        case 0x1B: Key = 0x1B; break;  // 6
        case 0x1C: Key = 0x1C; break;  // 7
        case 0x22: Key = 0x22; break;  // 8
        case 0x27: Key = 0x27; break;  // 9
        case 0x28: Key = 0x28; break;  // *
        case 0x2D: Key = 0x2D; break;  // #
        case 0x82: Key = 0x82; break;  // Enter
        case 0x87: Key = 0x87; break;  // Right arrow
        case 0x88: Key = 0x88; break;  // Left arrow
        case 0xAF: Key = 0xAF; break;  // Arm: Stay
        case 0xB1: Key = 0xB1; break;  // Arm: Away
        case 0xBB: Key = 0xBB; break;  // Door chime
        case 0xDA: Key = 0xDA; break;  // Reset
        case 0xE1: Key = 0xE1; break;  // Quick exit
        case 0xF7: Key = 0xF7; break;  // LCD keypad navigation
        default: KeyAvailable = false; break;
      }
      keyData = 0xFF;
    }
  }

  return true;
}


#if defined(__AVR__)
void dscKeypadInterface::dscClockInterrupt() {
#elif defined(ESP8266)
void ICACHE_RAM_ATTR dscKeypadInterface::dscClockInterrupt() {
#elif defined(ESP32)
void IRAM_ATTR dscKeypadInterface::dscClockInterrupt() {
#endif

  // Toggles the clock pin for the length of a panel command
  if (clockCycleCount < clockCycleTotal) {
    static bool clockHigh = true;
    if (clockHigh) {
      clockHigh = false;
      digitalWrite(dscClockPin, HIGH);
      digitalWrite(dscWritePin, LOW);
    }
    else {
      clockHigh = true;
      digitalWrite(dscClockPin, LOW);
      if (isrModuleByteCount < dscReadSize) {

        // Data is captured in each byte by shifting left by 1 bit and writing to bit 0
        if (isrModuleBitCount < 8) {
          isrModuleData[isrModuleByteCount] <<= 1;
          if (digitalRead(dscReadPin) == HIGH) {
            isrModuleData[isrModuleByteCount] |= 1;
          }
          else {
            moduleDataDetected = true;  // Keypads and modules send data by pulling the data line low
          }
        }

        // Stores the stop bit by itself in byte 1 - this aligns the Keybus bytes with moduleData[] bytes
        if (isrModuleBitTotal == 8) {
          isrModuleData[1] = 1;  // Sets the stop bit manually to 1 in byte 1
          isrModuleBitCount = 0;
          isrModuleByteCount++;
        }

        // Increments the bit counter if the byte is incomplete
        else if (isrModuleBitCount < 7) {
          isrModuleBitCount++;
        }

        // Byte is complete, set the counters for the next byte
        else {
          isrModuleBitCount = 0;
          isrModuleByteCount++;
        }

        isrModuleBitTotal++;
      }

      // Write panel data
      if (isrPanelBitTotal == 8) {
        digitalWrite(dscWritePin, HIGH);  // Stop bit
        if (panelCommand[0] == 0x1C) {
          alarmKeyResponsePending = false;
          alarmKeyData = isrModuleData[0];
        }
        isrPanelBitTotal++;
      }
      else if (isrPanelBitCount == 7) {
        if (!bitRead(panelCommand[panelCommandByteCount], 0)) digitalWrite(dscWritePin, HIGH);
        isrPanelBitCount = 0;
        isrPanelBitTotal++;
        panelCommandByteCount++;
      }
      else if (panelCommandByteCount < panelCommandByteTotal) {
        switch (isrPanelBitCount) {
          case 0: if (!bitRead(panelCommand[panelCommandByteCount], 7)) digitalWrite(dscWritePin, HIGH); break;
          case 1: if (!bitRead(panelCommand[panelCommandByteCount], 6)) digitalWrite(dscWritePin, HIGH); break;
          case 2: if (!bitRead(panelCommand[panelCommandByteCount], 5)) digitalWrite(dscWritePin, HIGH); break;
          case 3: if (!bitRead(panelCommand[panelCommandByteCount], 4)) digitalWrite(dscWritePin, HIGH); break;
          case 4: if (!bitRead(panelCommand[panelCommandByteCount], 3)) digitalWrite(dscWritePin, HIGH); break;
          case 5: if (!bitRead(panelCommand[panelCommandByteCount], 2)) digitalWrite(dscWritePin, HIGH); break;
          case 6: if (!bitRead(panelCommand[panelCommandByteCount], 1)) digitalWrite(dscWritePin, HIGH); break;
        }
        isrPanelBitCount++;
        isrPanelBitTotal++;
      }
    }
    clockCycleCount++;
  }

  // Panel command complete
  else {
    digitalWrite(dscClockPin, LOW);

    // Checks for module data
    if (moduleDataDetected) {
      moduleDataDetected = false;
      moduleDataCaptured = true;
      for (byte i = 0; i < dscReadSize; i++) moduleData[i] = isrModuleData[i];

      // Checks for an alarm key press
      if (isrModuleData[0] != 0xFF && panelCommand[0] != 0x1C) {
        alarmKeyDetected = true;
      }

      // Checks for a partition 1 key
      if (isrModuleData[2] != 0xFF && panelCommand[0] == 0x05) {
        keyData = isrModuleData[2];
      }
    }

    // Resets counters
    for (byte i = 0; i < dscReadSize; i++) isrModuleData[i] = 0;
    isrModuleBitTotal = 0;
    isrModuleBitCount = 0;
    isrModuleByteCount = 0;
    panelCommandByteCount = 0;
    isrPanelBitTotal = 0;
    isrPanelBitCount = 0;
    commandReady = true;
    #if defined(__AVR__)
    TIMSK1 = 0;  // Disables AVR Timer 1 interrupt
    #elif defined(ESP8266)
    timer1_disable();
    #elif defined(ESP32)
    #if ESP_IDF_VERSION_MAJOR < 4
    timerStop(timer0);
    #else  // IDF4+
    esp_timer_stop(timer2);
    #endif  // ESP_IDF_VERSION_MAJOR
    #endif
  }

  #if defined(__AVR__)
  TCNT1 = clockInterval;
  #endif
}
