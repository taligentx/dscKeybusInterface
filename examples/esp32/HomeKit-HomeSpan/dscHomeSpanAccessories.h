/*
 *  HomeKit-HomeSpan 1.0 (esp32)
 *
 *  This defines security system components as HomeKit accessories, processes security system status
 *  changes to update HomeKit, and handles HomeKit requests to change the security system state.
 *
 *  All accessories are configured in HomeKit-HomeSpan.ino.
 *
 *  Issues and (especially) pull requests are welcome:
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *  This example code is in the public domain.
 */

// HomeKit security system states are defined as integers per HomeKit Accessory Protocol Specification R2
#define HOMEKIT_STAY 0
#define HOMEKIT_AWAY 1
#define HOMEKIT_NIGHT 2
#define HOMEKIT_DISARM 3
#define HOMEKIT_ALARM 4

// Tracks which partitions, zones, and PGMs are configured in the sketch - only the configured accessories will be processed for status
bool configuredPartitions[8];
byte configuredZones[8], configuredPGMs[2], configuredCommandPGMs[2], pendingPGMs[2];


// Partitions are defined as separate Security System accessories
struct dscPartition : Service::SecuritySystem {
  byte partition;
  char exitState;
  SpanCharacteristic *partitionCurrentState, *partitionTargetState;
  dscPartition(byte setPartition) : Service::SecuritySystem() {
    partition = setPartition - 1;
    configuredPartitions[partition] = true;
    partitionCurrentState = new Characteristic::SecuritySystemCurrentState(HOMEKIT_DISARM);  // Sets initial state to disarmed
    partitionTargetState = new Characteristic::SecuritySystemTargetState(HOMEKIT_DISARM);    // Sets initial state to disarmed
  }


  // Handles requests received from HomeKit
  boolean update() {
    byte targetState = partitionTargetState->getNewVal();
    
    // Sets night arm (no entry delay) while armed
    if (targetState == HOMEKIT_NIGHT && dsc.armed[partition]) {
      dsc.writePartition = partition + 1;    // Sets writes to the partition number
      dsc.write('n');  // Keypad no entry delay
      exitState = 'N';
      return(true);
    }
  
    // Disables night arm while armed stay
    if (targetState == HOMEKIT_STAY && dsc.armedStay[partition] && dsc.noEntryDelay[partition]) {
      dsc.writePartition = partition + 1;    // Sets writes to the partition number
      dsc.write('n');  // Keypad no entry delay
      exitState = 'S';
      return(true);
    }
  
    // Disables night arm while armed away
    if (targetState == HOMEKIT_AWAY && dsc.armedAway[partition] && dsc.noEntryDelay[partition]) {
      dsc.writePartition = partition + 1;    // Sets writes to the partition number
      dsc.write('n');  // Keypad no entry delay
      exitState = 'A';
      return(true);
    }
  
    // Changes from arm away to arm stay after the exit delay
    if (targetState == HOMEKIT_STAY && dsc.armedAway[partition]) {
      dsc.writePartition = partition + 1;    // Sets writes to the partition number
      dsc.write("s");
      exitState = 'S';
      return(true);
    }
  
    // Changes from arm stay to arm away after the exit delay
    if (targetState == HOMEKIT_AWAY && dsc.armedStay[partition]) {
      dsc.writePartition = partition + 1;    // Sets writes to the partition number
      dsc.write("w");
      exitState = 'A';
      return(true);
    }
  
    // Resets the HomeKit target state if attempting to change the armed mode while not ready
    if (targetState != HOMEKIT_DISARM && !dsc.ready[partition]) {
      dsc.armedChanged[partition] = true;
      dsc.statusChanged = true;
      return(true);
    }
  
    // Resets the HomeKit target state if attempting to change the arming mode during the exit delay
    if (targetState != HOMEKIT_DISARM && dsc.exitDelay[partition] && exitState != 0) {
      if (exitState == 'S') partitionTargetState->setVal(HOMEKIT_STAY);
      else if (exitState == 'A') partitionTargetState->setVal(HOMEKIT_AWAY);
      else if (exitState == 'N') partitionTargetState->setVal(HOMEKIT_NIGHT);
    }
  
    // Stay arm
    if (targetState == HOMEKIT_STAY && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
      dsc.writePartition = partition + 1;    // Sets writes to the partition number
      dsc.write('s');  // Keypad stay arm
      exitState = 'S';
      return(true);
    }
  
    // Away arm
    if (targetState == HOMEKIT_AWAY && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
      dsc.writePartition = partition + 1;    // Sets writes to the partition number
      dsc.write('w');  // Keypad away arm
      exitState = 'A';
      return(true);
    }
  
    // Night arm
    if (targetState == HOMEKIT_NIGHT && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
      dsc.writePartition = partition + 1;    // Sets writes to the partition number
      dsc.write('n');  // Keypad arm with no entry delay
      exitState = 'N';
      return(true);
    }
  
    // Disarm
    if (targetState == HOMEKIT_DISARM && (dsc.armed[partition] || dsc.exitDelay[partition] || dsc.alarm[partition])) {
      dsc.writePartition = partition + 1;    // Sets writes to the partition number
      dsc.write(accessCode);
      return(true);
    }
    
    return(true);
  }


