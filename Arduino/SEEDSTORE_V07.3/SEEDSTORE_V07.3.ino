// BIP-39 Recovery Seed Storage for Arduino Uno + LCD-SHIED 4-KEY
// Joe Tinker

/* LCD-SHIED 4-KEY P/N:7026
  AD3 - KEY Left
  AD4 - SCL EEPROM 24LC512
  AD5 - SDA EEPROM 24LC512
  D12 - KEY select
  D2  - KEY up
  D3  - KEY down
  D4  - LCD D4
  D5  - LCD D5
  D6  - LCD D6
  D7  - LCD D7
  D8  - LCD RS
  D9  - LCD EN
  D10 - LCD Back light control (LCD only)
  D11 - LCD Contrast control (LCD) / LCD R/W (OLED)
*/

#include <EEPROM.h>   // for internal EEPROM
#include <Crypto.h>   // Southern Storm Software, Pty Ltd.
#include <AES.h>
#include <SHA256.h>
#include <string.h>
#include <avr/boot.h>;

#include "EEP_LIB.h"  // for external EEPROM
#include "types.h"    // type declaration

AES128 aes128;
SHA256 sha256;

//#define HASH_SIZE 16  //32
byte buffer[16];

// global variables 
static long runtime = 0;           // relative time in seconds after switching on
static int keyPressTime = 0;       // doba drzeni klavesy v násobcích 0.1s
static int nonePressTime = 0;      // button holding time from the last press in multiples of 0.1s
static boolean setupMode = false;  // true = device is in setup mode
static boolean backLight = true;   // true = LCD back light is on
static boolean cursorOn  = false;  // true = cursor is on
static byte LCDcontrast = 55;      // default LCD contrast value 
static byte menuItem = 1;          // the current main menu item
static byte setupItem = 1;         // athe current setup menu item
static char defPin[] = "123";      // default PIN after reset
static byte selectedSeed = 0;      // selected seed number
static byte lcd_key = 0;           // code of pressed button
static byte lastKeyPress = btnNONE; // last button code
//static int adc_key_in = 0;
static byte wordNum = 0;           // selected secret word
static int sword[maxWord] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static char seedName[maxSeed][nameLen+1] = {"ALPHA","BETA","GAMMA"};  // default Recovery seed names
static long countDown = oneDayDelay*90;   // time for PIN recovery (90 days)
static byte delay10days = 9;        // actual PIN recovery delay in 10 days
static char lineBuffer[17];         // buffer for LCD line

/* for Arduino LCD KeyPad Shield (SKU: DFR0009)
// read the buttons
int read_LCD_buttons() {
adc_key_in = analogRead(A0); // read the value from the sensor 
// my buttons when read are centered at these valies: 0, 144, 329, 504, 741
// we add approx 50 to those values and check to see if we are close
if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be the most likely result
if (adc_key_in < 50) return btnRIGHT; 
if (adc_key_in < 195) return btnUP; 
if (adc_key_in < 380) return btnDOWN; 
if (adc_key_in < 555) return btnLEFT; 
if (adc_key_in < 790) return btnSELECT; 
return btnNONE; // when all others fail, return this...
}
*/

/* for LCD shield 4-Key P/N:7026  */
// read the buttons

int read_LCD_buttons() {

if (!digitalRead(2)) return btnUP; 
if (!digitalRead(3)) return btnDOWN; 
if (!digitalRead(A3)) return btnLEFT; 
if (!digitalRead(12)) return btnSELECT; 
return btnNONE; // when all others fail, return this...
}

static line_t line = {0,0,true,false,false,""};  // cursor, margin, changed, numeric, hidden, value
static line_t PIN =  {0,0,true,true,true,""};

void mainLoop(byte &lcd_key, byte &lastKeyPress);

void saveSeedName(byte num) { 
  int adr = (num * (nameLen+1) + addrNames);
  EEPROM.put(adr, seedName[num]); 
}

