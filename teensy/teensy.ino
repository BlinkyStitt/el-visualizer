/* control for the Teensy 3.2 in Bryan's Sound-reactive EL Jacket
 *
 * when it is quiet, lights flash out morse code from an SD card
 * when it is loud, lights flash with the music
 *
 * TODO: analog button press to set minInputSensitivity
 *
 */

/*
 * you can easily customize these
 *
 * todo: define or const?
 */
#define BUTTON_HOLD_MS 200
#define BUTTON_INTERVAL 20
#define DEBUG true    // todo: write a debug_print that uses this to print to serial
#define DEFAULT_MORSE_STRING "HELLO WORLD"  // what to blink if no morse.txt on the SD card
#define EMA_ALPHA 0.2
#define FFT_IGNORED_BINS 2  // skip the first FFT_IGNORED_BINS * FFT_HZ_PER_BIN Hz
#define INPUT_NUM_OUTPUTS_KNOB 15
#define MAX_MORSE_ARRAY_LENGTH 256  // todo: tune this
#define MAX_OFF_MS 6000
#define MINIMUM_INPUT_RANGE 0.50  // activate outputs on sounds that are at least this % as loud as the loudest sound

const int minimumOnMs = 200;

float minInputSensitivity = 0.035;

// TODO: tune these to look pretty
const int morseDitMs = 250;
const int morseDahMs = morseDitMs * 3;
const int morseElementSpaceMs = morseDitMs;
const int morseWordSpaceMs = morseDitMs * 7;
/*
 * END you can easily customize these
 */

#define FFT_HZ_PER_BIN 43
#define MAX_OUTPUTS 8  // EL Sequencer only has 8 wires, but you could easily make this larger and drive other things
#define OUTPUT_A 0
#define OUTPUT_B 1
#define OUTPUT_C 2
#define OUTPUT_D 3
#define OUTPUT_E 4
#define OUTPUT_F 5
#define OUTPUT_G 8
#define OUTPUT_H 20
const int outputPins[MAX_OUTPUTS] = {OUTPUT_A, OUTPUT_B, OUTPUT_C, OUTPUT_D, OUTPUT_E, OUTPUT_F, OUTPUT_G, OUTPUT_H};
int outputBins[MAX_OUTPUTS];

#include <Audio.h>
#include <elapsedMillis.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

AudioInputI2S audioInput;
AudioOutputI2S audioOutput; // even though we don't output to this, it still needs to be defined for the shield to work
AudioAnalyzeFFT1024 myFFT;
AudioConnection patchCord1(audioInput, audioOutput);
AudioConnection patchCord2(audioInput, myFFT);
AudioControlSGTL5000 audioShield;

// save on/off/off+next states. if current time < runningSumOfCheckMs, than do action. else check next time
struct TimedAction {
  unsigned int checkMs;
  unsigned int outputAction;
};
int morseArrayLength = 0;
TimedAction morseArray[MAX_MORSE_ARRAY_LENGTH];


int morseDit(TimedAction morseArray[], int i) {
  Serial.print('.');
  // todo: protect overflowing morseArray
  morseArray[i].checkMs = morseDitMs;
  morseArray[i].outputAction = HIGH;
  i++;
  return morse_element_space(morseArray, i);
}

int morseDah(TimedAction morseArray[], int i) {
  Serial.print('-');
  // todo: protect overflowing morseArray
  morseArray[i].checkMs = morseDahMs;
  morseArray[i].outputAction = HIGH;
  i++;
  return morse_element_space(morseArray, i);
}

int morse_element_space(TimedAction morseArray[], int i) {
  // todo: protect overflowing morseArray
  morseArray[i].checkMs = morseElementSpaceMs;
  morseArray[i].outputAction = LOW;
  i++;
  return i;
}

int morse_word_space(TimedAction morseArray[], int i) {
  Serial.print(" / ");
  // todo: protect overflowing morseArray
  morseArray[i].checkMs = morseWordSpaceMs;
  morseArray[i].outputAction = LOW;
  i++;
  return i;
}