  // Checks for partition status changes to send to HomeKit
  void loop() {
    if (updatePartitions) {
      updatePartitions = false;
      
      if (dsc.armedChanged[partition]) {
        if (dsc.armed[partition]) {
          exitState = 0;
          if (dsc.armedAway[partition] && dsc.noEntryDelay[partition]) {  // Night armed away
            partitionTargetState->setVal(HOMEKIT_NIGHT);
            partitionCurrentState->setVal(HOMEKIT_NIGHT);
          }
          else if (dsc.armedAway[partition]) {  // Away armed
            partitionTargetState->setVal(HOMEKIT_AWAY);
            partitionCurrentState->setVal(HOMEKIT_AWAY);
          }
          else if (dsc.armedStay[partition] && dsc.noEntryDelay[partition]) {  // Night armed stay
            partitionTargetState->setVal(HOMEKIT_NIGHT);
            partitionCurrentState->setVal(HOMEKIT_NIGHT);
          }
          else if (dsc.armedStay[partition]) {  // Stay armed
            partitionTargetState->setVal(HOMEKIT_STAY);
            partitionCurrentState->setVal(HOMEKIT_STAY);
          }
        }
        else {  // Disarmed
          partitionTargetState->setVal(HOMEKIT_DISARM);
          partitionCurrentState->setVal(HOMEKIT_DISARM);
        }
      }
      
      // Updates exit delay status
      if (dsc.exitDelayChanged[partition]) {
        dsc.exitDelayChanged[partition] = false;  // Resets the exit delay status flag
        
        if (dsc.exitDelay[partition]) {

          // Sets the arming target state if the panel is armed externally
          if (exitState == 0 || dsc.exitStateChanged[partition]) {
            dsc.exitStateChanged[partition] = 0;
            switch (dsc.exitState[partition]) {
              case DSC_EXIT_STAY: {
                exitState = 'S';
                partitionTargetState->setVal(HOMEKIT_STAY);
                break;
              }
              case DSC_EXIT_AWAY: {
                exitState = 'A';
                partitionTargetState->setVal(HOMEKIT_AWAY);
                break;
              }
              case DSC_EXIT_NO_ENTRY_DELAY: {
                exitState = 'N';
                partitionTargetState->setVal(HOMEKIT_NIGHT);
                break;
              }
            }
          }
        }

        // Disarmed during exit delay
        else if (!dsc.armed[partition]) {
          exitState = 0;
          partitionTargetState->setVal(HOMEKIT_DISARM);
          partitionCurrentState->setVal(HOMEKIT_DISARM);
        }
      }

      // Publishes alarm triggered status
      if (dsc.alarmChanged[partition]) {
        dsc.alarmChanged[partition] = false;  // Resets the partition alarm status flag
        if (dsc.alarm[partition]) {
          partitionCurrentState->setVal(HOMEKIT_ALARM);  // Alarm triggered
        }
        else if (!dsc.armedChanged[partition]) {
          partitionTargetState->setVal(HOMEKIT_DISARM);
          partitionCurrentState->setVal(HOMEKIT_DISARM);
        }
      }
      
      if (dsc.armedChanged[partition]) dsc.armedChanged[partition] = false;  // Resets the partition armed status flag

      // Checks for changed status in additional partitions
      for (byte checkPartition = 0; checkPartition < dscPartitions; checkPartition++) {
  
        // Skips processing if the partition is disabled, in installer programming, or not configured in the sketch
        if (dsc.disabled[checkPartition] || !configuredPartitions[checkPartition]) continue;
  
        // Checks for changed status in a partition
        if (dsc.armedChanged[checkPartition] || dsc.exitDelayChanged[checkPartition] || dsc.alarmChanged[checkPartition]) {
          updatePartitions = true;
        }
      }
    }
  }
};


// Zones are defined as Contact Sensor accessories
struct dscZone : Service::ContactSensor {
  byte zoneGroup, zoneBit;
  SpanCharacteristic *zoneState;
  dscZone(byte zone) : Service::ContactSensor() {
    zoneGroup = (zone - 1) / 8;
    zoneBit = (zone - 1) - (zoneGroup * 8);
    bitWrite(configuredZones[zoneGroup], zoneBit, 1);  // Sets a zone as being configured
    zoneState = new Characteristic::ContactSensorState(0);
  }

