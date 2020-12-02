#include "esphome.h"
//for documentation see project at https://github.com/Dilbert66/esphome-dsckeybus

#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin D2   // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscWritePin D8  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
  
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
bool forceDisconnect;

void disconnectKeybus() {
  dsc.stop();
  dsc.keybusConnected = false;
  dsc.statusChanged = false;
  forceDisconnect = true;
 
}

class DSCkeybushome : public Component, public CustomAPIDevice {
 public:
   DSCkeybushome( const char *accessCode="")
   : accessCode(accessCode)
  {}
 
  std::function<void (uint8_t, bool)> zoneStatusChangeCallback;
  std::function<void (uint8_t, bool)> zoneAlarmChangeCallback;
  std::function<void (std::string)> systemStatusChangeCallback;
  std::function<void (bool)> troubleStatusChangeCallback;
  std::function<void (uint8_t, bool)> fireStatusChangeCallback;
  std::function<void (uint8_t,std::string)> partitionStatusChangeCallback; 
  std::function<void (uint8_t,std::string)> partitionMsgChangeCallback;    
  

  const std::string STATUS_PENDING = "pending";
  const std::string STATUS_ARM = "armed_away";
  const std::string STATUS_STAY = "armed_home";
  const std::string STATUS_NIGHT = "armed_night";
  const std::string STATUS_OFF = "disarmed";
  const std::string STATUS_ONLINE = "online";
  const std::string STATUS_OFFLINE = "offline";
  const std::string STATUS_TRIGGERED = "triggered";
  const std::string STATUS_READY = "ready";
  const std::string STATUS_NOT_READY = "unavailable"; //ha alarm panel likes to see "unavailable" instead of not_ready when the system can't be armed
  const std::string MSG_ZONE_BYPASS = "zone_bypass_entered";
  const std::string MSG_ARMED_BYPASS = "armed_custom_bypass";
  const std::string MSG_NO_ENTRY_DELAY = "no_entry_delay";
  const std::string MSG_NONE = "no_messages";
 
  void onZoneStatusChange(std::function<void (uint8_t zone, bool isOpen)> callback) { zoneStatusChangeCallback = callback; }
  void onZoneAlarmChange(std::function<void (uint8_t zone, bool isOpen)> callback) { zoneAlarmChangeCallback = callback; }
  void onSystemStatusChange(std::function<void (std::string status)> callback) { systemStatusChangeCallback = callback; }
  void onFireStatusChange(std::function<void (uint8_t partition, bool isOpen)> callback) { fireStatusChangeCallback = callback; }
  void onTroubleStatusChange(std::function<void (bool isOpen)> callback) { troubleStatusChangeCallback = callback; }
  void onPartitionStatusChange(std::function<void (uint8_t partition,std::string status)> callback) { partitionStatusChangeCallback = callback; }
  void onPartitionMsgChange(std::function<void (uint8_t partition,std::string msg)> callback) { partitionMsgChangeCallback = callback; }
  
  byte debug;
  const char *accessCode;
  bool enable05Messages = true;
  
  private:
    uint8_t zone;
	byte lastStatus[dscPartitions];
	
  void setup() override {
	
    register_service(&DSCkeybushome::set_alarm_state,"set_alarm_state", {"partition","state","code"});
	register_service(&DSCkeybushome::alarm_disarm,"alarm_disarm",{"code"});
	register_service(&DSCkeybushome::alarm_arm_home,"alarm_arm_home");
	register_service(&DSCkeybushome::alarm_arm_night,"alarm_arm_night",{"code"});
	register_service(&DSCkeybushome::alarm_arm_away,"alarm_arm_away");
	register_service(&DSCkeybushome::alarm_trigger_panic,"alarm_trigger_panic");
	register_service(&DSCkeybushome::alarm_trigger_fire,"alarm_trigger_fire");
    register_service(&DSCkeybushome::alarm_keypress, "alarm_keypress",{"keys"});
	systemStatusChangeCallback(STATUS_OFFLINE);
	forceDisconnect = false;
	dsc.resetStatus();
	dsc.begin();
  }
  

void alarm_disarm (std::string code) {
	
	set_alarm_state(1,"D",code);
	
}

void alarm_arm_home () {
	
	set_alarm_state(1,"S");
	
}

void alarm_arm_night (std::string code) {
	
	set_alarm_state(1,"N",code);
	
}

void alarm_arm_away () {
	
	set_alarm_state(1,"A");
	
}

void alarm_trigger_fire () {
	
	set_alarm_state(1,"F");
	
}

void alarm_trigger_panic () {
	
	set_alarm_state(1,"P");
	
}

