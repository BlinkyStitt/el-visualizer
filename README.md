# EL Visualizer (DEPRECATED)

**Updated code at new repo: https://github.com/WyseNynja/teensy-visualizer**

Control el wires with the sound around you.

Demo Video v2: https://www.youtube.com/watch?v=WvVMDnH78To

Demo Video v1: https://www.youtube.com/watch?v=fih1qalJm8M


## Tools

- Soldering Iron

- FTDI Cable 5V - $17.95 - https://www.sparkfun.com/products/9718

- USB micro cable for powering the Teensy


## Parts

- Solder and Flux

- [Multicolored jumper cables - $10.00](https://smile.amazon.com/dp/B00M5WLZDW)
  - Any wire works, but these are easy to split apart and were nice while experimenting with the breadboard.
  - Once I was done experimenting with the breadboard and ready to solder, I cut the ends off, stripped the ends a bit, and melted some solder into the bare wire.

- [12v Portable Battery Pack - $33.99](https://smile.amazon.com/dp/B00ME3ZH7C)
  - I like this one because it has a 12v port for the inverters and a USB port for the Teensy
  - If you get a battery pack without a USB power, you will need to power the Teensy's USB some other way.
  - Fully charged, this has lasted over 12 hours for me. Out lasting the event is important because you might not always have time to charge between events.

- [Teensy 3.2 Audio Tutorial Kit - $60.00](https://www.pjrc.com/store/audio_tutorial_kit.html)
  - You can save some money by buying the Teensy, the Audio Adaptor, some extra parts, and soldering it all together, but the kit is a lot easier and then you can do all their awesome tutorials.

- up to 7 [Sparkfun EL Sequencer - $34.95](https://www.sparkfun.com/products/12781)
  - Each sequencer lets you connect up to 8 EL wires
  - I am not sure what the maximum is yet, but I think you could easily do up to 7. That is a lot of wire!
  - You will have to solder some headers

- [Break Away Headers Straight - $1.50](https://www.sparkfun.com/products/116)
  - 9 pins per EL Sequencer

- up to 7 [EL Inverter 12v - $14.95](https://www.sparkfun.com/products/10469)
  - You will need one of these for each EL Sequencer
  - The site says it powers 10-15m of EL Wire, but I've run 6 3m strands and its pretty bright. It is rare that you will need to power all the wires at the same time.

- 0 to 6 [DC 5.5x2.1mm 1 Female to 2 Male Power Splitter Cable](https://www.sparkfun.com/products/10469)
  - 1 of these comes with the Battery Pack. You will need 1 more per additional EL Sequencer.

- Lots of [EL Wire - $9.95 each](https://www.sparkfun.com/products/10200)

- Clear thread or fishing line.

- Something to attach the wire to (Jackets, shower curtains, or whatever else you want to light up).


## Software

- [Teensyduino](https://www.pjrc.com/teensy/teensyduino.html)
  - The install steps will walk you through installing the Arduino IDE


## Putting it Together

1. Label the FTDI cable near the USB end "FTDI".
2. Label the micro USB cable near the USB end "TEENSY".
3. Label the JST connector on the inverters "100V AC".
4. Label the Barrel Jack on the inverters "12v BATT".
5. Label the EL Wire near the connectors with the color and a number (Red 1, Red 2, Orange 1, etc.)
6. Wet solder sponge and plug in soldering iron.
7. Solder the 6 FTDI header pins onto each EL Sequencer board.
8. Solder header pins to power, ground, and A5 on each EL Sequencer board.
9. Use the wire strippers, the soldering iron, flux, and solder to prepare the power, ground, and serial wires that will connect the Teensy to the EL sequencers
10. Solder the header pins and the Teensy together with the wire.
    - Sequencer power -> Teensy power
    - Sequencer ground -> Teensy ground
    - Sequencer A5 -> Teensy 0 or Teensy 3. More should work, but you will have to modify the Teensy code
11. Unplug the soldering iron.
12. Flash the Sequencers
    1. Flip the power switch on the sequencer to "USB". (I sometimes had trouble flashing if they were powered through the Teensy)
    2. Plug the FTDI USB cable into your computer and into the Sequencer. Everything should power up when the sequecer is plugged in.
    3. Flash the sequencers with the sequencer code.
    4. Unplug the FTDI cable
    5. Flip the power switch on the sequencers to "BATT" which is **not** plugged in. Power actually comes from the Teensy.
13. Plug the USB cable into your computer and the Teensy. Everything should power up when the Teensy is plugged in.
14. Flash the Teensy with the teensy code. Leave the USB cable connected to your computer.
15. Plug the EL wire into the sequencers.
16. Plug the inverters into the sequencers.
17. Plug the inverters into the battery.
18. Turn the inverters on.
19. Turn the battery on. The inverters should make a high pitched whine.
20. Open the serial console.
21. Play some music and watch lights blink while the serial console flies by.

While debugging, it is helpful to power the Teensy from your computer. For an actual event, it is simpler to use the USB battery power on the Battery Pack.

If you flash the Teensy while the inverters are powered, the lights will flicker, but nothing will break.


## Ideas

v3 will use SPI instead of Serial and so will be able to support even more outputs.
