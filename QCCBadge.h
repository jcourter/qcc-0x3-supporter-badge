//----------------------------------------------------------------------------------------------+
//               PIN MAP for  ATmega328P - Each I/O pin used is defined . . .
//----------------------------------------------------------------------------------------------+

// PIN MAP - Each I/O pin (used or unused) is defined . . .
//                        19             // (A5) RESERVED for I2C
//                        18             // (A4) RESERVED for I2C
#define SAO_GPIO2         15             // (A1) Simple Add-On header GPIO2 pin
#define SAO_GPIO1         14             // (A0) Simple Add-On header GPIO1 pin
#define GREEN_LED_PIN     10             // RGB LED Green Pin
#define ANTENNA_PIN        9             // PWM output for RF noise maker (it's cleaner than a spark gap)
#define TONE_PIN           9             // PWM output to speaker or piezo for tone mode
#define VOL_UP_BUTTON      8             // Volume up button (SW2)
#define VOL_DN_BUTTON      7             // Volume up button (SW3)
#define RED_LED_PIN        6             // RGB LED Red Pin
#define BLUE_LED_PIN       5             // RGB LED Blue Pin
#define RADIO_SEEK_BUTTON  4             // Button to move to the next FM station (SW4)
#define GM_TUBE_PIN        2             // Interrupt 0 for Geiger Mueller tube pulses
//                         D1 & D0          serial comm

//----------------------------------------------------------------------------------------------+
//                                 other defines . . .
//----------------------------------------------------------------------------------------------+
#define BGYRM_SPECTRUM    false           // set to true for blue->green->yellow->red->magenta, set to false for green->yellow->red->magenta
#define LED_MAX_INTENSITY   127         // maximum intensity for the LED

#define LED_RADIATION_MODE  0           //Show radiation level on the LED
#define LED_RSSI_MODE       1           //Show signal strength on the LED
#define LED_PULSE_MODE      2           //Make the LED slowly pulse
#define LED_RANDOM_MODE     3           //The LED will flash random colors

#define LOW_VCC            4200 //mV    // if Vcc < LOW_VCC give low voltage warning
#define ONE_SEC_MAX          20         // elements in the oneSecond accumulator array
#define DEBOUNCE_MS          50         // buttom debounce period in mS
#define AVGBGRAD_uSv         0.27       // global average background radiation level in uSv/h
#define AVBGRAD_uR           10.388     // global average background radiation level in uR/h
#define AVBGRAD_mR           0.010388   // global average background radiation level in mR/h
#define INFINITY             65534      // if scalerPeriod is set to this value, it will just do a cumulative count forever

#define QCC_MAX_FREQ       8800		// max frequency for radio stations
#define QCC_MIN_FREQ       7600		// min frequency for radio stations

// Dose units
#define DOSE_uSV        0   // microSieverts per hour
#define DOSE_uRH        1   // microRoentgen per hour
#define DOSE_mRH        2   // milliRads per hour

//----------------------------------------------------------------------------------------------+
//                           Menu configuration parameters 
//----------------------------------------------------------------------------------------------+

// These are DEFAULTS! - only used if menu has not been run
#define LOGGING_PERIOD    60            // defaults a 60 sec logging period
#define PRI_RATIO        175.43         // defaults to SBM-20 ratio - used to log dose rate to serial and to calculate the normal background counts for the tube
#define FULL_SCALE      1000            // max CPM for all 6 bars 
#define DOSE_UNIT       DOSE_uSV        // dose unit to use for logging

//----------------------------------------------------------------------------------------------+
//                                     Globals
//----------------------------------------------------------------------------------------------+

// These hold the local values that have been read from EEPROM
unsigned long LoggingPeriod;            // mS between writes to serial
float doseRatio;                        // holds the rate selected by jumper

byte doseUnit;                          // 0 - uSv/H, 1 - uR/H, 2 - mR/H

// variables for counting periods and counts . . .
volatile unsigned long fastCnt;
unsigned long logPeriodStart;           // for logging period
volatile unsigned long logCnt;          // to count and log CPM
unsigned long fastCountStart;           // counter for LED refresh period
unsigned long radioPeriodStart;

unsigned long oneSecond[ONE_SEC_MAX];   // array holding counts for 1 second running average

boolean cwTransmitEnabled = false;      // enables CW transmitter when set to true
byte ledMode = 0;                       // current mode for the LED - see defines above
static byte lastR, lastG, lastB = {0};  // stores the current RGB values for the LED

byte TCCR1A_default;                    // stores the value of the TCCR1A register before we mess with it
byte TCCR1B_default;                    // stores the value of the TCCR1B register before we mess with it
byte OCR1A_default;                     // stores the value of the OCR1A register before we mess with it

RDA5807 rx;                             // FM radio module

MorseSender *morseSender;               // Morse code transmitter

#if __has_include("ctf.h")
  #include "ctf.h"
#else
  #define CTF_SECRET_MESSAGE_PREAMBLE   F("      C C C ")
  #define CTF_SECRET_MESSAGE	      F("greetings qcc0x3 attendee B 73 de abend S")
#endif

// unit strings used for logging - use u instead of mu since nobody interprets chars above 127 consistently
#define MAX_UNIT  2
const char unit_0[] PROGMEM = "uSv/h";
const char unit_1[] PROGMEM = "uR/h";
const char unit_2[] PROGMEM = "mR/h";

const char * const unit_table[] PROGMEM = // PROGMEM array to hold unit strings
{
  unit_0,
  unit_1,
  unit_2
};

