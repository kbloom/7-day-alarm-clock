This is a project to create an alarm clock with separate alarm times for each day of the week. It is based on an Arduino and the Sparkfun Qwicc system.

It uses the following components:

 * [SparkFun RedBoard Qwiic DEV-15123](https://www.sparkfun.com/products/15123)
 * [SparkFun Real Time Clock Module - RV-1805 (Qwiic) BOB-14558](https://www.sparkfun.com/products/14558)
 * [SparkFun Qwiic MP3 Trigger DEV-15165](https://www.sparkfun.com/products/15165)
 * [SparkFun 16x2 SerLCD - RGB Text (Qwiic) LCD-16397](https://www.sparkfun.com/products/16397)
 * [SparkFun Qwiic Keypad - 12 Button COM-15290](https://www.sparkfun.com/products/15290)
 * 2x SparkFun Qwiic Arcade buttons. I used the following, but you can choose
   other colors. You'll need to change the I2C address of the snooze button to
   110 (0x6E) using
   [Example5_ChangeI2CAddress](https://github.com/sparkfun/SparkFun_Qwiic_Button_Arduino_Library/tree/master/examples/Example5_ChangeI2CAddress).
   * [Red SPX-15591](https://www.sparkfun.com/products/15591) for the stop button.
   * [Blue SPX-15592](https://www.sparkfun.com/products/15592) for the snooze button.
 * A speaker. I used [this one](https://www.amazon.com/gp/product/B0738NLFTG).
 * Qwiic cables.
 * a [wall wart TOL-15314](https://www.sparkfun.com/products/15314)
 * a micro-SD card
 * some sort of enclosure for the final product. I used [this box](https://www.amazon.com/gp/product/B018QLQFR6). I cut out the square holes with a coping saw, and made the button holes and speaker hole using spade bits.

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
 * The alarm also supports a shabbat mode alarm, which sounds for one minute, cannot be skipped if you wake up early, and all buttons are disabled while it is sounding.

Menu navigation:

The menu navigtaion is as follows:

 * Press any key on the keypad to activate the alarm.
 * Press '2' to move up the menu, and '8' or '0' to move down the menu.
 * Press '5' to select a menu item whose action is to select (primarily setting times.)
 * Press '4' or '6' to cycle through options
   * All alarms enabled/disabled
   * Individual alarm on/off/shabbat
   * Volume control
 * Press '\*' or '#' to exit any menu or input prompt.

# serlcd_charset
`serlcd_charset`, is a demo to get me acquainted with the SerLCD display by showing me which characters it can display.
