/*
 * Renard Transmitter
 *
 *  Author: Keith Woodard
 *      12/2014
 *   This transmitter reads Renard data via the Serial port and transmits it via RF
 */

#include <RF24Wrapper.h>
#include <RenardControl.h>
#include <printf.h>
#include <OTAConfig.h>
#include <MemoryFree.h>
#include <EEPROMUtils.h>
#include <EEPROM.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <SPI.h>

#include "RFShowControl.h"


/********************* START OF REQUIRED CONFIGURATION ***********************/
// NRF_TYPE Description: http://learn.komby.com/wiki/58/configuration-settings#NRF_TYPE
// Valid Values: RF1, MINIMALIST_SHIELD, WL_SHIELD, WM_2999_NRF, RFCOLOR_2_4
#define NRF_TYPE                        WL_SHIELD
/********************** END OF REQUIRED CONFIGURATION ************************/

/****************** START OF NON-OTA CONFIGURATION SECTION *******************/
// TRANSMIT_CHANNEL Description: http://learn.komby.com/wiki/58/configuration-settings#TRANSMIT_CHANNEL
// Valid Values: 0-83, 101-127  (Note: use of channels 84-100 is not allowed in the US)
#define TRANSMIT_CHANNEL                10

// DATA_RATE Description: http://learn.komby.com/wiki/58/configuration-settings#DATA_RATE
// Valid Values: RF24_250KBPS, RF24_1MBPS
#define DATA_RATE                       RF24_250KBPS

// RENARD_BAUD_RATE Description: http://learn.komby.com/wiki/58/configuration-settings#RENARD_BAUD_RATE
// Valid Values: 19200, 38400, 57600, 115200, 230400, 460800
#define RENARD_BAUD_RATE                57600

#define HARDCODED_NUM_CHANNELS          128
/******************* END OF NON-OTA CONFIGURATION SECTION ********************/

/************** START OF ADVANCED SETTINGS SECTION (OPTIONAL) ****************/
// DEBUG Description: http://learn.komby.com/wiki/58/configuration-settings#DEBUG
//#define DEBUG                           1
/********************* END OF ADVANCED SETTINGS SECTION **********************/


#define PIXEL_TYPE                      NONE
#define RF_WRAPPER                      0
//Include this after all configuration variables are set
#include "RFShowControlConfig.h"

#define RF_NUM_PACKETS                  ((int)HARDCODED_NUM_CHANNELS + 29/30)     // Number of packets required to be transmitted.   Round up.
#define RECEIVER_UNIQUE_ID              0


RenardControl renard(RENARD_BAUD_RATE );
const uint8_t logicalControllerNumber = 0;

void setup(void)
{
  radio.Initialize(radio.TRANSMITTER, pipes, TRANSMIT_CHANNEL, DATA_RATE, RECEIVER_UNIQUE_ID);
  renard.Begin(radio.GetControllerDataBase(logicalControllerNumber) , HARDCODED_NUM_CHANNELS);
  radio.SendPackets(logicalControllerNumber);
}

void loop(void)
{
  //If the renard protocol returned valid data stream, write it to the RF interface
  if (renard.Read() == true)
  {
    radio.SendPackets(logicalControllerNumber);
  }
}

