// control for Bryan's Jacket with a configurable number of EL wires
//
// when it is quiet, lights flash out morse code
// when it is loud, lights flash with the music
//

/*
 * you can easily customize these
 *
 * todo: define or const?
 */
#define BUTTON_HOLD_MS 200
#define BUTTON_INTERVAL 20
#define INPUT_NUM_OUTPUTS_KNOB 15
#define EMA_ALPHA 0.2
#define FFT_IGNORED_BINS 1  // skip the first FFT_IGNORED_BINS * 43 Hz
#define MAX_OFF_MS 10000
#define MAX_MORSE_ARRAY_LENGTH 256  // todo: tune this
#define MINIMUM_INPUT_SENSITIVITY 0.030  // todo: make this a button that sets this to whatever the current volume is?
#define MINIMUM_INPUT_RANGE 0.50  // activate outputs on sounds that are at least this % as loud as the loudest sound
#define DEFAULT_MORSE_STRING "HELLO WORLD"  // what to blink if no morse.txt on the SD card
#define DEBUG true    // todo: write a debug_print that uses this to print to serial

const int minimumOnMs = 200;

// max of 512 * 43 Hz == 22k Hz
// 16k Hz = 372
// 10k Hz = 233
// 5k Hz = 116
/*
 *

Sub-bass       20 to  60 Hz
Bass           60 to 250 Hz
Low midrange  250 to 500 Hz
Midrange      500 Hz to 2 kHz
Upper midrange  2 to 4 kHz
Presence        4 to 6 kHz
Brilliance      6 to 20 kHz

 */

const int numFftBinsWeCareAbout = 233 - FFT_IGNORED_BINS;
// TODO! we shouldn't cut the spectrum into even chunks. the higher frequencies should be combined

// TODO: tune these to look pretty
const int morseDitMs = 400;
const int morseDahMs = morseDitMs * 3;
const int morseElementSpaceMs = morseDahMs * 1.5;
const int morseWordSpaceMs = morseElementSpaceMs * 3;
/*
 * END you can easily customize these
 */

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

AudioInputI2S            audioInput;
AudioOutputI2S           audioOutput; // even though we don't output to this, it still needs to be defined for the shield to work
AudioAnalyzeFFT1024      myFFT;
AudioConnection          patchCord1(audioInput, audioOutput);
AudioConnection          patchCord2(audioInput, myFFT);
AudioControlSGTL5000     audioShield;

// save on/off/off+next states. if current time < runningSumOfCheckMs, than do action. else check next time
struct TimedAction {
  unsigned int checkMs;
  unsigned int outputAction;
};
int morse_array_length = MAX_MORSE_ARRAY_LENGTH;
TimedAction morse_array[MAX_MORSE_ARRAY_LENGTH];


/*
   HELPER FUNCTIONS
*/

int morseDit(TimedAction morse_array[], int i) {
  Serial.print('.');
  // todo: protect overflowing morse_array
  morse_array[i].checkMs = morseDitMs;
  morse_array[i].outputAction = HIGH;
  i++;
  return morse_element_space(morse_array, i);
}

int morseDah(TimedAction morse_array[], int i) {
  Serial.print('-');
  // todo: protect overflowing morse_array
  morse_array[i].checkMs = morseDahMs;
  morse_array[i].outputAction = HIGH;
  i++;
  return morse_element_space(morse_array, i);
}

int morse_element_space(TimedAction morse_array[], int i) {
  // todo: protect overflowing morse_array
  morse_array[i].checkMs = morseElementSpaceMs;
  morse_array[i].outputAction = LOW;
  i++;
  return i;
}

int morse_word_space(TimedAction morse_array[], int i) {
  Serial.print(" / ");
  // todo: protect overflowing morse_array
  morse_array[i].checkMs = morseWordSpaceMs;
  morse_array[i].outputAction = LOW;
  i++;
  return i;
}

