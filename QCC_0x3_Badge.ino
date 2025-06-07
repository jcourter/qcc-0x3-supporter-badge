/* QueenCityCon 0x3 badge sketch - Written by Jeremy Courter <jeremy@courter.org>.  A portion of this code 
 * was borrowed from the code for the original DIY Geiger Kit written by my friend John Giametti, with 
 * significant contributions from me circa 2013-2014.  John no longer sells the kits but the code and 
 * schematics are available here:
 *
 *      https://sites.google.com/site/diygeigercounter/home
 *
 * The badge and its code are a tribute to the fun times I had working with John on the software
 * and using it to pick up a whole bunch of radioactive rocks.
 *
 *  *** Original license from the DIY Geiger kit follows and applies since this project is based on it *** 
 *
 * LICENSE: Creative Commons NonCommercial-ShareAlike 4.0
 * See http://creativecommons.org/licenses/by-nc-sa/4.0/ for a full definition. 
 * Some key features are:
 * - You are granted a royalty-free, non-exclusive, license to use and Share this Licensed Material
 *   in whole or in part, for NonCommercial purposes only.
 * - Every recipient of this software automatically receives the same rights.  
 *   You may not impose any additional or different terms or conditions on this material.
 * - If You Share this material (including in modified form), you must retain the identification 
 *   of the DIY Geiger, this copyright notice, and indicate this Public License, 
 * 
 * This program is distributed WITHOUT ANY WARRANTY implied or otherwise, and with no warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * Do not remove information from this header.
 * 
 * Live long and prosper.
 */

//----------------------------------------------------------------------------------------------+
//                              User setup #defines
//----------------------------------------------------------------------------------------------+

//----------------------------------------------------------------------------------------------+
//                       End user setup #defines (others in GeigerKit.h)
//----------------------------------------------------------------------------------------------+
#include <math.h>
#include <Arduino.h>
#include <EEPROM.h>
#include <PinChangeInterrupt.h>
#include <RDA5807.h> 
#include "radioxmit.h"
#include "QCCBadge.h"

//----------------------------------------------------------------------------------------------+
//                                     DEBUG Defines
//----------------------------------------------------------------------------------------------+
#define DEBUG          true            // if true, sends debug info to the serial port

//----------------------------------------------------------------------------------------------+
//                                      Functions
//----------------------------------------------------------------------------------------------+

void setup(){
  float globalBgRadAvg;
  Serial.begin(9600);                   // comspec 96,N,8,1
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(GM_TUBE_PIN),GetEvent,FALLING);  // Geiger event on pin 2 triggers interrupt
  pinMode(RED_LED_PIN,OUTPUT);              // setup LED pin
  digitalWrite(RED_LED_PIN,HIGH);
  pinMode(GREEN_LED_PIN,OUTPUT);              // setup LED pin
  digitalWrite(GREEN_LED_PIN,HIGH);
  pinMode(BLUE_LED_PIN,OUTPUT);              // setup LED pin
  digitalWrite(BLUE_LED_PIN,HIGH);
  pinMode(RADIO_SEEK_BUTTON,INPUT_PULLUP);
  pinMode(VOL_UP_BUTTON,INPUT_PULLUP);
  pinMode(VOL_DN_BUTTON,INPUT_PULLUP);
  Get_Settings();

  if(readButton(RADIO_SEEK_BUTTON) == LOW) {
#if (DEBUG)
    Serial.println("Signal hunt mode enabled!");
#endif
    ledMode = LED_RSSI_MODE;
  } else if(readButton(VOL_UP_BUTTON) == LOW) {
#if (DEBUG)
    Serial.println("Random color mode enabled!");
#endif
    ledMode = LED_RANDOM_MODE;
  } else if (readButton(VOL_DN_BUTTON) == LOW) {
#if (DEBUG)
    Serial.println("Pulse mode enabled!");
#endif
    ledMode = LED_PULSE_MODE;
  } else {
    ledMode = LED_RADIATION_MODE;   // Default to radiation mode for the full badge
  }

  rx.setup(); // Starts the FM radio receiver with default parameters
  rx.setBand(1);  // set the band to the Japanese broadcast band (76-91MHz), which overlaps nicely with the currently unused VHF TV channels 5 and 6 - code in the loop further limits this to between QCC_MIN_FREQ and QCC_MAX_FREQ
  rx.setStep(200);  // set tuning step to 200kHz
  rx.setVolume(7);  // Start at the middle of the volume range
  rx.setBass(true);  //Turn on extra bass for the tiny earphones

  Serial.print(F("CPM,"));            // print header for CSV output to serial
  serialprint_P((const char *)pgm_read_word(&(unit_table[doseUnit])));  // print dose unit (uSv/h, uR/h, or mR/h) to serial
  Serial.print(F(",Vcc\r\n"));

  if (doseUnit == 0) {
    globalBgRadAvg = AVGBGRAD_uSv;      // global average background radiation in uSv/h
  } 
  else if (doseUnit == 1) {
    globalBgRadAvg = AVBGRAD_uR;        // global average background radiation in uR/h
  } 
  else {
    globalBgRadAvg = AVBGRAD_mR;        // global average background radiation in mR/h
  }

  fastCountStart = radioPeriodStart = logPeriodStart = millis();;     // start timers
  fastCnt = logCnt = 0;     //initialize counts
}