 void alarm_keypress(std::string keystring) {
	  const char* keys =  strcpy(new char[keystring.length() +1],keystring.c_str());
	   if (debug > 0) ESP_LOGD("Debug","Writing keys: %s",keystring.c_str());
	   dsc.write(keys);
 }		

bool isInt(std::string s, int base){
   if(s.empty() || std::isspace(s[0])) return false ;
   char * p ;
   strtol(s.c_str(), &p, base) ;
   return (*p == 0) ;
}  
    

 void set_alarm_state(int partition,std::string state,std::string code="") {

	if (code.length() != 4 || !isInt(code,10) ) code=""; // ensure we get a numeric 4 digit code
	const char* alarmCode =  strcpy(new char[code.length() +1],code.c_str());
	if (partition) partition = partition-1; // adjust to 0-xx range

    // Arm stay
    if (state.compare("S") == 0 && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
      dsc.writePartition = partition+1;         // Sets writes to the partition number
	  dsc.write('s');                             // Virtual keypad arm stay
    }
    // Arm away
    else if (state.compare("A") == 0 && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
	  dsc.writePartition = partition+1;         // Sets writes to the partition number
      dsc.write('w');                             // Virtual keypad arm away
    }
	// Arm night  ** this depends on the accessCode setup in the yaml
	else if (state.compare("N") == 0 && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
      //ensure you have the accessCode setup correctly in the yaml for this to work
      dsc.writePartition = partition+1;         // Sets writes to the partition number
      dsc.write('n');                             // Virtual keypad arm away
	  if (code.length() == 4 && !isInt(accessCode,10) ) { // if the code is sent and the yaml code is not active use this.
        dsc.write(alarmCode);
	  }
    }
	// Fire command
	else if (state.compare("F") == 0 ) {
      dsc.writePartition = partition+1;         // Sets writes to the partition number
      dsc.write('f');                             // Virtual keypad arm away
    }
	// Panic command
	else if (state.compare("P") == 0 ) {
      dsc.writePartition = partition+1;         // Sets writes to the partition number
      dsc.write('p');                             // Virtual keypad arm away
    }
    // Disarm
    else if (state.compare("D") == 0 && (dsc.armed[partition] || dsc.exitDelay[partition])) {
		dsc.writePartition = partition+1;         // Sets writes to the partition number
		if (code.length() == 4 ) { // ensure we get 4 digit code
			dsc.write(alarmCode);
		}
	}
}