/*
TimedAction[] string2morse(String str) {
  // todo: move all the morse stuff in setup up to here
}
*/

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
  }
  // fallback to our default string if nothing loaded
  if (morseString == "") {
    Serial.println("No morse.txt...");
    morseString = DEFAULT_MORSE_STRING; // todo: PROGMEM for this?
  }

  Serial.print("morseString: ");
  Serial.println(morseString);

  int morse_array_length = 0;
  for (unsigned int i=0; i<morseString.length(); i++) {
    // todo: make sure morse_array_length never exceeds max
    // https://upload.wikimedia.org/wikipedia/en/5/5a/Morse_comparison.svg
    // todo: make everything upcase?
    // todo: put this in its own library?
    switch (morseString[i]) {
      case 'E':
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'T':
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'I':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'A':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'N':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'M':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'S':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'U':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'R':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'W':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'D':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'K':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'G':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'O':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'H':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'V':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'F':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case ' ':
        morse_array_length = morse_word_space(morse_array, morse_array_length);
        break;
      case 'L':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'P':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'J':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'B':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'X':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'C':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'Y':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'Z':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'Q':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case '5':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case '4':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case '3':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case '2':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case '1':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case '6':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case '7':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case '8':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case '9':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case '0':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case '.':
        // todo: do this on a different wire?
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
      default:
        break;
    }

    Serial.print(' ');
    morse_array_length = morse_element_space(morse_array, morse_array_length);
  }
  Serial.println();

  Serial.print("morse array length: ");
  Serial.println(morse_array_length);

  audioShield.unmuteHeadphone();  // for debugging

  // todo: is this enough?
  randomSeed(analogRead(0));

  Serial.println("Starting...");
}


elapsedMillis nowMs = 0;

int blinkOutput = OUTPUT_A;
int numOutputs = 1;

// Exponential Moving Average of each wire individually and then all the wires together
float avgInputLevel[MAX_OUTPUTS + 1];
float lastLoudestLevel = 0;

unsigned long lastOnMs;
unsigned long lastOnMsArray[MAX_OUTPUTS];
int outputStates[MAX_OUTPUTS];

int lastMorseId[MAX_OUTPUTS];   // todo: fill this with -1 on start?


void updateNumOutputs() {
  // configure number of outputs
  int oldNumOutputs = numOutputs;
  numOutputs = (int)analogRead(INPUT_NUM_OUTPUTS_KNOB) / (1024 / MAX_OUTPUTS) + 1;

  if (oldNumOutputs == numOutputs) {
    return;
  }
  // we changed numOutputs

  // turn all the wires off
  for (int i=0; i<MAX_OUTPUTS; i++) {
    digitalWrite(outputPins[i], LOW);
  }

  Serial.print("New numOutputs: ");
  Serial.println(numOutputs);

  // blink the new number of outputs on all the wires
  // todo: do this in morse code?
  for (int i=0; i<numOutputs; i++) {
    // turn all the wires on
    for (int j=0; j<numOutputs; j++) {
      digitalWrite(outputPins[j], HIGH);
    }
    // wait
    delay(minimumOnMs);
    // turn all the wires off
    for (int j=0; j<numOutputs; j++) {
      digitalWrite(outputPins[j], LOW);
    }
    // wait
    delay(minimumOnMs);
  }
  // wait
  delay(minimumOnMs * 2);
}



