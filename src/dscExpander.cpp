#include "dscKeybus.h"
#ifdef EXPANDER
void dscKeybusInterface::setSupervisorySlot(byte address,bool set=true) {
       //set our response data for the 0x11 supervisory request
       if (panelVersion < 3) {
            switch (address) {
                     //for older versions we need to set 2 slots since they expect groups of 4 zones per slot
            case 9:   moduleSlots[2]=set?moduleSlots[2]&0x3f:moduleSlots[2]|~0x3f; //pc5108 
                      moduleSlots[2]=set?moduleSlots[2]&0xcf:moduleSlots[2]|~0xcf;
                      break; 
            case 10:  moduleSlots[2]=set?moduleSlots[2]&0xf3:moduleSlots[2]|~0xf3; //pc5108
                      moduleSlots[2]=set?moduleSlots[2]&0xfc:moduleSlots[2]|~0xfc;
                      break; 
            case 11:  moduleSlots[3]=set?moduleSlots[3]&0x3f:moduleSlots[3]|~0x3f;//pc5108
                      moduleSlots[3]=set?moduleSlots[3]&0xcf:moduleSlots[3]|~0xcf;
                      break; 
            case 18:  moduleSlots[3]=set?moduleSlots[3]&0xfc:moduleSlots[3]|~0xfc;break;  // pc5208 relay board reports as 18 but reports on slot 16
            default: return;
        }
           
       } else {
        switch (address) {
            //11111111 1 00111111 11111111 11111111 11111111 11111111 11111100 11111111 16
            //11111111 1 00111111 11111111 11111111 00111111 11111111 11111111 11111111 13
            // 1111111 1 00111111 11111111 00111111 11111111 11111111 11111111 11111111 slots 9
            //11111111 1 00111111 11111111 11111111 11111111 11111111 11111100 11111111 slots 16
            case 9:   moduleSlots[2]=set?moduleSlots[2]&0x3f:moduleSlots[2]|~0x3f;break; //pc5108 
            case 10:  moduleSlots[2]=set?moduleSlots[2]&0xcf:moduleSlots[2]|~0xcf;break; //pc5108
            case 11:  moduleSlots[2]=set?moduleSlots[2]&0xf3:moduleSlots[2]|~0xf3;break; //pc5108
            case 12:  if  (maxZones>32) {moduleSlots[2]=set?moduleSlots[2]&0xfc:moduleSlots[2]|~0xfc;}break; //pc5108
            case 13:  if  (maxZones>32) {moduleSlots[3]=set?moduleSlots[3]&0x3f:moduleSlots[3]|~0x3f;}break; //pc5108
            case 14:  if  (maxZones>32) {moduleSlots[3]=set?moduleSlots[3]&0xcf:moduleSlots[3]|~0xcf;}break; //pc5108
            case 16:  if  (maxZones>32) {moduleSlots[5]=set?moduleSlots[5]&0x3f:moduleSlots[5]|~0x3f;}break; //pc5108 (shows on slot24)// reports as 16 in panel
            //reports as 18 in panel
            case 18:  moduleSlots[3]=set?moduleSlots[3]&0xfc:moduleSlots[3]|~0xfc;break;  // pc5208 relay board shows on slot 16 but reports as 18
            default: return;
        }
       }
    
}

zoneMaskType dscKeybusInterface::getUpdateMask(byte address) {

        //get our request byte and mask to send data for the zone that we need to publish info on. This gets sent on the 05 command
        //11111111 1 11111111 11111111 11111111 11111111 11111111 01111111 11111111 11111111 (12)
        //11111111 1 11111111 11111111 10111111 11111111 11111111 11111111 11111111 11111111 (9)
        //11111111 1 11111111 11111111 10111111 11111111 version 2 zone 9 -16
        //11111111 1 11111111 11111111 11101111 11111111  version 2 system zone 27-32

        zoneMaskType zm;
        switch (address) {
            case 9:  zm.idx=2;zm.mask=0xbf; break; //5108
            case 10: zm.idx=2;zm.mask=0xdf; break; //5108
            case 11: zm.idx=2;zm.mask=0xef; break;//5108
            case 12: if (maxZones>32) {zm.idx=5;zm.mask=0x7f;} break;
            case 13: if (maxZones>32) {zm.idx=5;zm.mask=0xbf;} break;
            case 14: if (maxZones>32) {zm.idx=5;zm.mask=0xdf;} break;
            case 16: if (maxZones>32) {zm.idx=5;zm.mask=0xef;} break;//5108 sends to slot 15
            default: zm.idx=0;zm.mask=0;
        }
        return zm;
}


