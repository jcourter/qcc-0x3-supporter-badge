# qcc-0x3-supporter-badge

<b>Badge Summary</b>

The QCC 0x3 supporter badge contains a Geiger counter circuit, an RDA5807M FM radio receiver, an RGB LED, and an ATmega328P microcontroller to manage it all.  The microcontroller runs at 16MHz, has been flashed with an Arduino bootloader, and a USB to TTL serial dongle has been included, so you can use the Arduino IDE to modify the code and upload it to the badge.  The badge is compatible with an Arduino Uno, so choose that in the IDE as your board type.

<i><b>NOTE FOR CTF PARTICIPANTS:</b> The badge contains a flag which is not present in the public release of the source code.  If you are planning to work on the CTF badge challenge, I highly recommend you DO NOT re-flash the badge until you've completed it.</i>

<b>Badge Operation</b>

When the badge powers on, the Geiger Mueller tube will detect beta and gamma radiation.  Each detection event will send a pulse to pin PD2 on the microcontroller, as well as to the cyan LED and the piezo sounder.  As supplied, the badge keeps track of a rolling 10 second count, which is used to give a visual indication of radiation intensity by changing the color of the RGB LED.  As the radiation level goes from normal background levels to higher intensity, the LED will change color from green, to yellow, then to red, and finally to magenta.

To listen to low power signals at the conference, plug in the included earbuds.  Pressing SW4 will cause the RDA5807M to step to the next frequency.  The frequency band is 3MHz wide and each step is 200kHz, so 15 presses of SW4 will cycle you back to where you started.  Pressing SW2 and SW3 will adjust the volume up and down, respectively.  To toggle the radio between the conference frequency range and the NA FM broadcast range, press and hold SW2 + SW3 + SW4 simultaneously while turning the badge on.

To initiate the hidden signal for the CTF challenge, press SW2 and SW3 simultaneously.  The LED should glow a dim red color when the signal is being emitted.  To return to normal operation, press SW4 or try turning the badge off and then on again.

<b>Alternate LED modes</b>

If the ambient radiation level isn't generating enough visual stimulus for you, there are a couple of preprogrammed alternatives for the LED.

If you press and hold SW2 while powering on the badge, the LED will flash random colors about every 100ms.

If you press and hold SW3 while powering on the badge, the LED cycle through the color spectrum continuously.

If you press and hold SW4 while powering on the badge, the LED will show the signal strength of the currently tuned radio signal instead of radiation levels.
