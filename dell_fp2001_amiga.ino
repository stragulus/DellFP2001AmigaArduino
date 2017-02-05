/*
(C) 2015 Bram Avontuur

This project is for an Arduino that will operate a Dell FP 2001 monitor's physical buttons.
If pressing 2 buttons simultaneously, a sequence of button presses is initiated to use the 
monitor's OSD menu to configure it specifically for my hooked up Amiga 500.

The arduino is installed inside the monitor, and is installed as a proxy in between the 
monitor's button PCB and the monitor's custom controller. This way, the original button's
functions can still be used (the arduino passes regular buttonpresses through to the monitor's
controller). Only by pressing 2 buttons simultaneously for about a second will trigger the
special button sequence.

All timing values were manually determined with a lot of trial and error and probably will not
work as-is on another Dell FP2001. In fact, I had to switch from one FP2001 to a newer one halfway
through the project, and the timings had to be adjusted.

This only works on an older revision Dell FP2001 (pre-2005?) that allows an Amiga 500 to be hooked up
directly without a scan doubler, as its vga port accepts a 15kHz input signal. Newer versions don't.
*/

// Symbols for the 4 OSD menu buttons
#define BTN_INPUT 1 
#define BTN_MENU 2
#define BTN_PLUS 3
#define BTN_MIN 4

// Inputs connected to the monitor's buttons. Each input maps to two buttons.
#define PIN_IN_INPUTMENU A0
#define PIN_IN_MINPLUS A1

// PWM-modulated output pins connected to the LCD Monitor's microcontroller.
// The input & menu buttons
#define PIN_OUT_INPUTMENU 9
// The minus and plus buttons
#define PIN_OUT_MINPLUS 10

// Values to write to PWM-modulated output pins
// V measured on original schematics: 
// No buttons: 2V, 1 button: 1.66V (83%), other button: 1.12v (56%), both: 0V
// Output values are manually determined and verified with volt reader to match original
#define BTNVALOUT_NONE 100
// 82 / 51 worked for the original Dell 2001FP. A newer 2001FP worked poorly, button 2 worked somewhat, button 1 sporadicallly.
// After soldering everything: 82 is too high, 75 is too low
#define BTNVALOUT_ONE 78
#define BTNVALOUT_TWO 50
#define BTNVALOUT_BOTH 0

// Delay required to simulate a single button press-and-release event that is registered as a single button press by the monitor.
// Tweakable. Too short? It will sometimes miss a press. Too long? It will sometimes register a press-and-hold, which will make
// the OSD menu's cursor advance one position too many.
// My old 2001FP (built in 2003) wants 150 here, newer 2001FP (2006) wants 165 here
#define BUTTONPRESS_DELAY 150

/**
 * Divides a given PWM pin frequency by a divisor.
 * 
 * The resulting frequency is equal to the base frequency divided by
 * the given divisor:
 *   - Base frequencies:
 *      o The base frequency for pins 3, 9, 10, and 11 is 31250 Hz.
 *      o The base frequency for pins 5 and 6 is 62500 Hz.
 *   - Divisors:
 *      o The divisors available on pins 5, 6, 9 and 10 are: 1, 8, 64,
 *        256, and 1024.
 *      o The divisors available on pins 3 and 11 are: 1, 8, 32, 64,
 *        128, 256, and 1024.
 * 
 * PWM frequencies are tied together in pairs of pins. If one in a
 * pair is changed, the other is also changed to match:
 *   - Pins 5 and 6 are paired on timer0
 *   - Pins 9 and 10 are paired on timer1
 *   - Pins 3 and 11 are paired on timer2
 * 
 * Note that this function will have side effects on anything else
 * that uses timers:
 *   - Changes on pins 3, 5, 6, or 11 may cause the delay() and
 *     millis() functions to stop working. Other timing-related
 *     functions may also be affected.
 *   - Changes on pins 9 or 10 will cause the Servo library to function
 *     incorrectly.
 * 
 * Thanks to macegr of the Arduino forums for his documentation of the
 * PWM frequency divisors. His post can be viewed at:
 *   http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1235060559/0#4
 */
