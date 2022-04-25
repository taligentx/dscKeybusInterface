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

#ifndef dscKeybusInterface_h
#define dscKeybusInterface_h


// DSC Classic Series
#if defined dscClassicSeries
#include "dscClassic.h"

byte dscClassicInterface::dscClockPin;
byte dscClassicInterface::dscReadPin;
byte dscClassicInterface::dscPC16Pin;
byte dscClassicInterface::dscWritePin;
char dscClassicInterface::writeKey;
byte dscClassicInterface::writePartition;
byte dscClassicInterface::writeByte;
byte dscClassicInterface::writeBit;
bool dscClassicInterface::virtualKeypad;
bool dscClassicInterface::processModuleData;
byte dscClassicInterface::panelData[dscDataSize];
byte dscClassicInterface::pc16Data[dscDataSize];
byte dscClassicInterface::panelByteCount;
byte dscClassicInterface::panelBitCount;
volatile bool dscClassicInterface::writeKeyPending;
volatile bool dscClassicInterface::writeKeyWait;
volatile byte dscClassicInterface::moduleData[dscDataSize];
volatile bool dscClassicInterface::moduleDataCaptured;
volatile byte dscClassicInterface::moduleByteCount;
volatile byte dscClassicInterface::moduleBitCount;
volatile bool dscClassicInterface::writeAlarm;
volatile bool dscClassicInterface::starKeyDetected;
volatile bool dscClassicInterface::starKeyCheck;
volatile bool dscClassicInterface::starKeyWait;
volatile bool dscClassicInterface::bufferOverflow;
volatile byte dscClassicInterface::panelBufferLength;
volatile byte dscClassicInterface::panelBuffer[dscBufferSize][dscDataSize];
volatile byte dscClassicInterface::pc16Buffer[dscBufferSize][dscDataSize];
volatile byte dscClassicInterface::panelBufferBitCount[dscBufferSize];
volatile byte dscClassicInterface::panelBufferByteCount[dscBufferSize];
volatile byte dscClassicInterface::isrPanelData[dscDataSize];
volatile byte dscClassicInterface::isrPC16Data[dscDataSize];
volatile byte dscClassicInterface::isrPanelByteCount;
volatile byte dscClassicInterface::isrPanelBitCount;
volatile byte dscClassicInterface::isrPanelBitTotal;
volatile byte dscClassicInterface::isrModuleData[dscDataSize];
volatile byte dscClassicInterface::isrModuleByteCount;
volatile byte dscClassicInterface::isrModuleBitCount;
volatile byte dscClassicInterface::isrModuleBitTotal;
volatile byte dscClassicInterface::moduleCmd;
volatile bool dscClassicInterface::readyLight;
volatile bool dscClassicInterface::lightBlink;
volatile unsigned long dscClassicInterface::clockHighTime;
volatile unsigned long dscClassicInterface::keybusTime;
volatile unsigned long dscClassicInterface::writeCompleteTime;

// Interrupt function called after 250us by dscClockInterrupt() using AVR Timer1, disables the timer and calls
// dscDataInterrupt() to read the data line
#if defined(__AVR__)
ISR(TIMER1_OVF_vect) {
  TCCR1B = 0;  // Disables Timer1
  dscClassicInterface::dscDataInterrupt();
}
#endif  // __AVR__


// DSC Keypad Interface
#elif defined dscKeypad
#include "dscKeypad.h"

byte dscKeypadInterface::dscClockPin;
byte dscKeypadInterface::dscReadPin;
byte dscKeypadInterface::dscWritePin;
int  dscKeypadInterface::clockInterval;
volatile byte dscKeypadInterface::keyData;
volatile byte dscKeypadInterface::keyBufferLength;
volatile byte dscKeypadInterface::keyBuffer[dscBufferSize];
volatile bool dscKeypadInterface::bufferOverflow;
volatile bool dscKeypadInterface::commandReady;
volatile bool dscKeypadInterface::moduleDataDetected;
volatile bool dscKeypadInterface::alarmKeyDetected;
volatile bool dscKeypadInterface::alarmKeyResponsePending;
volatile byte dscKeypadInterface::clockCycleCount;
volatile byte dscKeypadInterface::clockCycleTotal;
volatile byte dscKeypadInterface::panelCommand[dscDataSize];
volatile byte dscKeypadInterface::isrPanelBitTotal;
volatile byte dscKeypadInterface::isrPanelBitCount;
volatile byte dscKeypadInterface::panelCommandByteCount;
volatile byte dscKeypadInterface::isrModuleData[dscDataSize];
volatile byte dscKeypadInterface::isrModuleBitTotal;
volatile byte dscKeypadInterface::isrModuleBitCount;
volatile byte dscKeypadInterface::isrModuleByteCount;
volatile byte dscKeypadInterface::panelCommandByteTotal;
volatile byte dscKeypadInterface::moduleData[dscDataSize];

