#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <elapsedMillis.h>

// Use these with the audio adaptor board
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14
#define VOLUME_KNOB      A2

AudioInputI2S             i2s1;           //xy=139,91
AudioOutputI2S            i2s2;           //xy=392,32
AudioAnalyzeFFT1024       fft1024;
AudioConnection           patchCord1(i2s1, 0, i2s2, 0);
AudioConnection           patchCord2(i2s1, 0, fft1024, 0);
AudioControlSGTL5000      audioShield;    //xy=366,225

elapsedMillis elapsedMs = 0;    // todo: do we care if this overflows?

// An array to hold the frequency bands
const int maxLevels = 8;  // TODO: should this be a define?
float level[maxLevels];

int numLevels = maxLevels;  // TODO: read this from the SD card

unsigned long turnOffMsArray[maxLevels];
unsigned long lastUpdate = 0;

// the sequencer also has this minOnMs logi
// set the sequencer value as low as looks good with your lights.
// set the teensy value to the value that looks good with the type of music you are listening to
uint minOnMs = 150; // 118? 150? 184? 200?  // the shortest amount of time to leave an output on. todo: set this based on some sort of bpm detection? read from the SD card? have a button to switch between common settings?

// 1 byte one for the sequencer board
unsigned char output = 0;

// An array to hold an EMA of the 16 frequency bands
// When the signal drops quickly, these are used to lower the on-screen level
// slowly which looks more pleasing to corresponds to human sound perception.
float smoothing = 0.60;  // todo: tune this and read this from the SD card
float smoothLevel[maxLevels];

float decay = 0.9997;  // TODO: read this from the SD card

// TODO: I'm still not sure I like how minMin interacts with scaling the levels
float minMinLevel = 0.23;  // todo: tune this. read this from the SD card. maybe connect to the pot
float minMaxLevel = 0.60;  // this is the lowest magnitude that will return a 1.  todo: read this from the SD card

float maxLevel = minMaxLevel;
float minLevel = minMinLevel;

float scaleFactor = maxLevel - minLevel;


void setup() {
  Serial.begin(115200);  // todo: tune this
  Serial1.begin(115200);  // todo: tune this

  // delay(200);

  // setup SPI for the Audio board (which has an SD card reader)
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);

  // slave select pin for SPI
  pinMode(SDCARD_CS_PIN, OUTPUT);

  SPI.begin();
  // TODO: read SD card here to configure things

  // Audio requires memory to work. I haven't seen this go over 11
  AudioMemory(12);

  // Enable the audio shield and set the output volume.
  audioShield.enable();
  audioShield.muteHeadphone(); // to avoid any clicks
  audioShield.inputSelect(AUDIO_INPUT_MIC);
  audioShield.volume(0.5);
  audioShield.micGain(60);  // was 63, then 40  // 0-63 // TODO: tune this

  audioShield.audioPreProcessorEnable();  // todo: pre or post?

  // bass, mid_bass, midrange, mid_treble, treble
  // TODO: tune this. maybe read from SD card
  audioShield.eqSelect(GRAPHIC_EQUALIZER);
  // audioShield.eqBands(-0.80, -0.75, -0.50, 0.50, 0.80);  // the great northern
  // audioShield.eqBands(-0.5, -.2, 0, .2, .5);  // todo: tune this
  audioShield.eqBands(-0.80, -0.10, 0, 0.10, 0.33);  // todo: tune this

  audioShield.unmuteHeadphone();  // for debugging
}

