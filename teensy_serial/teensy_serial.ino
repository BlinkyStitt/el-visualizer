// these can be overridden by files on the SD card
uint  numOutputs = 6;
uint  fftIgnoredBins = 1;  // skip the first fftIgnoredBins * FFT_HZ_PER_BIN Hz
float numbEMAAlpha = 0.005;  // alpha for calculating background sound to ignore. this probably needs tuning
float numbPercent = 0.75;  // how much of the average level to subtract from the input. TODO! TUNE THIS
// end - these can be overridden by files on the SD card

#define VERSION "1.0.0"

#define BETWEEN(value, min, max) (value < max && value > min)
#define FFT_HZ_PER_BIN 43
#define MAX_FFT_BINS 512
#define MAX_OUTPUTS 8

#include <Audio.h>
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

int   outputBins[MAX_OUTPUTS];
float avgInputLevel[MAX_FFT_BINS];


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

void readConfigBool(const char* filename, bool& config_ref) {
  // this could by DRYer

  String override = filename2string(filename);
  if (override == "") {
    Serial.print("Default for ");
  } else {
    Serial.print("Override for ");
    config_ref = override == "1";
  }

  Serial.print(filename);
  Serial.print(" (bool): ");
  Serial.println(config_ref);
}

void readConfigUint(const char* filename, uint& config_ref) {
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

void readConfigFloat(const char* filename, float& config_ref) {
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

void readConfigUlong(const char* filename, unsigned long& config_ref) {
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

void setOutputBins(uint& numOutputs) {
  // https://en.wikipedia.org/wiki/Piano_key_frequencies
  // http://www.sengpielaudio.com/calculator-notenames.htm
  // f(n) = 440 * 2 ^ ((n-49)/12)
  // TODO! TUNE THESE maybe figure out an algorithm. maybe use midi frequencies?
  // todo: read this from the sd card, too?
  switch(numOutputs) {
    case 0:
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
      outputBins[0] = 6;   // todo: tune this
      outputBins[1] = 20;   // todo: tune this
      outputBins[2] = 41;  // todo: tune this
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
    default:
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
}

void setup() {
  // TODO: tune this
  Serial.begin(9600);
  delay(500);

  // setup SPI for the Audio board (which has an SD card reader)
  SPI.setMOSI(7);
  SPI.setSCK(14);

  // read config off the SD card
  if (SD.begin(10)) {
    // SD only supports 8.3 filenames. most of these variable names are too long

    // 8 char max   ________
    readConfigUint("FFTIGNOR.TXT", fftIgnoredBins);
    readConfigUint("NUMOUT.TXT", numOutputs);
    // 8 char max   ________

    // 8 char max    ________
    readConfigFloat("NUMBEMA.TXT", numbEMAAlpha);
    readConfigFloat("NUMBPCT.TXT", numbPercent);
    // 8 char max    ________

    // write the version of this program to the SD card?
    File versionFile = SD.open("VERSION.TXT", O_WRITE | O_CREAT | O_TRUNC);
    versionFile.println(VERSION);
    versionFile.close();
  } else {
    Serial.println("ERR: Unable to read SD card! Using defaults for everything");
  }

  Serial.print("Version: ");
  Serial.println(VERSION);

  setOutputBins(numOutputs);

  // setup audio shield
  AudioMemory(12); // todo: tune this. so far max i've seen is 11
  audioShield.enable();
  audioShield.muteHeadphone(); // to avoid any clicks
  audioShield.inputSelect(AUDIO_INPUT_MIC);
  audioShield.volume(0.5);  // for debugging
  audioShield.micGain(63);  // 0-63

  audioShield.audioPreProcessorEnable();  // todo: pre or post?

  // bass, mid_bass, midrange, mid_treble, treble
  // TODO: tune this. maybe read from SD card
  audioShield.eqSelect(GRAPHIC_EQUALIZER);
  audioShield.eqBands(-0.80, -0.10, 0, 0.10, 0.33);  // todo: tune this

  audioShield.unmuteHeadphone();  // for debugging

  Serial.print("Starting...");
}

// TODO: should we define the variables here? Is that more efficient? no need to worry about scope

void loop() {
  // The 1024 point FFT always updates at approximately 86 times per second (every 11-12ms).
  if (myFFT.available()) {
    // ignore the first fftIgnoredBins bins as they can be far too noisy
    uint outputStartBin = fftIgnoredBins;

    // print the magnitudes for each frequency range
    for (uint outputId = 0; outputId < numOutputs; outputId++) {
      uint nextOutputStartBin = outputBins[outputId] + 1;

      // TODO: do we need this check? given valid data, it shouldn't ever happen
      // TODO: if we do need this, should we also check that we don't go over MAX_FFT_BINS?
      if (outputStartBin >= nextOutputStartBin) {
        nextOutputStartBin = outputStartBin + 1;
      }

      // find the loudest bin in this output's frequency range
      // TODO: should we be summing the bin instead?
      //       or maybe average power (magnitude^2)? (ignoring negative values)
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

      // print the magnitude
      // TODO: is this efficient?
      Serial.print(inputLevel);
      Serial.print(" ");

      // prepare for the next iteration that prints the next output
      outputStartBin = nextOutputStartBin;
    }

    // prepare for the next set of FFT data
    Serial.println();
  }
}
