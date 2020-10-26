This is a project to create an alarm clock with separate alarm times for each day of the week. It is based on an Arduino and the Sparkfun Qwicc system.

It uses the following components:

 * [SparkFun RedBoard Qwiic DEV-15123](https://www.sparkfun.com/products/15123)
 * [SparkFun Real Time Clock Module - RV-1805 (Qwiic) BOB-14558](https://www.sparkfun.com/products/14558)
 * [SparkFun Qwiic MP3 Trigger DEV-15165](https://www.sparkfun.com/products/15165)
 * [SparkFun 16x2 SerLCD - RGB Text (Qwiic) LCD-16397](https://www.sparkfun.com/products/16397)
 * [SparkFun Qwiic Keypad - 12 Button COM-15290](https://www.sparkfun.com/products/15290)
 * 2x SparkFun Qwiic Arcade buttons. I used the following, but you can choose other colors:
   * [Red SPX-15591](https://www.sparkfun.com/products/15591) for the stop button.
   * [Blue SPX-15592](https://www.sparkfun.com/products/15592) for the snooze button.
 * A speaker
 * Qwiic cables
 * a [wall wart TOL-15314](https://www.sparkfun.com/products/15314)
 * a micro-SD card
 * some sort fo enclosure for the final product

The idea behind this project is that we can set the time and alarm using the keypad and a menu system.

The alarm has 3 states:

 * Waiting
   * Hitting the snooze button will immediately set an alarm 8 minutes from now in Snooze mode. (E.g. if you hit the stop button, to stop the alarm, and then changed your mind. Or if you want to take a short impromptu nap, you can hit the snooze button several times.)
   * Hitting the stop button will toggle whether to skip the next alarm. (e.g. if you woke up significantly before your alarm went off, and decided not to go back to sleep.)
 * Snooze
   * Hitting the snooze button will extend the snooze by another 8 minutes.
   * Hitting the stop button will cancel the snooze.
 * Sounding
   * Hitting the snooze button will stop the alarm and start snoozing for 8 minutes.
   * Hitting hte stop button will stop the alarm  and transition to the waiting state for the next day's alarm.

There are several sketches here currently. The first two will merge at some point to create the final product.

 * `menu_system`, where I develop the set menu system without worrying about alarm clock logic
 * `alarm_clock`, where I develop the alarm clock state machine. 
 * `serlcd_charset`, a demo to get me acquainted with the SerLCD display by showing me which characters it can display.
