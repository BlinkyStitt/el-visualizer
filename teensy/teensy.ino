/* control for the Teensy 3.2 in Bryan's Sound-reactive EL Jacket
 *
 * when it is quiet, lights flash out morse code from an SD card
 * when it is loud, lights flash with the music
 *
 * TODO: analog button press to set minInputSensitivity
 *
 */

/*
 * you can easily customize these by creating a file with the name of the variable on the SD card
 *
 * TODO! actually read them from the SD card
 * todo: what units are the read values from the FFT in?
 *
 */
bool  debug = true;    // todo: write a debug_print that uses this to print to serial

unsigned long randomizeOutputMs = 1000 * 60 * 5;  // if 0, don't randomize the outputs

uint  fftIgnoredBins = 1;  // skip the first fftIgnoredBins * FFT_HZ_PER_BIN Hz

unsigned long maxOffMs = 7000;  // how long to be off before blinking morse cod

float minInputRange = 0.80;  // activate outputs on sounds that are at least this % as loud as the loudest sound

float minInputSensitivity = 0.060;  // the quietest sound that will blink the lights. 1.0 represents a full scale sine wave
float minInputSensitivityEMAAlpha = 0.95;  // alpha for calculating how fast to adjust sensitivity based on the loudest sound

uint  minOnMs = 118;  // the shortest amount of time to leave an output on

uint  numOutputs = 6;   // this will be updated by a file on the SD card

float numbEMAAlpha = 0.005;  // alpha for calculating background sound to ignore. do how should we do this?
float numbPercent = 0.75;  // how much of the average level to subtract from the input. TODO! TUNE THIS

float audioShieldVolume = 0.5;  // Set the headphone volume level. Range is 0 to 1.0, but 0.8 corresponds to the maximum undistorted output for a full scale signal. Usually 0.5 is a comfortable listening level
uint  audioShieldMicGain = 63;  // decibels

// how long to blink for morse code
uint  morseDitMs = 250;
uint  morseDahMs = morseDitMs * 3;
uint  morseElementSpaceMs = morseDitMs;
uint  morseWordSpaceMs = morseDitMs * 7;

/*
 * END you can easily customize these
 */

#define BETWEEN(value, min, max) (value < max && value > min)
#define FFT_HZ_PER_BIN 43
#define MAX_FFT_BINS 512
#define MAX_OUTPUTS 8  // EL Sequencer only has 8 wires, but you could easily make this larger and drive other things
#define MAX_PATTERN_ARRAY_LENGTH 512  // todo: tune this
#define OUTPUT_PIN_A 0
#define OUTPUT_PIN_B 1
#define OUTPUT_PIN_C 2
#define OUTPUT_PIN_D 3
#define OUTPUT_PIN_E 4
#define OUTPUT_PIN_F 5
// pins 6, 7, 9, 10, 18, 19, 11, 12, 13, 14, 15, 22, 23 are used by the audio shield
#define OUTPUT_PIN_G 8
#define INPUT_PIN_VOLUME 15
#define OUTPUT_PIN_H 20
const int outputPins[MAX_OUTPUTS] = {OUTPUT_PIN_A, OUTPUT_PIN_B, OUTPUT_PIN_C, OUTPUT_PIN_D, OUTPUT_PIN_E, OUTPUT_PIN_F, OUTPUT_PIN_G, OUTPUT_PIN_H};
int outputBins[MAX_OUTPUTS];

#include <Audio.h>
#include <elapsedMillis.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

AudioInputI2S        audioInput;
AudioOutputI2S       audioOutput; // even though we don't output to this, it still needs to be defined for the shield to work
AudioAnalyzeFFT1024  myFFT;
AudioConnection      patchCord1(audioInput, audioOutput);
AudioConnection      patchCord2(audioInput, myFFT);
AudioControlSGTL5000 audioShield;


elapsedMillis elapsedMsForLastOutput = 0;    // todo: do we care if this overflows?
elapsedMillis elapsedMsForRandomization = 0;

uint          randomizedOutputIds[MAX_OUTPUTS];

float         lastLoudestLevel = minOnMs;

unsigned long lastOnMs;
unsigned long lastOnMsArray[MAX_OUTPUTS];
bool          outputStates[MAX_OUTPUTS];

float         avgInputLevel[MAX_FFT_BINS];

uint          lastPatternActionId = 9;  // start this at non-zero

