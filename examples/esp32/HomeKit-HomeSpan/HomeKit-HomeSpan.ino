/*
 *  HomeKit-HomeSpan 1.0 (esp32)
 *
 *  Processes the security system status and allows for control using Apple HomeKit, including the Home app
 *  and Siri.  This example uses HomeSpan to interface the esp32 directly with HomeKit without requiring
 *  a separate service or device.
 *
 *  HomeSpan: https://github.com/HomeSpan/HomeSpan
 *
 *  This sketch demonstrates using partition armed and alarm states as HomeKit Security System accessories,
 *  zone states as Contact Sensor accessories, fire alarm states as Smoke Sensor accessories, PGM output states
 *  as Contact Sensor accessories, and command outputs as Switch accessories.
 *
 *  Usage:
 *    1. Set the security system access code in the sketch settings to permit disarming and command outputs
 *       through HomeKit.
 *    2. Define the security system components to add to HomeKit as accessories in the setup() section of
 *       the sketch, using the example accessories as a template.  Each accessory requires its own
 *       SpanAccessory() definition (one per partition, zone, etc).
 *    3. Upload the sketch.
 *    4. Open the esp32 serial interface to configure WiFi using the HomeSpan interface as
 *       per https://github.com/HomeSpan/HomeSpan/blob/master/docs/CLI.md
 *    5. Open the iOS Home app and add the new bridge accessory, which will include all accessories
 *       configured in setup().  By default, HomeSpan uses pairing code: 466-37-726
 *
 *  Release notes:
 *    1.0 - Initial release
 *    1.1 - Changed how zone changes are sent back to HomeKit.
 *        - Added better accessories (dscDoor, dscMotion, dscLeak)
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
 *      Classic series only, PGM configured for PC-16 output:
 *      DSC PGM ---+-- 1k ohm resistor --- DSC Aux(+)
 *                 |
 *                 |                       +--- dscPC16Pin   // Default: 17
 *                 +-- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
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

// DSC Classic series: uncomment for PC1500/PC1550 support (requires PC16-OUT configuration per README.md)
//#define dscClassicSeries

#include "HomeSpan.h" 
#include <dscKeybusInterface.h>

// Settings
const char* accessCode = "1234";  // An access code is required to disarm/night arm and may be required to arm or enable command outputs based on panel configuration.

// Configures the Keybus interface with the specified pins - dscWritePin is optional, leaving it out disables the
// virtual keypad.
#define dscClockPin 18  // 4,13,16-39
#define dscReadPin  19  // 4,13,16-39
#define dscPC16Pin  17  // DSC Classic Series only, 4,13,16-39
#define dscWritePin 21  // 4,13,16-33

// Initialize components
#ifndef dscClassicSeries
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
#else
dscClassicInterface dsc(dscClockPin, dscReadPin, dscPC16Pin, dscWritePin, accessCode);
#endif
bool updatePartitions, updatePGMs, updateSmokeSensors;
     
#include "dscHomeSpanAccessories.h"  // Processes security system components as HomeKit accessories

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  homeSpan.begin(Category::Bridges,"DSC Security System");  // Customizable name for the HomeKit accessory bridge

  // Accessory identification
  new SpanAccessory();  
    new homeSpanIdentify("DSC Security System","DSC","000000","PC1864","3.0");  // Customizable name, manufacturer, serial number, model, firmware revision
    new Service::HAPProtocolInformation();
      new Characteristic::Version("1.1.0");  // HomeKit requires specifying HAP protocol version 1.1.0

  /* 
   *  HomeKit accessories - define partitions, zones, fire alarms, PGM outputs, and command outputs each as a
   *  separate SpanAccessory() using the definitions below as a template
   * 
   *  Instantiated Services (dscPartition, dscDoor, etc) are added to the HomeSpan HAP Database and 
   *  associated with the last Accessory (SpanAccessory) instantiated.
   */ 
  
  // Partition 1: Security System accessory
  new SpanAccessory();                                                          
    new homeSpanIdentify("Partition 1 - House","DSC","000000","Alarm","3.0");
    new dscPartition(1);  // Set the partition number
    
  // Partition 2: Security System accessory
  new SpanAccessory();                                                          
    new homeSpanIdentify("Partition 2 - Outdoor shed","DSC","000000","Alarm","3.0");
    new dscPartition(2);  // Set the partition number

  // Zone 1 - Motion detector in garage
  new SpanAccessory();                                                          
    new homeSpanIdentify("Garage","DSC","000000","Unknown", "1.0");  // Set the zone name
    new dscMotion(1);  // Zone 1

  // Zone 2 - Fire alarm in partition 1
  new SpanAccessory();                                                          
    new homeSpanIdentify("House","DSC","000000", "", "");  // Set the fire sensor name
    new dscFire(1);  // In partition 1, which is the house

  // Zone 3: Big garage door
  new SpanAccessory();                                                          
    new homeSpanIdentify("Big garage door","DSC","000000","Unknown", "1.0");  // Set the zone name
    new dscDoor(3);  // Zone 3

  // Zone 4 - Motion detector on main floor
  new SpanAccessory();                                                          
    new homeSpanIdentify("Motion Main Floor","DSC","000000","Unknown", "1.0");  // Set the zone name
    new dscMotion(4);  // Zone 4

  // Zone 5 - Motion Basement
  new SpanAccessory();                                                          
    new homeSpanIdentify("Motion Basement","DSC","000000","Unknown", "1.0");  // Set the zone name
    new dscMotion(5);  // Zone 5

  // Zone 6 - Water leak
  new SpanAccessory();                                                          
    new homeSpanIdentify("Water leak","DSC","000000","Unknown", "1.0");  // Set the zone name
    new dscLeak(6);  // Zone 6

  // Zone 7 - Entry door in backyard
  new SpanAccessory();                                                          
    new homeSpanIdentify("Door backyard","DSC","000000","Unknown", "1.0");  // Set the zone name
    new dscDoor(7);  // Zone 7

  // Zone 8 - Door 2nd garage
  new SpanAccessory();                                                          
    new homeSpanIdentify("Door second garage","DSC","000000","Unknown", "1.0");  // Set the zone name
    new dscDoor(8);  // Zone 8

  // Zone 9 - Pedestrian garage door
  new SpanAccessory();                                                          
    new homeSpanIdentify("Pedestrian garage door","DSC","000000","Unknown", "1.0");  // Set the zone name
    new dscDoor(9);  // Zone 9

  // Zone 10 - Main entry door of the house
  new SpanAccessory();
    new homeSpanIdentify("Main entry door","DSC","000000","Unknown", "1.0");  // Set the zone name
    new dscDoor(10);  // Zone 10
  
  // Zone 11 - Motion detector of outside shed
  new SpanAccessory();                                                          
    new homeSpanIdentify("Motion detector outside shed","DSC","000000","Unknown", "1.0");  // Set the zone name
    new dscMotion(11);  // Zone 11
    
  // PGM output 1: Contact Sensor accessory
  /*
  new SpanAccessory();                                                          
    new homeSpanIdentify("PGM 1","DSC","000000","Sensor","3.0");  // Set the PGM output name
    new dscPGM(1);  // Set the PGM output number
  */  
  // PGM output 14: Contact Sensor accessory
  new SpanAccessory();                                                          
    new homeSpanIdentify("PGM 14","DSC","000000","Sensor","3.0");  // Set the PGM output name
    new dscPGM(14);  // Set the PGM output number
    
  // Command output 1: Switch accessory - this allows HomeKit to activate the PGM outputs assigned to the command output
  new SpanAccessory();                                                          
    new homeSpanIdentify("Command 1","DSC","000000","Sensor","3.0");  // Set the command output name
    new dscCommand(1, 1, 1);  // Set the command output number (1-4), one of the PGM outputs (1-14) assigned to the command output, and the assigned partition: dscCommand(cmd, pgm, partition);
    
  // Command output 4: Switch accessory - this allows HomeKit to activate the PGM outputs assigned to the command output
  new SpanAccessory();                                                          
    new homeSpanIdentify("Command 4","DSC","000000","Sensor","3.0");  // Set the command output name
    new dscCommand(4, 4, 1);  // Set the command output number (1-4), one of the PGM outputs (1-14) assigned to the command output, and the assigned partition: dscCommand(cmd, pgm, partition);
    
  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), etc.
  dsc.begin();
}