void loadSeedName(byte num) {
  int adr = (num * (nameLen+1) + addrNames);  
  EEPROM.get(adr, seedName[num]); 
}

// save secure word of recovery seed into internal EEPROM, num is seed number
void saveWords(byte num) { 
//AES - for next versions
//  memcpy(data->text,storedPin[0],pinLen); memcpy(data->text+pinLen,storedPin[1],pinLen);
//  cipher->setKey(data->key, cipher->keySize());
//  cipher->encryptBlock(buffer, data->text);
//  EEPROM.put(addrPins, buffer); 

  int adr = (num * sizeof(sword) + addrWords);
  EEPROM.put(adr, sword); 
}

// load secure word of recovery seed from internal EEPROM, num is seed number
void loadWords(byte num) { 
//AES - for next versions
//  memcpy(data->text,storedPin[0],pinLen); memcpy(data->text+pinLen,storedPin[1],pinLen);
//  cipher->setKey(data->key, cipher->keySize());
//  cipher->encryptBlock(buffer, data->text);
//  EEPROM.put(addrPins, buffer); 

  int adr = (num * sizeof(sword) + addrWords);
  EEPROM.get(adr, sword);
}

// calculates the checksum of seed, num is seed number
word calcSeedCheckSum(byte num) {
  word x = 0; word sum = 0;
  int adr = (num * sizeof(sword) + addrWords);
  for (byte i = 0; i < maxWord; i++) {
    EEPROM.get(adr + i*2, x); sum += x;
  }
  return(sum);
}

// calculates SHA-256 hash of pin
void hashPin(Hash *hash, byte seedNum, char *pin) {
//SHA-256
    hash->reset();
    hash->update(pin, strlen(pin));   
    hash->finalize(buffer, sizeof(buffer));
}

// save pin hash from buffer
void savePinHash(byte seedNum) {
  EEPROM.put(addrPins + seedNum * sizeof(buffer), buffer);
  // debug mode only
  #ifdef DEBUGMODE
    Serial.print("Save hash: "); phex(buffer, sizeof(buffer));     
  #endif
}

// load pin hash into buffer
void loadPinHash(byte seedNum) { 
   EEPROM.get(addrPins + seedNum * sizeof(buffer), buffer);
   // debug mode only
   #ifdef DEBUGMODE
      Serial.print("Load hash: "); phex(buffer, sizeof(buffer)); Serial.println();
   #endif   
}

void delay1() { 
  // waiting for the button to be released
  while (lcd_key != btnNONE) {
    delay(1);
    if (millis()/100 > runtime) { runtime++; scanKey(lcd_key, lastKeyPress, nonePressTime, keyPressTime); };
  }
  // wait 3s or press and release select button
  lastKeyPress = btnNONE;
  for (int i = 0; i < 3000; i++) {
    delay(1);
    if (millis()/100 > runtime) { runtime++; scanKey(lcd_key, lastKeyPress, nonePressTime, keyPressTime); };
    if (lcd_key == btnNONE && lastKeyPress == btnSELECT) { lastKeyPress = btnNONE; return; };
  }; 
}

void dynamicDelay() {static bool first = true; 
  if (lcd_key == btnNONE) first = true;
  else {
    while (lcd_key != btnNONE) {
    delay(5); scanKey(lcd_key, lastKeyPress, nonePressTime, keyPressTime); 
    if (keyPressTime > 8) {delay(200); return; };     // repeat after delays 
    }
  }
  if (first) {delay(100); first = false; return; };   // the first is immediate
}

/*
// entering into setup
void showSetupInfo() { 
  cursorOn = false; lcd.setCursor(0,0); lcd.print("Setup mode"); delay1(); 
};
*/