void loop() {
  updateNumOutputs();

  // parse FFT data
  // The 1024 point FFT always updates at approximately 86 times per second.
  if (myFFT.available()) {
    int numOutputBins, nextOutputStartBin, outputId = 0;
    int outputStartBin = FFT_IGNORED_BINS;  // ignore the first FFT_IGNORED_BINS bins as they can be far too noisy. maybe the eq/filters can help tho?

    int expectedBinsPerOutput = numFftBinsWeCareAbout / numOutputs;

    // set the overall sensitivity based on how loud the previous inputs were
    float inputSensitivity = lastLoudestLevel * MINIMUM_INPUT_RANGE;
    if (inputSensitivity < MINIMUM_INPUT_SENSITIVITY) {
      inputSensitivity = MINIMUM_INPUT_SENSITIVITY;
    }
    lastLoudestLevel = 0;

    // DEBUGGING
    Serial.print(numFftBinsWeCareAbout);
    Serial.print(" FFT bins >");
    Serial.print(inputSensitivity, 3);
    Serial.print(" across ");
    Serial.print(numOutputs);
    Serial.print(" outputs (");
    Serial.print(avgInputLevel[MAX_OUTPUTS]);
    Serial.print(" avg): | ");
    // END DEBUGGING

    float inputLevelAccumulator = 0;
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
      float inputLevel = myFFT.read(outputStartBin, nextOutputStartBin - 1) / (float)numOutputBins;

      // take an average of the true input
      avgInputLevel[outputId] += EMA_ALPHA * (inputLevel - avgInputLevel[outputId]);

      // go numb to sounds that are louder than the average
      if (avgInputLevel[outputId] > inputSensitivity) {
        inputLevel -= avgInputLevel[outputId] * 1.0;    // todo: tune this

        // todo: not sure about this
        if (inputLevel < 0) {
          inputLevel = 0;
        }
      }

      // save our loudest (modified) sound
      if (inputLevel > lastLoudestLevel) {
        lastLoudestLevel = inputLevel;
      }

      // keep track of the sum of our outputs so we can calculate an average later
      // todo: not sure if excluding negatives is going to be better
      inputLevelAccumulator += inputLevel;

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
      // END DEBUGGING

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
          Serial.print(" 0 ");
          lastOnMsArray[outputId] = lastOnMs;
        } else {
          lastOnMsArray[outputId] += 25;    // todo: tune this. add a variable amount based on how far into the window we are?
          Serial.print(" - ");
        }
      } else {
        if (nowMs - lastOnMsArray[outputId] > minimumOnMs) {
          // the output has been on for at least minimumOnMs. turn it off
          // don't actually turn off here because that might break the morse code
          outputStates[outputId] = LOW;
          Serial.print("   ");   // we turned the output off. show blank space
        } else {
          Serial.print(" . ");   // we left the output on
        }
      }
      Serial.print(" | ");

      outputStartBin = nextOutputStartBin;
      outputId += 1;
    }
    Serial.println();

    avgInputLevel[MAX_OUTPUTS] += EMA_ALPHA * (inputLevelAccumulator - avgInputLevel[MAX_OUTPUTS]);   // todo: not sure about this
  }

  if (nowMs - lastOnMs < MAX_OFF_MS) {
    // we turned a light on recently. send output states to the wires
    for (int i=0; i<numOutputs; i++) {
      digitalWrite(outputPins[i], outputStates[i]);
    }
  } else {
    // no lights have been turned on for at least MAX_OFF_MS

    bool morse_command_found = false;

    for (int morseOutputId = 0; morseOutputId < numOutputs; morseOutputId++) {
      unsigned long checkMs = MAX_OFF_MS;
      unsigned long offset = morseOutputId * 50;  // todo: tune this
      if (offset > MAX_OFF_MS) {
        checkMs = 0;
      } else {
        checkMs = MAX_OFF_MS - offset;
      }
      for (int i = 0; i < morse_array_length; i++) {
        TimedAction morse_command = morse_array[i];
        checkMs += morse_command.checkMs;
        if (nowMs < checkMs) {
          bool firstRun = (lastMorseId[morseOutputId] != i);
          if (firstRun) {
            if (morseOutputId == 0) {
              Serial.print("AudioMemoryUsageMax: ");
              Serial.println(AudioMemoryUsageMax());
            }
            lastMorseId[morseOutputId] = i;
            // DEBUGGING
            /*
            Serial.print(nowMs);
            Serial.print(" < ");
            Serial.print(checkMs);
            Serial.print(" - Output: ");
            Serial.print(morseOutputId);
            Serial.print(" - Action #");
            Serial.print(i);
            Serial.print(": ");
            Serial.print(morse_command.outputAction);
            Serial.print(" for ");
            Serial.print(morse_command.checkMs);
            Serial.println();
            */
            // END DEBUGGING

            digitalWrite(morseOutputId, (bool)morse_command.outputAction);
          }

          morse_command_found = true;
          break;
        }
      }
    }

    if (not morse_command_found) {
      // we hit the end of the loop
      // reset time so we start at the beginning of the message
      Serial.println("Loop morse code message...");
      lastOnMs = 0;
      nowMs = MAX_OFF_MS;

      /*
      // reset the lastOnMsArray loop. todo: we might not need this
      for (int i = 0; i < MAX_OUTPUTS; i++) {
        lastOnMsArray[i] = 0;
      }
      */
    }
  }
}