unsigned long lastUpdate = 0;
float         inputSensitivity = minInputSensitivity;

bool          patternCompleted = false;

// if current time < runningSumOfCheckMs, than turn the output on/off based on the action. else check next time
struct TimedAction {
  unsigned long checkMs;
  bool outputActions[MAX_OUTPUTS];    // do we need more than just HIGH/LOW?
};
uint        patternArrayLength = 0;
TimedAction patternArray[MAX_PATTERN_ARRAY_LENGTH];


// todo: this morse stuff should probably be in its own class, but it works
int morseDit(TimedAction patternArray[], int i) {
  if (i + 1 >= MAX_PATTERN_ARRAY_LENGTH) {
    Serial.println("Pattern too long!");
    return i;
  }

  Serial.print('.');
  patternArray[i].checkMs = morseDitMs;
  for (uint j = 0; j < MAX_OUTPUTS; j++) {
    // everything the same is boring. figure out something prettier
    patternArray[i].outputActions[j] = HIGH;
  }
  i++;
  return morse_element_space(patternArray, i);
}

int morseDah(TimedAction patternArray[], int i) {
  if (i + 1 >= MAX_PATTERN_ARRAY_LENGTH) {
    Serial.println("Pattern too long!");
    return i;
  }

  Serial.print('-');
  // todo: protect overflowing patternArray
  patternArray[i].checkMs = morseDahMs;
  for (uint j = 0; j < MAX_OUTPUTS; j++) {
    // everything the same is boring. figure out something prettier
    patternArray[i].outputActions[j] = HIGH;
  }
  i++;
  return morse_element_space(patternArray, i);
}

int morse_element_space(TimedAction patternArray[], int i) {
  if (i + 1 >= MAX_PATTERN_ARRAY_LENGTH) {
    Serial.println("Pattern too long!");
    return i;
  }

  // todo: protect overflowing patternArray
  patternArray[i].checkMs = morseElementSpaceMs;
  for (uint j = 0; j < MAX_OUTPUTS; j++) {
    // everything the same is boring. figure out something prettier
    patternArray[i].outputActions[j] = LOW;
  }
  i++;
  return i;
}

int morse_word_space(TimedAction patternArray[], int i) {
  if (i + 1 >= MAX_PATTERN_ARRAY_LENGTH) {
    Serial.println("Pattern too long!");
    return i;
  }

  Serial.print(" / ");
  // todo: protect overflowing patternArray
  patternArray[i].checkMs = morseWordSpaceMs;
  for (uint j = 0; j < MAX_OUTPUTS; j++) {
    // everything the same is boring. figure out something prettier
    // todo: should this also randomize the outputs?
    patternArray[i].outputActions[j] = LOW;
  }
  i++;
  return i;
}

void filename2morse(const char* filename, TimedAction patternArray[], uint& patternArrayLength) {
  // todo: put this in its own library?

  // todo: should we just read the file into chars instead of building a string first?
  String morseString = filename2string(filename);

  Serial.print("morseString: ");
  Serial.println(morseString);

  for (uint i = 0; i < morseString.length(); i++) {
    // https://upload.wikimedia.org/wikipedia/en/5/5a/Morse_comparison.svg
    // todo: make everything uppercase?
    switch (morseString[i]) {
      case 'E':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case 'T':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case 'I':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case 'A':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case 'N':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case 'M':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case 'S':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case 'U':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case 'R':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case 'W':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case 'D':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case 'K':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case 'G':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case 'O':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case 'H':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case 'V':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case 'F':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case ' ':
        patternArrayLength = morse_word_space(patternArray, patternArrayLength);
        break;
      case 'L':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case 'P':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case 'J':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case 'B':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case 'X':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case 'C':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case 'Y':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case 'Z':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case 'Q':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case '5':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case '4':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case '3':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case '2':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case '1':
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case '6':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case '7':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case '8':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case '9':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        break;
      case '0':
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        break;
      case '.':
        // todo: do this on a different wire?
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
        patternArrayLength = morseDit(patternArray, patternArrayLength);
        patternArrayLength = morseDah(patternArray, patternArrayLength);
      default:
        break;
    }

    Serial.print(' ');
    patternArrayLength = morse_element_space(patternArray, patternArrayLength);
  }
  Serial.println();

  Serial.print("patternArrayLength: ");
  Serial.println(patternArrayLength);
}