// edit
bool editLine(char *line1, char *line2, line_t &line) {
  static bool autoPressRight = false;
  static bool hide = false;
  // update after change
  if (line.changed) {
    cursorOn = true; 
    lcd.setCursor(0,0); lcd.print(line1);
    lcd.setCursor(0,1); lcd.print(line2);
    line.print(hide);
  };
  // for HW without right button, it is automatically emulated
  if (autoPressRight && nonePressTime > 6) { autoPressRight = false; lcd_key = btnRIGHT; nonePressTime = 0; };
  line.changed = (lcd_key != btnNONE); // pokud je něco zmáčknuto => true 
  if (lcd_key == btnUP)   {line.charUp(); autoPressRight = true; hide = false; };
  if (lcd_key == btnDOWN) {line.charDown(); autoPressRight = true; hide = false;};
  if (lcd_key == btnLEFT) {line.left(); hide = false;};
  if (lcd_key == btnRIGHT) {line.right(); hide = false;};
  if (lcd_key == btnNONE && lastKeyPress == btnSELECT) { lastKeyPress = btnNONE; line.changed = true; lcd.clear(); return(true); }; 
  if (!line.changed && !hide && nonePressTime > 6) { line.changed = true; hide = true; };
  dynamicDelay(); return(false);
}  

// Show
bool showLine(char *line1, char *line2, byte &wordNum, byte minNum = 0, byte maxNum = maxWord-1) {
  cursorOn = false;
  lcd.setCursor(0,0); lcd.print(line1); printSpaces(maxLineLen-strlen(line1)); 
  lcd.setCursor(0,1); lcd.print(line2); printSpaces(maxLineLen-strlen(line2)); 
  if (lcd_key == btnUP)   { wordNum = change(wordNum,minNum,-1); };  
  if (lcd_key == btnDOWN) { wordNum = change(wordNum,maxNum,1); };   
  if (lcd_key == btnNONE && lastKeyPress == btnSELECT) { lastKeyPress = btnNONE; lcd.clear(); return(true);}; 
  dynamicDelay(); return(false);
}
 
// edit seed name in seed setup
bool editSeedName(byte seedNum) {
  char tmp[3]; itoa(seedNum+1,tmp,10);
  char line2[8] = "Name "; strcat(line2, tmp); strcat(line2, ":");
  if (editLine("Setting", line2, line)) { strcpy(seedName[seedNum],line.txt); return(true); }
  else return(false);
}  

// enter PIN recovery delay
bool editDelay(byte seedNum) { 
  PIN.hidden = false;
  char line1[18] = "Delay for "; strcat(line1, seedName[seedNum]); 
  if (editLine(line1, "", PIN)) return(true); 
  else return(false);
}  

// edit secure word in seed setup
bool editSecureWord(byte seedNum, byte &wordNum) {
  char tmp[3]; int wid;
  itoa(wordNum+1,tmp,10);
  char line2[9] = "Word "; strcat(line2, tmp); strcat(line2, ":");
  if (editLine("Setting", line2, line)) { 
    //memcpy(sword[wordNum],line.txt,wordLen); 
    wid = searchWordIndex(line.txt);
    if(wid >= 0) sword[wordNum] = wid; //getWord(wid).toCharArray(sword[wordNum],8);
    return(true); }
  else return(false);
}  

// edit description for seed 
bool editDescription(byte seedNum, byte &wordNum) {
  char tmp[3]; int wid;
  itoa(wordNum+1,tmp,10);
  char line1[16] = "Description "; strcat(line1, tmp); strcat(line1, ":");
  if (editLine(line1, "", line)) { // line1, line2, object
  }
  return(true); 
}

// displaying secret words (1)
bool showWords(byte seedNum, byte &wordNum) { // wordNum = číslo zvoleného slova
  char tmp1[3]; itoa(wordNum+1,tmp1,10);
  char tmp2[wordLen+1]; 
  strcpy(tmp2,getWord(sword[wordNum])); // tmp = ;tmp[wordLen]='\0'; getWord(sword[wordNum]).toCharArray(tmp,wordLen+1)
  char line1[17] = "Words for "; strcat(line1, seedName[seedNum]);
  char line2[17] = "#"; strcat(line2, tmp1); strcat(line2, " "); strcat(line2, tmp2);
  return showLine(line1, line2, wordNum);
}  

