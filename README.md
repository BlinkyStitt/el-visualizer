**Under Construction**

Demo Video: https://www.youtube.com/watch?v=fih1qalJm8M

Control 1-8 el wires with the sound around you.

If there is no sound around you for 5 seconds, it reads `morse.txt` off an SD card and blinks that on random wires instead.

If the EL Sequencer board doesn't receive any inputs at all for 7 seconds, it will blink wires randomly for random amounts of time.


Tools
-----

- Soldering Iron
- Heat Shrink Gun

- FTDI Cable 5V - $17.95 - https://www.sparkfun.com/products/9718 - This can be replaced by a FTDI Breakout Board 5V


Parts
-----

- Solder and Flux
- Heat Shrink Tubing
- Wire for extensions to the EL Wire or Battery Pack

- FTDI Breakout Board 5V - $14.95 - https://www.sparkfun.com/products/9716 - (Only if not using FTDI Cable)

- Sparkfun EL Sequencer - $34.95 - https://www.sparkfun.com/products/12781 - You will have to do minimal soldering to the board for it to work with the 12v Inverter

- EL Inverter 12v - $14.95 - https://www.sparkfun.com/products/10469 - Powers 10-15m of EL Wire

- Jumper Wire - JST Black Red - $0.95 - https://www.sparkfun.com/products/8670 - This will replace the barrel plug cable on the Inverter so that it can be powered by the board. You can also buy a big pack of these for cheap

- 12v Portable Battery Pack $30-50
 - I chose https://www.amazon.com/gp/product/B00ME3ZH7C/ref=oh_aui_detailpage_o01_s00?ie=UTF8&psc=1 since its always nice having a spare USB port and it isn't too giant. This is larger than a simple Li-ion pack, but is easy to charge and has status lights.

- Barrel Jack to 2-pin JST - $2.95 - https://www.sparkfun.com/products/8734 - This makes it easy to connect the Battery Pack to the Sequencer Board. You could also solder a JST connected onto your Battery Pack's cable, but I wanted the option to plug that into other things easily.

- Up to ~15m of EL Wire - https://www.sparkfun.com/products/10200
 - If you want to use "chasing" wire, you need https://www.sparkfun.com/products/12934 ($1.50). Chasing wire takes up 3 slots.

With the above parts you can power up to 8 strands of EL Wire (chasing counts as 3) by writing small programs for the sequencer board and using the FTDI Cable/Breakout.


Simple Sound Reactive
---------------------

- Sparkfun Sound Detector - $10.95 - https://www.sparkfun.com/products/12642 - This simple board will send a signal whenever there is sound.


Advanced Sound Reactive
-----------------------

- Teensy 3.2 - $19.95 - https://www.pjrc.com/store/teensy32.html - This awesome board can be easily hooked up to a bunch of sensors. Than you wire this board up to the inputs of the Sequencer board and done. This logic could maybe bo done by the sequencer board itself, but the FFT library for the Teensy makes this really easy (code coming soon).

- Teensy Audio Board - $14.95 - https://www.sparkfun.com/products/12767 - This makes it easy to hook a mic and do the math for an equalizer. I also plan on using the audio out to do something trippy like https://itunes.apple.com/us/app/the-app-formerly-known-as-h-r/id1087530357?mt=8

- Microphone and thumb wheel for Audio Board


Todo
----

Instructions for putting it all together
