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
#define BGYRM_SPECTRUM      false       // set to true for blue->green->yellow->red->magenta, set to false for green->yellow->red->magenta
#define LED_MAX_INTENSITY   255         // maximum intensity for the LED

#define LED_RADIATION_MODE  0           //Show radiation level on the LED
#define LED_RSSI_MODE       1           //Show signal strength on the LED
#define LED_PULSE_MODE      2           //Make the LED slowly pulse
#define LED_RANDOM_MODE     3           //The LED will flash random colors

#define FAST_ARRAY_MAX       50         // elements in the fast accumulator array
#define FAST_AVG_PERIOD      10000      // milliseconds of history to store in the fast accumulator for an average
#define SLOW_ARRAY_MAX       12         // elements in the slow accumulator array
#define SLOW_AVG_PERIOD      60000      // milliseconds of history to store in the slow accumulator for an average
#define DEBOUNCE_MS          50         // buttom debounce period in mS
#define AVGBGRAD_uSv         0.27       // global average background radiation level in uSv/h
#define AVBGRAD_uR           10.388     // global average background radiation level in uR/h
#define AVBGRAD_mR           0.010388   // global average background radiation level in mR/h
#define INFINITY             65534      // if scalerPeriod is set to this value, it will just do a cumulative count forever

#define QCC_MAX_FREQ       7900		// max frequency for radio stations
#define QCC_MIN_FREQ       7600		// min frequency for radio stations
#define RADIO_MODE_QCC     0      // QCC mode - locks tuner to 76-79MHz
#define RADIO_MODE_BCAST   1      // Broadcast mode - sets tuner for normal North America FM broadcast band
#define RADIO_MODE_ADDR    0x01        //EEPROM address for radio mode setting

// Dose units
#define DOSE_uSV        0   // microSieverts per hour
#define DOSE_uRH        1   // microRoentgen per hour
#define DOSE_mRH        2   // milliRads per hour

#define USE_OLED        true  // set to true to use an SSD1306 OLED display on the I2C bus

// 0X3C+SA0 - 0x3C or 0x3D
#define I2C_ADDRESS 0x3C  // I2C address for the SSD1306 display

//----------------------------------------------------------------------------------------------+
//                           Menu configuration parameters 
//----------------------------------------------------------------------------------------------+

#define LOGGING_PERIOD    60            // defaults a 60 sec logging period
#define PRI_RATIO        153.80         // J-321 tube conversion ratio - 153.8 CPM/uSv/h
#define DEAD_TIME_uS     800            // Dead time for the J-321 tube in this circuit, in microseconds
#define RAD_SCALE_MAX_CPS (unsigned long)((PRI_RATIO*5.71)/60.0)            // we'll have full magenta at the equivalent of 5.71uSv/hr, based on OSHA annual exposure limits for radiation workers
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
volatile unsigned long slowCnt;
unsigned long logPeriodStart;           // for logging period
volatile unsigned long logCnt;          // to count and log CPM
unsigned long fastCountStart;           // counter for updating fast moving average
unsigned long slowCountStart;           // counter for updating slow moving average
unsigned long radioPeriodStart;         // interval for checking signal strength

unsigned int fastAverage[FAST_ARRAY_MAX];   // array holding counts for fast running average
unsigned int slowAverage[SLOW_ARRAY_MAX];   // array holding counts for fast running average

boolean cwTransmitEnabled = false;      // enables CW transmitter when set to true
byte ledMode = 0;                       // current mode for the LED - see defines above
byte radioMode = 0;                       // current mode for the radio - see defines above
byte lastR = 0 , lastG = 0, lastB = 0;  // stores the current RGB values for the LED

byte TCCR1A_default;                    // stores the value of the TCCR1A register before we mess with it
byte TCCR1B_default;                    // stores the value of the TCCR1B register before we mess with it
byte OCR1A_default;                     // stores the value of the OCR1A register before we mess with it

RDA5807 rx;                             // FM radio module

#if (USE_OLED)
SSD1306AsciiWire oled;                  //SSD1306 OLED display
#endif

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