void setPwmFrequency(int pin, int divisor) {
  byte mode;
  if(pin == 5 || pin == 6 || pin == 9 || pin == 10) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 64: mode = 0x03; break;
      case 256: mode = 0x04; break;
      case 1024: mode = 0x05; break;
      default: return;
    }
    if(pin == 5 || pin == 6) {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    } else {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  } else if(pin == 3 || pin == 11) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 32: mode = 0x03; break;
      case 64: mode = 0x04; break;
      case 128: mode = 0x05; break;
      case 256: mode = 0x06; break;
      case 1024: mode = 0x7; break;
      default: return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
  }
}

// The setup function runs once when you press reset, or power the board.
void setup() {
  // initialize digital pin 13 as an output.
  pinMode(PIN_IN_INPUTMENU, INPUT_PULLUP); // Reads buttons + and -
  pinMode(PIN_IN_MINPLUS, INPUT_PULLUP); // Reads buttons + and -
  // set PWM frequency to the max value
  // XXX this should only be done for pin 9 & 10 as otherwise all delays will be messed up
  setPwmFrequency(PIN_OUT_INPUTMENU, 1);
  setPwmFrequency(PIN_OUT_MINPLUS, 1);
  pinMode(PIN_OUT_INPUTMENU, OUTPUT); // PWM output of button press set 1, connects to a low-pass filter
  pinMode(PIN_OUT_MINPLUS, OUTPUT); // PWM output of button press set 2, connects to a low-pass filter
  pinMode(13, OUTPUT); // Arduino onboard LED
  Serial.begin(9600);
  Serial.println("Hello, world!");
  analogWrite(PIN_OUT_INPUTMENU, BTNVALOUT_NONE);
  analogWrite(PIN_OUT_MINPLUS, BTNVALOUT_NONE);
  // For debugging purposes, flash the onboard LED to indicate successful execution.
  digitalWrite(13, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(500);
  digitalWrite(13, LOW);    // turn the LED off by making the voltage LOW
  delay(500);
}

/*
 * Send a registered button press combination to the monitor's controller.
 */
int proxyPin(int pinInput, int pinOutput) {
  int val = analogRead(pinInput);
  //Serial.print("Buttons: ");
  //Serial.println(val);

  int button;

  if (val > 270) {
      button = 0;
  } else if (val > 200) {
      button = 1; // - button
  } else if (val > 90) {
      button = 2; // + button
  } else {
      button = 3; // + and - button
  }

  int output = BTNVALOUT_NONE;
  if (button == 1) {
    output = BTNVALOUT_ONE;
  } else if (button == 2) {
    output = BTNVALOUT_TWO;
  } else if (button == 3) {
    output = BTNVALOUT_BOTH; //both buttons
  }
  analogWrite(pinOutput, output);

  return button;
}

// Simulates a button release event.
void resetPin(int pinOutput) {
  analogWrite(pinOutput, BTNVALOUT_NONE);
}

// Releases all button presses.
void resetPins() {
  resetPin(PIN_OUT_INPUTMENU);
  resetPin(PIN_OUT_MINPLUS);
}

/*
 * Press and hold a button for a given time. Useful when in the OSD menu and you want to set a slider value
 * to the min or max value which is best achieved by holding the button, rather than trying to repeatedly press it.
 */
void pressButtonWithDelay(unsigned int button, int pressDelay) {
  int pin, value;
  
  switch(button) {
    case BTN_INPUT:
      pin = PIN_OUT_INPUTMENU; value = BTNVALOUT_ONE;
      break;
    case BTN_MENU:
      pin = PIN_OUT_INPUTMENU; value = BTNVALOUT_TWO;
      break;
    case BTN_MIN:
      pin = PIN_OUT_MINPLUS; value = BTNVALOUT_ONE;
      break;
    case BTN_PLUS:
      pin = PIN_OUT_MINPLUS; value = BTNVALOUT_TWO;
      break;
    default:
      pin = 0;
  }

  if (pin != 0) {
    analogWrite(pin, value);
    delay(pressDelay);
    resetPin(pin);
    delay(500);
  } else {
    Serial.print("Unknown button: ");
    Serial.println(button);
  }
}

/*
 * Simulate a single button press-and-release. It is timed such that the monitor will see it as a single
 * press.
 */
void pressButton(unsigned int button) {
  //150 works great for the old Dell 2001FP, the newer 2001FP likes 165 here!
  pressButtonWithDelay(button, BUTTONPRESS_DELAY);
}

/*
 * This sends the 'magic sequence' to the monitor to calibrate the screen position for the Amiga 500.
 * It is assumed that the monitor is not in the OSD menu. It will enter the menu, adjust horizontal and
 * vertical settings, and finally exit the menu.
 */
void setAmigaPreferences() {
  // Sequence of buttons to press:
  // Menu, min, Menu, min, Menu, min, min, min, Menu, min, Menu, plus, menu, plus,plus, menu, menu

  // BTN_HOLD is a bitflag indicating that a buttonpress should be held for a longer time, and can be OR'd to any
  //          button in the sequence
  unsigned int BTN_HOLD = 128;
  unsigned int sequence[] = { 
    BTN_MENU, BTN_MIN, BTN_MENU, BTN_MIN, BTN_MENU, // select horizontal settings
    BTN_MIN | BTN_HOLD, //set horizontal value
    BTN_MENU, BTN_MIN, BTN_MENU, // select vertical settings
    BTN_MIN, BTN_MIN, BTN_MIN, BTN_MIN,// set vertical value
    BTN_MENU, BTN_PLUS, BTN_PLUS, BTN_MENU, BTN_MENU // escape menu
  };
  int i;
  const int arraySize = sizeof(sequence) / sizeof(sequence[0]);

  for (int i=0; i < arraySize; i++) {
    unsigned int buttonToPress = sequence[i];

    if (buttonToPress & BTN_HOLD) {
      pressButtonWithDelay(buttonToPress & ~BTN_HOLD, 1000); //1 second ought to do it
    } else {
      pressButton(buttonToPress);
    }
  }
}

/*
 * Reliability test function - if you have button misses or doubles and are tweaking values, 
 * call this function to have it repeatedly press 1 button in the OSD menu so you can see 
 * whether your tweaks have improved anything. E.g. 10 Successful runs through the main
 * menu likely means stuff's working correctly.
 */
void cycleMenuEndlessly(int button) {
  pressButton(BTN_MENU);
  while (1) {
    pressButton(button);
  }
}

// The main function that controls the arduino's behavior. It will monitor keypresses.
// If the Plus and Minus buttons are pressed simultaneously, it will execute a magic
// button sequence to calibrate the screen. For any other button presses, it will pass
// them on to the monitor's microcontroller as-is, thus enabling the original functionality
// of the buttons. This works because the monitor did not assign a special function to
// double button presses, so I could intercept this and assign a function of my own.
void loop() {
  int buttonSet1Pressed = proxyPin(PIN_IN_INPUTMENU, PIN_OUT_INPUTMENU);
  int buttonSet2Pressed = proxyPin(PIN_IN_MINPLUS, PIN_OUT_MINPLUS);
  // light the led when any button is being pressed
  if (buttonSet1Pressed != 0 || buttonSet2Pressed != 0) {
    digitalWrite(13, HIGH);
  } else {
    digitalWrite(13, LOW);
  }

  if (buttonSet1Pressed != 0) {
    Serial.print("Buttonset 1 button(s) pressed: ");
    Serial.println(buttonSet1Pressed);
  }

  if (buttonSet2Pressed != 0) {
    Serial.print("Buttonset 2 button(s) pressed: ");
    Serial.println(buttonSet2Pressed);
  }
  
  if (buttonSet2Pressed == 3) {
      // Plus and minus pressed, let's execute our magic sequence!
      Serial.println("Sequence triggered");
      resetPins();
      delay(2000);
      setAmigaPreferences();
      //cycleMenuEndlessly(BTN_PLUS);
  }
}
