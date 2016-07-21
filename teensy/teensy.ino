// control for Bryan's Jacket with a configurable number of EL wires
//
// when it is quiet, lights flash out morse code BRYAN
// when it is loud, lights flash with the music
//

// you can customize these
#define MAX_OUTPUTS 8
#define OUTPUT_A 0
#define OUTPUT_B 1
#define OUTPUT_C 2
#define OUTPUT_D 3
#define OUTPUT_E 4
#define OUTPUT_F 5
#define OUTPUT_G 8
#define OUTPUT_H 16
#define FFT_AVERAGE_TOGETHER 8  // todo: tune this. higher values = slow refresh rate. default 8 = ~344/8 refreshes per second
#define FFT_IGNORED_BINS 1  // skip the first bin
#define FFT_MAX_BINS 127
#define MAX_OFF_MS 5000
#define MAX_MORSE_ARRAY_LENGTH 256  // todo: tune this
#define DEFAULT_OUTPUTS 6
#define DEFAULT_MORSE_STRING "HELLO WORLD"
// END you can customize these

const int outputPins[MAX_OUTPUTS] = {OUTPUT_A, OUTPUT_B, OUTPUT_C, OUTPUT_D, OUTPUT_E, OUTPUT_F, OUTPUT_G, OUTPUT_H};

#define OUTPUT_ON 0x0
#define OUTPUT_OFF 0x1
#define OUTPUT_OFF_NEXT_OUTPUT 0x2

#include <Audio.h>
#include <AnalogPin.h>
#include <Bounce2.h>
#include <elapsedMillis.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

AudioInputI2S            audioInput;
AudioOutputI2S           audioOutput; // even though we don't output to this, it still needs to be defined for the shield to work
AudioAnalyzeFFT256       myFFT;
AudioConnection          patchCord1(audioInput, audioOutput);
AudioConnection          patchCord2(audioInput, myFFT);
AudioControlSGTL5000     audioShield;
AnalogPin                numOutputKnob(A1);
Bounce                   outputSensitivityUpButton = Bounce();
Bounce                   outputSensitivityDownButton = Bounce();

struct MorseCommand {
  unsigned int checkTime;
  unsigned int outputAction;
};
int morse_array_length = MAX_MORSE_ARRAY_LENGTH;
MorseCommand morse_array[MAX_MORSE_ARRAY_LENGTH];
const int numFftBinsWeCareAbout = 24;


/*
   HELPER FUNCTIONS
*/

void printNumber(float n, float minimum = 0.008) {
  if (n >= minimum) {
    Serial.print(n, 3);
    Serial.print(" ");
  } else {
    Serial.print("   -  "); // don't print "0.00"
  }
}

/*
   END HELPER FUNCTIONS
*/

