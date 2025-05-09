#include "morse.h"

/* This code is based on an idea posted by terryking228 at https://forum.arduino.cc/t/turn-your-arduino-into-an-am-radio-transmitter/86696/16 */
 
/*
 The OCR1A variable is related to the frequency.
The OCR1A variable is one less than the actual divisor.
OCR1A - Frequency
15 - 500 khz
14 - ~530 khz
13 - ~570 khz   (WMCA New York )
12 - ~610 khz
11 - ~670 khz
10 - ~730 khz
9 - 800 khz
8 - ~890 khz
7 - 1000 khz   (Good for test)
6 - ~1140 khz
5 - ~1330 khz
4 - 1600 khz
3 - 2.0MHz
2 - 2.67MHz
1 - 4MHz
 */

class RFMorseSender: public MorseSender {
  protected:
    virtual void setOn();
    virtual void setOff();
    virtual void setReady();
    virtual void setComplete();
  public:
    // concert A = 440
    // middle C = 261.626; higher octaves = 523.251, 1046.502
    RFMorseSender(
      int outputPin,
      float wpm=WPM_DEFAULT);
};

void RFMorseSender::setOn() { pinMode(pin, OUTPUT); }
void RFMorseSender::setOff() { pinMode(pin, INPUT); }

void RFMorseSender::setReady() { setOff(); }
void RFMorseSender::setComplete() { pinMode(pin, INPUT); }
RFMorseSender::RFMorseSender( int outputPin, float wpm) : MorseSender(outputPin, wpm) {};

