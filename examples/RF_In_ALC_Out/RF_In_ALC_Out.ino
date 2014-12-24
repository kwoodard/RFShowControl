/*
 *  Arduino Light Controller 
 *
 *    Input: nRF
 *    Output: Arduino Light Controller
 *
 * Created on: December 23, 2014
 * Author: Keith Woodard <rkwoodard@gmail.com>
 *
 * License:
 *  The Arduino Light Controller Sketch is free software: you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  The Arduino Light Controller Sketch is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *   along with The Arduino Light Controller Sketch.  If not, see
 *  <http://www.gnu.org/licenses/>. 
 */
#include <tlc_config.h>
#include <Tlc5940.h> /* Include modified version to TLC5940 library */
#include <RF24.h>    /* Included for RF support using nRF24L01+ */
#include <EEPROM.h>
#include <SPI.h>
#include "printf.h"
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

#define ALC_NUMBER_OF_BOARDS            2
#define RECEIVER_UNIQUE_ID              32
#define HARDCODED_START_CHANNEL         0
#define ALC_LATCH_PIN_1                 7
#define ALC_LATCH_PIN_2                 4
#define ALC_LATCH_PIN_3                 5
#define ALC_LATCH_PIN_4                 6
/******************* END OF NON-OTA CONFIGURATION SECTION ********************/
#define OVER_THE_AIR_CONFIG_ENABLE      0 
#define HARDCODED_NUM_CHANNELS          (32 * ALC_NUMBER_OF_BOARDS)
#define PIXEL_TYPE                      NONE
//Include this after all configuration variables are set
#include "RFShowControlConfig.h"

#define NUMBER_OF_TLCS_TOTAL    2                       /* Each Shield has 2 TLC5940s on it */
#define NUMBER_OF_CHANNELS     16*NUMBER_OF_TLCS_TOTAL  /* Number of channels per shield (32) */
#define MAX_NUMBER_OF_SHIELDS  4                        /* Maximum number of shields that can be stacked on one Arduino */

#define INTERRUPT  0                                    /* Interrupt used for the zero cross signal */
#define _BLANK_PIN  9                                   /* Arduino pin connected to the TLC5940 BLANK signals */

/* Some definitions for light values */
/* Board design has inverted logic, so 0 = on, 4095 = off */
#define FULL_ON         0u
#define FULL_OFF        4095u

// Global Data
volatile int boardToLatch = -1;          /* Board to latch during next interrupt */
boolean dataReceived;                    /* Flag to indicate data has been received from vixen interface */

// Configuration data
const uint8_t latch_pin[MAX_NUMBER_OF_SHIELDS] = {ALC_LATCH_PIN_1, ALC_LATCH_PIN_2, ALC_LATCH_PIN_3, ALC_LATCH_PIN_4}; /* Store the latch pin for each possible board */
uint16_t  zeroCrossDelay;      /* Configurable delay for zero cross delay to tweak for phase shift of zero cross transformer */

uint8_t * data = NULL;  /* Pointer to the data buffer in the RFShowControl class */

/*
 * Function: setup
 * Description: Power-on configuration of the arduino.  Initialize the
 * correct pin modes for Arduino pins used.  Initialize serial port
 * communications, the TLC5940, and all global data. 
 */
void setup() {
  /* Setup pins as needed */
  pinMode(2, INPUT_PULLUP);
  pinMode(_BLANK_PIN, OUTPUT);
  /* TODO - Figure out how to set the pin modes only for the latch pins configured for use */
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);

#ifdef DEBUG
  Serial.begin(57600);
  printf_begin();
  Serial.println("Initializing Radio");
#endif

  radio.EnableOverTheAirConfiguration(OVER_THE_AIR_CONFIG_ENABLE);
  uint8_t logicalControllerNumber = 0;
  if(!OVER_THE_AIR_CONFIG_ENABLE)
  {
    radio.AddLogicalController(logicalControllerNumber, HARDCODED_START_CHANNEL, ALC_NUMBER_OF_BOARDS * 32, 0);
  }

  radio.Initialize(radio.RECEIVER, pipes, TRANSMIT_CHANNEL, DATA_RATE, RECEIVER_UNIQUE_ID);

#ifdef DEBUG
  radio.printDetails();
#endif

  logicalControllerNumber = 0;
  int numberOfChannels = radio.GetNumberOfChannels(0);

#ifdef DEBUG
  Serial.print(F("Number of channels configured "));
  printf("%d\n", numberOfChannels);
#endif

 /* Mark no data received, no board to latch, init the TLC5940 library,
   * attach interrupt, and begin boot mode.
   */
  dataReceived = false;
  boardToLatch = -1;
  // Initialize TLC5940 Library
  Tlc.init();   
  
  // Attach interrupt to handle zero-crossing
  attachInterrupt(INT0, ZeroCross, RISING);
 
  //Initialize the data for channels
  uint8_t * data = (uint8_t*) radio.GetControllerDataBase(logicalControllerNumber);
  memset(data, FULL_OFF, numberOfChannels * sizeof(uint8_t));

  WriteDataToShields();
}

/* Function: ZeroCross
 * Description: This is the main interrupt handler for zero cross detection.
 *   This function toggles the blank pin (to signal the TLC5940's) to begin
 *   the next PWM cycle.  It also checks to see if new data needs to be 
 *   latched into the TLC5940s.  If new data is ready to be latched, it
 *   toggles the latch pin as well.
 */
void ZeroCross()
{
  static unsigned long time = micros();
  
  /* For some reason, 4 interrupts are created for every zero cross.  This
   * logic filters out all of them except the first one.
   */
  if (micros() - time > 7000)
  {
    time = micros();
    delayMicroseconds(zeroCrossDelay);
    digitalWrite(_BLANK_PIN, HIGH);
    if ( (boardToLatch != -1) )
    {
        // Pulse the latch to latch in the new data
        // pulse must be 16 nS.  This should work fine.
        digitalWrite(latch_pin[boardToLatch], HIGH);
        digitalWrite(latch_pin[boardToLatch], LOW);
    }
    digitalWrite(_BLANK_PIN, LOW);
    boardToLatch = -1;
  }
}

/* Function: loop
 * Description: Main processing loop of the application. This loop
 * executes the main state machine for the board.
 */
void loop() {
     /* Read data from RF interface and then write it to the shield */
    if (radio.Listen())
    { 
      WriteDataToShields();
    }
}

/* Function: WriteDataToShields
 * Description: Write channel data to each shield.  If more than one
 *   shield is used, data is written to only one shield at a time by
 *   writing out the data to one shield and then waiting until that 
 *   data has been latched (as part of Zero Cross Interrupt) before
 *   writing data to the next shield.
 */
void WriteDataToShields()
{
  for (uint32_t board = 0; board < ALC_NUMBER_OF_BOARDS; board++)
  {
    for (uint32_t i=0; i < NUMBER_OF_CHANNELS; i++)
    {
      Tlc.set(i, data[board*NUMBER_OF_CHANNELS + i]);
    }
    Tlc.update();
    boardToLatch = board;

    /* busy loop until interrupt handler clears this flag */
    while (boardToLatch != -1) {}
  }
}