// show description for seed 
bool showDescription(byte seedNum, byte &descNum) {
  char tmp[3]; itoa(descNum+1,tmp,10); 
  char tmp2[wordLen+1]; 
  char line1[15] = "Description "; strcat(line1, tmp); strcat(line1, ":");
  char line2[17] = ""; strcpy(line2,getDescription(seedNum, descNum));
  if (showLine(line1, line2, descNum) || strlen(line2) == 0) { lastKeyPress = btnNONE; lcd.clear(); return true; }
  else return false;
}

// PIN recovery process
bool delayedAcess() {
  char buffer[4];
  if (runtime%10 == countDown%10) {countDown--; 
    lcd.setCursor(0,0); lcd.print(getCharROM(rDelaeydAccess)); lcd.setCursor(0,1); lcd.print(countDown/86400); lcd.print("D "); 
    sprintf(buffer, "%02d", countDown/3600%24); lcd.print(buffer); lcd.print(":");
    sprintf(buffer, "%02d", countDown/60%60); lcd.print(buffer); lcd.print(":"); 
    sprintf(buffer, "%02d", countDown%60); lcd.print(buffer); lcd.print(" "); }; 
  return(countDown<=0);  // true = end
}

// displays the message and waits for a button to be pressed
bool message(char* line1, char* line2){ 
  byte x = 0;
  return showLine(line1, line2, x);
}

// display a yes/no query
bool yesNo(char* message,bool &yes) { 
  cursorOn = false; 
  lcd.setCursor(0,0); lcd.print(message); printSpaces(maxLineLen-strlen(message)); 
  lcd.setCursor(0,1); lcd.print(getCharROM(rYesNo));
  if (lcd_key == btnNONE && lastKeyPress == btnUP) {
     yes = true; lastKeyPress = btnNONE; return(true); };
  if (lcd_key == btnNONE && lastKeyPress == btnSELECT) { 
     yes = false; lastKeyPress = btnNONE; return(true); };
  return(false);
}

// display a save name yes/no query
bool saveNameQ(byte seedNum) { bool yes;
  if (yesNo("Save?",yes)) 
    if (yes) {lcd.setCursor(0,1); lcd.print("Saved"); printSpaces(14); saveSeedName(seedNum); 
      delay1(); lcd.clear(); return(true); }
    else { loadSeedName(seedNum); lcd.clear(); return(true); };
  return(false);
};     

// display a save PIN yes/no query
bool savePinQ(byte seedNum) { bool yes;
  if (yesNo("Save?",yes)) 
    if (yes) {lcd.setCursor(0,1); lcd.print("Saved"); printSpaces(14); //strcpy(storedPin[seedNum],PIN.txt); 
      hashPin(&sha256, seedNum, PIN.txt); savePinHash(seedNum); delay1(); lcd.clear(); return(true); }
    else { lcd.clear(); return(true); }; 
  return(false);
};     

// display a save words yes/no query
bool saveWordsQ(byte seedNum) { bool yes;
  if (yesNo("Save words?",yes)) 
    if (yes) {lcd.setCursor(0,1); lcd.print("Saved"); printSpaces(11); 
      saveWords(seedNum); delay1(); lcd.clear(); return(true); }
    else { lcd.clear(); return(true); }; 
  return(false);
};     

// select recovery seed (1)
boolean selectSeed(byte &menuItem) { static byte checkSum = 0;   // kontrolní součet vybraného seedu
  lcd.setCursor(0,0); lcd.print("Select seed name"); lcd.setCursor(0,1); cursorOn = false;
  lcd.print(seedName[menuItem]); printSpaces(maxLineLen-strlen(seedName[menuItem])-5); 
  lcd.print('#'); char res[5]; sprintf(&res[0], "%04x", calcSeedCheckSum(menuItem)); lcd.print(res); //lcd.print(calcSeedCheckSum(menuItem), HEX); 
  // word x = calcSeedCheckSum(menuItem); lcd.print(byte(x >> 12), HEX); lcd.print(byte(x >> 8) & 0x0F , HEX); 
  
  if (lcd_key == btnUP)   { menuItem = change(menuItem,maxSeed-1,1); loadSeedName(menuItem); };
  if (lcd_key == btnDOWN) { menuItem = change(menuItem,0,-1); loadSeedName(menuItem); };   
  if (lcd_key == btnNONE && lastKeyPress == btnSELECT) { lastKeyPress = btnNONE; lcd.clear(); return(true); }; 
  dynamicDelay(); return(false); 
}  

