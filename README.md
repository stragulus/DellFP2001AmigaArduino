# DellFP2001AmigaArduino
This is an arduino project to control a Dell FP 2001 monitor's OSD menu to center its picture for my Amiga 500. By pressing a magic button sequence, the monitor will configure itself. [This video demonstrates it perfectly](https://www.youtube.com/watch?v=gw1YQZlBUE4).

# How does it work?

In short, the arduino's inputs are hooked up to the monitor's buttons, and its outputs to the monitor's controller. It thus intercepts button presses, and will either generate its own button presses, or simply passes through the buttons pressed by the user. The code is configured to generate a sequence of button presses once the user presses and holds 2 buttons at the same time. In any other case, it will simply pass the button presses on to the monitor's controller, thus transparently establishing the original behavior.

With this trick in place, the double button press kicks off a very specific, hardcoded sequence of button presses. These button presses will bring up the monitor's OSD menu, navigate to horizontal/vertical position settings, and adjust them such that my Amiga 500's display is centered properly.

# But... why??

Earlier revision Dell FP2001 monitors accept a 15kHz signal on their vga input connector. This is the same frequency used by regular good old composite-out video connectors, and matches a 30Hz NTSC or 25Hz PAL signal as output by the Amiga. Normally, VGA connectors only accept signals at a higher frequency, and to connect the Amiga, you'd have to use a scan doubler to work with a normal VGA monitor. 

Since the Dell FP 2001 is one of the rare monitors that work with this frequency, and I had it gathering dust, I wanted to hook up the Amiga directly. While this works, the horizontal/vertical position was way off as the monitor's auto-calibrate did not work well with this out-of-spec signal. This is correctable manually through the OSD menu though, but it requires a lot of button presses. Additionally, the monitor does not memorize settings, and thus you'd have to do this every time you turn it on. That's a situation that just begs to be automated.

# Schematics

Two of the arduino's analog inputs are wired to the monitor's button LCD. Each input reads 2 buttons; Each of the 2 buttons is wired in series with a resistor, and both buttons are wired in parallel. By using different resistor values, the distinction between the buttons can be made. The button/resistor circuitry is part of the monitor's PCB.

The arduino's outputs are connected to the monitor's controller that,  before the addition of the arduino, was hooked up to the buttons directly. The outputs are configured as PWM outputs, as they have to simulate an analog network with different voltages, rather than a simple digital one that is either high or low. Since a PWM output is a block signal, it will confuse the monitor, which expects an analog signal with a steady signal at a certain level. To this end, I added a simple RC circuit to the PWM outputs to smoothen the signal. See references for examples.

TODO: Actually add a schema with the exact components

# References

* RC circuit theory: http://www.allaboutcircuits.com/technical-articles/low-pass-filter-a-pwm-signal-into-an-analog-voltage/
* RC circuit values calculator: http://www.referencedesigner.com/rfcal/cal_05.php