void string2morseArray(String morseString) {
  morseArrayLength = 0;
  for (unsigned int i = 0; i < morseString.length(); i++) {
    // todo: make sure morseArrayLength never exceeds max
    // https://upload.wikimedia.org/wikipedia/en/5/5a/Morse_comparison.svg
    // todo: make everything uppercase?
    // todo: put this in its own library?
    switch (morseString[i]) {
      case 'E':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'T':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'I':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'A':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'N':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'M':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'S':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'U':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'R':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'W':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'D':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'K':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'G':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'O':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'H':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'V':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'F':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case ' ':
        morseArrayLength = morse_word_space(morseArray, morseArrayLength);
        break;
      case 'L':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'P':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'J':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'B':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'X':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'C':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'Y':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'Z':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'Q':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case '5':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case '4':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case '3':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case '2':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case '1':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case '6':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case '7':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case '8':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case '9':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case '0':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case '.':
        // todo: do this on a different wire?
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
      default:
        break;
    }

    Serial.print(' ');
    morseArrayLength = morse_element_space(morseArray, morseArrayLength);
  }
  Serial.println();
}

/*
   END HELPER FUNCTIONS
*/

void setup() {
  Serial.begin(9600);  // TODO! disable this if DEBUG mode on. optimizer will get rid of it

  // setup audio shield
  AudioMemory(12); // todo: tune this. so far max i've seen is 11
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
  audioShield.eqBands(-1.0, -0.10, 0.10, 0.10, 0.33);  // todo: tune this

  // todo: autoVolume?

  // setup outputs
  for (int i=0; i<MAX_OUTPUTS; i++) {
    pinMode(outputPins[i], OUTPUT);
  }

  // setup numOutputs knob
  pinMode(INPUT_NUM_OUTPUTS_KNOB, INPUT);

  // read text off the SD card and translate it into morse code
  String morseString = "";
  SPI.setMOSI(7);
  SPI.setSCK(14);
  if (SD.begin(10)) {
    // todo: support a directory with multiple text files?
    File morseFile = SD.open("morse.txt", FILE_READ);
    if (morseFile) {
      Serial.println("Reading morse.txt...");
      // if the file is available, read it:
      while (morseFile.available()) {
        // todo: this is wrong, but im just testing
        morseString += (char)morseFile.read();
      }
      morseFile.close();
    }

    // todo: should we just do this as we read? theres no real need to build a String besides the pretty serial.print
    string2morseArray(morseString);
  }

  Serial.print("morse array length: ");
  Serial.println(morseArrayLength);

  audioShield.unmuteHeadphone();  // for debugging

  randomSeed(morseArrayLength + analogRead(0));

  Serial.println("Waiting for EL Sequencer...");
  delay(500);

  Serial.println("Starting...");
}


elapsedMillis nowMs = 0;    // todo: do we care if this overflows?

int blinkOutput = OUTPUT_A;
int numOutputs = 1;

// Exponential Moving Average of each wire individually and then all the wires together
float avgInputLevel[MAX_OUTPUTS + 1];
float lastLoudestLevel = 0;

unsigned long lastOnMs;
unsigned long lastOnMsArray[MAX_OUTPUTS];
int outputStates[MAX_OUTPUTS];

int lastMorseCommandId = -1;