  void loop() override {
    	 
		 
	if (!forceDisconnect  && dsc.loop())  { 

		if (debug == 1 && (dsc.panelData[0] == 0x05 || dsc.panelData[0]==0x27)) ESP_LOGD("Debug11","Panel data: %02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X",dsc.panelData[0],dsc.panelData[1],dsc.panelData[2],dsc.panelData[3],dsc.panelData[4],dsc.panelData[5],dsc.panelData[6],dsc.panelData[7],dsc.panelData[8],dsc.panelData[9],dsc.panelData[10],dsc.panelData[11]);
	
		if (debug > 2 ) ESP_LOGD("Debug11","Panel data: %02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X",dsc.panelData[0],dsc.panelData[1],dsc.panelData[2],dsc.panelData[3],dsc.panelData[4],dsc.panelData[5],dsc.panelData[6],dsc.panelData[7],dsc.panelData[8],dsc.panelData[9],dsc.panelData[10],dsc.panelData[11]);
		
	}

    if ( dsc.statusChanged ) {   // Processes data only when a valid Keybus command has been read
		dsc.statusChanged = false;                   // Reset the status tracking flag
			 
		// If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
		// handlePanel() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
		if (dsc.bufferOverflow) ESP_LOGD("Error","Keybus buffer overflow");
		dsc.bufferOverflow = false;

		// Checks if the interface is connected to the Keybus
		if (dsc.keybusChanged) {
			dsc.keybusChanged = false;  // Resets the Keybus data status flag
			if (dsc.keybusConnected) {
				systemStatusChangeCallback(STATUS_ONLINE);
			} else systemStatusChangeCallback(STATUS_OFFLINE);
		}

		// Sends the access code when needed by the panel for arming
		if (dsc.accessCodePrompt && dsc.writeReady && isInt(accessCode,10)) {
			dsc.accessCodePrompt = false;
			dsc.write(accessCode);
			if (debug > 0) ESP_LOGD("Debug","got access code prompt");
		}

		if (dsc.powerChanged && enable05Messages) {
			dsc.powerChanged=false;
			if (dsc.powerTrouble) partitionMsgChangeCallback(1,"AC power failure");
		}	
		if (dsc.batteryChanged && enable05Messages) {
			dsc.batteryChanged=false;
			if (dsc.batteryTrouble) partitionMsgChangeCallback(1,"Battery trouble");
		}	
		if (dsc.keypadFireAlarm &&  enable05Messages) {
			dsc.keypadFireAlarm=false;
			partitionMsgChangeCallback(1,"Keypad Fire Alarm");
		}
		if (dsc.keypadPanicAlarm &&  enable05Messages) {
			dsc.keypadPanicAlarm=false;
			partitionMsgChangeCallback(1,"Keypad Panic Alarm");
		}
		// Publishes trouble status
		if (dsc.troubleChanged ) {
			dsc.troubleChanged = false;  // Resets the trouble status flag
			if (dsc.trouble) troubleStatusChangeCallback(true );  // Trouble alarm tripped
			else troubleStatusChangeCallback(false ); // Trouble alarm restored
		}
	if (debug > 0) ESP_LOGD("Debug22","Panel command data: %02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X",dsc.panelData[0],dsc.panelData[1],dsc.panelData[2],dsc.panelData[3],dsc.panelData[4],dsc.panelData[5],dsc.panelData[6],dsc.panelData[7],dsc.panelData[8],dsc.panelData[9]);
	 
		// Publishes status per partition
		for (byte partition = 0; partition < dscPartitions; partition++) {
			
		if (dsc.disabled[partition]) continue; //skip disabled or partitions in install programming	
		
		if (debug > 0) ESP_LOGD("Debug33","Partition data %02X: %02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X",partition,dsc.lights[partition], dsc.status[partition], dsc.armed[partition],dsc.armedAway[partition],dsc.armedStay[partition],dsc.noEntryDelay[partition],dsc.fire[partition],dsc.armedChanged[partition],dsc.exitDelay[partition],dsc.readyChanged[partition],dsc.ready[partition],dsc.alarmChanged[partition],dsc.alarm[partition]);
		 
			if (lastStatus[partition] != dsc.status[partition]  ) {
				lastStatus[partition]=dsc.status[partition];
				char msg[50];
				sprintf(msg,"%02X: %s", dsc.status[partition], String(statusText(dsc.status[partition])).c_str());
				if (enable05Messages) partitionMsgChangeCallback(partition+1,msg);
			}

			// Publishes alarm status
			if (dsc.alarmChanged[partition] ) {
				dsc.alarmChanged[partition] = false;  // Resets the partition alarm status flag
				if (dsc.alarm[partition]) {
					dsc.readyChanged[partition] = false;  //if we are triggered no need to trigger a ready state change
					dsc.armedChanged[partition] = false;  // no need to display armed changed
					partitionStatusChangeCallback(partition+1,STATUS_TRIGGERED );
				}
			}
			
			// Publishes armed/disarmed status
			if (dsc.armedChanged[partition] ) {
				dsc.armedChanged[partition] = false;  // Resets the partition armed status flag
				if (dsc.armed[partition]) {
					if ((dsc.armedAway[partition] || dsc.armedStay[partition] )&& dsc.noEntryDelay[partition]) 	partitionStatusChangeCallback(partition+1,STATUS_NIGHT);
					else if (dsc.armedStay[partition]) partitionStatusChangeCallback(partition+1,STATUS_STAY );
					else partitionStatusChangeCallback(partition+1,STATUS_ARM);
				} else partitionStatusChangeCallback(partition+1,STATUS_OFF ); 

			}
			// Publishes exit delay status
			if (dsc.exitDelayChanged[partition] ) {
				dsc.exitDelayChanged[partition] = false;  // Resets the exit delay status flag
				if (dsc.exitDelay[partition]) partitionStatusChangeCallback(partition+1,STATUS_PENDING );  
				else if (!dsc.exitDelay[partition] && !dsc.armed[partition]) partitionStatusChangeCallback(partition+1,STATUS_OFF );
			}
			
			// Publishes ready status
			if (dsc.readyChanged[partition] ) {
				dsc.readyChanged[partition] = false;  // Resets the partition alarm status flag
				if (dsc.ready[partition] ) 	partitionStatusChangeCallback(partition+1,STATUS_OFF ); 
				else if (!dsc.armed[partition]) partitionStatusChangeCallback(partition+1,STATUS_NOT_READY );
			}

			// Publishes fire alarm status
			if (dsc.fireChanged[partition] ) {
				dsc.fireChanged[partition] = false;  // Resets the fire status flag
				if (dsc.fire[partition]) fireStatusChangeCallback(partition+1,true );  // Fire alarm tripped
				else fireStatusChangeCallback(partition+1,false ); // Fire alarm restored
			}
		}

		// Publishes zones 1-64 status in a separate topic per zone
		// Zone status is stored in the openZones[] and openZonesChanged[] arrays using 1 bit per zone, up to 64 zones:
		//   openZones[0] and openZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
		//   openZones[1] and openZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
		//   ...
		//   openZones[7] and openZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
		if (dsc.openZonesStatusChanged  ) {
			dsc.openZonesStatusChanged = false;                           // Resets the open zones status flag
			for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
				for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
					if (bitRead(dsc.openZonesChanged[zoneGroup], zoneBit)) {  // Checks an individual open zone status flag
						bitWrite(dsc.openZonesChanged[zoneGroup], zoneBit, 0);  // Resets the individual open zone status flag
						zone=zoneBit + 1 + (zoneGroup * 8);
						if (bitRead(dsc.openZones[zoneGroup], zoneBit)) {
							zoneStatusChangeCallback(zone, true);  // Zone open
						}
						else  zoneStatusChangeCallback(zone, false);        // Zone closed
					}
				}
			}
		}
		
