**Under Construction**

Demo Video: https://www.youtube.com/watch?v=fih1qalJm8M

Control 1-8 el wires with the sound around you.

If there is no sound around you for 5 seconds, it reads `morse.txt` off an SD card and blinks that on random wires instead.

If the EL Sequencer board doesn't receive any inputs at all for 7 seconds, it will blink wires randomly for random amounts of time.


Tools
-----

- Soldering Iron

- FTDI Cable 5V - $17.95 - https://www.sparkfun.com/products/9718

- USB micro cable with the red wire cut

- Small philips screw driver


Parts
-----

- Solder and Flux

- [Multicolored jumper cables - $10.00](https://amzn.com/B00M5WLZDW)
  - Any wire works, but these are easy to split apart and were nice while experimenting with the breadboard.
  - Once I was done experimenting with the breadboard and ready to solder, I cut the ends off, stripped the ends a bit, and melted some solder into the bare wire.

- [12v Portable Battery Pack - $23.99](http://smile.amazon.com/dp/B00MHNQIR2)
  - Fully charged, this has lasted over 8 hours for me.

- [Teensy 3.2 Audio Tutorial Kit - $60.00](https://www.pjrc.com/store/audio_tutorial_kit.html)
  - You can save some money by buying the Teensy, the Audio Adaptor, some extra parts, and soldering it all together, but the kit is a lot easier and then you can do all their awesome tutorials.

- [Break Away Headers Straight - $1.50](https://www.sparkfun.com/products/116)
  - 6 for the FTDI headers
  - 8 for the Teensy

- [Sparkfun EL Sequencer - $34.95](https://www.sparkfun.com/products/12781)
  - You will have to solder some header pins and close a jumper.

- [EL Inverter 12v - $14.95](https://www.sparkfun.com/products/10469)
  - The site says it powers 10-15m of EL Wire, but I've run 6 3m strands and its pretty bright.

- [Jumper Wire - JST Black Red - $0.95](https://www.sparkfun.com/products/8670)
  - This will replace the barrel plug cable on the Inverter so that it can be powered by the board.

- [Barrel Jack to 2-pin JST - $2.95](https://www.sparkfun.com/products/8734)

- Up to 6 strands of [EL Wire - $9.95 each](https://www.sparkfun.com/products/10200)
  - A later version of this visualizer will support up to 8 strands and "chasing" wire.

- Clear thread or fishing line.

- A loose fitting jacket that is easy to dance in and will keep you warm outside at night.


Software
--------

- Teensy

- Arduino 2 IDE


Putting it Together
-------------------

#. Label the FTDI cable near the USB end "FTDI".
#. Label the micro USB cable near the USB end "NO POWER/TEENSY".
#. Label the existing JST connector on the inverter "100V AC".
#. Label the new JST connector for the inverter "12V DC".
#. Wet solder sponge and plug in soldering iron.
#. Remove the quality control sticker from the inverter.
#. Unscrew the inverter and open it up.
#. Replace the barrel plug on the inverter with the JST connector.
#. Place a dot of solder on SJ1 so the 12V battery directly powers the inverter.
#. Solder the 6 FTDI header pins in place.
#. Solder the power, ground, and 6 analog header pins in place.
#. Unplug the soldering iron.
#. Insulate the capacitors inside the inverter somehow. Hot glue? Epoxy? Rubber insulation?
#. Glue down the new JST connector on the inverter.
#. Screw the inverter back together.
#. Solder the header pins and the Teensy together.
  - Sequencer power -> Teensy power
  - Sequencer ground -> Teensy ground
  - Sequencer A2 -> Teensy 0
  - Sequencer A3 -> Teensy 1
  - Sequencer A4 -> Teensy 2
  - Sequencer A5 -> Teensy 3
  - Sequencer A6 -> Teensy 4
  - Sequencer A7 -> Teensy 5
#. Label the Barrel Jack to 2-pin JST connector "12v BATT".
#. Plug the Barrel Jack to 2-pin JST connector into the Sequencer board.
#. Plug the inverter into the sequencer board.
#. Flip the power switch to "BATT".
#. Plug the battery into the Barrel Jack.
#. Plug the FTDI cable into the sequencer board.
#. Plug the micro USB into the Teensy.
#. Plug the USB from the sequencer into your computer.
#. Plug your EL wire into the sequencer
#. Turn the battery on.
#. Upload "el_sequencer.ino".
#. Open the serial console.
#. Unplug the FTDI cable from your computer.
#. Plug the USB **WITH THE RED WIRE CUT** from the Teensy into your computer.
#. Upload "teensy.ino"
#. Open the serial console.
#. Play some music.