void updateNumOutputs() {
  // configure number of outputs
  int oldNumOutputs = numOutputs;
  numOutputs = (int)analogRead(INPUT_NUM_OUTPUTS_KNOB) / (1024 / MAX_OUTPUTS) + 1;

  if (oldNumOutputs == numOutputs) {
    return;
  }
  // we changed numOutputs

  Serial.print("New numOutputs: ");
  Serial.println(numOutputs);

  // turn all the wires off
  for (int i=0; i<MAX_OUTPUTS; i++) {
    digitalWrite(outputPins[i], LOW);
  }

  /*
    // max of 512 * 43 Hz == 22k Hz
    // 16k Hz = 372
    // 10k Hz = 233
    // 5k Hz = 116

    Sub-bass       20 to  60 Hz        40     1        1
    Bass           60 to 250 Hz       190     4.75     2
    Low midrange  250 to 500 Hz       250     6.25     4
    Midrange      500 Hz to 2 kHz    1500    37.5      8
    Upper midrange  2 to 4 kHz       2000    50       16
    Presence        4 to 6 kHz       2000    50       32
    Brilliance      6 to 20 kHz     14000   350       64

    // http://www.phy.mtu.edu/~suits/notefreqs.html
    The basic formula for the frequencies of the notes of the equal tempered scale is given by
    fn = f0 * (a)n
    where
    f0 = the frequency of one fixed note which must be defined. A common choice is setting the A above middle C (A4) at f0 = 440 Hz.
    n = the number of half steps away from the fixed note you are. If you are at a higher note, n is positive. If you are on a lower note, n is negative.
    fn = the frequency of the note n half steps away.
    a = (2)^(1/12) = the twelth root of 2 = the number which when multiplied by itself 12 times equals 2 = 1.059463094359...
   */
  switch(numOutputs) {
    // TODO! TUNE THIS AND FILL OUT THE REST
    case 1:
      // everything averaged together
      outputBins[0] = (6000 / 43);
      break;
    case 2:
      outputBins[0] = (440 / 43);  // everything below 440 Hz
      outputBins[1] = (6000 / 43);  // everything below 6 kHz
      break;
    case 6:
      // TODO! TUNE THIS.
      outputBins[0] = 6;  // round(250 / 43)
      outputBins[1] = 12;  // round(500 / 43)
      outputBins[2] = 47;  // round(2000 / 43)
      outputBins[3] = 79;  // round(3000 / 43)
      outputBins[4] = 93;  // round(4000 / 43)
      outputBins[5] = 128;  // round(5500 / 43)
      break;
    // todo: maybe a default case that just does a simple formula with bigger buckets at the end
  }

  // blink the new number of outputs on all the wires
  // todo: do this in morse code?
  for (int i = 0; i < numOutputs; i++) {
    // turn all the wires on
    for (int j = 0; j < numOutputs; j++) {
      digitalWrite(outputPins[j], HIGH);
    }
    // wait
    delay(morseDitMs);
    // turn all the wires off
    for (int j=0; j<numOutputs; j++) {
      digitalWrite(outputPins[j], LOW);
    }
    // wait
    delay(morseElementSpaceMs);
  }
  // wait
  delay(morseWordSpaceMs);
}


void updateOutputStatesFromFFT() {
  // parse FFT data
  // The 1024 point FFT always updates at approximately 86 times per second.
  if (myFFT.available()) {
    int numOutputBins, nextOutputStartBin;
    int outputStartBin = FFT_IGNORED_BINS;  // ignore the first FFT_IGNORED_BINS bins as they can be far too noisy

    // set the overall sensitivity based on how loud the previous inputs were
    float inputSensitivity = lastLoudestLevel * MINIMUM_INPUT_RANGE;
    if (inputSensitivity < minInputSensitivity) {
      inputSensitivity = minInputSensitivity;
    }

    // now that we've used lastLoudestLevel, reset it to 0 so we can get the latest.
    lastLoudestLevel = 0;

    // DEBUGGING
    Serial.print("FFT >");
    Serial.print(inputSensitivity, 3);
    Serial.print(" across ");
    Serial.print(numOutputs);
    Serial.print(" outputs (");
    Serial.print(avgInputLevel[MAX_OUTPUTS]);
    Serial.print(" avg): | ");
    // END DEBUGGING

    float inputLevelAccumulator = 0;
    for (int outputId = 0; outputId < numOutputs; outputId++) {
      nextOutputStartBin = outputBins[outputId] + 1;
      numOutputBins = nextOutputStartBin - outputStartBin;
      if (numOutputBins < 1) {
        numOutputBins = 1;
      }

      // .read(x, y) gives us a sum of bins x through y so we divide to keep the average
      float inputLevel = myFFT.read(outputStartBin, nextOutputStartBin - 1) / (float)numOutputBins;

      // record exponential moving average of the inputLevel
      avgInputLevel[outputId] += EMA_ALPHA * (inputLevel - avgInputLevel[outputId]);

      // go numb to sounds that are louder than the average inputLevel for this outputId
      if (avgInputLevel[outputId] > inputSensitivity) {
        inputLevel -= avgInputLevel[outputId] * 1;    // todo: tune this

        // but don't go negative.
        if (inputLevel < 0) {
          inputLevel = 0;
        }
      }

      // save our loudest (modified) level
      if (inputLevel > lastLoudestLevel) {
        lastLoudestLevel = inputLevel;
      }

      // keep track of the sum of our modified levels so we can calculate an average later
      inputLevelAccumulator += inputLevel;

      /*
      // DEBUGGING
      Serial.println();
      Serial.print("Reading ");
      Serial.print(outputStartBin);
      Serial.print(" to ");
      Serial.print(nextOutputStartBin - 1);
      Serial.print(" (");
      Serial.print(numOutputBins);
      Serial.println(")");
      // END DEBUGGING
      */

      // turn the light on if inputLevel > inputSensitivity and this isn't a solitary sound. off otherwise
      if (inputLevel > inputSensitivity) {
        /*
        // todo: this keeps skipping actual music. figure out a better way to do this
        if (avgInputLevel[MAX_OUTPUTS] < inputSensitivity) {    // todo: tune this
          // we have a loud sound, but the other outputs are quiet, so ignore it
          Serial.print("XXX");  // input ignored
        } else {
        */
        outputStates[outputId] = HIGH;

        // save the time this output turned on. morse code waits for this to be old
        lastOnMs = nowMs;

        // save the time we turned on unless we are in the front X% of the minimumOnMs window of a previous on
        if (nowMs - lastOnMsArray[outputId] > minimumOnMs * 0.80) {   // todo: tune this
          // todo: instead of setting to the true time, set to some average of the old time and the new?
          Serial.print(inputLevel, 3);
          lastOnMsArray[outputId] = lastOnMs;
        } else {
          lastOnMsArray[outputId] += 25;    // todo: tune this. add a variable amount based on how far into the window we are?
          Serial.print(" -   ");
        }
      } else {
        if (nowMs - lastOnMsArray[outputId] > minimumOnMs) {
          // the output has been on for at least minimumOnMs. turn it off
          // don't actually turn off here because that might break the morse code
          outputStates[outputId] = LOW;
          Serial.print("     ");   // we turned the output off. show blank space
        } else {
          Serial.print(" .   ");   // we left the output on
        }
      }
      Serial.print(" | ");

      // prepare for the next iteration
      outputStartBin = nextOutputStartBin;
    }
    Serial.println();

    avgInputLevel[MAX_OUTPUTS] += EMA_ALPHA * (inputLevelAccumulator - avgInputLevel[MAX_OUTPUTS]);   // todo: not sure about this
  }
}