// enter pin (2)
boolean enterPin(byte seedNum, bool hidden) { 
  PIN.hidden = hidden; PIN.margin=0; 
  char line1[20] = "PIN for "; strcat(line1, seedName[seedNum]); 
  if (editLine(line1, "", PIN)) return(true); 
  else return(false);
}  

// check pin (3)
boolean checkPin(byte seedNum) {
  byte temp[16];
  lcd.setCursor(0,0); lcd.print("PIN for "); lcd.print(seedName[seedNum]); lcd.setCursor(0,1); lcd.noCursor();
  lastKeyPress == btnNONE; 
  hashPin(&sha256, seedNum, PIN.txt); memcpy(temp, buffer, sizeof(temp));
  loadPinHash(seedNum);
  if (memcmp(buffer, temp, sizeof(temp)) == 0) {
    lcd.print("is OK"); delay1(); lcd.clear(); loadWords(seedNum); return(true); }
  else {
    lcd.print("is WRONG"); delay1(); lcd.clear(); return(false); };
}  

// load PIN recoverydelay
void loadDelay(byte seedNum) {
  EEPROM.get(addrDelays+seedNum, delay10days);
  countDown = delay10days*10*oneDayDelay;
}

// save PIN recoverydelay
void saveDelay(byte seedNum) {
  int x = atoi(PIN.txt)/10;
  if (x<9) {delay10days=9; goto saveval; }      // Min is 90 days
  if (x>250) {delay10days=250; goto saveval; }  // Max is 2500 days
  delay10days = x;
saveval:
  EEPROM.put(addrDelays+seedNum, delay10days);
  countDown = delay10days*10*oneDayDelay;
}

void scanKey(byte &lcd_key, byte &lastKeyPress, int &nonePressTime, int &keyPressTime) {
  lcd_key = read_LCD_buttons(); // read the buttons
  switch (lcd_key) // depending on which button was pushed, we perform an action
  {
    case btnRIGHT: lastKeyPress = lcd_key; break; 
    case btnLEFT: lastKeyPress = lcd_key; break;
    case btnUP: lastKeyPress = lcd_key; break; 
    case btnDOWN: lastKeyPress = lcd_key; break; 
    case btnSELECT: lastKeyPress = lcd_key; break;
    case btnNONE: break;  
  } 
  if (lcd_key == btnNONE) { // time non press key in 0.1s
     nonePressTime++; keyPressTime = 0;  }  
  else { // hold press time in 0.1s
     nonePressTime = 0; keyPressTime++;  }
}

byte get_security() {
  cli();
  uint8_t lowBits      = boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS);
  uint8_t highBits     = boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS);
  uint8_t extendedBits = boot_lock_fuse_bits_get(GET_EXTENDED_FUSE_BITS);
  uint8_t lockBits     = boot_lock_fuse_bits_get(GET_LOCK_BITS);
  sei();
  return lockBits;
}

// the main loop is triggered every tenth of a second 
void mainLoop() {
//  if (keyPressTime > 20 && lcd_key == btnSELECT) {setupMode = true; lcd.clear(); setupItem = 1; };
  if (cursorOn && (runtime/5 % 2 == 0)) lcd.cursor(); else lcd.noCursor();      
}