void loop(){
  static unsigned long lastButtonTime;  // counter for pressing the button too quickly
  static unsigned long lastFastCnt = 0;
  static boolean blnLogStarted = false;
  static unsigned int lastFrequency = 0;
  static byte lastRssi = 0;
  static unsigned int rgbValue = 0;
  static boolean rgbDirection = 0;
  static byte currentMorseMessage;

  if (cwTransmitEnabled) { //only do this if init function has been executed - this happens if someone presses the vol_up and vol_dn buttons simultaneously
    if(!morseSender->continueSending()) {
      if (currentMorseMessage==0) {
        currentMorseMessage=1;
        morseSender->setMessage(String(CTF_SECRET_MESSAGE));
      } else {
        currentMorseMessage=0;
        morseSender->setMessage(String(CTF_SECRET_MESSAGE_PREAMBLE));
      }
      morseSender->startSending();
    }
  } else {
    if (ledMode==LED_PULSE_MODE) {
      byte r, g, b;
      if (rgbDirection==0) {
        if (rgbValue>=65534) {
          rgbDirection=1;
        }
        rgbValue++;
      } else {
        if (rgbValue<=1) {
          rgbDirection=0;
        }
        rgbValue--;
      }
      getRGBFromSpectrum(rgbValue, &r, &g, &b);
      if (lastR!=r) {
        lastR=r;
        analogWrite(RED_LED_PIN, 255-r);
      }
      if (lastG!=g) {
        lastG=g;
        analogWrite(GREEN_LED_PIN, 255-g);
      }
      if (lastB!=b) {
        lastB=b;
        analogWrite(BLUE_LED_PIN, 255-b);
      }
    }
  }

  if (readButton(RADIO_SEEK_BUTTON)== LOW && millis() >= lastButtonTime + 500){ // wait a bit between button pushes
    lastButtonTime = millis();          // reset the period time
    #if (DEBUG)
      Serial.println("RADIO_SEEK_BUTTON");
    #endif
    if (cwTransmitEnabled) {
      stopMorseSender();
      rx.powerUp();
    } else {
      //TODO make this flip between con frequencies
      //rx.seek(RDA_SEEK_WRAP,RDA_SEEK_UP,NULL);
      rx.setFrequencyUp();
    }
  }
 
  if (readButton(VOL_UP_BUTTON) == LOW && readButton(VOL_DN_BUTTON) == LOW && (!cwTransmitEnabled) && millis() >= lastButtonTime + 500){ // wait a bit between button pushes
    lastButtonTime = millis();          // reset the period time
    #if (DEBUG)
      Serial.println("VOL_UP_BUTTON && VOL_DN_BUTTON");
    #endif
    rx.powerDown();  //turn off our FM receiver because our noisy transmitter will be detected as noise and spoil all the fun
    analogWrite(RED_LED_PIN,192);
    analogWrite(GREEN_LED_PIN,255);
    analogWrite(BLUE_LED_PIN,255);
    lastR = 192;
    lastG = lastB = 255;
    initMorseSender();
    currentMorseMessage=0;
    morseSender->setMessage(String(CTF_SECRET_MESSAGE_PREAMBLE));
    morseSender->startSending();
  } else if (readButton(VOL_UP_BUTTON)== LOW && millis() >= lastButtonTime + 500){ // wait a bit between button pushes
    lastButtonTime = millis();          // reset the period time
    #if (DEBUG)
      Serial.println("VOL_UP_BUTTON");
    #endif
    rx.setVolumeUp();
  } else if (readButton(VOL_DN_BUTTON)== LOW && millis() >= lastButtonTime + 500){ // wait a bit between button pushes
    lastButtonTime = millis();          // reset the period time
    #if (DEBUG)
      Serial.println("VOL_DN_BUTTON");
    #endif
    rx.setVolumeDown();
  }

  if (millis() >= fastCountStart + 10000/FAST_ARRAY_MAX){ // update the 10 sec moving average
    fastAvgCount(fastCnt);
    fastCnt=0;                          // reset counts
    fastCountStart = millis();          // reset the period time
    if (!cwTransmitEnabled) {
      if (ledMode==LED_RADIATION_MODE) {
        CPStoRGB(getFastAvgCount());  //Update the LED color based on the fast average count
      }
    }
  }

  if (millis() >= radioPeriodStart + 100) { //query the radio module every 100ms
    radioPeriodStart=millis();             // reset the period time
    if(!cwTransmitEnabled) {
      unsigned int freq = rx.getRealFrequency();
      byte rssi = (byte)rx.getRssi();
      if (freq!=lastFrequency || rssi!=lastRssi) {
        lastFrequency = freq;
        lastRssi = rssi;
  #if (DEBUG)
        Serial.print("Tuned to ");
        Serial.print(freq);
        Serial.print(", RSSI: ");
        Serial.println(rssi);
  #endif
      }
      if(freq>=QCC_MAX_FREQ) {
#if (DEBUG)
        Serial.println("Freq out of bounds");
#endif
        rx.setFrequency(QCC_MIN_FREQ);
        //rx.seek(RDA_SEEK_WRAP,RDA_SEEK_UP,NULL);
      }
      if (ledMode==LED_RSSI_MODE) {
        CPStoRGB(rssi);  //Update the LED color based on the signal strength
      } else if (ledMode==LED_RANDOM_MODE) {
        analogWrite(RED_LED_PIN,(256-LED_MAX_INTENSITY)+rand()%LED_MAX_INTENSITY);
        analogWrite(GREEN_LED_PIN,(256-LED_MAX_INTENSITY)+rand()%LED_MAX_INTENSITY);
        analogWrite(BLUE_LED_PIN,(256-LED_MAX_INTENSITY)+rand()%LED_MAX_INTENSITY);
      }
    }
  }

  if (millis() >= logPeriodStart + LoggingPeriod && LoggingPeriod > 0){ // LOGGING PERIOD
    logCount(logCnt);                   // pass in the counts to be logged
    logCnt = 0;                         // reset log event counter
    logPeriodStart = millis(); // reset log time and display time too
  }
}