		// Zone alarm status is stored in the alarmZones[] and alarmZonesChanged[] arrays using 1 bit per zone, up to 64 zones
		//   alarmZones[0] and alarmZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
		//   alarmZones[1] and alarmZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
		//   ...
		//   alarmZones[7] and alarmZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64	
		if (dsc.alarmZonesStatusChanged  ) {
			dsc.alarmZonesStatusChanged = false;                           // Resets the alarm zones status flag
			for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
				for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
					if (bitRead(dsc.alarmZonesChanged[zoneGroup], zoneBit)) {  // Checks an individual alarm zone status flag
						bitWrite(dsc.alarmZonesChanged[zoneGroup], zoneBit, 0);  // Resets the individual alarm zone status flag
						zone=zoneBit + 1 + (zoneGroup * 8);
						if (bitRead(dsc.alarmZones[zoneGroup], zoneBit)) {
							zoneAlarmChangeCallback(zone, true);  // Zone alarm
						}
						else  zoneAlarmChangeCallback(zone, false);        // Zone restored
					}
				}
			}
		}
		
	}
		
  }

const __FlashStringHelper *statusText(uint8_t statusCode)
{
    switch (statusCode) {
        case 0x01: return F("Ready");
        case 0x02: return F("Stay zones open");
        case 0x03: return F("Zones open");
        case 0x04: return F("Armed stay");
        case 0x05: return F("Armed away");
        case 0x06: return F("No entry delay");
        case 0x07: return F("Failed to arm");
        case 0x08: return F("Exit delay");
        case 0x09: return F("No entry delay");
        case 0x0B: return F("Quick exit");
        case 0x0C: return F("Entry delay");
        case 0x0D: return F("Alarm memory");
        case 0x10: return F("Keypad lockout");
        case 0x11: return F("Alarm");
        case 0x14: return F("Auto-arm");
        case 0x15: return F("Arm with bypass");
        case 0x16: return F("No entry delay");
        case 0x17: return F("Power failure");//??? not sure
        case 0x22: return F("Alarm memory");
        case 0x33: return F("Busy");
        case 0x3D: return F("Disarmed");
        case 0x3E: return F("Disarmed");
        case 0x40: return F("Keypad blanked");
        case 0x8A: return F("Activate zones");
        case 0x8B: return F("Quick exit");
        case 0x8E: return F("Invalid option");
        case 0x8F: return F("Invalid code");
        case 0x9E: return F("Enter * code");
        case 0x9F: return F("Access code");
        case 0xA0: return F("Zone bypass");
        case 0xA1: return F("Trouble menu");
        case 0xA2: return F("Alarm memory");
        case 0xA3: return F("Door chime on");
        case 0xA4: return F("Door chime off");
        case 0xA5: return F("Master code");
        case 0xA6: return F("Access codes");
        case 0xA7: return F("Enter new code");
        case 0xA9: return F("User function");
        case 0xAA: return F("Time and Date");
        case 0xAB: return F("Auto-arm time");
        case 0xAC: return F("Auto-arm on");
        case 0xAD: return F("Auto-arm off");
        case 0xAF: return F("System test");
        case 0xB0: return F("Enable DLS");
        case 0xB2: return F("Command output");
        case 0xB7: return F("Installer code");
        case 0xB8: return F("Enter * code");
        case 0xB9: return F("Zone tamper");
        case 0xBA: return F("Zones low batt.");
        case 0xC6: return F("Zone fault menu");
        case 0xC8: return F("Service required");
        case 0xD0: return F("Keypads low batt");
        case 0xD1: return F("Wireless low bat");
        case 0xE4: return F("Installer menu");
        case 0xE5: return F("Keypad slot");
        case 0xE6: return F("Input: 2 digits");
        case 0xE7: return F("Input: 3 digits");
        case 0xE8: return F("Input: 4 digits");
        case 0xEA: return F("Code: 2 digits");
        case 0xEB: return F("Code: 4 digits");
        case 0xEC: return F("Input: 6 digits");
        case 0xED: return F("Input: 32 digits");
        case 0xEE: return F("Input: option");
        case 0xF0: return F("Function key 1");
        case 0xF1: return F("Function key 2");
        case 0xF2: return F("Function key 3");
        case 0xF3: return F("Function key 4");
        case 0xF4: return F("Function key 5");
        case 0xF8: return F("Keypad program");
        case 0xFF: return F("Disabled");
        default: return F("Unknown");
    }
}


};