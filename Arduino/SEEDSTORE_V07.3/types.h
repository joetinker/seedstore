// global constants
const byte maxLineLen     = 16;  // LCD line length
const byte maxWord        = 24;  // number of words in the recovery seed
const byte wordLen        = 8;   // word length is 8 chars (fix)
const byte pinLen         = 8;   // max PIN length
const byte nameLen        = 8;   // max seed name length
const long oneDayDelay    = 86400;  // time for one day (PIN recovery)

// uncomment for debug only !
//#define DEBUGMODE

// uncomment selected display type LCD or OLED
#define LCD
//#define OLED


const char rAuthor[] PROGMEM = "Joe Tinker";   // Author or other info
const char rSysVer[] PROGMEM = "Seed store 0.7.3";  // product info & version (under development !!)
const char rSelfTest[] PROGMEM = "Self test ";
const char rUnlock[] PROGMEM = "UNLOCK";
const char rDelaeydAccess[] PROGMEM = "Delayed access";
const char rYesNo[] PROGMEM = "Select=No Up=Yes";
const char rChangeName[] PROGMEM = "Change name?";
const char rChangeDelay[] PROGMEM = "Change delay?";
const char rChangeWords[] PROGMEM = "Change words?";
const char rChangePIN[] PROGMEM = "Change PIN?";
const char rChangeDesc[] PROGMEM = "Change descr. ?";
const char rPINisSet[] PROGMEM = "PIN has been set";
const char rSetContrast[] PROGMEM = "Set LCD contrast";
const char rUpDown[] PROGMEM = "Up / Down";
const char rExit[] PROGMEM = "Exit?";
const char rBackupSeedQ[] PROGMEM = "Backup seed?";
const char rBackupFound[] PROGMEM = "Backup is found!";
const char rShowSeed[] PROGMEM = "Show BIP39 seed?";
const char rEraseBackup[] PROGMEM = "Erase s. backup?";
const char rRecoveryBackup[] PROGMEM = "Recovery seed?";
const char rForgottenPINQ[] PROGMEM = "Forgotten PIN?";
const char rSetupQ[] PROGMEM = "Setup?";
const char rTempUnsec[] PROGMEM = "Temprorary unsec";
const char rOnlyForUp[] PROGMEM = "for upgrade only";
const char rAreYouSureQ[] PROGMEM = "Are you sure?";

#define maxSeed 3        // number of supported recovery seeds
#define addrContr 1      // LCD contrast
#define addrNames 4      // EEPROM address for names
#define addrDelays 31    // EEPROM address for delays (x30)
#define addrPins 64      // EEPROM address for PINs
#define addrWords 112    // EEPROM address for secure words
#define led 13           // internal LED port
#define pwmout 11        // LCD only - out for contrast control (PWM)
//#define timeOutBL 120    // timeout for LCD turn off
#define formId 74        // 74 is formatted EEPROM identifier
// Buttons:
#define btnRIGHT 0
#define btnUP 1
#define btnDOWN 2
#define btnLEFT 3
#define btnSELECT 4
#define btnNONE 5

// OLED Display only
#ifdef OLED 
#include <Adafruit_CharacterOLED.h>
// OLED_V1 = older, OLED_V2 = newer, RS, R/W, EN, D4, D5, D6, D7
Adafruit_CharacterOLED   lcd(OLED_V2, 8, 11, 9, 4,  5,  6,  7);
#endif

// LCD Display only
#ifdef LCD
#include <LiquidCrystal.h>
// used pins RS, EN, D4, D5, D6, D7)
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
#endif

// universal inc / dec value at the limit
byte change(int x, byte limit, int dif) {
  int val;
  val = x + dif;
  if (dif > 0 and val >= limit) return limit;  // max value
  if (dif < 0 and val <= limit) return limit;  // min value
  return byte(val);
};

void changeChar(char* str, byte pos, bool up) {
  if (str[pos] > 'Z') {str[pos]='Z'; return; }; // max value 
  if (str[pos] < ' ') {str[pos]=' '; return; }; // min value
  if (up) str[pos]++; else str[pos]--;
  return;
}

void changeNum(char* str, byte pos, bool up) {
  if (up) str[pos]++; else str[pos]--;
  if (str[pos] > '9') str[pos]='9';  // max value
  if (str[pos] < '0') str[pos]='0';  // min value
  return;
}

void printSpaces(byte num) {
  for (byte i=0; i<num; i++) {
    lcd.print(" ");
  }; 
}

// line editor
typedef struct line_t {  
    byte cursorPos;        // cursor position
    byte margin;           // left margin    
    bool changed;          // flag change, true => refresh display
    bool numeric;          // True = number; False = chars 
    bool hidden;           // True = cover by stars
    char txt[maxLineLen+1]; //txt[wordLen+1];   // line
    void left() {
      // removes the last character on the right from txt
      txt[cursorPos]='\0';  
      
      // if the cursor is not at the end, moves it to the left
      cursorPos = change(cursorPos,0,-1);      
    }
    void right() {
      cursorPos = change(cursorPos,15,1);   // wordLen-1
    }
    void charUp() {
      if (txt[cursorPos]=='\0') {
          if (numeric) txt[cursorPos]='0'; else txt[cursorPos]='A'; 
          txt[cursorPos+1]='\0'; }
      else 
          if (numeric) changeNum(txt,cursorPos,true); else changeChar(txt,cursorPos,true);
    }
    void charDown() {
      if (txt[cursorPos]=='\0') {
          if (numeric) txt[cursorPos]='9'; else txt[cursorPos]='Z'; 
          txt[cursorPos+1]='\0'; }
      else 
          if (numeric) changeNum(txt,cursorPos,false); else changeChar(txt,cursorPos,false);
    }
    void erase() {
      for (int i = maxLineLen; i >= 0; i--){ txt[i] = char('\0'); };
      cursorPos = 0; 
    }    
    void end() {
      cursorPos = strlen(txt); // - 1;
      if (cursorPos+margin>15) cursorPos = 15-margin;   // limit for display length (16 chars)
    }
    void print(bool hide) {
      lcd.setCursor(margin,1); 
      if (hidden) { // cisla se zakryvaji hvezdickami 
        for (byte i = 0; i < cursorPos; i++) lcd.print('*');
        if (txt[cursorPos] != '\0') {
           if (!hide) lcd.print(txt[cursorPos]); else lcd.print('*');
        }; } 
      else lcd.print(txt);   
      printSpaces(14); lcd.setCursor(cursorPos + margin,1); 
    }
};

struct Vector
{
    byte key[16];
    byte text[16];
};

// Define the ECB test vectors from the FIPS specification.
static Vector vectorAES128= {
    .key    = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
               0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
    .text   = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
               0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
};

// prints string as hex
static void phex(uint8_t* str, uint8_t len)
{
    for(byte i = 0; i < len; ++i) {Serial.print(str[i], HEX); Serial.print(','); };
    Serial.println();
} 

char* getCharROM(const char* messageAdr) {
  static char msg[17]; 
  byte len = strlen_P(messageAdr);
  for (byte i = 0; i <= len; i++) {
    msg[i] =  pgm_read_byte_near(messageAdr + i);
  }
  return msg;
}