void filename2pattern(const char* filename, TimedAction patternArray[], uint& patternArrayLength) {
  Serial.println("TODO: READ 'pattern' off the SD card and turn it into patternArray");
}

// from http://forum.arduino.cc/index.php?topic=43424.0
// generate a value between 0 <= x < n, thus there are n possible outputs
int rand_range(int n) {
  int r, ul;
  ul = RAND_MAX - RAND_MAX % n;
  while ((r = rand()) >= ul) {
    ;  // get a random number that is fair
  }
  return r % n;
}

// from http://forum.arduino.cc/index.php?topic=43424.0
void bubbleUnsort(uint *list, uint elem) {
  Serial.println("!!! Randomizing Outputs !!!");
  for (int a = elem - 1; a > 0; a--) {
    // todo: not sure what is better. apparently the built in random has some sort of bias?
    //int r = random(a+1);
    int r = rand_range(a + 1);
    if (r != a) {
      /*
      int temp = list[a];
      list[a] = list[r];
      list[r] = temp;
      */
      // https://betterexplained.com/articles/swap-two-variables-using-xor/
      list[a] = list[a] xor list[r];
      list[r] = list[a] xor list[r];
      list[a] = list[a] xor list[r];
    }
  }
}

String filename2string(const char* filename) {
  // read the first line of a file and return it as a String
  // todo: a String might be heavier than we need. probably should just use char*

  File file = SD.open(filename, FILE_READ);

  delay(20);  // debugging

  String str = "";

  // if the file is available, read it:
  if (file) {
    Serial.print("Reading ");
    Serial.print(filename);
    Serial.println("...");

    while (file.available()) {
      int inChar = file.read();
      if (inChar == '\n') {
        break;
      }
      str += (char)inChar;
    }
    file.close();

    return str;
  }

  Serial.print("Unable to read ");
  Serial.print(filename);
  Serial.println("...");
  return "";
}

void updateConfigBool(const char* filename, bool& config_ref) {
  // this could by DRYer

  String override = filename2string(filename);
  if (override == "") {
    Serial.print("Default for ");
  } else {
    Serial.print("Override for ");
    config_ref = override == "true";
  }

  Serial.print(filename);
  Serial.print(" (bool): ");
  Serial.println(config_ref);
}

void updateConfigUint(const char* filename, uint& config_ref) {
  // this could by DRYer

  String override = filename2string(filename);
  if (override == "") {
    Serial.print("Default for ");
  } else {
    Serial.print("Override for ");
    // toInt actually returns a long...
    config_ref = (uint)override.toInt();
  }

  Serial.print(filename);
  Serial.print(" (uint): ");
  Serial.println(config_ref);
}

void updateConfigFloat(const char* filename, float& config_ref) {
  // this could by DRYer

  String override = filename2string(filename);
  if (override == "") {
    Serial.print("Default for ");
  } else {
    Serial.print("Override for ");
    // toInt actually returns a long...
    config_ref = override.toFloat();
  }

  Serial.print(filename);
  Serial.print(" (float): ");
  Serial.println(config_ref);
}

void updateConfigUlong(const char* filename, unsigned long& config_ref) {
  // this could by DRYer

  String override = filename2string(filename);
  if (override == "") {
    Serial.print("Default for ");
  } else {
    Serial.print("Override for ");
    // toInt actually returns a long...
    config_ref = (unsigned long)override.toInt();
  }

  Serial.print(filename);
  Serial.print(" (ulong): ");
  Serial.println(config_ref);
}