void Get_Settings(){ // get settings - the original kit stored these in the EEPROM but to simplify things for the badge, we'll just use values from QCCBadge.h
  doseRatio = PRI_RATIO;

  LoggingPeriod = LOGGING_PERIOD;
  LoggingPeriod *= 1000;                         // convert seconds to ms

  doseUnit = DOSE_uSV;          // default to uSv

  logPeriodStart = 0;     // start logging timer
}

unsigned long getFastAvgCount() {
  unsigned long tempSum = 0;
  for (int i = 0; i <= FAST_ARRAY_MAX-1; i++){ // sum up 1 second counts
    tempSum = tempSum + fastAverage[i];
  }
  return tempSum;
}

void logCount(unsigned long lcnt){ // unlike logging sketch, just outputs to serial
  unsigned long logCPM;                 // log CPM
  float uSvLogged = 0.0;                // logging CPM converted to "unofficial" uSv

  logCPM = float(lcnt) / (float(LoggingPeriod) / 60000);

  uSvLogged = (float)logCPM / doseRatio; // make uSV conversion

  // Print to serial in a format that might be used by Excel
  Serial.print(logCPM,DEC);
  Serial.write(',');    
  Serial.print(uSvLogged,4);
  Serial.write(','); // comma delimited
  Serial.print(readVcc()/1000. ,2);   // print as volts with 2 dec. places
  Serial.print(F("\r\n"));
}