// we could/should pass fft and level as args
void levelFromFFT() {
  // TODO: go numb to constant noise on a per-bin basis

  // read the 512 FFT frequencies into numLevels levels
  // music is heard in octaves, but the FFT data
  // is linear, so for the higher octaves, read
  // many FFT bins together.

  // See this conversation to change this to more or less than 16 log-scaled bands
  // https://forum.pjrc.com/threads/32677-Is-there-a-logarithmic-function-for-FFT-bin-selection-for-any-given-of-bands

  // TODO: tune these. maybe write a formula, but i think tuning by hand will be prettiest
  switch(numLevels) {
    case 0:
    case 1:
      // TODO: this doesn't look right on our line graph
      level[0] =  fft1024.read(0, 465);  // TODO: tune this 465 = 20k
      break;
    case 2:
      level[0] =  fft1024.read(1, 10);  // TODO: tune this
      level[1] =  fft1024.read(11, 82);  // TODO: tune this
      break;
    case 3:
      level[0] =  fft1024.read(1, 6);  // TODO: tune this
      level[1] =  fft1024.read(7, 10);  // TODO: tune this
      level[2] =  fft1024.read(11, 82);  // TODO: tune this
      break;
    case 4:
      level[0] =  fft1024.read(1, 6);  // TODO: tune this
      level[1] =  fft1024.read(7, 20);  // TODO: tune this
      level[2] =  fft1024.read(21, 41);  // TODO: tune this
      level[3] =  fft1024.read(42, 186);  // TODO: tune this
      break;
    case 5:
      level[0] =  fft1024.read(1, 6);  // TODO: tune this
      level[1] =  fft1024.read(7, 10);  // TODO: tune this
      level[2] =  fft1024.read(11, 20);  // TODO: tune this
      level[3] =  fft1024.read(21, 41);  // TODO: tune this
      level[4] =  fft1024.read(42, 186);  // TODO: tune this
      break;
    case 6:
      level[0] = fft1024.read(1, 3);   // 110
      level[1] = fft1024.read(4, 6);   // 220
      level[2] = fft1024.read(7, 10);  // 440
      level[3] = fft1024.read(11, 20);  // 880
      level[4] = fft1024.read(21, 41);  // 1763
      level[5] = fft1024.read(42, 186);  // 7998 todo: tune this
      break;
    case 7:
      level[0] =  fft1024.read(1, 3);  // TODO: tune this
      level[1] =  fft1024.read(4, 6);  // TODO: tune this
      level[2] =  fft1024.read(7, 10);  // TODO: tune this
      level[3] =  fft1024.read(11, 20);  // TODO: tune this
      level[4] =  fft1024.read(21, 41);  // TODO: tune this
      level[5] =  fft1024.read(42, 82);  // TODO: tune this
      level[6] =  fft1024.read(83, 186);  // TODO: tune this
      break;
    default:
    case 8:
      // level[0] =  fft1024.read(1, 6);  // TODO: tune this
      // level[1] =  fft1024.read(7, 22);  // TODO: tune this
      // level[2] =  fft1024.read(23, 32);  // TODO: tune this
      // level[3] =  fft1024.read(33, 46);  // TODO: tune this
      // level[4] =  fft1024.read(47, 66);  // TODO: tune this
      // level[5] =  fft1024.read(67, 93);  // TODO: tune this
      // level[6] =  fft1024.read(94, 184);  // TODO: tune this
      // level[7] =  fft1024.read(185, 465);  // 396 = 17k  // TODO: tune this
      level[0] =  fft1024.read(1, 15);    // TODO: ignore bin 0?
      level[1] =  fft1024.read(16, 32);
      level[2] =  fft1024.read(33, 46);
      level[3] =  fft1024.read(47, 66);
      level[4] = fft1024.read(67, 93);
      level[5] = fft1024.read(94, 131);
      level[6] = fft1024.read(132, 184);
      level[7] = fft1024.read(185, 419);
      break;
    // case 9:
    // case 10:
    // case 11:
    // case 12:
    // case 13:
    // case 14:
    // case 15:
    // case 16:
    //   // these bands are from pjrc. we should try to have the other ones match this growth rate
    //   // TODO: maybe skip the top and bottom bins?
    //   level[0] =  fft1024.read(0);  // TODO: skip this bin?
    //   level[1] =  fft1024.read(1);
    //   level[2] =  fft1024.read(2, 3);
    //   level[3] =  fft1024.read(4, 6);
    //   level[4] =  fft1024.read(7, 10);
    //   level[5] =  fft1024.read(11, 15);
    //   level[6] =  fft1024.read(16, 22);
    //   level[7] =  fft1024.read(23, 32);
    //   level[8] =  fft1024.read(33, 46);
    //   level[9] =  fft1024.read(47, 66);
    //   level[10] = fft1024.read(67, 93);
    //   level[11] = fft1024.read(94, 131);
    //   level[12] = fft1024.read(132, 184);
    //   level[13] = fft1024.read(185, 257);
    //   level[14] = fft1024.read(258, 359);
    //   level[15] = fft1024.read(360, 465);   // 465 = 20k
    //   break;
  }
}

