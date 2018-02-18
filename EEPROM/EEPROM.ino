// EEPROM worlist loader for SeedStore

// compatible with SeedStore V07

#include "Wire.h"

#define EEPROM_I2C_ADDRESS 0x50

void setup() {
  Wire.begin();
  Serial.begin(9600);
  Serial.println("EEPROM loader is ready");
}

void saveWord(int wordNum, String line){ 
  byte b;
  for (int i = 0; i < 8; i++) {
      b = 0; if (i <= line.length()) b = (byte)line[i]; 
      writeAddress(wordNum*8+i, b);
  }
}

void loadWord(int wordNum, String &line){ 
  char tmpStr[9];
  for (int i = 0; i < 8; i++) {
      tmpStr[i] = readAddress(wordNum*8+i);
  }
  tmpStr[8]='\0'; line = tmpStr;
}

void loop(){
  String line; String line2;
  line = Serial.readStringUntil('\n'); line.trim();
  if (line == "BEGIN") {
      Serial.println("BEGIN"); 
      for (int i = 0; i < 2048; i++) {
          line = Serial.readStringUntil('\n'); line.trim();
          if (line == "") {Serial.println("FAIL"); break; } else {Serial.println(line); }
          saveWord(i, line);     
      }
  }
  if (line == "VERIFY") {
      Serial.println("VERIFY"); 
      for (int i = 0; i < 2048; i++) {
          line = Serial.readStringUntil('\n'); line.trim();
          if (line == "") {Serial.println("FAIL"); break; };
          loadWord(i,line2); 
          if (line2 == line) Serial.println("OK"); else {Serial.println("FAIL"); break; }
      }
  }
  if (line == "DUMP") {
      byte x;
      Serial.println("DUMP"); 
      for (int i = 0; i < 2048*8; i++) {
          x = readAddress(i); 
          Serial.print(i); Serial.print(' '); 
          if (x<32) Serial.print(' '); else Serial.print((char)x); 
          Serial.print(' '); Serial.println(x);
      }
  }  
  if (line == "") Serial.println("WAIT");
}

void writeAddress(int address, byte val){
  Wire.beginTransmission(EEPROM_I2C_ADDRESS);
  Wire.write((int)(address >> 8));   // MSB
  Wire.write((int)(address & 0xFF)); // LSB
   
  Wire.write(val);
  Wire.endTransmission();

  delay(5);
}

byte readAddress(int address){
  byte rData = 0xFF;
  
  Wire.beginTransmission(EEPROM_I2C_ADDRESS);
  
  Wire.write((int)(address >> 8));   // MSB
  Wire.write((int)(address & 0xFF)); // LSB
  Wire.endTransmission();  

  Wire.requestFrom(EEPROM_I2C_ADDRESS, 1);  

  rData =  Wire.read();

  return rData;
}