void setup() {
  Serial.begin(9600);  // TODO! disable this on production build

  AudioMemory(12); // todo: tune this
  audioShield.enable();
  audioShield.muteHeadphone(); // to avoid any clicks
  audioShield.inputSelect(AUDIO_INPUT_MIC);
  // audioShield.lineInLevel(4);  // 0-15. 5 is default. 0 is highest voltage
  audioShield.volume(0.5);  // for debugging
  audioShield.micGain(63);  // 0-63

  audioShield.audioPreProcessorEnable();  // todo: i dont think post goes to the 

  //audioShield.enhanceBassEnable();
  //audioShield.enhanceBass(1.0, -1.0, false, 5);  // todo: tune this

  // bass, mid_bass, midrange, mid_treble, treble
  audioShield.eqSelect(GRAPHIC_EQUALIZER);
  audioShield.eqBands(-1.0, -0.10, 0, 0.10, 0.33);  // todo: tune this

  // todo: autoVolume?

  myFFT.averageTogether(FFT_AVERAGE_TOGETHER);

  for (int i=0; i<MAX_OUTPUTS; i++) {
    pinMode(outputPins[i], OUTPUT);
  }

  pinMode(A3, INPUT_PULLUP);
  outputSensitivityUpButton.attach(A3);
  outputSensitivityUpButton.interval(15);

  pinMode(A2, INPUT_PULLUP);
  outputSensitivityDownButton.attach(A2);
  outputSensitivityDownButton.interval(15);

  // read text off the SD card and translate it into morse code
  SPI.setMOSI(7);
  SPI.setSCK(14);
  if (!(SD.begin(10))) {
    while (1) {
      // todo: allow not having a SD card. just do the default string then
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  // todo: support a directory with multiple text files and pick randomly
  File morseFile = SD.open("morse.txt", FILE_READ);

  String morseString = "";
  if (morseFile) {
    // if the file is available, read it:
    while (morseFile.available()) {
      // todo: this is wrong, but im just testing
      morseString += (char)morseFile.read();
    }
    morseFile.close();
    // TODO! do something to turn the morse code string into something we can easily use to blink
  } else {
    // if the file isn't open, pop up an error:
    Serial.println("error opening morse.txt");
    morseString = DEFAULT_MORSE_STRING;
  }

  Serial.print("morseString: ");
  Serial.println(morseString);

  // TODO! actually translate morseString into a list of on/off commands
  // http://www.kent-engineers.com/codespeed.htm
  morse_array[0].checkTime = 1000;
  morse_array[0].outputAction = OUTPUT_ON;
  morse_array[1].checkTime = 1000;
  morse_array[1].outputAction = OUTPUT_OFF_NEXT_OUTPUT;
  morse_array[2].checkTime = 1000;
  morse_array[2].outputAction = OUTPUT_ON;
  morse_array[3].checkTime = 1000;
  morse_array[3].outputAction = OUTPUT_OFF_NEXT_OUTPUT;
  morse_array[4].checkTime = 1000;
  morse_array[4].outputAction = OUTPUT_ON;
  morse_array[5].checkTime = 1000;
  morse_array[5].outputAction = OUTPUT_OFF_NEXT_OUTPUT;
  morse_array_length = 6;

  audioShield.unmuteHeadphone();  // for debugging

  // todo: is this enough?
  randomSeed(analogRead(0));

  // give things some time to start
  // todo: not sure if this is actually needed
  delay(500);
}


elapsedMillis blinkTime = 0;
int blinkOutput = OUTPUT_A, numOutputs = DEFAULT_OUTPUTS, morseOutputId = random(numOutputs);
float outputSensitivity = 1.025;  // debugging the morse code
bool outputSensitivityUpButtonState, outputSensitivityDownButtonState = false;
unsigned long outputSensitivityUpButtonPressTimeStamp, outputSensitivityDownButtonPressTimeStamp;


void loop() {
  // configure number of outputs
  // todo: have one pin based on motion sensor

  // todo: analog to read numOutputs

  // todo: too much copypasta in this
  if (outputSensitivityUpButton.update()) {
    if (outputSensitivityUpButton.fell()) {
      Serial.println("Output sensitivity up button pressed");
      outputSensitivityUpButtonState = true;
      outputSensitivityUpButtonPressTimeStamp = millis();
      outputSensitivity = outputSensitivity + 0.001;
      if (outputSensitivity > 1.001) {
        outputSensitivity = 1.001;
      }
    } else {
      outputSensitivityUpButtonState = false;
    }
  } else if (outputSensitivityUpButtonState) {
    if (millis() - outputSensitivityUpButtonPressTimeStamp >= 50) {
      Serial.println("Output sensitivity up button held down");
      outputSensitivityUpButtonPressTimeStamp = millis();
      outputSensitivity = outputSensitivity + 0.002;
      if (outputSensitivity > 1.001) {
        outputSensitivity = 1.001;
      }
    }
  } else if (outputSensitivityDownButton.update()) {
    if (outputSensitivityDownButton.fell()) {
      Serial.println("down button pressed");
      outputSensitivityDownButtonState = true;
      outputSensitivity = outputSensitivity - 0.001;
      if (outputSensitivity < -0.001) {
        outputSensitivity = -0.001;
      }
    } else {
      outputSensitivityDownButtonState = false;
    }
  } else if (outputSensitivityDownButtonState) {
    if (millis() - outputSensitivityDownButtonPressTimeStamp >= 50) {
      Serial.println("Output sensitivity down button held down");
      outputSensitivityDownButtonPressTimeStamp = millis();
      outputSensitivity = outputSensitivity - 0.002;
      if (outputSensitivity < -0.001) {
        outputSensitivity = -0.001;
      }
    }
  }

  /*
  // configure output sensitivity
  // todo: tune read and divisor
  int outputSensitivity = outputSensitivityKnob.read(1);
  // make the ends of the knob easier to use
  if (outputSensitivity < 24) {
    outputSensitivity = 0;
  } else if (outputSensitivity > 1000) {  // TODO: once we used AnalogPin library, this dropped
    outputSensitivity = 1024;
  }
  */

  if (outputSensitivity < 1 && myFFT.available()) {
    // configure number of fft bins to read
    /*
    // we have 128 bins total, but the first and the most all of the top are worthless. andy uses 10%
    int maxFftBinsWeCareAbout = FFT_MAX_BINS - FFT_IGNORED_BINS;
    // todo: tune read
    int numFftBinsWeCareAbout = (int)numFftBinsWeCareAboutKnob.readSmoothed(24) / (1024 / maxFftBinsWeCareAbout) + 1;
    if (numFftBinsWeCareAbout < numOutputs) {
      numFftBinsWeCareAbout = numOutputs;
    } else if (numFftBinsWeCareAbout > maxFftBinsWeCareAbout) {
      numFftBinsWeCareAbout = maxFftBinsWeCareAbout;
    }
    */
    int expectedBinsPerOutput = numFftBinsWeCareAbout / numOutputs;

    int numOutputBins, nextOutputStartBin, outputId = 0;
    int outputStartBin = FFT_IGNORED_BINS;  // ignore bin 0. it is far too noisy. maybe the eq/filters can help tho?

    // DEBUGGING
    Serial.print(numFftBinsWeCareAbout);
    Serial.print(" FFT bins >");
    Serial.print(outputSensitivity, 3);
    Serial.print(" across ");
    Serial.print(numOutputs);
    Serial.print(" outputs (");
    Serial.print(expectedBinsPerOutput);
    Serial.print("):  ");
    // END DEBUGGING

    while (outputStartBin < numFftBinsWeCareAbout + FFT_IGNORED_BINS) {
      // todo: do we actually want even output sizes? maybe the first bins should be smaller
      if (outputId < numOutputs - 1) {
        nextOutputStartBin = outputStartBin + expectedBinsPerOutput;
        numOutputBins = expectedBinsPerOutput;
      } else {
        // last output
        // make sure we get the last bin in case of rounding errors
        nextOutputStartBin = numFftBinsWeCareAbout + FFT_IGNORED_BINS;
        numOutputBins = nextOutputStartBin - outputStartBin;
      }

      // .read(x, y) gives us a sum of bins x through y so we divide to keep the average
      float outputLevel = myFFT.read(outputStartBin, nextOutputStartBin - 1) / (float)numOutputBins;
      /*
      // DEBUGGING
      Serial.print("Reading ");
      Serial.print(outputStartBin);
      Serial.print(" to ");
      Serial.print(nextOutputStartBin - 1);
      Serial.print(" (");
      Serial.print(numOutputBins);
      Serial.println(")");
      */
      printNumber(outputLevel, outputSensitivity);
      // END DEBUGGING

      // turn the light on if outputLevel > outputSensitivity. off otherwise
      if (outputLevel > outputSensitivity) {
        // todo: save the time that we turned the light on
        digitalWrite(outputPins[outputId], HIGH);

        // reset blinkTime any time lights turn on
        blinkTime = 0;
      } else {
        // todo: only turn the light off if it has been long enough
        digitalWrite(outputPins[outputId], LOW);
      }

      outputStartBin = nextOutputStartBin;
      outputId = outputId + 1;
    }

    Serial.println();
  }

  // TODO! if no lights are on, blink in morse code
  if (blinkTime < MAX_OFF_MS) {
    ; // do nothing. leave the lights as they were for up to MAX_OFF_MS seconds

    // todo: remove this delay and instead make sure lights stay on for a minimum amount of time
    delay(200);
  } else {
    unsigned long checkTime = MAX_OFF_MS;

    bool morse_command_found = false;

    for (int i = 0; i < morse_array_length; i++) {
      MorseCommand morse_command = morse_array[i];
      checkTime = checkTime + morse_command.checkTime;
      if (blinkTime < checkTime) {
        morse_command_found = true;
        /*
        Serial.print(blinkTime);
        Serial.print(" < ");
        Serial.print(checkTime);
        Serial.println();
        */
        if (morse_command.outputAction == OUTPUT_ON) {
          digitalWrite(morseOutputId, HIGH);
        } else if (morse_command.outputAction == OUTPUT_OFF) {
          digitalWrite(morseOutputId, LOW);
        } else {
          // make sure the wire is off
          digitalWrite(morseOutputId, LOW);
          // turn on a random wire
          // TODO: should we allow doing random or next?
          // morseOutputId = random(numOutputs);
          // TODO: this is wrong! this advances multiple times! we need to make sure this only happens once!
          delay(1000);
          morseOutputId = (morseOutputId + 1 ) % numOutputs;
          Serial.print("New morseOutputId: ");
          Serial.println(morseOutputId); 
        }
        break;
      }
    }

    if (not morse_command_found) {
      // we hit the end of the loop
      // reset blink so we start at the beginning of the message
      Serial.println("Looping morse code message...");
      blinkTime = MAX_OFF_MS;
      // blink on a new wire
      // morseOutputId = random(numOutputs);
    }

    /*
    // blink for 1/4 of each second
    // todo: blink morse code
    if (checkTime < 2000) {
      // turn the EL wire is on
      digitalWrite(outputPins[blinkOutput], HIGH);
    } else if (checkTime < 3500) {
      // turn the EL wire off
      digitalWrite(outputPins[blinkOutput], LOW);
    } else {
      // make sure the EL wire is off (the previous else should have done this, but just in case)
      digitalWrite(outputPins[blinkOutput], LOW);

      // pick a random wire for the next blink
      blinkOutput = random(numOutputs);
      Serial.print("Blinking output: ");
      Serial.println(blinkOutput);

      // reset blinkTime to the point where we will keep blinking if no inputs are HIGH
      blinkTime = MAX_OFF_MS;
    }
    */
  }
}

