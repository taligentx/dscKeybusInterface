/*
 *  TimeSyncNTP 1.0 (Arduino with Ethernet)
 *
 *  Sets the security system time from an NTP server using the Time library from the
 *  Arduino IDE/PlatformIO library manager: https://github.com/PaulStoffregen/Time
 *
 *  Note that DSC panels begin their clock seconds at power-on and do not reset seconds
 *  to zero when the time is set in programming.  This results in a potential error of up
 *  to 59 seconds in the DSC clock.  This error can be reduced by powering on the panel
 *  3 seconds before the start of a new minute (to account for power-up time).
 *
 *  Release notes:
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Aux(+) --- Arduino Vin pin
 *
 *      DSC Aux(-) --- Arduino Ground
 *
 *                                         +--- dscClockPin (Arduino Uno: 2,3)
 *      DSC Yellow --- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin (Arduino Uno: 2-12)
 *      DSC Green ---- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *      Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin (Arduino Uno: 2-12)
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

#include <TimeLib.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>
#include <dscKeybusInterface.h>

#define DST_US 1
#define DST_EU 2

// Settings
const char* accessCode = "";             // Set the access code to set the panel time
const char* ntpServer = "pool.ntp.org";  // Set the NTP server
const int timeZoneOffset = -5;           // Offset from UTC in hours (example: US Eastern is UTC - 5)
const byte dstRegion = DST_US;           // Set to DST_US, DST_EU, or 0 to disable DST adjustments
const byte timePartition = 1;            // Set the partition to use for setting the time
byte mac[] = { 0xAA, 0x61, 0x0A, 0x00, 0x00, 0x01 };  // Set a MAC address unique to the local network

// Configures the Keybus interface with the specified pins
#define dscClockPin 3  // Arduino Uno hardware interrupt pin: 2,3
#define dscPC16Pin  4  // DSC Classic Series only, Arduino Uno: 2-12
#define dscReadPin  5  // Arduino Uno: 2-12
#define dscWritePin 6  // Arduino Uno: 2-12

// Initialize components
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
EthernetUDP ipClient;
byte ntpBuffer[48];
unsigned int localPort = 8888;
bool ntpSynced = true;
byte ntpOffset;


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  // Initializes ethernet with DHCP
  Serial.print(F("Ethernet...."));
  while(!Ethernet.begin(mac)) {
      Serial.print(".");
      delay(1000);
  }
  Serial.print(F("connected: "));
  Serial.println(Ethernet.localIP());
  ipClient.begin(localPort);

  Serial.print(F("NTP time...."));
  setSyncProvider(getDstCorrectedTime);  // Initiates the NTP client, synced hourly
  setSyncInterval(3600);
  while (timeStatus() != timeSet) {
    Serial.print(".");
    delay(2000);
  }
  Serial.print(F("synchronized: "));
  time_t ntpTime = now();
  Serial.print(year(ntpTime));
  Serial.print(".");
  if (month(ntpTime) < 10) Serial.print("0");
  Serial.print(month(ntpTime));
  Serial.print(".");
  if (day(ntpTime) < 10) Serial.print("0");
  Serial.print(day(ntpTime));
  Serial.print(" ");
  if (hour(ntpTime) < 10) Serial.print("0");
  Serial.print(hour(ntpTime));
  Serial.print(":");
  if (minute(ntpTime) < 10) Serial.print("0");
  Serial.print(minute(ntpTime));
  Serial.print(":");
  if (second(ntpTime) < 10) Serial.print("0");
  Serial.println(second(ntpTime));

  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), etc.
  dsc.begin();
  Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {
  dsc.loop();

  // Sets the time if not synchronized
  if (!ntpSynced && timeStatus() == timeSet) {
    time_t ntpTime = now();
    int ntpSecond = second(ntpTime);

    if (ntpSecond == ntpOffset) {
      int ntpYear = year(ntpTime);
      int ntpMonth = month(ntpTime);
      int ntpDay = day(ntpTime);
      int ntpHour = hour(ntpTime);
      int ntpMinute = minute(ntpTime);
      if (dsc.ready[timePartition - 1] && dsc.setTime(ntpYear, ntpMonth, ntpDay, ntpHour, ntpMinute, accessCode, timePartition)) {
        ntpSynced = true;
        Serial.println(F("Time synchronizing"));
      }
    }
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
      if (timeStatus() == timeSet) {
        time_t ntpTime = now();
        int ntpYear = year(ntpTime);
        int ntpMonth = month(ntpTime);
        int ntpDay = day(ntpTime);
        int ntpHour = hour(ntpTime);
        int ntpMinute = minute(ntpTime);
        int ntpSecond = second(ntpTime);

        // Checks if the time is synchronized
        if (dsc.ready[timePartition - 1] && (dsc.year != ntpYear || dsc.month != ntpMonth || dsc.day != ntpDay || dsc.hour != ntpHour || dsc.minute != ntpMinute)) {
          ntpOffset = ntpSecond;
          ntpSynced = false;
        }
      }
    }
  }
}


/*
 * Time handling from the Time library TimeNTP_ENC28J60 example,
 * modified to add support for selecting US DST:
 * https://github.com/PaulStoffregen/Time/blob/master/examples/TimeNTP_ENC28J60/TimeNTP_ENC28J60.ino
 */