  // Checks for zone status changes to send to HomeKit
  void loop() {
    if (updateZones) {
      updateZones = false;
      
      if (bitRead(dsc.openZonesChanged[zoneGroup], zoneBit)) {   // Checks an individual open zone status flag
        bitWrite(dsc.openZonesChanged[zoneGroup], zoneBit, 0);   // Resets the individual open zone status flag
        
        if (bitRead(dsc.openZones[zoneGroup], zoneBit)) zoneState->setVal(1);  // Set zone status open
        else zoneState->setVal(0);  // Set zone status closed
      }

      // Checks if additional configured zones have changed
      for (byte checkZoneGroup = 0; checkZoneGroup < dscZones; checkZoneGroup++) {
        for (byte checkZoneBit = 0; checkZoneBit < 8; checkZoneBit++) {
          if (bitRead(configuredZones[checkZoneGroup], checkZoneBit) && bitRead(dsc.openZonesChanged[checkZoneGroup], checkZoneBit)) {   // Checks if additional zones have changed
            updateZones = true;  // Sets a flag to continue processing remaining changed zones that are configured
          }
        }
      }
    } 
  }
};


// Fire alarms are defined as separate Smoke Sensor accessories
struct dscFire : Service::SmokeSensor {
  byte partition;
  SpanCharacteristic *fireState;
  dscFire(byte setPartition) : Service::SmokeSensor() {
    partition = setPartition - 1;
    fireState = new Characteristic::SmokeDetected(0);
  }

  // Checks for fire status changes to send to HomeKit
  void loop() {
    if (updateSmokeSensors) {
      updateSmokeSensors = false;
      
      dsc.fireChanged[partition] = false;  // Resets the fire status flag

      if (dsc.fire[partition]) fireState->setVal(1);  // Fire alarm tripped
      else fireState->setVal(0);  // Fire alarm restored
      
      // Checks for changed status in additional partitions
      for (byte checkPartition = 0; checkPartition < dscPartitions; checkPartition++) {
  
        // Skips processing if the partition is disabled, in installer programming, or not configured in the sketch
        if (dsc.disabled[checkPartition] || !configuredPartitions[checkPartition]) continue;
  
        // Checks for changed fire status in a partition
        if (dsc.fireChanged[checkPartition]) updateSmokeSensors = true;
      }
    }
  }
};


// PGM outputs are defined as Contact Sensor accessories
struct dscPGM : Service::ContactSensor {
  byte pgmGroup, pgmBit;
  SpanCharacteristic *pgmState;
  dscPGM(byte pgm) : Service::ContactSensor() {
    pgmGroup = (pgm - 1) / 8;
    pgmBit = (pgm - 1) - (pgmGroup * 8);
    bitWrite(configuredPGMs[pgmGroup], pgmBit, 1);  // Sets a PGM output as being configured
    pgmState = new Characteristic::ContactSensorState(0);
  }

  // Checks for PGM status changes to send to HomeKit
  void loop() {
    if (updatePGMs) {
      updatePGMs = false;

      if (bitRead(dsc.pgmOutputsChanged[pgmGroup], pgmBit)) {
        
        // Handles PGMs defined both as this contact sensor accessory and for a command switch output switch accessory
        if (bitRead(configuredCommandPGMs[pgmGroup], pgmBit)) {

          // Sets processing status of the PGM depending on the order in which this accessory is handled
          if (bitRead(pendingPGMs[pgmGroup], pgmBit)) {
            bitWrite(pendingPGMs[pgmGroup], pgmBit, 0);
            bitWrite(dsc.pgmOutputsChanged[pgmGroup], pgmBit, 0);
          }
          else bitWrite(pendingPGMs[pgmGroup], pgmBit, 1);
          
          if (bitRead(dsc.pgmOutputs[pgmGroup], pgmBit)) pgmState->setVal(1);  // Set PGM output status on
          else pgmState->setVal(0);  // Set PGM output status off
        }
        
        // Handles PGMs defined only for this contact sensor accessory
        else {
          bitWrite(dsc.pgmOutputsChanged[pgmGroup], pgmBit, 0);
          if (bitRead(dsc.pgmOutputs[pgmGroup], pgmBit)) pgmState->setVal(1);  // Set command output status on
          else pgmState->setVal(0);  // Set command output status off
        }
      }

      // Checks if additional configured PGM outputs have changed
      for (byte checkPGMGroup = 0; checkPGMGroup < 2; checkPGMGroup++) {
        for (byte checkPGMBit = 0; checkPGMBit < 8; checkPGMBit++) {
          if (bitRead(dsc.pgmOutputsChanged[checkPGMGroup], checkPGMBit)) {
            if (bitRead(configuredPGMs[checkPGMGroup], checkPGMBit) || bitRead(configuredCommandPGMs[checkPGMGroup], checkPGMBit)) {   // Checks if additional PGM outputs have changed
              updatePGMs = true;  // Sets a flag to continue processing changed PGM outputs that are configured
            }
          }
        }
      }
    } 
  }
};


