#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <SoftwareSerial.h>
#include <elapsedMillis.h>

// Use these with the audio adaptor board
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14
#define VOLUME_KNOB      A2

// TODO: maybe better to use SPI, but this is less to solder
// we don't even use RX on this
// TODO: can we set the pin to null somehow?
SoftwareSerial            mySerial1(2, 3);  // RX, TX
SoftwareSerial            mySerial2(2, 4);  // RX, TX

AudioInputI2S             i2s1;           //xy=139,91
AudioOutputI2S            i2s2;           //xy=392,32
AudioAnalyzeFFT1024       fft1024;
AudioConnection           patchCord1(i2s1, 0, i2s2, 0);
AudioConnection           patchCord2(i2s1, 0, fft1024, 0);
AudioControlSGTL5000      audioShield;    //xy=366,225

elapsedMillis elapsedMs = 0;    // todo: do we care if this overflows?

// 1 byte one for each sequencer board
const unsigned int numOutputs = 1;  // TODO: eventually go up to 3
unsigned char output[numOutputs];

float decay = 0.75;  // EMA factor  // TODO: tune this. read from SD card

// An array to hold the frequency bands that will be turned into on/off signals
const int numLevels = 8 * numOutputs;
float averageLevel[numLevels];
float currentLevel[numLevels];

unsigned long turnOffMsArray[numLevels];
unsigned long lastUpdate = 0;

// the sequencer also has this minOnMs logi
// set the sequencer value as low as looks good with your lights.
// set the teensy value to the value that looks good with the type of music you are listening to
uint minOnMs = 150; // 118? 150? 184? 200?  // the shortest amount of time to leave an output on. todo: set this based on some sort of bpm detection? read from the SD card? have a button to switch between common settings?

// minimum level that will turn a light on
float minLevel = 0.23;  // todo: tune this. read this from the SD card. maybe connect to the pot