void setup() {
  pinMode(led, OUTPUT); digitalWrite(led, LOW); // turn off the integrated LED
  pinMode(10, INPUT);                           // disable back light control
#ifdef LCD                                      // LCD display only
  if(EEPROM.read(addrContr) != 0xFF) LCDcontrast = EEPROM.read(addrContr); // load LCD contrast value if is set
  pinMode(pwmout, OUTPUT); analogWrite(pwmout, LCDcontrast); // set default LCD contrast
#endif
  pinMode(A0, INPUT); pinMode(A1, INPUT); pinMode(A2, INPUT); // set headers to input mode
  pinMode(A3, INPUT_PULLUP); pinMode(12, INPUT_PULLUP); // left,select buttons
  pinMode(2, INPUT_PULLUP); pinMode(3, INPUT_PULLUP);   // up,down Buttons
  
  lcd.begin(16, 2); // start LCD the library 
  lcd.clear();      // INIT LCD
  lcd.display();

#ifdef DEBUGMODE                              // Serial port is activated in debug mode only
    Serial.begin(9600); Serial.println("Debug:");
#endif
  
  // for UNO (LCD contrast control)
  TCCR2B = TCCR2B & 0b11111000 | 0x01;    // Set Timer 2 to 31kHz 
  //TCCR2B = TCCR2B & 0b11111000 | 0x03;  // Set Timer 2 to 500Hz

  if(EEPROM.read(0) == formId) {
    loadSeedName(selectedSeed); }  // if finds formatted eeprom flag
  else {
    lcd.clear(); lcd.print("Formating..."); delay(10000); 
    for (byte i = 0; i < maxSeed; i++){
       saveSeedName(i); hashPin(&sha256, i, defPin); 
       savePinHash(i);  saveWords(i); 
       EEPROM.write(addrDelays+i,18);
    }  
    EEPROM.write(0,formId);  // write formatted eeprom flag
    //lcd.clear();
  }
  
  Wire.begin();   // init I2C for external EEPROM (24LC512)
  lcd.setCursor(0,0); lcd.print(getCharROM(rSysVer));
  lcd.setCursor(0,1); lcd.print(getCharROM(rSelfTest)); 
  if (get_security() & 3) lcd.print(getCharROM(rUnlock)); 
  
  // when the checksum is not correct, the word table is not loaded or damaged (use loader script first)
  lcd.setCursor(10,1); 
  if (getChecksum()!=-14663) {lcd.print("FAIL! "); delay(6000); }

  menuItem = 85;  // check backup
}