void fastAvgCount(unsigned long dcnt) {
  static byte fastArrayIndex = 0;

  fastAverage[fastArrayIndex++] = dcnt;
  if(fastArrayIndex >= FAST_ARRAY_MAX) {
    fastArrayIndex = 0;
  }
}

unsigned long readVcc() { // SecretVoltmeter from TinkerIt
  unsigned long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result; // Back-calculate AVcc in mV
  return result;
}

// rolling your own map function saves a lot of memory
unsigned long lmap(unsigned long x, unsigned long in_min, unsigned long in_max, unsigned long out_min, unsigned long out_max){
  return x>in_max ? out_max : (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static void CPStoRGB(unsigned long counts) {
  unsigned char r, g, b;
  unsigned long scaleMax;
  unsigned int scaledCounts;
  float normalizedCounts;

  scaleMax = RAD_SCALE_MAX_CPS;   // Full magenta happens at this number of counts per second
#if (DEBUG)
  //Serial.print("rawcounts:");
  //Serial.print(counts);
#endif
  
  if (counts > scaleMax) counts=scaleMax;
  
  normalizedCounts=sqrt((float)counts/(float)scaleMax);

  if (normalizedCounts > 1.0) normalizedCounts=1.0;
  
  scaledCounts=(unsigned int)(normalizedCounts * 65534.0);
  getRGBFromSpectrum(scaledCounts, &r, &g, &b);

#if (DEBUG)
/*  Serial.print("counts:");
  Serial.print(counts);
  Serial.print(",scaledCounts:");
  Serial.print(scaledCounts);
  Serial.print(",r:");
  Serial.print(r);
  Serial.print(",g:");
  Serial.print(g);
  Serial.print(",b:");
  Serial.println(b);*/
#endif

  if (r!=lastR) {
    lastR=r;
    analogWrite(RED_LED_PIN, 255-r);
  }
  if (g!=lastG) {
    lastG=g;
    analogWrite(GREEN_LED_PIN, 255-g);
  }
  if (b!=lastB) {
    lastB=b;
    analogWrite(BLUE_LED_PIN,255-b);
  }
}
#if (BGYRM_SPECTRUM)
void getRGBFromSpectrum(unsigned int value, unsigned char *r, unsigned char *g, unsigned char *b) {
    /*Given a value from 0 to 65535, this function maps the number to a color on a spectrum starting
    with blue, changing to green, then progressing to yellow, then to red, and finally ending at magenta*/
    // Clamp value to 0-65535
    if (value > 65535) value = 65535;

    // Determine the region (each range is 65536 / 4 = 16384)
    unsigned int region = value / 16384;  // Total of 4 regions
    unsigned int position = value % 16384;  // Position within the region

    // Scale position to a 0-LED_MAX_INTENSITY range
    unsigned char intensity = ((unsigned long)position * (unsigned long)LED_MAX_INTENSITY) / (unsigned long)16384;

    // Calculate RGB based on region
    if (region == 0) {  // Blue to green (increase green, decrease blue)
        *r = 0;
        *g = intensity;
        *b = (LED_MAX_INTENSITY - intensity);
    } else if (region == 1) {  // Green to Yellow (increase red)
        *r = intensity;
        *g = LED_MAX_INTENSITY;
        *b = 0;
    } else if (region == 2) {  // Yellow to Red (decrease green)
        *r = LED_MAX_INTENSITY;
        *g = (LED_MAX_INTENSITY - intensity);
        *b = 0;
    } else if (region == 3) {  // Red to Magenta (increase blue)
        *r = LED_MAX_INTENSITY;
        *g = 0;
        *b = intensity;
    }
}
#else
void getRGBFromSpectrum(unsigned int value, unsigned char *r, unsigned char *g, unsigned char *b) {
    /*Given a value from 0 to 65535, this function maps the number to a color on a spectrum starting
    with green, then progressing to yellow, then to red, and finally ending at magenta*/
    // Clamp value to 0-65535
    if (value > 65535) value = 65535;

    // Determine the region (each range is 65536 / 3 = 21845)
    unsigned int region = value / 21845;  // Total of 3 regions
    unsigned int position = value % 21845;  // Position within the region

    // Scale position to a 0-LED_MAX_INTENSITY range
    unsigned char intensity = ((unsigned long)position * (unsigned long)LED_MAX_INTENSITY) / (unsigned long)21845;

    // Calculate RGB based on region
    if (region == 0) {  // Green to Yellow (increase red)
        *r = intensity;
        *g = LED_MAX_INTENSITY;
        *b = 0;
    } else if (region == 1) {  // Yellow to Red (decrease green)
        *r = LED_MAX_INTENSITY;
        *g = (LED_MAX_INTENSITY - intensity);
        *b = 0;
    } else if (region == 2) {  // Red to Magenta (increase blue)
        *r = LED_MAX_INTENSITY;
        *g = 0;
        *b = intensity;
    }
}
#endif


void initMorseSender() {
  static boolean initFlag = false;
  // save timer 1 values
  TCCR1A_default = TCCR1A;
  TCCR1B_default = TCCR1B;
  OCR1A_default = OCR1A;
  // set up Timer 1
  TCCR1A = _BV (COM1A0);           // toggle OC1A on Compare Match
  TCCR1B = _BV(WGM12) | _BV(CS10); // CTC, no prescaler
  OCR1A =  2; //(From frequency table in radioxmit.h ( compare A register value to 10 (zero relative))

  if (initFlag==false) {
    morseSender = new RFMorseSender(ANTENNA_PIN, 17.0);
    morseSender->setup();
    initFlag=true;
  }
  cwTransmitEnabled=true;
} 

void stopMorseSender() {
  TCCR1A = TCCR1A_default;
  TCCR1B = TCCR1B_default;
  OCR1A = OCR1A_default;
  cwTransmitEnabled = false;
}
//----------------------------------------------------------------------------------------------+
//                                      Utilities
//----------------------------------------------------------------------------------------------+

// variables created by the build process when compiling the sketch
extern int __bss_end;
extern void *__brkval;

int AvailRam(){ 
  int freeValue;
  if ((int)__brkval == 0)
    freeValue = ((int)&freeValue) - ((int)&__bss_end);
  else
    freeValue = ((int)&freeValue) - ((int)__brkval);
  return freeValue;
} 

byte getLength(unsigned long number){
  byte length = 0;
  unsigned long t = 1;
  do {
    length++;
    t*=10;
  } 
  while(t <= number);
  return length;
}

byte readButton(int buttonPin) { // reads LOW ACTIVE push buttom and debounces
  if (digitalRead(buttonPin)) return HIGH;    // still high, nothing happened, get out
  else {                                      // it's LOW - switch pushed
    delay(DEBOUNCE_MS);                       // wait for debounce period
    if (digitalRead(buttonPin)) return HIGH;  // no longer pressed
    else return LOW;                          // 'twas pressed
  }
}

static void serialprint_P(const char *text) {  // print a string from progmem to the serial object
  /* Usage: serialprint_P(pstring) or serialprint_P(pstring_table[5].  If the string 
   table is stored in progmem and the index is a variable, the syntax is
   serialprint_P((const char *)pgm_read_word(&(pstring_table[index])))*/
  while (pgm_read_byte(text) != 0x00)
    Serial.write(pgm_read_byte(text++));
}

static void cycleRGB() {
//This just runs the RGB LED through the spectrum defined in getRGBFromSpectrum()
  unsigned char r, g, b;
  static unsigned char lastR, lastG, lastB;

  // Test the function with some example values

  for (unsigned int i = 0; i < 65535; i++) {
    getRGBFromSpectrum(i, &r, &g, &b);
    if (r!=lastR) {
      lastR=r;
      analogWrite(RED_LED_PIN,255-r);
    }
    if (g!=lastG) {
      lastG=g;
      analogWrite(GREEN_LED_PIN, 255-g);
    }
    if (b!=lastB) {
      lastB=b;
      analogWrite(BLUE_LED_PIN,255-b);
    }
  }
  for (unsigned int i = 65535; i > 0; i--) {
    getRGBFromSpectrum(i, &r, &g, &b);
    if (r!=lastR) {
      lastR=r;
      analogWrite(RED_LED_PIN,255-r);
    }
    if (g!=lastG) {
      lastG=g;
      analogWrite(GREEN_LED_PIN, 255-g);
    }
    if (b!=lastB) {
      lastB=b;
      analogWrite(BLUE_LED_PIN,255-b);
    }
  }
}


//----------------------------------------------------------------------------------------------+
//                                        ISR
//----------------------------------------------------------------------------------------------+

void GetEvent(){   // ISR triggered for each new event (count)
  logCnt++;
  fastCnt++;
}
