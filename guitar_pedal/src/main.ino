#include <Audio.h>
#include <stdio.h>
#include <LiquidCrystal_I2C.h> //YWROBOT
#include <IntervalTimer.h>

#define MOVING_AVERAGE 1
#define ECHO 2
#define AUDIO_MASK 0x0000FFFF
#define NUM_BLOCKS 64
#define CIRCULAR_BUFFER_LENGTH AUDIO_BLOCK_SAMPLES * NUM_BLOCKS
#define CIRCULAR_BUFFER_MASK (CIRCULAR_BUFFER_LENGTH - 1)
#define MAX_SAMPLE_BLOCKS NUM_BLOCKS - 1
//Audio library macro, AUDIO_BLOCK_SAMPLES = 128

AudioInputI2S i2s_in;
AudioOutputI2S i2s_out;
AudioRecordQueue Q_in_L;
AudioRecordQueue Q_in_R;
AudioPlayQueue Q_out_L;
AudioPlayQueue Q_out_R;
AudioConnection patchCord1(i2s_in, 0, Q_in_L, 0);
AudioConnection patchCord2(i2s_in, 1, Q_in_R, 0);
AudioConnection patchCord3(Q_out_L, 0, i2s_out, 0);
AudioConnection patchCord4(Q_out_R, 0, i2s_out, 1);
AudioControlSGTL5000 sgtl5000;
LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27
IntervalTimer eventProcesser;

const int myInput = AUDIO_INPUT_LINEIN;
volatile bool updateLCD = false;
volatile int count = 0;

// Temperary arrays to hold new values of current sample block
double leftIn[CIRCULAR_BUFFER_LENGTH] = {0.0};
double rightIn[CIRCULAR_BUFFER_LENGTH] = {0.0};
double leftOut[CIRCULAR_BUFFER_LENGTH] = {0.0};
double rightOut[CIRCULAR_BUFFER_LENGTH] = {0.0};
int16_t *bp_L, *bp_R;
int16_t curBlock = 0;
uint32_t curSample = 0;
uint32_t startIndex = 0;

void refreshLCD()
{
  updateLCD=false;
  count++;
  lcd.clear();
  char str[255];
  int maxMem = AudioMemoryUsageMax(); 
  sprintf(str,"Count: %d Count: %d", maxMem, count);
 // sprintf(str,"Count: %d Count: %d", count, count);
  lcd.print(str);
  lcd.setCursor(0, 1);   
  lcd.print(str);
  lcd.setCursor(0, 2);   
  lcd.print(str);
  lcd.setCursor(0, 3);   
  lcd.print(str); 
}

void audioProcess(int effect){
  bp_L = Q_in_L.readBuffer();
  bp_R = Q_in_R.readBuffer();

  startIndex = curBlock * AUDIO_BLOCK_SAMPLES;
  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
    curSample = startIndex + i;
    leftIn[curSample] = (double)bp_L[i];
    rightIn[curSample] = (double)bp_R[i];
  }

  Q_in_L.freeBuffer();
  Q_in_R.freeBuffer();

  // Get pointers to "empty" output buffers
  bp_L = Q_out_L.getBuffer();
  bp_R = Q_out_R.getBuffer();

  switch(effect)
  {
    case MOVING_AVERAGE:
      audioMovingAverage();
      break;
    case ECHO:
      audioEcho();
      break;
    default:
      audioPassThrough();
  }

  // copy the processed data block back to the output
  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
    curSample = startIndex + i;
    bp_L[i] = (int16_t)leftOut[curSample];
    bp_R[i] = (int16_t)rightOut[curSample];
  }

  curBlock++;
  curBlock &= MAX_SAMPLE_BLOCKS;

  // and play them back into the audio queues
  Q_out_L.playBuffer();
  Q_out_R.playBuffer();
}

void audioPassThrough(){
  // Operate on each value in current block that has been recieved
  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) 
  {
    curSample = startIndex + i;
    leftOut[curSample] = leftIn[curSample];
    rightOut[curSample] = rightIn[curSample];    
  }
}

void audioMovingAverage(){
  int filLen = 200;
  int amplify = 3;

  for(int i = 0; i < AUDIO_BLOCK_SAMPLES; i++){
    curSample = startIndex + i;

    for (int j = 0; j < filLen; ++j)
    {
        leftOut[curSample] += leftIn[(curSample - j) & CIRCULAR_BUFFER_MASK];
        rightOut[curSample] += rightIn[(curSample - j) & CIRCULAR_BUFFER_MASK];
    }
    leftOut[curSample] = amplify * (leftOut[curSample] / filLen);
    rightOut[curSample] = amplify * (rightOut[curSample] /  filLen); 
 }
}

void audioEcho(){
  int delay = CIRCULAR_BUFFER_MASK * (1/8);
  float feedback = 0.4;

  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
    curSample = startIndex + i;
  
    if(curSample < delay){
      leftOut[curSample] = leftIn[curSample];
    }
    else{
      leftOut[curSample] = leftIn[curSample] + feedback*leftOut[curSample-delay];
    }
  }
}

void intvlFunction(void){
  updateLCD = true;
}

void setup(void){
  lcd.init();
  // Print a message to the LCD.
  lcd.backlight();
  eventProcesser.begin(intvlFunction, 1e6/4);
  eventProcesser.priority(255);
  // Audio connections require memory. and the record queue
  // uses this memory to buffer incoming audio.
  AudioMemory(138);
  // Enable the audio shield. select input. and enable output
  sgtl5000.enable();
  sgtl5000.inputSelect(myInput);
  sgtl5000.volume(0.5);
  // Start the record queues
  Q_in_L.begin();
  Q_in_R.begin();
}

void loop(void){
  while (!Q_in_L.available() && !Q_in_R.available());

  audioProcess(2);

  if(updateLCD)
  {
    refreshLCD();
  }
}
