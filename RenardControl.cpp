/*
 * RenardControl
 *
 * Created on: June 20th, 2013
 * Author: Mat Mrosko
 *
 * Revision History: 
 *   May 18, 2014 - Mat Mrosko, Materdaddy, rfpixelcontrol@matmrosko.com
 *   December 24, 2014 - Keith Woodard, kwoodard, rkwoodard@gmail.com
 *
 * License:
 *    Users of this software agree to hold harmless the creators and
 *    contributors of this software.  By using this software you agree that
 *    you are doing so at your own risk, you could kill yourself or someone
 *    else by using this software and/or modifying the factory controller.
 *    By using this software you are assuming all legal responsibility for
 *    the use of the software and any hardware it is used on.
 *
 *    The Commercial Use of this Software is Prohibited.
 */

#include "RenardControl.h"

#define PAD                             0x7D
#define SYNC                            0x7E
#define ESCAPE                          0x7F
#define COMMAND                         0x80

static uint32_t numberOfChannels = 0;
static uint8_t * buffer = NULL;
static uint32_t renardBaudRate = 0;

// Constructor
// Inputs:
//   baud_rate - the desired baud rate of the serial port
RenardControl::RenardControl(uint32_t baud_rate)
{
  renardBaudRate = baud_rate;
}

//   dataBuffer - a pointer to a buffer containing channel data.  One byte per channel.  This buffer 
//                should be sized large enough to contain data for channelCount channels.
//   channelCount - number of channels of data
void RenardControl::Begin(uint8_t * dataBuffer, uint32_t channelCount)
{
  Serial.begin(renardBaudRate);
  channelCount = numberOfChannels;
  buffer = dataBuffer;
  // Initialize dataBuffer to all 0's
  memset(dataBuffer, 0, channelCount*sizeof(uint8_t));
}

void RenardControl::Write(void)
{
  Serial.write(SYNC);
  Serial.write(COMMAND);

  for ( int i = 0; i < numberOfChannels; i++ )
  {
    switch (buffer[i])
    {
      case 0x7D:
        Serial.write(ESCAPE);
        Serial.write(0x2F);
        break;

      case 0x7E:
        Serial.write(ESCAPE);
        Serial.write(0x30);
        break;

      case 0x7F:
        Serial.write(ESCAPE);
        Serial.write(0x31);
        break;

      default:
        Serial.write(buffer[i]);
        break;
    }
  }
}

// Function: Read
// Description: Reads a Renard stream of data from serial port containing channelCount
//              number of channels.  Returns when a complete buffer has been read.
//              This function will scan for the 0x7E start byte followed by 0x80 before
//              reading the channel data.  If a 0x7E is detected, but not followed by 0x80,
//              this function will return a false indication.
boolean RenardControl::Read(void)
{
  boolean success = false;  
  boolean escapeSequence = false;
  
  int index = 0;
  
  // Wait for first start byte (0x7E)
  int startByte1 = 0;
  while (startByte1 != SYNC)
  {
    if (Serial.available() > 0)
    {
      startByte1 = Serial.read();
    }
  }
  
  // Then, make sure second start byte is 0x80
  while (Serial.available() == 0) {} 
  int startByte2 = Serial.read();
  
  if (startByte2 == COMMAND)
  {
    while (index < numberOfChannels)
    {
      if (Serial.available() > 0)
      {
        /* Read the next byte from the serial port.  If the index exceeds
         * the starting address for this board, store off the value into the 
         * data array.
         */
         uint8_t byteRead = Serial.read();
         if (escapeSequence == false) 
         {
           if (byteRead == ESCAPE)
           {
             escapeSequence = 1;
           } else if (byteRead == PAD ) 
           {
             /* Do nothing with pad byte, but discard */
           } else
           {
             buffer[index++] = byteRead;
           }
         } else /* Escape bit is set */
         {
           escapeSequence = false;
           if (byteRead == 0x2F)
           {
             buffer[index++] = 0x7D;
           } else if (byteRead == 0x30)
           {
             buffer[index++] = 0x7E;
           } else if (byteRead == 0x31)
           {
             buffer[index++] = 0x7F;
           }          
         }
      } 
    }
    success = true;
  } else {
    // Failed to detect beginning of Renard stream.  Need to restart read.
    success = false;
  }
  return success;
}
