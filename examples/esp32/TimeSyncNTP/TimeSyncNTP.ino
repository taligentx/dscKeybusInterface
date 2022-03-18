/*
 *  TimeSyncNTP 1.0 (esp32)
 *
 *  Sets the security system time from an NTP server.  Note that DSC panels begin their clock seconds at power-on
 *  and do not reset seconds to zero when the time is set in programming.  This results in a potential error of up
 *  to 59 seconds in the DSC clock.  This error can be reduced by powering on the panel 3 seconds before the start
 *  of a new minute (to account for power-up time).
 *
 *  Release notes:
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Aux(+) --- 5v voltage regulator --- esp32 development board 5v pin
 *
 *      DSC Aux(-) --- esp32 Ground
 *
 *                                         +--- dscClockPin  // Default: 18
 *      DSC Yellow --- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin   // Default: 19
 *      DSC Green ---- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *      Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin  // Default: 21
 *            Ground --- NPN emitter --/
 *
 *  Virtual keypad uses an NPN transistor to pull the data line low - most small signal NPN transistors should
 *  be suitable, for example:
 *   -- 2N3904
 *   -- BC547, BC548, BC549
 *
 *  Issues and (especially) pull requests are welcome:
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *  This example code is in the public domain.
 */

#include <WiFi.h>
#include <dscKeybusInterface.h>
#include <time.h>

// Settings
const char* wifiSSID = "";
const char* wifiPassword = "";
const char* accessCode = "";             // Set the access code to set the panel time
const long  timeZoneOffset = -5 * 3600;  // Offset from UTC in seconds (example: US Eastern is UTC - 5)
const int   daylightOffset = 1 * 3600;   // Daylight savings time offset in seconds
const char* ntpServer = "pool.ntp.org";  // Set the NTP server
const byte  timePartition = 1;           // Set the partition to use for setting the time

// Configures the Keybus interface with the specified pins.
#define dscClockPin 18  // 4,13,16-39
#define dscReadPin  19  // 4,13,16-39
#define dscWritePin 21  // 4,13,16-33

// Initialize components
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
WiFiClient ipClient;
bool wifiConnected = true;
bool ntpSynced = true;
byte ntpOffset;
tm ntpTime;


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  Serial.print(F("WiFi...."));
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print(F("connected: "));
  Serial.println(WiFi.localIP());

  Serial.print(F("NTP time...."));
  configTime(timeZoneOffset, daylightOffset, ntpServer);  // Initiates the NTP client, synced hourly
  while (!getLocalTime(&ntpTime)) {
    Serial.print(".");
    delay(2000);
  }
  Serial.println(&ntpTime, "synchronized: %a %b %d %Y %H:%M:%S");

  // Starts the Keybus interface
  dsc.begin();
  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {

  // Updates status if WiFi drops and reconnects
  if (!wifiConnected && WiFi.status() == WL_CONNECTED) {
    Serial.println(F("WiFi reconnected"));
    wifiConnected = true;
    dsc.pauseStatus = false;
    dsc.statusChanged = true;
  }
  else if (WiFi.status() != WL_CONNECTED && wifiConnected) {
    Serial.println(F("WiFi disconnected"));
    wifiConnected = false;
    dsc.pauseStatus = true;
  }

  dsc.loop();

  // Sets the time if not synchronized
  if (!ntpSynced) {
    if (getLocalTime(&ntpTime)) {
      int ntpSecond = ntpTime.tm_sec;

      if (ntpSecond == ntpOffset) {
        int ntpYear = ntpTime.tm_year + 1900;
        int ntpMonth = ntpTime.tm_mon + 1;
        int ntpDay = ntpTime.tm_mday;
        int ntpHour = ntpTime.tm_hour;
        int ntpMinute = ntpTime.tm_min;
        if (dsc.ready[timePartition - 1] && dsc.setTime(ntpYear, ntpMonth, ntpDay, ntpHour, ntpMinute, accessCode, timePartition)) {
          ntpSynced = true;
          Serial.println(F("Time synchronizing"));
        }
      }
    }
    else Serial.println(F("NTP connection error"));
  }

  if (dsc.statusChanged) {      // Checks if the security system status has changed
    dsc.statusChanged = false;  // Reset the status tracking flag

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // loop() more often, or increase dscBufferSize in the library: src/dscKeybus.h
    if (dsc.bufferOverflow) {
      Serial.println(F("Keybus buffer overflow"));
      dsc.bufferOverflow = false;
    }

    // Checks if the interface is connected to the Keybus
    if (dsc.keybusChanged) {
      dsc.keybusChanged = false;                 // Resets the Keybus data status flag
      if (dsc.keybusConnected) Serial.println(F("Keybus connected"));
      else Serial.println(F("Keybus disconnected"));
    }

    /*  Checks for a panel timestamp
     *
     *  The panel time can be set using dsc.setTime(year, month, day, hour, minute, "accessCode", partition) - the
     *  partition is optional and defaults to partition 1:
     *
     *    dsc.setTime(2015, 12, 21, 20, 38, "1234")     # Sets 2015.12.21 20:38 with access code 1234
     *    dsc.setTime(2020, 05, 30, 15, 22, "1234", 2)  # Sets 2020.05.30 15:22 with access code 1234 on partition 2
     */
    if (dsc.timestampChanged) {
      dsc.timestampChanged = false;
      Serial.print(F("Timestamp: "));
      Serial.print(dsc.year);                  // Returns year as a 4-digit unsigned int
      Serial.print(".");
      if (dsc.month < 10) Serial.print("0");
      Serial.print(dsc.month);                 // Returns month as a byte
      Serial.print(".");
      if (dsc.day < 10) Serial.print("0");
      Serial.print(dsc.day);                   // Returns day as a byte
      Serial.print(" ");
      if (dsc.hour < 10) Serial.print("0");
      Serial.print(dsc.hour);                  // Returns hour as a byte
      Serial.print(":");
      if (dsc.minute < 10) Serial.print("0");
      Serial.println(dsc.minute);              // Returns minute as a byte

      // Checks the current NTP time
      if (getLocalTime(&ntpTime)) {
        int ntpYear = ntpTime.tm_year + 1900;
        int ntpMonth = ntpTime.tm_mon + 1;
        int ntpDay = ntpTime.tm_mday;
        int ntpHour = ntpTime.tm_hour;
        int ntpMinute = ntpTime.tm_min;
        int ntpSecond = ntpTime.tm_sec;

        // Checks if the time is synchronized
        if (dsc.ready[timePartition - 1] && (dsc.year != ntpYear || dsc.month != ntpMonth || dsc.day != ntpDay || dsc.hour != ntpHour || dsc.minute != ntpMinute)) {
          ntpOffset = ntpSecond;
          ntpSynced = false;
        }
      }
      else Serial.println(F("NTP connection error."));
    }
  }
}