void updateNumOutputs(uint& numOutputs) {
  Serial.print("numOutputs: ");
  Serial.println(numOutputs);

  for (int i = 0; i < MAX_OUTPUTS; i++) {
    // turn all the wires off
    digitalWrite(outputPins[i], LOW);

    // put the randomized inputs back in order in case we lowered numOutputs
    randomizedOutputIds[i] = i;
  }

  if (randomizeOutputMs) {
    // shuffle the outputs
    bubbleUnsort(randomizedOutputIds, numOutputs);
    // do it twice since we started off ordered
    bubbleUnsort(randomizedOutputIds, numOutputs);
  }

  // https://en.wikipedia.org/wiki/Piano_key_frequencies
  // TODO! TUNE THESE
  // todo: read this from the sd card, too?
  switch(numOutputs) {
    case 1:
      outputBins[0] = 82;  // todo: tune this
      break;
    case 2:
      outputBins[0] = 10;  // everything below 440 Hz
      outputBins[1] = 82;  // everything below 6 kHz
      break;
    case 3:
      outputBins[0] = 6;   // todo: tune this
      outputBins[1] = 10;  // todo: tune this
      outputBins[2] = 82;  // todo: tune this
      break;
    case 4:
      outputBins[0] = 3;   // todo: tune this
      outputBins[1] = 6;   // todo: tune this
      outputBins[2] = 10;  // todo: tune this
      outputBins[3] = 186;  // todo: tune this
      break;
    case 5:
      outputBins[0] = 6;   // todo: tune this
      outputBins[1] = 10;   // todo: tune this
      outputBins[2] = 20;  // todo: tune this
      outputBins[3] = 41;  // todo: tune this
      outputBins[4] = 186;  // todo: tune this
      break;
    case 6:
      outputBins[0] = 3;   // 110
      outputBins[1] = 6;   // 220
      outputBins[2] = 10;  // 440
      outputBins[3] = 20;  // 880
      outputBins[4] = 41;  // 1763
      outputBins[5] = 186;  // 7998 todo: tune this
      break;
    case 7:
      outputBins[0] = 3;   // 110
      outputBins[1] = 6;   // 220
      outputBins[2] = 10;  // 440
      outputBins[3] = 20;  // 880
      outputBins[4] = 41;  // 1760
      outputBins[5] = 82;  // 3520
      outputBins[6] = 186;  // todo: tune this
      break;
    case 8:
      outputBins[0] = 3;   // 110
      outputBins[1] = 6;   // 220
      outputBins[2] = 10;  // 440
      outputBins[3] = 20;  // 880
      outputBins[4] = 41;  // 1760
      outputBins[5] = 82;  // 3520
      outputBins[6] = 164;  // todo: tune this
      outputBins[7] = 186;  // todo: tune this
      break;
  }

  // blink the new number of outputs on all the outputs
  Serial.println("Blinking numOutputs...");

  for (uint blinkCount = 0; blinkCount < numOutputs; blinkCount++) {
    // turn all the outputs on
    for (uint outputId = 0; outputId < numOutputs; outputId++) {
      digitalWrite(outputPins[outputId], HIGH);
    }
    delay(morseDitMs);

    // turn all the outputs off
    for (uint outputId = 0; outputId < numOutputs; outputId++) {
      digitalWrite(outputPins[outputId], LOW);
    }
    delay(morseElementSpaceMs);
  }
  delay(morseWordSpaceMs);

  if (randomizeOutputMs) {
    // turn the lights on in the randomized order
    Serial.println("Showing random...");
    // todo: do this after flashing the number
    // randomizedOutputIds is actually sorted if randomizeOutputMs is not set
    for (uint i = 0; i < numOutputs; i++) {
      digitalWrite(outputPins[randomizedOutputIds[i]], HIGH);
      delay(morseDahMs);
    }
    delay(morseElementSpaceMs);

    // turn all the outputs off
    for (uint i = 0; i < numOutputs; i++) {
      digitalWrite(outputPins[i], LOW);
    }
    delay(morseElementSpaceMs);
  }
}