int numOn = 0;


void loop() {
  // TODO: determine the note being played?
  // TODO: determine the tempo?
  // TODO: find the sum as well as the bins at least 10% as loud as the loudest bin IDs for extra details?

  if (fft1024.available()) {
    levelFromFFT();

    // TODO: this is too noisy. how can we improve this? i'd like switchs for micGain, minMin, minMax, and minOn
    // read the knob position
    // int knob = analogRead(VOLUME_KNOB);
    // float vol = (float)knob / 1024.0;   // was 1280
    // Serial.print(vol);
    // Serial.print(" | ");

    minLevel = maxLevel;  // reset minLevel
    for (int i = 0; i < numLevels; i++) {
      // Smooth using exponential moving average
      if (level[i] > smoothLevel[i]) {
        // jump up immediatly
        smoothLevel[i] = level[i];
      } else {
        // slide down slowly
        // smoothLevel[i] = (smoothing * smoothLevel[i]) + ((1 - smoothing) * level[i]);
        // TODO: maybe we don't actually need smoothLevel
        smoothLevel[i] = level[i];
      }

      // Find max and min values ever displayed across whole spectrum
      if (smoothLevel[i] > maxLevel) {
        maxLevel = smoothLevel[i];
      } else if (smoothLevel[i] < minLevel) {
        // TODO: i'm not sure how much tracking the minLevel like this does.
        // TODO: i think we should maybe also base minLevel off some % of maxLevel, too
        minLevel = max(smoothLevel[i], minMinLevel);
      }
    }

    Serial.print(minLevel);
    Serial.print(" ");
    Serial.print(maxLevel);
    Serial.print(" | ");

    // Calculate the total range of smoothed spectrum
    // consider 95% of the loudest as equally loud as the loudest
    scaleFactor = (maxLevel - minLevel) * 0.95;

    // output the levels
    numOn = 0;
    for (int i = 0; i < numLevels; i++) {
      // scale so that 1 is the loudest sound we've heard
      float scaledLevel = (smoothLevel[i] - minLevel) / scaleFactor;

      // todo: limit the number of lights we turn on to the loudest 6 of 8 instead of the first 6
      //       i think we might still turn on 7 here with our turnOffMsArray logic :(
      if ((numOn < 6) and (scaledLevel > 0.04)) {   // TODO: tune this
        // debug print
        Serial.print(scaledLevel);
        Serial.print(" ");

        // prepare to send over serial
        bitSet(output, i);

        // make sure we stay on for a minimum amount of time
        turnOffMsArray[i] = elapsedMs + minOnMs;

        numOn += 1;
      } else {
        if (elapsedMs < turnOffMsArray[i]) {
          // the output has not been on for long enough. leave it on.

          // debug print
          Serial.print(" *   ");

          numOn += 1;
        } else {
          // the output has been on for at least minOnMs and is quiet now. turn it off
          bitClear(output, i);

          // debug print
          Serial.print(" -   ");
        }
      }
    }

    // debug print
    Serial.print("| ");
    Serial.print(AudioMemoryUsageMax());
    Serial.print(" blocks | ");

    // send byte to Serial1
    Serial1.write(output);

    // make sure that first byte finished writing
    Serial1.flush();

    // decay maxLevel
    maxLevel = max(minMaxLevel, maxLevel * decay);

    // TODO: do something to minLevel?

    Serial.print(elapsedMs - lastUpdate);
    Serial.print("ms | ");
    lastUpdate = elapsedMs;
    Serial.println(output);
    Serial.flush();
  }
}

