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
#include <avr/wdt.h>
#include <Wire.h>
#include <limits.h>
#include "radioxmit.h"
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include "QCCBadge.h"

//----------------------------------------------------------------------------------------------+
//                                     DEBUG Defines
//----------------------------------------------------------------------------------------------+
#define DEBUG          false            // if true, sends debug info to the serial port

//----------------------------------------------------------------------------------------------+
//                                      Functions
//----------------------------------------------------------------------------------------------+

void setup(){
  float globalBgRadAvg;
  
  wdt_disable();

  Wire.begin();
  Wire.setClock(100000L);  // Setting this to 400kHz will cause audible noise on the FM radio module

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

  if(readButton(RADIO_SEEK_BUTTON)== LOW && readButton(VOL_UP_BUTTON)== LOW && readButton(VOL_DN_BUTTON) == LOW) {
    if (radioMode==RADIO_MODE_QCC) radioMode=RADIO_MODE_BCAST;
    else radioMode=RADIO_MODE_QCC;
    EEPROM.update(RADIO_MODE_ADDR, radioMode);
  } else if(readButton(RADIO_SEEK_BUTTON) == LOW) {
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
  if (radioMode==RADIO_MODE_QCC) {
    rx.setBand(1);  // set the band to the Japanese broadcast band (76-91MHz), which overlaps nicely with the currently unused VHF TV channels 5 and 6 - code in the loop further limits this to between QCC_MIN_FREQ and QCC_MAX_FREQ
  } else {
    rx.setBand(0); // NA/EU FM broadcast band
  }
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

  fastCountStart = slowCountStart = radioPeriodStart = logPeriodStart = millis();;     // start timers
  fastCnt = slowCnt = logCnt = 0;     //initialize counts

#if (USE_OLED)
  oledInit();
#endif

  wdt_enable(WDTO_2S);
}

void loop(){
  static unsigned long lastButtonTime;  // counter for pressing the button too quickly
  static unsigned long lastFastCnt = 0;
  static unsigned long lastSlowCnt = 0;
  static boolean blnLogStarted = false;
  static unsigned int lastFrequency = 0;
  static byte lastRssi = 0;
  static byte lastVolume = 0;
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
      rx.setFrequency(rx.getRealFrequency()+20);  //doing it this way instead of setFrequencyUp() because setFrequencyUp sometimes doesn't work right and because the tuning step doesn't work right either
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

  if (millis() >= fastCountStart + FAST_AVG_PERIOD/FAST_ARRAY_MAX){ // update the fast moving average
    fastAvgCount(fastCnt);
    fastCnt=0;                          // reset counts
    fastCountStart = millis();          // reset the period time

    unsigned long fastAverage=getFastAvgCount()*((unsigned long)60000/(unsigned long)FAST_AVG_PERIOD);  // convert to counts per minute
    fastAverage = (float)fastAverage/(1.0-(float)fastAverage*((float)DEAD_TIME_uS/60000000.0));  // compensate for dead time (first converting the dead time to minutes)
    if (!cwTransmitEnabled) {
      if (ledMode==LED_RADIATION_MODE) {
        CPStoRGB(fastAverage/60);  //Update the LED color based on the fast average count in counts per second
      }
      oledFastCount(fastAverage);
    }
  }

  if (millis() >= slowCountStart + SLOW_AVG_PERIOD/SLOW_ARRAY_MAX){ // update the slow moving average
    slowAvgCount(slowCnt);
    slowCnt=0;                          // reset counts
    slowCountStart = millis();          // reset the period time
    if (millis() >= SLOW_AVG_PERIOD) { // wait until we have the buckets filled to give an accurate average before displaying
      unsigned long slowAverage=getSlowAvgCount()*((unsigned long)60000/(unsigned long)SLOW_AVG_PERIOD);  // convert to counts per minute
      slowAverage = (float)slowAverage/(1.0-(float)slowAverage*((float)DEAD_TIME_uS/60000000.0));  // compensate for dead time (first converting the dead time to minutes)
      if (!cwTransmitEnabled) {
        oledSlowCount(slowAverage);
      }
    }
  }
  if (millis() >= radioPeriodStart + 333) { //query the radio module every 100ms
    radioPeriodStart=millis();             // reset the period time
    if(!cwTransmitEnabled) {
      unsigned int freq = rx.getRealFrequency();
      byte rssi = (byte)rx.getRssi();
      byte volume = (byte)rx.getVolume();
      if (freq!=lastFrequency || rssi!=lastRssi || volume!=lastVolume) {
        lastFrequency = freq;
        lastRssi = rssi;
        lastVolume = volume;
        oledUpdateFMInfo(freq, volume, rssi);
  #if (DEBUG)
        Serial.print("Tuned to ");
        Serial.print(freq);
        Serial.print(", RSSI: ");
        Serial.println(rssi);
  #endif
      }
      if(radioMode==RADIO_MODE_QCC && freq>=QCC_MAX_FREQ) {
#if (DEBUG)
        Serial.println(F("Freq out of bounds"));
#endif
        rx.setFrequency(QCC_MIN_FREQ);
      } else if (radioMode==RADIO_MODE_BCAST && (freq < 8710 || freq > 10790)) {
#if (DEBUG)
        Serial.println(F("Freq out of bounds"));
#endif
        rx.setFrequency(8710);
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
  wdt_reset();
}

void oledInit(){
/*
  oledInit() — Initialize OLED display hardware and clear screen

  Prepares the OLED module for use by:
    - Beginning I2C communication (via the display library)
    - Setting display parameters such as font and orientation (if supported)
    - Clearing the screen to ensure a fresh start

  This function should be called once during `setup()` to configure the display 
  before writing any content. Ensures that all subsequent OLED output routines 
  (e.g., oledFastCount(), oledSlowCount(), oledUpdateFMInfo()) function correctly.

  Note:
    - Relies on the `oled` object being correctly defined and linked to the screen
    - May vary depending on the display driver (e.g., SSD1306 or SH1106)

  Tip:
    If using low-power modes, re-init might be required after wake-up or reset.
*/
#if (USE_OLED)
  oled.begin(&Adafruit128x64, I2C_ADDRESS);
  oled.setFont(System5x7);
  oled.clear();
  oled.setCursor(1,5);
  oled.print(F("Queen City Con 0x3"));
#endif
}

void oledFastCount(unsigned long fastAverage) {
/*
  Update the OLED display with the fast-moving dose rate.

  Displays the calculated fast average radiation level (CPM converted to dose) 
  along with its unit (e.g., µSv/h) on a specific OLED display row.

  Parameters:
    - fastAverage: Dose-equivalent CPM value, scaled and dead-time corrected
*/  
  static unsigned long lastFastAverage = ULONG_MAX;
  if (fastAverage==lastFastAverage) return;
  else lastFastAverage=fastAverage;
#if (USE_OLED)
  oled.setCursor(1,1);
  for (char i = 0;i < 20*fastAverage/200;i++) {
    oled.print('*');
  }
  oled.clearToEOL();

  oled.setCursor(1,2);
  oled.print(fastAverage);
  oled.print(" counts/min");
  oled.clearToEOL();

  oled.setCursor(1,3);
  oled.print((float) fastAverage / doseRatio, 2);
  oled.print(' ');
  oledprint_P((const char *)pgm_read_word(&(unit_table[doseUnit])));  // print dose unit (uSv/h, uR/h, or mR/h) to serial
  oled.clearToEOL();
#endif
}

void oledSlowCount(unsigned long slowAverage) {
/*
  Display the current slow average (long-term dose rate) on the OLED.

  This function checks whether the displayed value has changed to avoid unnecessary
  redraws. It then prints the current average along with appropriate units and
  the averaging time period (e.g., "60s avg: 0.11 uSv/h").

  Parameters:
    - slowAverage: Dose rate scaled to counts per minute, post-dead-time compensation
*/
  static unsigned long lastSlowAverage = ULONG_MAX;
  if (slowAverage==lastSlowAverage) return;
  else lastSlowAverage=slowAverage;
#if (USE_OLED)
  oled.setCursor(1,5);
  if(SLOW_AVG_PERIOD % 3600000 == 0) {  // display the averaging period in hours
    oled.print(SLOW_AVG_PERIOD/3600000,DEC);
    oled.print('h');
  }else if(SLOW_AVG_PERIOD % 60000 == 0) {
    oled.print(SLOW_AVG_PERIOD/60000,DEC); // display the averaging period in minutes
    oled.print('m');
  } else { // display the averaging period in seconds
    oled.print(SLOW_AVG_PERIOD/1000,DEC);
    oled.print('s');
  }
  oled.print(F(" avg: "));
  oled.print((float) slowAverage / doseRatio, 2);
  oled.write(' ');
  oledprint_P((const char *)pgm_read_word(&(unit_table[doseUnit])));  // print dose unit (uSv/h, uR/h, or mR/h) to serial
  oled.clearToEOL();
#endif
}

void oledUpdateFMInfo (unsigned int freq, byte volume, byte rssi) {
/*
  Display FM radio status on the OLED.

  Updates the OLED with current radio frequency, volume level, and signal strength (RSSI).
  This provides real-time feedback when tuning or adjusting the FM module.

  Parameters:
    - freq: Frequency in tens of kHz (e.g., 8810 = 88.1 MHz)
    - volume: Current audio volume (0–15)
    - rssi: Received signal strength indicator (range varies by specific FM module, 30–110 typical)
*/
#if (USE_OLED)
  oled.setCursor(1,7);
  oled.print((int)freq/100);
  oled.print('.');
  oled.print((freq%100)/10);
  oled.print(F("MHz  S"));
  oled.print(rssi);
  if(rssi < 10) oled.print(' ');
  if(rssi < 100) oled.print(' ');
  oled.print(F(" Vol "));
  if(volume < 10) oled.print(' ');
  oled.print(volume);
  oled.clearToEOL();
#endif
}

void Get_Settings(){
/*
  Retrieve configuration settings from non-volatile memory and/or set variables to static #defined values (this is a holdover from the original kit and several of these were simplified from runtime settings to static values).

  This function pulls user-selected options (e.g., LED mode, radio mode) 
  that were previously saved. Settings may be loaded from EEPROM or other storage.

  Initializes:
    - doseRatio (conversion rate from CPM to the rad unit of choice, set up in QCCBadge.h)
    - LoggingPeriod
    - radioMode
    - doseUnit (e.g., µSv/h, µR/h)
*/
  doseRatio = PRI_RATIO;

  LoggingPeriod = LOGGING_PERIOD;
  LoggingPeriod *= 1000;                         // convert seconds to ms

  doseUnit = DOSE_uSV;          // default to uSv

  radioMode=EEPROM.read(RADIO_MODE_ADDR);
  if (radioMode != RADIO_MODE_QCC && radioMode != RADIO_MODE_BCAST) {
    radioMode = RADIO_MODE_QCC;  // Default to QCC mode
    EEPROM.update(RADIO_MODE_ADDR, radioMode);
  }
}

unsigned long getFastAvgCount() {
/*
  Calculate the total count over the fast average window.

  This sums all values in `fastAverage[]` to produce the aggregate count over the
  defined fast averaging period (e.g., 10 seconds). The result can be scaled to CPM.

  Returns:
    - Total count over `FAST_AVG_PERIOD`
*/
  unsigned long tempSum = 0;
  for (int i = 0; i <= FAST_ARRAY_MAX-1; i++){ // sum up fast average counts
    tempSum = tempSum + (unsigned long)fastAverage[i];
  }
  return tempSum;
}

unsigned long getSlowAvgCount() {
/*
  Calculate the total count over the slow average window.

  This function returns the sum of all buckets in `slowAverage[]`, representing the
  total counts across the `SLOW_AVG_PERIOD` window (e.g., 60 seconds).

  Returns:
    - Total count across all slow averaging buckets
*/
  unsigned long tempSum = 0;
  for (int i = 0; i <= SLOW_ARRAY_MAX-1; i++){ // sum up slow average counts
    tempSum = tempSum + (unsigned long)slowAverage[i];
  }
  return tempSum;
}

void logCount(unsigned long lcnt){
/*
  Output radiation logging data to serial.

  Prints a comma-separated data row including:
    - CPM (counts per minute)
    - dose rate in selected units (e.g., µSv/h)
    - battery voltage

  Used by external tools to log radiation exposure over time or build dose graphs.

  Parameters:
    - lcnt: Total number of counts during the logging interval
*/
  unsigned long logCPM;                 // log CPM
  unsigned long compensatedCPM;         // CPM after compensating for dead time
  float uSvLogged = 0.0;                // logging CPM converted to "unofficial" uSv

  logCPM = float(lcnt) / (float(LoggingPeriod) / 60000);

  compensatedCPM = (float)logCPM/(1.0-(float)logCPM*((float)DEAD_TIME_uS/60000000.0));  // compensate for dead time (first converting the dead time to minutes)

  uSvLogged = (float)compensatedCPM / doseRatio; // make uSV conversion

  // Print to serial in a format that might be used by Excel
  //Serial.print(logCPM,DEC);  //write the pre-compensated CPM value
  //Serial.write(',');    
  Serial.print(compensatedCPM,DEC);
  Serial.write(',');    
  Serial.print(uSvLogged,4);
  Serial.write(','); // comma delimited
  Serial.print(readVcc()/1000. ,2);   // print as volts with 2 dec. places
  Serial.print(F("\r\n"));
}

void fastAvgCount(unsigned long dcnt) {
/*
  Update the fast moving average bucket array.

  This function stores the latest `dcnt` (count) into the `fastAverage[]` ring buffer.
  It updates a static index that wraps back to zero when it reaches the end of the array.

  Purpose:
    - Tracks short-term activity (e.g., 10-second rolling window) for live responsiveness.
    - Allows conversion to fast CPM estimate, updated frequently.

  Parameters:
    - dcnt: The number of counts accumulated during the fast averaging window (e.g., 200 ms)

  Side Effects:
    - Overwrites oldest entry in `fastAverage[]`
*/
  static byte fastArrayIndex = 0;

  fastAverage[fastArrayIndex++] = dcnt;
  if(fastArrayIndex >= FAST_ARRAY_MAX) {
    fastArrayIndex = 0;
  }
}

void slowAvgCount(unsigned long dcnt) {
/*
  Update the slow moving average bucket array.

  This function stores the latest count into `slowAverage[]`, a circular buffer that
  represents the slower, longer-term average (e.g., 1 minute over 12 buckets).

  Purpose:
    - Enables frequent refresh of long-term rate display (every ~5 seconds)
    - Keeps memory footprint small while maintaining accurate trends

  Parameters:
    - dcnt: The number of counts detected during the current slow window slice

  Side Effects:
    - Rotates through `slowAverage[]` and replaces old values as time progresses
*/
  static byte slowArrayIndex = 0;

  slowAverage[slowArrayIndex++] = dcnt;
  if(slowArrayIndex >= SLOW_ARRAY_MAX) {
    slowArrayIndex = 0;
  }
}

unsigned long readVcc() { // SecretVoltmeter from TinkerIt
/*
  Measure the system supply voltage (Vcc) using the internal reference.

  Uses the ATmega’s internal 1.1 V bandgap to calculate actual supply voltage (in mV),
  useful for battery monitoring without external hardware.

  Returns:
    - Vcc in millivolts (e.g., 5000 = 5.00 V)

  Technique:
    - Based on "Secret Voltmeter" from TinkerIt
    - Uses ADC to measure internal reference and back-calculate Vcc
*/
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

static void CPStoRGB(unsigned long counts) {
/*
  CPStoRGB() — Map radiation event rate to RGB LED color

  This function translates the current count rate (cps: counts per second)
  into a corresponding color value for the onboard RGB LED.

  Purpose:
    - Provide a visual cue of radiation intensity: low rates glow blue/green, 
      while higher rates transition to red/purple.
    - Useful for at-a-glance awareness without requiring numerical display.

  How it works:
    - cps (counts per second) is normalized against RAD_SCALE_MAX_CPS.
    - Based on the normalized value, red, green, and blue intensities are 
      computed using a stepped or gradient color scheme.
    - LED outputs are updated using PWM-capable pins.

  Example Color Mapping (depends on implementation):
    - Low cps (background): cool colors (blue/green)
    - Medium cps: yellow/orange
    - High cps (alert levels): red/magenta

  Inputs:
    - cps: Radiation activity in counts per second (float or int)

  Side Effects:
    - Updates global or hardware PWM outputs for the RGB LED
*/
  unsigned char r, g, b;
  unsigned long scaleMax;
  unsigned int scaledCounts;
  float normalizedCounts;

  scaleMax = RAD_SCALE_MAX_CPS;   // Full magenta happens at this number of counts per second
  
  if (counts > scaleMax) counts=scaleMax;
  
  normalizedCounts=sqrt((float)counts/(float)scaleMax);

  if (normalizedCounts > 1.0) normalizedCounts=1.0;
  
  scaledCounts=(unsigned int)(normalizedCounts * 65534.0);
  getRGBFromSpectrum(scaledCounts, &r, &g, &b);

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

void getRGBFromSpectrum(unsigned int value, unsigned char *r, unsigned char *g, unsigned char *b) {
/*
  Maps a 16-bit value (0–65535) to an RGB color along a custom gradient spectrum.

  If BGYRM_SPECTRUM is true, the gradient proceeds:
    Blue → Green → Yellow → Red → Magenta
    (4 regions)

  Otherwise, it proceeds:
    Green → Yellow → Red → Magenta
    (3 regions)
*/

    if (value > 65535) value = 65535;

    // Define regions dynamically based on selected spectrum mode
#if (BGYRM_SPECTRUM)
    const unsigned int totalRegions = 4;
#else
    const unsigned int totalRegions = 3;
#endif

    unsigned int regionSize = 65536 / totalRegions;
    unsigned int region = value / regionSize;
    unsigned int position = value % regionSize;

    unsigned char intensity = ((unsigned long)position * (unsigned long)LED_MAX_INTENSITY) / regionSize;

    // Apply color blending based on region
#if (!BGYRM_SPECTRUM)  // skip region 0 if BGYRM_SPECTRUM is false
 region++;
#endif
    switch (region) {
        case 0:  // Blue to Green
            *r = 0;
            *g = intensity;
            *b = LED_MAX_INTENSITY - intensity;
            break;
        case 1:  // Green to Yellow
            *r = intensity;
            *g = LED_MAX_INTENSITY;
            *b = 0;
            break;
        case 2:  // Yellow to Red
            *r = LED_MAX_INTENSITY;
            *g = LED_MAX_INTENSITY - intensity;
            *b = 0;
            break;
        case 3:  // Red to Magenta
        default:
            *r = LED_MAX_INTENSITY;
            *g = 0;
            *b = intensity;
            break;
    }
}

void initMorseSender() {
/*
  initMorseSender() — Initialize Morse code transmission system

  Core Responsibilities:
    - Instantiate the appropriate MorseSender subclass (e.g., RFMorseSender)
    - Assign the output pin to be modulated
    - Set the transmission speed in words per minute (WPM)
    - Prepare the output (e.g., configure pin modes, disable output initially via setReady)

*/
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
/*
  stopMorseSender() — Halt Morse transmission and clean up state

  This function stops any ongoing Morse message in progress and ensures that
  the output is turned off.
*/
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

byte readButton(int buttonPin) { // reads LOW ACTIVE push buttom and debounces
/*
  readButton() — Check and debounce button input

  Reads the current state of the user button and applies a software debounce 
  mechanism to avoid detecting multiple presses from mechanical bounce.

  Common behavior:
    - Returns true only once per actual press
    - Ignores spurious toggles within DEBOUNCE_MS
    - Typically called from loop() to detect mode changes or user input

  Returns:
    - true if a new button press has been detected
    - false otherwise
*/
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

#if (USE_OLED)
static void oledprint_P(const char *text) {  // print a string from progmem to the serial object
  /* Usage: oledprint_P(pstring) or oledprint_P(pstring_table[5].  If the string 
   table is stored in progmem and the index is a variable, the syntax is
   serialprint_P((const char *)pgm_read_word(&(pstring_table[index])))*/
  while (pgm_read_byte(text) != 0x00)
    oled.write(pgm_read_byte(text++));
}
#endif

//----------------------------------------------------------------------------------------------+
//                                        ISR
//----------------------------------------------------------------------------------------------+

void GetEvent(){   // ISR triggered for each new event (count)
/*
  Interrupt Service Routine (ISR) for incoming Geiger events.

  This ISR is triggered on each detection pulse from the Geiger tube. It increments
  three counters:
    - logCnt: Used for periodic CSV logging
    - fastCnt: Used for fast averaging
    - slowCnt: Used for slow averaging

  Note:
    - Keep this ISR minimal to avoid timing disruptions.
    - Counters are updated atomically as `volatile` globals.
*/
  logCnt++;
  fastCnt++;
  slowCnt++;
}