bool blinkMorseCode() {
  // blink all of the wires according to morseArray

  bool morseCommandFound = false;
  unsigned long checkMs = MAX_OFF_MS;
  for (int morseCommandId = 0; morseCommandId < morseArrayLength; morseCommandId++) {
    TimedAction morseCommand = morseArray[morseCommandId];
    checkMs += morseCommand.checkMs;
    if (nowMs < checkMs) {
      if (lastMorseCommandId != morseCommandId) {
        // this is the first time we've seen this command this iteration
        Serial.print("AudioMemoryUsageMax: ");
        Serial.println(AudioMemoryUsageMax());

        // save that we've done this command already
        lastMorseCommandId = morseCommandId;
        // DEBUGGING
        /*
        Serial.print(nowMs);
        Serial.print(" < ");
        Serial.print(checkMs);
        Serial.print(" - Output: ");
        Serial.print(morseOutputId);
        Serial.print(" - Action #");
        Serial.print(morseCommandId);
        Serial.print(": ");
        Serial.print(morseCommand.outputAction);
        Serial.print(" for ");
        Serial.print(morseCommand.checkMs);
        Serial.println();
        */
        // END DEBUGGING

        for (int outputId = 0; outputId < numOutputs; outputId++) {
          digitalWrite(outputId, morseCommand.outputAction);
        }
      }

      morseCommandFound = true;
      break;
    }
  }

  // return true when we are done playing the message
  return not morseCommandFound;
}

bool morseCommandCompleted = not (bool)morseArrayLength;

void loop() {
  updateNumOutputs();

  updateOutputStatesFromFFT();

  // todo: check our buttons to see if we should update minInputSensitivity

  if (nowMs - lastOnMs < MAX_OFF_MS) {
    // we turned a light on recently. send output states to the wires
    for (int i = 0; i < numOutputs; i++) {
      digitalWrite(outputPins[i], outputStates[i]);
    }
    morseCommandCompleted = false;
  } else {
    // no lights have been turned on for at least MAX_OFF_MS
    // blink more code
    // todo: play the message twice?
    if (not morseCommandCompleted) {
      morseCommandCompleted = blinkMorseCode();
    }
  }
}
