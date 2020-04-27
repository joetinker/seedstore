// External EEPROM lib for SeedStore

#include "Wire.h"

#define EEPROM_I2C_ADDRESS 0x50

#define EE_namesAddr 16384  // 3x8
#define EE_pinsAddr  16408  // 3x16
#define EE_seedsAddr 16456  // 3x48
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

// save seed description to external EEPROM
void putDescription(byte seedNum, byte wordNum, char* str){ 
  byte b;
  for (byte i = 0; i < 16; i++) {
      if (i <= strlen(str)) b = (byte)str[i]; 
      else b = 0; 
      writeAddress(addrSeedDesc + seedNum*128 + wordNum*16+i, b);
  }
}

// get seed description from external EEPROM
char* getDescription(byte seedNum, byte wordNum){ 
  static char tmpStr[17];
  for (byte i = 0; i < 16; i++) {
      tmpStr[i] = readAddress(addrSeedDesc + seedNum*128 + wordNum*16+i);
  }
  tmpStr[16]='\0'; return(tmpStr);
}

// save seed copy to external EEPROM (for FW upgrade only)
void backupSeed(byte seedNum, int *seed){
  int adr = EE_seedsAddr + seedNum*48;    
  for (byte i = 0; i < 24; i++) {  // size of seed
     writeAddress(adr, byte(seed[i])); adr++;
     writeAddress(adr, byte(seed[i] >> 8)); adr++;
     #ifdef DEBUGMODE
     Serial.print(seed[i], HEX); Serial.print(',');
     #endif
  }
  #ifdef DEBUGMODE
  Serial.println();
  #endif
}

// recovery seed copy from external EEPROM (after FW upgrade only)
void recoverySeed(byte seedNum, int *seed){
  int adr = EE_seedsAddr + seedNum*48; 
  for (byte i = 0; i < 24; i++) {  // size of seed
      seed[i] = word(readAddress(adr)); adr++;
      seed[i] = seed[i] | (word(readAddress(adr)) << 8); adr++;
      #ifdef DEBUGMODE
      Serial.print(seed[i], HEX); Serial.print(',');
      #endif
  }
  #ifdef DEBUGMODE
  Serial.println();
  #endif
}

// true = EEPROM part fro backup is not empty
bool isSeedBackup(byte seedNum){ 
  for (byte i = 0; i < 48; i++) {  // size of seed
      if  (readAddress(EE_seedsAddr + seedNum*48 + i) != 0xFF) return true; // found something
  };
  return false; // backup storage in external EEPROM is empty
}

// clear part of EEPROM for seed backup
eraseSeedBackup(byte seedNum){ 
  for (byte i = 0; i < 48; i++) {  // size of seed
     writeAddress(EE_seedsAddr + seedNum*48 + i, 0xFF);
  };
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