void setup() {
  Serial.begin(115200);  // todo: tune this
  Serial1.begin(115200);  // todo: tune this
  mySerial1.begin(115200);  // todo: tune this
  mySerial2.begin(115200);  // todo: tune this

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
      currentLevel[0]  = fft1024.read(0, 465);  // TODO: tune this 465 = 20k
      break;
    case 2:
      currentLevel[0]  = fft1024.read(1, 10);  // TODO: tune this
      currentLevel[1]  = fft1024.read(11, 82);  // TODO: tune this
      break;
    case 3:
      currentLevel[0]  = fft1024.read(1, 6);  // TODO: tune this
      currentLevel[1]  = fft1024.read(7, 10);  // TODO: tune this
      currentLevel[2]  = fft1024.read(11, 82);  // TODO: tune this
      break;
    case 4:
      currentLevel[0]  = fft1024.read(1, 6);  // TODO: tune this
      currentLevel[1]  = fft1024.read(7, 20);  // TODO: tune this
      currentLevel[2]  = fft1024.read(21, 41);  // TODO: tune this
      currentLevel[3]  = fft1024.read(42, 186);  // TODO: tune this
      break;
    case 5:
      currentLevel[0]  = fft1024.read(1, 6);  // TODO: tune this
      currentLevel[1]  = fft1024.read(7, 10);  // TODO: tune this
      currentLevel[2]  = fft1024.read(11, 20);  // TODO: tune this
      currentLevel[3]  = fft1024.read(21, 41);  // TODO: tune this
      currentLevel[4]  = fft1024.read(42, 186);  // TODO: tune this
      break;
    case 6:
      currentLevel[0]  = fft1024.read(1, 3);   // 110
      currentLevel[1]  = fft1024.read(4, 6);   // 220
      currentLevel[2]  = fft1024.read(7, 10);  // 440
      currentLevel[3]  = fft1024.read(11, 20);  // 880
      currentLevel[4]  = fft1024.read(21, 41);  // 1763
      currentLevel[5]  = fft1024.read(42, 186);  // 7998 todo: tune this
      break;
    case 7:
      currentLevel[0]  = fft1024.read(1, 3);  // TODO: tune this
      currentLevel[1]  = fft1024.read(4, 6);  // TODO: tune this
      currentLevel[2]  = fft1024.read(7, 10);  // TODO: tune this
      currentLevel[3]  = fft1024.read(11, 20);  // TODO: tune this
      currentLevel[4]  = fft1024.read(21, 41);  // TODO: tune this
      currentLevel[5]  = fft1024.read(42, 82);  // TODO: tune this
      currentLevel[6]  = fft1024.read(83, 186);  // TODO: tune this
      break;
    case 8:
      currentLevel[0]  = fft1024.read(1, 15);
      currentLevel[1]  = fft1024.read(16, 32);
      currentLevel[2]  = fft1024.read(33, 46);
      currentLevel[3]  = fft1024.read(47, 66);
      currentLevel[4]  = fft1024.read(67, 93);
      currentLevel[5]  = fft1024.read(94, 131);
      currentLevel[6]  = fft1024.read(132, 184);
      currentLevel[7]  = fft1024.read(185, 419);  // 18kHz
      break;
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    default:
      // these bands are from pjrc. we should try to have the other ones match this growth rate
      // TODO: maybe skip the top and bottom bins?
      currentLevel[0]  = fft1024.read(0);  // TODO: skip this bin?
      currentLevel[1]  = fft1024.read(1);
      currentLevel[2]  = fft1024.read(2, 3);
      currentLevel[3]  = fft1024.read(4, 6);
      currentLevel[4]  = fft1024.read(7, 10);
      currentLevel[5]  = fft1024.read(11, 15);
      currentLevel[6]  = fft1024.read(16, 22);
      currentLevel[7]  = fft1024.read(23, 32);
      currentLevel[8]  = fft1024.read(33, 46);
      currentLevel[9]  = fft1024.read(47, 66);
      currentLevel[10] = fft1024.read(67, 93);
      currentLevel[11] = fft1024.read(94, 131);
      currentLevel[12] = fft1024.read(132, 184);
      currentLevel[13] = fft1024.read(185, 257);
      currentLevel[14] = fft1024.read(258, 359);
      currentLevel[15] = fft1024.read(360, 465);   // 465 = 20k
      break;
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 23:
    case 24:
      // these bands are from pjrc. we should try to have the other ones match this growth rate
      // TODO: maybe skip the top and bottom bins?
      currentLevel[0]  = fft1024.read(0);  // TODO: skip this bin?
      currentLevel[1]  = fft1024.read(1);
      currentLevel[2]  = fft1024.read(2, 3);
      currentLevel[3]  = fft1024.read(4, 6);
      currentLevel[4]  = fft1024.read(7, 10);
      currentLevel[5]  = fft1024.read(11, 15);
      currentLevel[6]  = fft1024.read(16, 22);
      currentLevel[7]  = fft1024.read(23, 32);
      currentLevel[8]  = fft1024.read(33, 46);
      currentLevel[9]  = fft1024.read(47, 66);
      currentLevel[10] = fft1024.read(67, 93);
      currentLevel[11] = fft1024.read(94, 131);
      currentLevel[12] = fft1024.read(132, 184);
      currentLevel[13] = fft1024.read(185, 257);
      currentLevel[14] = fft1024.read(258, 359);
      currentLevel[15] = fft1024.read(360, 465);
      currentLevel[16] = fft1024.read(360, 465);
      currentLevel[17] = fft1024.read(360, 465);
      currentLevel[18] = fft1024.read(360, 465);
      currentLevel[19] = fft1024.read(360, 465);
      currentLevel[20] = fft1024.read(360, 465);
      currentLevel[21] = fft1024.read(360, 465);
      currentLevel[22] = fft1024.read(360, 465);
      currentLevel[23] = fft1024.read(360, 465);   // 465 = 20k
      break;
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

    // output the levels
    numOn = 0;
    for (int i = 0; i < numLevels; i++) {
      averageLevel[i] = (decay * averageLevel[i]) + ((1 - decay) * currentLevel[i]);

      // todo: limit the number of lights we turn on to the loudest 6 of 8 instead of the first 6
      //       i think we might still turn on 7 here with our turnOffMsArray logic :(
      if ((numOn < 6) and (currentLevel[i] > averageLevel[i] + 0.15)) {   // TODO: tune this
        // debug print
        Serial.print(currentLevel[i] - averageLevel[i]);
        Serial.print(" ");

        // prepare to send over serial
        bitSet(output[i / 8], i % 8);

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
          bitClear(output[i / 8], i % 8);

          // debug print
          Serial.print(" -   ");
        }
      }
    }

    // debug print
    Serial.print("| ");
    Serial.print(AudioMemoryUsageMax());
    Serial.print(" blocks | ");

    // TODO: we should use SPI here instead
    // send the bytes to their devices
    Serial1.write(output[0]);
    mySerial1.write(output[1]);
    mySerial2.write(output[2]);

    // make sure that first byte finished writing
    Serial1.flush();
    // mySerial doesn't have a flush method :(

    // TODO: do something to minLevel?

    Serial.print(elapsedMs - lastUpdate);
    Serial.println("ms");
    lastUpdate = elapsedMs;
    Serial.flush();
  }
}