//clears all emulated zones on the panel 
void dscKeybusInterface:: clearZoneRanges() {
     byte zoneupdate[maxFields05];
     memset(zoneupdate,0xFF,maxFields05);
     for (int x=0;x<moduleIdx;x++) { 
        modules[x].faultBuffer[0]=0x55;
        modules[x].faultBuffer[1]=0;
        modules[x].faultBuffer[2]=0x55;
        modules[x].faultBuffer[3]=0;
        modules[x].faultBuffer[4]= 0xaa ;  //cksum for 01010101 00000000 
        if (!modules[x].zoneStatusByte) continue;
        zoneupdate[modules[x].zoneStatusByte]&=modules[x].zoneStatusMask; //set update slot
      }
      writeCharsToQueue(zoneupdate,9,maxFields05);
}

//once we know what panelversion we have, we can then update the modules with the correct info here
void dscKeybusInterface::updateModules() {
          
    //start expander
          //sanity check - we make sure that our module responses fit the cmd size
        if (keybusVersion1) {
            maxFields05=4; 
            maxFields11=4; 
            panelVersion=2;
        } else {
            maxFields05=6; 
            maxFields11=6; 
            panelVersion=3;
        }

    for (int x=0;x<moduleIdx;x++) {
        zoneMaskType zm=getUpdateMask(modules[x].address);
        if (!zm.idx) {
            //we don't have an update idx so it means our address is invalid. clear it
            modules[x].address=0;
        } else {
            modules[x].zoneStatusByte=zm.idx;
            modules[x].zoneStatusMask=zm.mask;
            if (enableModuleSupervision)
                setSupervisorySlot(modules[x].address,true);
        }
    }
}

//add new expander modules and init zone fields
void dscKeybusInterface::addModule(byte address) {
    
if (!address || (address > 12 && maxZones <=32)) return;
 if (moduleIdx < maxModules ) {
   modules[moduleIdx].address=address;
   memset(modules[moduleIdx].fields,0x55,4);
   moduleIdx++;
 }
}

void dscKeybusInterface::addRelayModule() {
    if (enableModuleSupervision)
        setSupervisorySlot(18,true);
}

void dscKeybusInterface::removeModule(byte address) {
    int idx;
    for (idx=0;idx<moduleIdx;idx++) {
        if (modules[idx].address==address) break;
    }
    if (idx==moduleIdx) return;
   modules[idx].address=0;
   setSupervisorySlot(address,false); //remove it from the supervisory response

}