void loop() {  
  homeSpan.poll();

  dsc.loop();

  if (dsc.statusChanged) {      // Checks if the security system status has changed
    dsc.statusChanged = false;  // Reset the status tracking flag

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // loop() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
    if (dsc.bufferOverflow) {
      Serial.println(F("Keybus buffer overflow"));
      dsc.bufferOverflow = false;
    }

    // Sends the access code when needed by the panel for arming or command outputs
    if (dsc.accessCodePrompt) {
      dsc.accessCodePrompt = false;
      dsc.write(accessCode);
    }

    // Checks status per partition
    for (byte partition = 0; partition < dscPartitions; partition++) {

      // Skips processing if the partition is disabled or in installer programming
      if (dsc.disabled[partition]) continue;

      // Checks partition status and sets a flag to update security system partition accessories
      if (dsc.armedChanged[partition] || dsc.exitDelayChanged[partition] || dsc.alarmChanged[partition]) {
        updatePartitions = true;
      }
      
      // Checks fire alarm status and sets a flag to update smoke sensor accessories
      if (dsc.fireChanged[partition]) {
        updateSmokeSensors = true;
      }
    }
    
    // Checks PGM outputs status and sets a flag to update PGM accessories
    if (dsc.pgmOutputsStatusChanged) {
      dsc.pgmOutputsStatusChanged = false;  // Resets the PGM outputs status flag
      updatePGMs = true;                    // Updates PGM output and command output accessories
    }
  } 
}