void updateOutputStatesFromFFT() {
  // parse FFT data
  // The 1024 point FFT always updates at approximately 86 times per second (every 11-12ms).
  if (myFFT.available()) {
    /*

    psueodocode for improved(?) process

    for wire_bins in wires_bins:
        for bin_level in wire_bins:
            subtract some % of a long average from the bin_level to go numb to constant noise

            save the loudest bin across each wire and all wires

    set outputSensitivity based on some EMA of the loudest wire * SOME_PERCENT

    for loudest_bin_level in loudest_wire_bins:
        if loudest_bin_level < outputSensitivity:
            turn the wire off
        elif loudest_bin_level * SOME_PERCENT > previous_loudest_bin_level:
            turn wire on
        elif loudest_bin_level > previous_loudest_bin_level * SOME_PERCENT:
            keep state
        else:
            turn the wire off
    */

    uint numOutputBins, nextOutputStartBin;
    uint outputStartBin = fftIgnoredBins;  // ignore the first fftIgnoredBins bins as they can be far too noisy

    // set the overall sensitivity based on how loud the previous inputs were
    // todo: i think this should be an exponential moving average
    // TODO! Do this for the current data instead of using last data
    inputSensitivity += minInputSensitivityEMAAlpha * (max(lastLoudestLevel * minInputRange, minInputSensitivity) - inputSensitivity);
    if (inputSensitivity < minInputSensitivity) {
      inputSensitivity = minInputSensitivity;
    } else if (inputSensitivity > 1) {
      inputSensitivity = 1;
    }

    // now that we've used lastLoudestLevel, reset it to 0 so we can get the latest.
    lastLoudestLevel = 0;

    // DEBUGGING
    Serial.print("FFT >");
    Serial.print(inputSensitivity, 3);
    Serial.print(" across ");
    Serial.print(numOutputs);
    Serial.print(" outputs: | ");
    // END DEBUGGING

    float inputLevelAccumulator = 0;
    for (uint outputId = 0; outputId < numOutputs; outputId++) {
      nextOutputStartBin = outputBins[outputId] + 1;
      numOutputBins = nextOutputStartBin - outputStartBin;
      if (numOutputBins < 1) {
        numOutputBins = 1;
      }

      // find the loudest bin in this outputs frequency range
      float inputLevel = 0;
      for (uint binId = outputStartBin; binId < nextOutputStartBin; binId++) {
        float binInputLevel = myFFT.read(binId);

        // go numb to sounds that are constantly loud. todo: this probably needs some tuning
        avgInputLevel[binId] += numbEMAAlpha * (binInputLevel - avgInputLevel[binId]);
        binInputLevel -= avgInputLevel[binId] * numbPercent;

        if (binInputLevel > inputLevel) {
          inputLevel = binInputLevel;
        }
      }

      // do this by keeping a long moving average

      // save our loudest (modified) level
      if (inputLevel > lastLoudestLevel) {
        lastLoudestLevel = inputLevel;
      }

      // keep track of the sum of our modified levels so we can calculate an average later
      inputLevelAccumulator += inputLevel;

      // turn the light on if inputLevel > inputSensitivity
      if (inputLevel > inputSensitivity) {
        outputStates[outputId] = HIGH;

        // save the time this output turned on. morse code waits for this to be old
        lastOnMs = elapsedMsForLastOutput;

        // save the time we turned on unless we are in the front X% of the minOnMs window of a previous on
        if (elapsedMsForLastOutput - lastOnMsArray[outputId] > minOnMs * 0.80) {   // todo: tune this
          // todo: instead of setting to the true time, set to some average of the old time and the new?
          lastOnMsArray[outputId] = lastOnMs;
          Serial.print("*");
        } else {
          // ignore this input since we recently turned this light on
          Serial.print("+");
        }

        Serial.print(inputLevel - inputSensitivity, 3);
      } else {
        if (elapsedMsForLastOutput - lastOnMsArray[outputId] > minOnMs) {
          // the output has been on for at least minOnMs. turn it off
          // don't actually turn off here because that might break the morse code
          outputStates[outputId] = LOW;
          Serial.print("      ");   // we turned the output off. show blank space
          //Serial.print(inputLevel, 3);
        } else {
          Serial.print(inputLevel - inputSensitivity, 3);
        }
      }
      Serial.print(" | ");

      // prepare for the next iteration
      outputStartBin = nextOutputStartBin;
    }

    Serial.print(AudioMemoryUsageMax());
    Serial.print(" blocks | ");

    // todo: this doesn't interact right with the way we do morseCode
    Serial.print(elapsedMsForLastOutput - lastUpdate);
    Serial.println("ms");
    lastUpdate = elapsedMsForLastOutput;
  }
}


bool blinkPattern(TimedAction patternArray[], uint& lastPatternActionId, uint patternArrayLength, unsigned long checkMs) {
  // blink all of the wires according to patternArray
  bool patternActionFound = false;

  for (uint patternActionId = 0; patternActionId < patternArrayLength; patternActionId++) {
    TimedAction patternAction = patternArray[patternActionId];
    checkMs += patternAction.checkMs;
    if (elapsedMsForLastOutput < checkMs) {
      if (lastPatternActionId != patternActionId) {
        // this is the first time we've seen this command this iteration

        // save that we've done this command already
        lastPatternActionId = patternActionId;

        // randomize the outputs if this is the first command
        if ((patternActionId = 0) && (randomizeOutputMs)) {
          bubbleUnsort(randomizedOutputIds, numOutputs);
        }

        // todo: only do half (rounded up) of the lights?
        for (uint outputId = 0; outputId < numOutputs; outputId++) {
          digitalWrite(outputId, patternAction.outputActions[outputId]);
        }
      }

      patternActionFound = true;
      break;
    }
  }

  // return true when we are done playing the pattern
  return not patternActionFound;
}


