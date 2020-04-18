// External EEPROM lib for SeedStore

#include "Wire.h"
#define EEPROM_I2C_ADDRESS 0x50

#define addrSeedDesc 16640  // EEPROM address for description

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

void writeAddress(int address, byte val){
  Wire.beginTransmission(EEPROM_I2C_ADDRESS);
  Wire.write((int)(address >> 8));   // MSB
  Wire.write((int)(address & 0xFF)); // LSB   
  Wire.write(val);
  Wire.endTransmission();
  delay(5);
}

// calculates the checksum of word list
int getChecksum(){
  word sum = 0;
  for (int i = 0; i < 2048*8; i++) {sum += (word)readAddress(i); };
  return(sum);
}

// getWord from external EEPROM
char* getWord(int wordNum){ // String
  static char tmpStr[9];
  if (wordNum<0) return ("?"); // -1 is no word
  for (int i = 0; i < 8; i++) {
      tmpStr[i] = readAddress(wordNum*8+i);
  }
  tmpStr[8]='\0'; return(tmpStr);
}

void putDescription(byte seedNum, byte wordNum, char* str){ 
  byte b;
  for (byte i = 0; i < 16; i++) {
      if (i <= strlen(str)) b = (byte)str[i]; 
      else b = 0; 
      writeAddress(addrSeedDesc + seedNum*128 + wordNum*16+i, b);
  }
}

char* getDescription(byte seedNum, byte wordNum){ 
  static char tmpStr[17];
  for (byte i = 0; i < 16; i++) {
      tmpStr[i] = readAddress(addrSeedDesc + seedNum*128 + wordNum*16+i);
  }
  tmpStr[16]='\0'; return(tmpStr);
}

bool compareWordChar(int i, char L, byte pos) {
  return(readAddress(i*8+pos) == L);
}

// looks for the index in the word table stored by the external eeprom
int searchWordIndex(char* str){
  for (int i=0; i<2048; i++){
    if (compareWordChar(i,str[0],0)) {
      if (compareWordChar(i,str[1],1)) {
        if (compareWordChar(i,str[2],2)) {
          if (compareWordChar(i,str[3],3)) {
            return(i);
          }
        }
      }
    }
  }
  return (-1);
}