#if defined(__AVR__)
ISR(TIMER1_OVF_vect) {
  dscKeypadInterface::dscClockInterrupt();
}
#endif  // __AVR__

// DSC Classic Keypad Interface
#elif defined dscClassicKeypad
#include "dscClassicKeypad.h"

byte dscClassicKeypadInterface::dscClockPin;
byte dscClassicKeypadInterface::dscReadPin;
byte dscClassicKeypadInterface::dscWritePin;
int  dscClassicKeypadInterface::clockInterval;
volatile byte dscClassicKeypadInterface::keyData;
volatile byte dscClassicKeypadInterface::keyBufferLength;
volatile byte dscClassicKeypadInterface::keyBuffer[dscBufferSize];
volatile bool dscClassicKeypadInterface::bufferOverflow;
volatile bool dscClassicKeypadInterface::commandReady;
volatile bool dscClassicKeypadInterface::moduleDataDetected;
volatile bool dscClassicKeypadInterface::alarmKeyDetected;
volatile bool dscClassicKeypadInterface::alarmKeyResponsePending;
volatile byte dscClassicKeypadInterface::clockCycleCount;
volatile byte dscClassicKeypadInterface::clockCycleTotal;
volatile byte dscClassicKeypadInterface::panelCommand[dscDataSize];
volatile byte dscClassicKeypadInterface::isrPanelBitTotal;
volatile byte dscClassicKeypadInterface::isrPanelBitCount;
volatile byte dscClassicKeypadInterface::panelCommandByteCount;
volatile byte dscClassicKeypadInterface::isrModuleData[dscDataSize];
volatile byte dscClassicKeypadInterface::isrModuleBitTotal;
volatile byte dscClassicKeypadInterface::isrModuleBitCount;
volatile byte dscClassicKeypadInterface::isrModuleByteCount;
volatile byte dscClassicKeypadInterface::panelCommandByteTotal;
volatile byte dscClassicKeypadInterface::moduleData[dscDataSize];
volatile unsigned long dscClassicKeypadInterface::intervalStart;
volatile unsigned long dscClassicKeypadInterface::beepInterval;
volatile unsigned long dscClassicKeypadInterface::repeatInterval;
volatile unsigned long dscClassicKeypadInterface::keyInterval;
volatile unsigned long dscClassicKeypadInterface::alarmKeyTime;
volatile unsigned long dscClassicKeypadInterface::alarmKeyInterval;

#if defined(__AVR__)
ISR(TIMER1_OVF_vect) {
  dscClassicKeypadInterface::dscClockInterrupt();
}
#endif  // __AVR__


// DSC PowerSeries Keybus Reader - includes capturing module data
#elif defined dscKeybusReader
#include "dscKeybusReader.h"

byte dscKeybusReaderInterface::dscClockPin;
byte dscKeybusReaderInterface::dscReadPin;
byte dscKeybusReaderInterface::dscWritePin;
char dscKeybusReaderInterface::writeKey;
byte dscKeybusReaderInterface::writePartition;
byte dscKeybusReaderInterface::writeByte;
byte dscKeybusReaderInterface::writeBit;
bool dscKeybusReaderInterface::virtualKeypad;
byte dscKeybusReaderInterface::panelData[dscDataSize];
byte dscKeybusReaderInterface::panelByteCount;
byte dscKeybusReaderInterface::panelBitCount;
volatile bool dscKeybusReaderInterface::writeKeyPending;
volatile byte dscKeybusReaderInterface::moduleData[dscDataSize];
volatile bool dscKeybusReaderInterface::moduleDataCaptured;
volatile bool dscKeybusReaderInterface::moduleDataDetected;
volatile byte dscKeybusReaderInterface::moduleByteCount;
volatile byte dscKeybusReaderInterface::moduleBitCount;
volatile bool dscKeybusReaderInterface::writeAlarm;
volatile bool dscKeybusReaderInterface::starKeyCheck;
volatile bool dscKeybusReaderInterface::starKeyWait[dscPartitions];
volatile bool dscKeybusReaderInterface::bufferOverflow;
volatile byte dscKeybusReaderInterface::panelBufferLength;
volatile byte dscKeybusReaderInterface::panelBuffer[dscBufferSize][dscDataSize];
volatile byte dscKeybusReaderInterface::panelBufferBitCount[dscBufferSize];
volatile byte dscKeybusReaderInterface::panelBufferByteCount[dscBufferSize];
volatile byte dscKeybusReaderInterface::isrPanelData[dscDataSize];
volatile byte dscKeybusReaderInterface::isrPanelByteCount;
volatile byte dscKeybusReaderInterface::isrPanelBitCount;
volatile byte dscKeybusReaderInterface::isrPanelBitTotal;
volatile byte dscKeybusReaderInterface::isrModuleData[dscDataSize];
volatile byte dscKeybusReaderInterface::currentCmd;
volatile byte dscKeybusReaderInterface::statusCmd;
volatile byte dscKeybusReaderInterface::moduleCmd;
volatile byte dscKeybusReaderInterface::moduleSubCmd;
volatile unsigned long dscKeybusReaderInterface::clockHighTime;
volatile unsigned long dscKeybusReaderInterface::keybusTime;