/*
   END HELPER FUNCTIONS
*/


void setup() {
  Serial.begin(9600);  // TODO! disable this if debug mode on. optimizer will get rid of it
  delay(500);
  Serial.println("Starting...");

  // todo: is it worth doing this better?
  randomSeed(analogRead(0) * analogRead(INPUT_PIN_VOLUME));

  // read config off the SD card
  SPI.setMOSI(7);
  SPI.setSCK(14);
  if (SD.begin(10)) {
    // todo: support a directory with multiple text files for morse code and patterns? maybe do morse line-by-line
    filename2morse("morse", patternArray, patternArrayLength);
    filename2pattern("pattern", patternArray, patternArrayLength);

    updateConfigBool("debug", debug);

    updateConfigFloat("audioShieldVolume", audioShieldVolume);
    updateConfigFloat("minInputRange", minInputRange);
    updateConfigFloat("minInputSensitivity", minInputSensitivity);
    updateConfigFloat("minInputSensitivityEMAAlpha", minInputSensitivityEMAAlpha);
    updateConfigFloat("numbEMAAlpha", numbEMAAlpha);
    updateConfigFloat("numbPercent", numbPercent);

    updateConfigUint("fftIgnoredBins", fftIgnoredBins);
    updateConfigUint("audioShieldMicGain", audioShieldMicGain);
    updateConfigUint("minOnMs", minOnMs);
    updateConfigUint("morseDahMs", morseDahMs);
    updateConfigUint("morseDitMs", morseDitMs);
    updateConfigUint("morseElementSpaceMs", morseElementSpaceMs);
    updateConfigUint("morseWordSpaceMs", morseWordSpaceMs);

    updateConfigUlong("maxOffMs", maxOffMs);
    updateConfigUlong("randomizeOutputMs", randomizeOutputMs);
  } else {
    Serial.println("Unable to read SD card! Using defaults for everything");
  }

  if (patternArrayLength) {
    patternCompleted = false;
  } else {
    patternCompleted = true;
  }

  // setup outputs
  for (int i = 0; i < MAX_OUTPUTS; i++) {
    pinMode(outputPins[i], OUTPUT);
  }
  updateNumOutputs(numOutputs);

  // setup audio shield
  AudioMemory(12); // todo: tune this. so far max i've seen is 11
  audioShield.enable();
  audioShield.muteHeadphone(); // to avoid any clicks
  audioShield.inputSelect(AUDIO_INPUT_MIC);
  audioShield.volume(audioShieldVolume);  // for debugging
  audioShield.micGain(audioShieldMicGain);  // 0-63

  audioShield.audioPreProcessorEnable();  // todo: pre or post?

  // bass, mid_bass, midrange, mid_treble, treble
  audioShield.eqSelect(GRAPHIC_EQUALIZER);
  audioShield.eqBands(-0.80, -0.10, 0, 0.10, 0.33);  // todo: tune this

  audioShield.unmuteHeadphone();  // for debugging

  Serial.print("Setup complete after ");
  Serial.print(elapsedMsForLastOutput);
  Serial.println("ms");
}

void loop() {
  // todo: these functions should pass values instead of modifying globals
  updateOutputStatesFromFFT();

  // todo: check our buttons to see if we should update minInputSensitivity

  if (randomizeOutputMs && (elapsedMsForRandomization > randomizeOutputMs)) {
    bubbleUnsort(randomizedOutputIds, numOutputs);
    elapsedMsForRandomization = 0;
  }

  if (elapsedMsForLastOutput - lastOnMs < maxOffMs) {
    // we turned a light on recently. send output states to the wires
    for (uint i = 0; i < numOutputs; i++) {
      // randomizedOutputIds is actually sorted if randomizeOutputMs is not set
      digitalWrite(outputPins[randomizedOutputIds[i]], outputStates[i]);
    }

    patternCompleted = true; // start the pattern from the beginning next time it plays
  } else {
    // no lights have been turned on for at least maxOffMs

    if (patternCompleted) {
      // the pattern completed. start it over
      lastOnMs = lastUpdate = 0;
      elapsedMsForLastOutput = maxOffMs;
    }

    // blink pretty
    patternCompleted = blinkPattern(patternArray, lastPatternActionId, patternArrayLength, maxOffMs);
  }
}