void loop() { bool yes; 
if (setupMode) 
  switch(setupItem) {
    case 1: setupItem = 2; PIN.erase(); break; // showSetupInfo(); 
    case 2: if (enterPin(selectedSeed,true)) setupItem = 3; break;
    case 3: if (checkPin(selectedSeed)) setupItem = 4; else {setupItem = 1; setupMode = false; }; break;
    case 4: if (yesNo(getCharROM(rChangePIN),yes)) 
      if (yes) setupItem = 5; else setupItem = 7; break;
    case 5: if (enterPin(selectedSeed,false)) setupItem = 6; break;
    case 6: if (savePinQ(selectedSeed)) setupItem = 7; break;
    case 7: 
      if (yesNo(getCharROM(rChangeName),yes)) 
        if (yes) {strcpy(line.txt,seedName[selectedSeed]); line.margin=8; line.end(); lcd.clear(); setupItem = 8; } 
        else setupItem = 10; break;  
    case 8: 
      if (editSeedName(selectedSeed)) setupItem = 9; break;
    case 9: 
      if (saveNameQ(selectedSeed)) setupItem = 10; break; 
    case 10: if (yesNo(getCharROM(rChangeDelay),yes)) 
      if (yes) {setupItem = 11; loadDelay(selectedSeed); itoa(delay10days*10,PIN.txt,10); PIN.end(); } 
      else setupItem = 12; break;
    case 11: if (editDelay(selectedSeed)) {setupItem = 12; saveDelay(selectedSeed); } break;
    case 12: 
      if (yesNo(getCharROM(rChangeWords),yes)) 
        if (yes) {setupItem = 13; } else setupItem = 16; 
    break;
    case 13: 
      if (showWords(selectedSeed, wordNum)) {//memcpy(line.txt,sword[wordNum],wordLen); line.txt[wordLen]='\0';
          strcpy(line.txt,getWord(sword[wordNum])); 
          if (line.txt[0] == '?') line.erase();
          line.margin=8; line.end(); setupItem = 14; };   //getWord(sword[wordNum]).toCharArray(line.txt,wordLen+1)
      if (lcd_key == btnLEFT) {setupItem = 15; lcd.clear(); }; 
    break;  
    case 14: 
      if (editSecureWord(selectedSeed, wordNum)) setupItem = 13; 
    break;
    case 15: 
      if (saveWordsQ(selectedSeed)) setupItem = 16; // next item 
    break;  
    case 16: 
      if (yesNo(getCharROM(rChangeDesc),yes)) 
        if (yes) {setupItem = 19; } 
        else setupItem = 20; // next item
      break;      
    case 17:  // edit description loop
      if (editDescription(selectedSeed, wordNum)) { 
        if (lcd_key == btnSELECT) { 
          if (strlen(line.txt) == 0) { setupItem = 18; lastKeyPress = btnNONE; lcd.clear(); }
          putDescription(selectedSeed, wordNum, line.txt); wordNum++;
          if (wordNum < 8)  { strcpy(line.txt,getDescription(selectedSeed, wordNum)); line.end(); }
        };
        if (wordNum >= 8 && lcd_key == btnNONE) { setupItem = 18; }
      }  
    break;
    case 18:  // exit from edit descripion
      if (yesNo(getCharROM(rExit),yes)) {
        if (yes) setupItem = 20; // exit
        else setupItem = 19; }
    break;
    case 19:  // begin edit descriptiom
      wordNum = 0; lcd.clear(); strcpy(line.txt,getDescription(selectedSeed, wordNum)); line.margin=0; line.end();
      setupItem = 17; break;  
    case 20: // backup seed 1
      if (yesNo(getCharROM(rBackupSeedQ),yes)) {
        if (yes) setupItem = 21;
        else setupItem = 23; 
      }
    break;
    case 21: // backup seed 2
      strcpy(lineBuffer,getCharROM(rTempUnsec));
      if (message(lineBuffer, getCharROM(rOnlyForUp))) setupItem = 22; 
    break;    
    case 22: // backup seed 
      if (yesNo(getCharROM(rAreYouSureQ),yes)) {
        setupItem = 23; 
        if (yes) { backupSeed(selectedSeed, sword); }
      }
    break;    
    case 23: // exit from setup
      setupItem = 1; menuItem = 1; setupMode = false; lcd.clear();
    break;
  }
else {
  switch (menuItem) {
    case 1:
      strcpy(lineBuffer,getCharROM(rSysVer));
      if (runtime < 20) {
          if (message(lineBuffer, getCharROM(rAuthor))) menuItem = 2; }
      else menuItem = 2;  // if (message(lineBuffer, seedName[selectedSeed])) menuItem = 2;
    break;
    case 2:
      if (selectSeed(selectedSeed)) {menuItem = 3; PIN.erase(); } ;
    break;  
    case 3:
      if (yesNo(getCharROM(rShowSeed),yes)) {lcd.clear(); 
        if (yes) {menuItem = 22; PIN.erase(); } else menuItem = 4; };
    break; 
    case 4:
      if (yesNo(getCharROM(rForgottenPINQ),yes)) {lcd.clear();
        if (yes) {menuItem = 31; loadDelay(selectedSeed); }
        else menuItem = 5; }
    break; 
    case 5:
      if (yesNo(getCharROM(rSetupQ),yes)) {lcd.clear(); 
        if (yes) {setupItem = 1; setupMode = true; } else menuItem = 6; }
    break;    

#ifdef LCD  // LCD display only    
    case 6:
      if (yesNo("LCD control?",yes)) {lcd.clear(); 
        if (yes) menuItem = 61; else menuItem = 99; }
    break;    
#endif

#ifdef OLED  // OLED display only    
    case 6:
      menuItem = 99;
    break;    
#endif

    case 22: if (enterPin(selectedSeed,true)) menuItem = 23; break;
    case 23: if (checkPin(selectedSeed)) {menuItem = 24; wordNum = 0; } else menuItem = 3; break;
    case 24: if (showWords(selectedSeed, wordNum)) {menuItem = 25; wordNum = 0; }; break;
    case 25: if (showDescription(selectedSeed, wordNum)) menuItem = 1; break;

    case 31: // restore access when is PIN loss
      if (delayedAcess()) {menuItem = 32; // reset PIN to default
          hashPin(&sha256, selectedSeed, defPin); savePinHash(selectedSeed); };        
      if (lcd_key == btnNONE && lastKeyPress == btnLEFT) 
          menuItem = 99; // lastKeyPress = btnNONE; menuItem = 1; lcd.clear(); 
    break;

    case 32: //
      if (message(getCharROM(rPINisSet),"to 123")) menuItem=1;
    break;

    case 61:  // Adjust LCD contrast
      //cursorOn = false; 
      strcpy(lineBuffer,getCharROM(rSetContrast));
      if (showLine(lineBuffer, getCharROM(rUpDown), LCDcontrast, 0, 200)){
        menuItem=99; EEPROM.write(addrContr,LCDcontrast); }
      else
        if (lcd_key != btnNONE) analogWrite(pwmout, LCDcontrast);
      
      /*
      lcd.setCursor(0,0); lcd.print(getCharROM(rSetContrast));
      lcd.setCursor(0,1); lcd.print(getCharROM(rUpDown));
      if (lcd_key == btnUP)   { LCDcontrast = change(LCDcontrast,150,1); analogWrite(pwmout, LCDcontrast); };
      if (lcd_key == btnDOWN) { LCDcontrast = change(LCDcontrast,0,-1); analogWrite(pwmout, LCDcontrast); };
      if (lcd_key == btnNONE && lastKeyPress == btnSELECT) { lastKeyPress = btnNONE; lcd.clear(); menuItem=1; EEPROM.write(addrContr,LCDcontrast);};  
      */
    break; 

    case 85: // search backup
      if (isSeedBackup(0)) {menuItem = 90; selectedSeed = 0; break; }
      if (isSeedBackup(1)) {menuItem = 90; selectedSeed = 1; break; }
      if (isSeedBackup(2)) {menuItem = 90; selectedSeed = 2; break; }
      //for (byte i = 0; i < 3; i++){
      //  if (isSeedBackup(i)) {menuItem = 90; selectedSeed = i; break; }  // Backup of seed is found
      //}
      selectedSeed = 0; menuItem = 1;
    break;
    
    case 90: // seed backup recovery 
      if (message(getCharROM(rBackupFound),seedName[selectedSeed])) menuItem = 91;  // 
    break;
    
    case 91:
      if (yesNo(getCharROM(rRecoveryBackup),yes)) {
        if (yes) {recoverySeed(selectedSeed, sword); saveWords(selectedSeed); eraseSeedBackup(selectedSeed); menuItem = 85; }
        else menuItem = 92;
      }  
    break;     
    
    case 92:
      if (yesNo(getCharROM(rEraseBackup),yes)) {
        menuItem = 85; if (yes) eraseSeedBackup(selectedSeed); 
      }  
    break; 
    
    case 99: // return to begin
      lastKeyPress = btnNONE; lcd.clear(); menuItem=1;
    break;
  };  
};

scanKey(lcd_key, lastKeyPress, nonePressTime, keyPressTime);
if ((millis()/100)%100 != runtime%100) {runtime++; mainLoop(); };
//if (millis()/100 > runtime) { runtime++; mainLoop(); };
}

/*
 
// LCD back light control
void setBackLight(bool on){
  if (on) analogWrite(10, LCDcontrast); else analogWrite(10, 0); 
}
*/
