/*
 *  TimeSyncNTP 1.0 (esp8266)
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
 *      DSC Aux(+) --- 5v voltage regulator --- esp8266 development board 5v pin (NodeMCU, Wemos)
 *
 *      DSC Aux(-) --- esp8266 Ground
 *
 *                                         +--- dscClockPin (esp8266: D1, D2, D8)
 *      DSC Yellow --- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin (esp8266: D1, D2, D8)
 *      DSC Green ---- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *  Virtual keypad:
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin (esp8266: D1, D2, D8)
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

#include <ESP8266WiFi.h>
#include <dscKeybusInterface.h>
#include <time.h>
#include <TZ.h>

// Settings
const char* wifiSSID = "";
const char* wifiPassword = "";
const char* accessCode = "";             // Set the access code to set the panel time
#define ntpTimeZone TZ_Etc_UTC           // Set the time zone (includes DST): https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h
const char* ntpServer = "pool.ntp.org";  // Set the NTP server
const byte timePartition = 1;            // Set the partition to use for setting the time

// Configures the Keybus interface with the specified pins.
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin  D2  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscWritePin D8  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)

// Initialize components
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
WiFiClient ipClient;
bool wifiConnected = true;
bool ntpSynced = true;
byte ntpOffset;
time_t ntpNow;
tm ntpTime;


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  Serial.print(F("WiFi..."));
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print(F("connected: "));
  Serial.println(WiFi.localIP());

  Serial.print(F("NTP time..."));
  configTime(ntpTimeZone, ntpServer);  // Initiates the NTP client, synced hourly
  time(&ntpNow);
  while (ntpNow < 1606784461)
  {
    Serial.print(".");
    delay(2000);
    time(&ntpNow);
  }
  Serial.print(F("synchronized: "));
  Serial.print(ctime(&ntpNow));

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
    time(&ntpNow);
    localtime_r(&ntpNow, &ntpTime);
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

  if (dsc.statusChanged) {      // Checks if the security system status has changed
    dsc.statusChanged = false;  // Reset the status tracking flag

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // loop() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
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
      time(&ntpNow);
      localtime_r(&ntpNow, &ntpTime);
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
  }
}
