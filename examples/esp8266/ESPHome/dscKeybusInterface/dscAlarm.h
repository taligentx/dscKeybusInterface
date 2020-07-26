#include "esphome.h"

#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin D2   // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscWritePin D8  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
  
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);
bool forceDisconnect;

void disconnectKeybus() {
  dsc.stop();
  dsc.keybusConnected = false;
  forceDisconnect = true;
}

bool getKeybusConnectionStatus() {
	return dsc.keybusConnected;
}

class DSCkeybushome : public Component, public CustomAPIDevice {
 public:
  std::function<void (uint8_t, bool)> zoneStatusChangeCallback;
  std::function<void (std::string)> systemStatusChangeCallback;
  std::function<void ( bool)> troubleStatusChangeCallback;
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
  const std::string STATUS_NOT_READY = "not_ready";
  const std::string MSG_ZONE_BYPASS = "zone_bypass_entered";
  const std::string MSG_ARMED_BYPASS = "armed_custom_bypass";
  const std::string MSG_NO_ENTRY_DELAY = "no_entry_delay";
  const std::string MSG_NONE = "no_messages";
 
  uint8_t zone;

  void onZoneStatusChange(std::function<void (uint8_t zone, bool isOpen)> callback) { zoneStatusChangeCallback = callback; }
  void onSystemStatusChange(std::function<void (std::string status)> callback) { systemStatusChangeCallback = callback; }
  void onFireStatusChange(std::function<void (uint8_t partition, bool isOpen)> callback) { fireStatusChangeCallback = callback; }
  void onTroubleStatusChange(std::function<void (bool isOpen)> callback) { troubleStatusChangeCallback = callback; }
  void onPartitionStatusChange(std::function<void (uint8_t partition,std::string status)> callback) { partitionStatusChangeCallback = callback; }
  void onPartitionMsgChange(std::function<void (uint8_t partition,std::string msg)> callback) { partitionMsgChangeCallback = callback; }
  
  byte lastStatus[dscPartitions];
  
  void setup() override {

    register_service(&DSCkeybushome::set_alarm_state,"set_alarm_state", {"partition","state"});
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
  
  
void alarm_disarm (std::string code="") {
	
	set_alarm_state(1,"D",code);
	
}

void alarm_arm_home () {
	
	set_alarm_state(1,"S");
	
}

void alarm_arm_night (std::string code="") {
	
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
	   ESP_LOGD("Debug","Writing keys: %s",keystring.c_str());
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
	const char* accessCode =  strcpy(new char[code.length() +1],code.c_str());
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
	/*
	// Arm night  #note this code does not work with my older PC5010 panel
	else if (state.compare("N") == 0 && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
      dsc.writePartition = partition+1;         // Sets writes to the partition number
      dsc.write('n');                             // Virtual keypad arm away
    }
	*/
	// Arm night
	else if (state.compare("N") == 0 && !dsc.armed[partition] &&  !dsc.exitDelay[partition]) {
		char cmd[3];
		dsc.writePartition = partition+1;         // Sets writes to the partition number
		strcpy(cmd,"*9");
		if (code.length() == 4 ) { // ensure we get a 4 digit code
			dsc.write(cmd);
			dsc.write(accessCode);
		}
	}
	// Fire command
	else if (state.compare("F") == 0 && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
      dsc.writePartition = partition+1;         // Sets writes to the partition number
      dsc.write('f');                             // Virtual keypad arm away
    }
	
	// Panic command
	else if (state.compare("P") == 0 && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
      dsc.writePartition = partition+1;         // Sets writes to the partition number
      dsc.write('p');                             // Virtual keypad arm away
    }
	
    // Disarm
    else if (state.compare("D") == 0 && (dsc.armed[partition] || dsc.exitDelay[partition])) {
		dsc.writePartition = partition+1;         // Sets writes to the partition number
		if (code.length()) == 4) { // ensure we get a 4 digit code
			dsc.write(accessCode);
		}
	}
	
}
	 

  void loop() override {
    	 
    if ( !forceDisconnect & dsc.loop() && dsc.statusChanged ) {   // Processes data only when a valid Keybus command has been read
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
		if (dsc.accessCodePrompt && dsc.writeReady) { 
			dsc.accessCodePrompt = false;
			char accessCode[4];
			sprintf(accessCode,"%04u",id(accesscode));
			dsc.write(accessCode);
			ESP_LOGD("Debug","got arming access code prompt");
		}

		if (dsc.troubleChanged ) {
			dsc.troubleChanged = false;  // Resets the trouble status flag
			if (dsc.trouble) troubleStatusChangeCallback(true);
			else troubleStatusChangeCallback(false);
		}
	
		// Publishes status per partition
		for (byte partition = 0; partition < dscPartitions; partition++) {
			
			if (lastStatus[partition] == 0) partitionMsgChangeCallback(partition+1,MSG_NONE ); //init msgs
			
			if (lastStatus[partition] != dsc.status[partition] ) {
				lastStatus[partition]=dsc.status[partition];
			
				if ( dsc.status[partition] == 0xA0 ) {
					partitionMsgChangeCallback(partition+1,MSG_ZONE_BYPASS );
				}	
				else if ( dsc.status[partition] == 0x15 ) {
					partitionMsgChangeCallback(partition+1,MSG_ARMED_BYPASS );
				}	
				else if ( dsc.status[partition] == 0x04 ) {
					partitionMsgChangeCallback(partition+1,STATUS_STAY );
				}	
				else if ( dsc.status[partition] == 0x06 || dsc.status[partition] == 0x16 ) {
					partitionMsgChangeCallback(partition+1,MSG_NO_ENTRY_DELAY );
				}	
				else if (dsc.status[partition] == 0x3E ) {
					partitionMsgChangeCallback(partition+1,MSG_NONE );
				}
			}
			
			// Publishes armed/disarmed status
			if (dsc.armedChanged[partition] ) {
				dsc.armedChanged[partition] = false;  // Resets the partition armed status flag
				if (dsc.armed[partition]) {
					if (dsc.armedAway[partition] && dsc.noEntryDelay[partition]) partitionStatusChangeCallback(partition+1,STATUS_NIGHT);
					else if (dsc.armedAway[partition]) partitionStatusChangeCallback(partition+1,STATUS_ARM);
					else if (dsc.armedStay[partition]) partitionStatusChangeCallback(partition+1,STATUS_STAY );
					//if (dsc.status[partition] == 0x15) partitionStatusChangeCallback(partition+1,MSG_ARMED_BYPASS );
				} else {
					partitionStatusChangeCallback(partition+1,STATUS_OFF );
					
				}
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
				if (dsc.ready[partition] ) {
					partitionStatusChangeCallback(partition+1,STATUS_OFF ); //instead of ready for HA template 
				} else if (!dsc.armed[partition]) partitionStatusChangeCallback(partition+1,STATUS_NOT_READY );
				
			}

			// Publishes alarm status
			if (dsc.alarmChanged[partition] ) {
				dsc.alarmChanged[partition] = false;  // Resets the partition alarm status flag
				if (dsc.alarm[partition]) {
					partitionStatusChangeCallback(partition+1,STATUS_TRIGGERED );
				}
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
		
	}
		
  }


};