// Command outputs 1-4 are defined as Switch accessories - this allows HomeKit to view status and
// control the PGM outputs assigned to each command output
struct dscCommand : Service::Switch {
  byte cmd, pgmGroup, pgmBit, partition;
  SpanCharacteristic *cmdState;
  dscCommand(byte setCMD, byte pgm, byte setPartition) : Service::Switch() {
    cmd = setCMD;
    partition = setPartition;
    pgmGroup = (pgm - 1) / 8;
    pgmBit = (pgm - 1) - (pgmGroup * 8);
    bitWrite(configuredCommandPGMs[pgmGroup], pgmBit, 1);  // Sets a PGM output as being configured
    cmdState = new Characteristic::On(0);
  }


  // Handles requests received from HomeKit
  boolean update() {
    byte targetState = cmdState->getNewVal();

    // HomeKit requests switch on - enables the command output if its assigned PGM output is inactive
    if (targetState == 1 && !bitRead(dsc.pgmOutputs[pgmGroup], pgmBit)) {
      dsc.writePartition = partition;
      switch (cmd) {
        case 1: dsc.write('['); break;
        case 2: dsc.write(']'); break;
        case 3: dsc.write('{'); break;
        case 4: dsc.write('}'); break;
        default: cmdState->setVal(0);
      }
    }

    // HomeKit requests switch off - resets the HomeKit state to On if the PGM output is still active
    else if (targetState == 0 && bitRead(dsc.pgmOutputs[pgmGroup], pgmBit)) {
      cmdState->setVal(1);
    }
    
    return(true);
  }


  // Checks for PGM status changes to send to HomeKit
  void loop() {
    if (updatePGMs) {
      updatePGMs = false;

      if (bitRead(dsc.pgmOutputsChanged[pgmGroup], pgmBit)) {

        // Handles PGMs defined both for this command switch output switch accessory and as a contact sensor accessory
        if (bitRead(configuredPGMs[pgmGroup], pgmBit)) {

          // Sets processing status of the PGM depending on the order in which this accessory is handled
          if (bitRead(pendingPGMs[pgmGroup], pgmBit)) {
            bitWrite(pendingPGMs[pgmGroup], pgmBit, 0);
            bitWrite(dsc.pgmOutputsChanged[pgmGroup], pgmBit, 0);
          }
          else bitWrite(pendingPGMs[pgmGroup], pgmBit, 1);
          
          if (bitRead(dsc.pgmOutputs[pgmGroup], pgmBit)) cmdState->setVal(1);  // Set command output status on
          else cmdState->setVal(0);  // Set command output status off
        }

        // Handles PGMs defined only for this command output switch accessory
        else {
          bitWrite(dsc.pgmOutputsChanged[pgmGroup], pgmBit, 0);
          if (bitRead(dsc.pgmOutputs[pgmGroup], pgmBit)) cmdState->setVal(1);  // Set command output status on
          else cmdState->setVal(0);  // Set command output status off
        }
      }

      // Checks if additional configured PGM outputs have changed
      for (byte checkPGMGroup = 0; checkPGMGroup < 2; checkPGMGroup++) {
        for (byte checkPGMBit = 0; checkPGMBit < 8; checkPGMBit++) {
          if (bitRead(dsc.pgmOutputsChanged[checkPGMGroup], checkPGMBit)) {
            if (bitRead(configuredPGMs[checkPGMGroup], checkPGMBit) || bitRead(configuredCommandPGMs[checkPGMGroup], checkPGMBit)) {   // Checks if additional PGM outputs have changed
              updatePGMs = true;  // Sets a flag to continue processing changed PGM outputs that are configured
            }
          }
        }
      }
    } 
  }
};


// HomeSpan Identify
struct homeSpanIdentify : Service::AccessoryInformation {
  homeSpanIdentify(const char *name, const char *manu, const char *sn, const char *model, const char *version) : Service::AccessoryInformation() {
    new Characteristic::Name(name);
    new Characteristic::Manufacturer(manu);
    new Characteristic::SerialNumber(sn);    
    new Characteristic::Model(model);
    new Characteristic::FirmwareRevision(version);
    new Characteristic::Identify();
  }
};
