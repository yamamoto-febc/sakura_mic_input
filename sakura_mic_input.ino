#include <SPI.h>
#include <SPISRAM.h>
#include <SakuraIO.h>
#include "constants.h"

SakuraIO_I2C sakuraio;
SPISRAM data(SRAM_CS_PIN);

uint32_t sample;
int switchOn = FLAG_OFF;
bool sending = FLAG_OFF;
int counter = 0;

unsigned long readDuration = 1000;
unsigned long startTime = -readDuration;


void setup() {

  pinMode(STATUS_LED_PIN, OUTPUT);

  pinMode(MIC_SRC_PIN, OUTPUT);
  pinMode(SW_PIN, INPUT);
  pinMode(MIC_OUT_PIN, INPUT);
  Serial.begin(115200);

  // for SRAM module
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV32);
  SPI.setDataMode(SPI_MODE0);

  Serial.print("Waiting to come online");
  for(;;){
    if( (sakuraio.getConnectionStatus() & 0x80) == 0x80 ) break;
    Serial.print(".");

    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(500);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(500);
  }
  // analogRead()のスピードアップのため、Arduinoのレジスタ設定
  ADCSRA = ADCSRA & 0xf8;
  ADCSRA = ADCSRA | 0x07;
  
  digitalWrite(STATUS_LED_PIN, LOW);
  Serial.println("");
}

void loop() {

  if (isReading()){
    sample = analogRead(MIC_OUT_PIN);
    Serial.println(counter);
    enqueueSamples(sample);
  }else{
    if (sending == FLAG_ON) {
      flushSamples();
      sending = FLAG_OFF;
    }
    
    switchOn = digitalRead(SW_PIN);
    if(switchOn == FLAG_ON){
      counter = 0;
      startMicReading();
      digitalWrite(STATUS_LED_PIN, HIGH);
      digitalWrite(MIC_SRC_PIN, HIGH);
    }else{
      digitalWrite(STATUS_LED_PIN, LOW);
      digitalWrite(MIC_SRC_PIN, LOW);
    }    
  }
  
}


void startMicReading(){
  sakuraio.enqueueTx(CONTROL_CHANNEL, START_CODE);
  sakuraio.send();
  startTime = millis();
  sending = FLAG_ON;
}

bool isReading(){
  return (startTime + readDuration > millis()) && counter < MAX_DATA_LEN;
}

void enqueueSamples(int sample){
  //data[counter++] = sample << 2;
  data.write(counter++ , sample << 2);
}

void flushSamples(){

  Serial.println("Start.");
  
  // Tx Queue
  uint8_t ret;
  uint8_t avail;
  uint8_t queued;
  uint8_t buf[8] = {0};
  
  for(int i = 0;i < MAX_DATA_LEN;i++){
    if (i > 0 && i % 8 == 0){
      ret = sakuraio.enqueueTx(DATA_CHANNEL, buf);
      memset(buf , 0 , sizeof(buf));
    }

    buf[i%8] = (uint8_t)data.read(i);
    
    sakuraio.getTxQueueLength(&avail, &queued);
    if(queued >= 30){
      ret = sakuraio.send();
      Serial.print("Send: ");
      Serial.println(ret);
      while(ret != 1){
        delay(500);
        digitalWrite(STATUS_LED_PIN, HIGH);
        delay(500);
        digitalWrite(STATUS_LED_PIN, LOW);
        ret = sakuraio.send();
        Serial.print(".");
        if (ret == 1){
          Serial.println("Done.");
        }
      }
    }
  }  

  sakuraio.enqueueTx(CONTROL_CHANNEL, END_CODE);
  sakuraio.send();
  Serial.println("End.");
}