#if defined(__AVR__)
void dscKeybusInterface::processCmd70() {
#elif defined(ESP8266)
void   ICACHE_RAM_ATTR dscKeybusInterface::processCmd70() {
#elif defined(ESP32)
void IRAM_ATTR dscKeybusInterface::processCmd70() {
#endif

      if (!pending70) return;
      int dataSum=0;
      pending70=false;       
      int len=pgmBuffer.idx+4<=pgmBuffer.len?4:pgmBuffer.len-pgmBuffer.idx;
      if (len < 1) return;
      writeBufferLength=5;
      writeDataBit=9;
      writeBufferIdx=0;           
      memset((void*)writeBuffer,0,5);
      for(byte x=0;x<len;x++) {
                writeBuffer[x]=pgmBuffer.data[pgmBuffer.idx+x];
                dataSum+=writeBuffer[x];
      }
      pgmBuffer.idx+=len;
      writeBuffer[4]=dataSum %256;
      writeDataPending=true; 
      
     byte key=0x2D; //'#' // setup to send final # cmd to complete write update to panel
     if (pgmBuffer.idx < pgmBuffer.len) {
        pending70=true;
        key=0xAA; //more data available so set up also for next group send request
     } 
     writeCharsToQueue(&key,9);

}

void dscKeybusInterface::setLCDReceive(byte len) {
       pgmBuffer.idx=0;
       pgmBuffer.len=len;
       pending6E=true;
       byte key=0xa5;     
       writeCharsToQueue(&key,9);
}

void dscKeybusInterface::setLCDSend() {
    if (pgmBuffer.idx==pgmBuffer.len) return;
     pending70=true;
     byte key=0xAA;
     writeCharsToQueue(&key,9);

}

void dscKeybusInterface::setZoneFault(byte zone,bool fault) {
      
    byte address=0;
    byte channel=0;
    bool change=false;
    
    //we try and do as much setup here so that the ISR functions do the mimimal work.
    if (zone > maxZones) return;
    
    //get address and channel from zone range
        if (zone > 8 && zone < 17) {
            address=9;
            channel=zone-9;
        } else if (zone > 16 && zone < 25) {
            address=10;
            channel=zone-17;
        } else if (zone > 24 && zone < 33) {
            address=11;
            channel=zone-25;
        } else if (zone > 32 && zone < 41) {
            address=12;
            channel=zone-33;
        } else if (zone > 40 && zone < 49) {
            address=13;
            channel=zone-41;
        } else if (zone > 48 && zone < 57 ) {
            address=14;
            channel=zone-49;
        } else if (zone > 56 && zone < 65) {
            address=16;
            channel=zone-57;
        } 
    

    if (!address ) return; //invalid zone, so return

 
    //see if we are emulating this zone range 
    int idx;
    for (idx=0;idx<moduleIdx;idx++) {
        if (modules[idx].address==address) break;
    }

    if (idx==moduleIdx) return;  //address not found in our emulation list so return
    
    uint8_t chk1=0xff;
    uint8_t chk2=0xff;
    
    if (channel < 4) { //set / reset bits according to fault value (open=11,closed=01)
        channel=channel*2;
        modules[idx].fields[0]=fault?modules[idx].fields[0] | (zoneOpen << channel):modules[idx].fields[0] & ~(zoneClosed << channel);
        chk1=((modules[idx].fields[0] >> 4)+(modules[idx].fields[0]&0x0f)+(modules[idx].fields[1]>>4)+(modules[idx].fields[1]&0x0f)) % 0x10;
    } else {
        channel=(channel-4)*2;
        modules[idx].fields[2]=fault?modules[idx].fields[2] | (zoneOpen << (channel)):modules[idx].fields[2] & ~(zoneClosed << (channel));
        chk2=((modules[idx].fields[2]>>4)+(modules[idx].fields[2]&0x0f)+(modules[idx].fields[3]>>4)+(modules[idx].fields[3]&0x0f)) % 0x10;
    }

    memset(modules[idx].faultBuffer,0xFF,5);

    if ( modules[idx].fields[0] != modules[idx].fields[1]) { //see if our current low channels changed from previous. 
         modules[idx].faultBuffer[0]=modules[idx].fields[0]; //populate faultbuffer with response data for low channel
         modules[idx].faultBuffer[1]=modules[idx].fields[1];
         modules[idx].faultBuffer[4]=(chk1 << 4) | 0x0F; 
         modules[idx].fields[1]=modules[idx].fields[0];  //copy current channel values to previous
         change=true;
    }
    if (modules[idx].fields[2] != modules[idx].fields[3]) {  //check high channels
         modules[idx].faultBuffer[2]=modules[idx].fields[2];
         modules[idx].faultBuffer[3]=modules[idx].fields[3];
         modules[idx].faultBuffer[4]= (modules[idx].faultBuffer[4] & 0xf0) | chk2 ; 
         modules[idx].fields[3]=modules[idx].fields[2];  //copy current channel values to previous
         change=true;
    }

    if (!change) return;  //nothing changed in our zones so return
     byte zoneupdate[maxFields05];
     memset(zoneupdate,0xFF,maxFields05); //set update slots to 1's. Only zero bits indicate a request
    if (modules[idx].zoneStatusByte) { 
       zoneupdate[modules[idx].zoneStatusByte]&=modules[idx].zoneStatusMask; //set update slot
       writeCharsToQueue(zoneupdate,9,maxFields05);
    }

}


#if defined(__AVR__)
void dscKeybusInterface::dscKeybusInterface::prepareModuleResponse(byte address,int bit) {
#elif defined(ESP8266)
void   ICACHE_RAM_ATTR dscKeybusInterface::dscKeybusInterface::prepareModuleResponse(byte address,int bit) {
#elif defined(ESP32)
void IRAM_ATTR dscKeybusInterface::dscKeybusInterface::prepareModuleResponse(byte address,int bit) {
#endif

    for (int idx=0;idx<moduleIdx;idx++) {  //get the buffer data from the module record that matches the address we need
        if (modules[idx].address==address) {
             //pendingZoneStatus[modules[idx].zoneStatusByte]|=~modules[idx].zoneStatusMask; //clear update slot
             updateWriteBuffer((byte*) modules[idx].faultBuffer,5,bit,false);
            return;
        }
    }

 
}

#endif