// Interrupt function called after 250us by dscClockInterrupt() using AVR Timer1, disables the timer and calls
// dscDataInterrupt() to read the data line
#if defined(__AVR__)
ISR(TIMER1_OVF_vect) {
  TCCR1B = 0;  // Disables Timer1
  dscKeybusReaderInterface::dscDataInterrupt();
}
#endif  // __AVR__


// DSC PowerSeries
#else
#include "dscKeybus.h"

byte dscKeybusInterface::dscClockPin;
byte dscKeybusInterface::dscReadPin;
byte dscKeybusInterface::dscWritePin;
char dscKeybusInterface::writeKey;
byte dscKeybusInterface::writePartition;
byte dscKeybusInterface::writeByte;
byte dscKeybusInterface::writeBit;
bool dscKeybusInterface::virtualKeypad;
byte dscKeybusInterface::panelData[dscDataSize];
byte dscKeybusInterface::panelByteCount;
byte dscKeybusInterface::panelBitCount;
volatile bool dscKeybusInterface::writeKeyPending;
volatile bool dscKeybusInterface::writeAlarm;
volatile bool dscKeybusInterface::starKeyCheck;
volatile bool dscKeybusInterface::starKeyWait[dscPartitions];
volatile bool dscKeybusInterface::bufferOverflow;
volatile byte dscKeybusInterface::panelBufferLength;
volatile byte dscKeybusInterface::panelBuffer[dscBufferSize][dscDataSize];
volatile byte dscKeybusInterface::panelBufferBitCount[dscBufferSize];
volatile byte dscKeybusInterface::panelBufferByteCount[dscBufferSize];
volatile byte dscKeybusInterface::isrPanelData[dscDataSize];
volatile byte dscKeybusInterface::isrPanelByteCount;
volatile byte dscKeybusInterface::isrPanelBitCount;
volatile byte dscKeybusInterface::isrPanelBitTotal;
volatile byte dscKeybusInterface::statusCmd;
volatile unsigned long dscKeybusInterface::clockHighTime;
volatile unsigned long dscKeybusInterface::keybusTime;
#ifdef EXPANDER
//start expander
bool dscKeybusInterface::debounce05;
byte dscKeybusInterface::moduleSlots[6];
writeQueueType dscKeybusInterface::writeQueue[writeQueueSize];
volatile byte dscKeybusInterface::writeBuffer[6];
//volatile byte dscKeybusInterface::pendingZoneStatus[6];
volatile bool dscKeybusInterface::pending70;
volatile bool dscKeybusInterface::pending6E;
moduleType dscKeybusInterface::modules[maxModules];
byte dscKeybusInterface::moduleIdx;byte dscKeybusInterface::inIdx;
byte dscKeybusInterface::outIdx;
byte dscKeybusInterface::maxFields05; 
byte dscKeybusInterface::maxFields11;
byte dscKeybusInterface::maxZones;
bool dscKeybusInterface::enableModuleSupervision;
volatile byte dscKeybusInterface::writeBufferIdx;
volatile byte dscKeybusInterface::writeBufferLength;
volatile bool dscKeybusInterface::writeDataPending;
byte dscKeybusInterface::writeDataBit;
volatile pgmBufferType dscKeybusInterface::pgmBuffer;
//end expander
#endif
// Interrupt function called after 250us by dscClockInterrupt() using AVR Timer1, disables the timer and calls
// dscDataInterrupt() to read the data line
#if defined(__AVR__)
ISR(TIMER1_OVF_vect) {
  TCCR1B = 0;  // Disables Timer1
  dscKeybusInterface::dscDataInterrupt();
}
#endif  // __AVR__
#endif  // dscClassicSeries, dscKeypadInterface
#endif  // dscKeybusInterface_h