/* This function returns the DST offset for the current UTC time:
 * http://www.webexhibits.org/daylightsaving/i.html
 *
 * Results have been checked for 2012-2030 (but should work since
 * 1996 to 2099) against the following references:
 * - http://www.uniquevisitor.it/magazine/ora-legale-italia.php
 * - http://www.calendario-365.it/ora-legale-orario-invernale.html
 */
byte dstOffset(byte day, byte month, unsigned int year, byte hour) {
  if (!dstRegion) return 0;
  byte startMonth, stopMonth, dstOn, dstOff;

  if (dstRegion == DST_US) {
    startMonth = 3;
    stopMonth = 11;
    dstOn = (1 + year * 5 / 4) % 7;
    dstOff = (1 + year * 5 / 4) % 7;
  }
  else if (dstRegion == DST_EU) {
    startMonth = 3;
    stopMonth = 10;
    dstOn = (31 - (5 * year / 4 + 4) % 7);
    dstOff = (31 - (5 * year / 4 + 1) % 7);
  }

  if ((month > startMonth && month < stopMonth) ||
      (month == startMonth && (day > dstOn || (day == dstOn && hour >= 1))) ||
      (month == stopMonth && (day < dstOff || (day == dstOff && hour <= 1))))
    return 1;
  else return 0;
}


time_t getDstCorrectedTime(void) {
  time_t ntpNow = getNtpTime();
  if (ntpNow > 0) {
    TimeElements tm;
    breakTime (ntpNow, tm);
    ntpNow += (timeZoneOffset + dstOffset(tm.Day, tm.Month, tm.Year + 1970, tm.Hour)) * SECS_PER_HOUR;
  }
  return ntpNow;
}


time_t getNtpTime() {
  while (ipClient.parsePacket() > 0) ;
  memset(ntpBuffer, 0, 48);
  ntpBuffer[0]  = 0b11100011;
  ntpBuffer[1]  = 0;
  ntpBuffer[2]  = 6;
  ntpBuffer[3]  = 0xEC;
  ntpBuffer[12] = 49;
  ntpBuffer[13] = 0x4E;
  ntpBuffer[14] = 49;
  ntpBuffer[15] = 52;
  ipClient.beginPacket(ntpServer, 123);
  ipClient.write(ntpBuffer, 48);
  ipClient.endPacket();

  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = ipClient.parsePacket();
    if (size >= 48) {
      ipClient.read(ntpBuffer, 48);
      unsigned long secsSince1900;
      secsSince1900 =  (unsigned long)ntpBuffer[40] << 24;
      secsSince1900 |= (unsigned long)ntpBuffer[41] << 16;
      secsSince1900 |= (unsigned long)ntpBuffer[42] << 8;
      secsSince1900 |= (unsigned long)ntpBuffer[43];
      return secsSince1900 - 2208988800UL;
    }
  }

  Serial.println(F("NTP connection error"));
  return 0;
}
