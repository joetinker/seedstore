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
static char seedName[maxSeed][nameLen+1] = {"ALFA","BETA","GAMA"};  // default Recovery seed names
static long countDown = defaultPinRecoveryDelay;   // time for PIN recovery (90 days)

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

static line_t line = {0,true,false,""};  // cursor, changed, numeric, value
static line_t PIN = {0,true,true,""};

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
      Serial.print("Load hash: "); phex(buffer, sizeof(buffer));
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

// entering into setup
void showSetupInfo() { 
  cursorOn = false; lcd.setCursor(0,0); lcd.print("Setup mode"); delay1(); 
};

// edit
bool editLine(char *line1, char *line2, line_t &line, byte leftMargin) {
  static bool autoPressRight = false;
  static bool hide = false;
  // update after change
  if (line.changed) {
    cursorOn = true; 
    lcd.setCursor(0,0); lcd.print(line1);
    lcd.setCursor(0,1); lcd.print(line2);
    line.print(leftMargin,hide);
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
  
// edit seed name in seed setup
bool editSeedName(byte seedNum) {
  char tmp[3]; itoa(seedNum+1,tmp,10);
  char line2[8] = "Name "; strcat(line2, tmp); strcat(line2, ":");
  if (editLine("Setting", line2, line, 8)) { strcpy(seedName[seedNum],line.txt); return(true); }
  else return(false);
}  

// edit secure word in seed setup
bool editSecureWord(byte seedNum, byte &wordNum) {
  char tmp[3]; int wid;
  itoa(wordNum+1,tmp,10);
  char line2[9] = "Word "; strcat(line2, tmp); strcat(line2, ":");
  if (editLine("Setting", line2, line, 8)) { 
    //memcpy(sword[wordNum],line.txt,wordLen); 
    wid = searchWordIndex(line.txt);
    if(wid >= 0) sword[wordNum] = wid; //getWord(wid).toCharArray(sword[wordNum],8);
    return(true); }
  else return(false);
}  

// displaying secret words (1)
bool showWords(byte seedNum, byte &wordNum) { // wordNum = číslo zvoleného slova
  char tmp[wordLen+1]; //memcpy(tmp, sword[wordNum], wordLen); 
  getWord(sword[wordNum]).toCharArray(tmp,wordLen+1); // tmp = ;tmp[wordLen]='\0';
  cursorOn = false;
  lcd.setCursor(0,0); lcd.print("Words for "); lcd.print(seedName[seedNum]); 
  lcd.setCursor(0,1); lcd.print("#"); lcd.print(wordNum+1); lcd.print(" "); lcd.print(tmp); printSpaces(14); 
  if (lcd_key == btnUP)   { wordNum = change(wordNum,0,-1); };
  if (lcd_key == btnDOWN) { wordNum = change(wordNum,maxWord-1,1);  };   
  if (lcd_key == btnNONE && lastKeyPress == btnSELECT) { lastKeyPress = btnNONE; lcd.clear(); return(true);}; 
  dynamicDelay(); return(false);
}  

// PIN recovery process
bool delayedAcess() {
  if (runtime%10 == countDown%10) {countDown--; 
    lcd.setCursor(0,0); lcd.print("Delayed access"); lcd.setCursor(0,1); lcd.print(countDown); lcd.print("s "); }; 
  return(countDown<=0);  // true = end
}

// displays the message and waits for a button to be pressed
bool message(char *line1, char *line2){ 
  cursorOn = false;
  lcd.setCursor(0,0); lcd.print(line1);
  lcd.setCursor(0,1); lcd.print(line2); printSpaces(maxLineLen-strlen(line2));
  if (lcd_key == btnNONE && lastKeyPress != btnNONE) { lastKeyPress = btnNONE; lcd.clear(); return(true);};   
  dynamicDelay(); return(false);
}

// display a yes/no query
bool yesNo(String message,bool &yes) { 
  cursorOn = false; lcd.setCursor(0,0); lcd.print(message);
  lcd.setCursor(0,1); lcd.print("Select=No Up=Yes");
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
  lcd.print('#'); lcd.print(String(calcSeedCheckSum(menuItem), HEX));
  if (lcd_key == btnUP)   { menuItem = change(menuItem,maxSeed-1,1); loadSeedName(menuItem); };
  if (lcd_key == btnDOWN) { menuItem = change(menuItem,0,-1); loadSeedName(menuItem); };   
  if (lcd_key == btnNONE && lastKeyPress == btnSELECT) { lastKeyPress = btnNONE; lcd.clear(); return(true); }; 
  dynamicDelay(); return(false); 
}  

// enter pin (2)
boolean enterPin(byte seedNum) { 
  char line1[20] = "PIN for "; strcat(line1, seedName[seedNum]); 
//  lcd.setCursor(0,0); lcd.print("Setting"); 
//  lcd.setCursor(0,1); lcd.print("Word "); lcd.print(wordNum+1); lcd.print(":"); 
  if (editLine(line1, "", PIN, 0)) return(true); 
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
    Serial.begin(9600);     
#endif
  
  // for UNO (LCD contrast control)
  TCCR2B = TCCR2B & 0b11111000 | 0x01;    // Set Timer 2 to 31kHz 
  //TCCR2B = TCCR2B & 0b11111000 | 0x03;  // Set Timer 2 to 500Hz

  if(EEPROM.read(0) == formId) {
    loadSeedName(selectedSeed); }  // if finds formatted eeprom flag
  else {
    lcd.println("Formating"); delay(10000); 
    for (byte i = 0; i < maxSeed; i++){
       saveSeedName(i); hashPin(&sha256, i, defPin); 
       savePinHash(i);  saveWords(i); 
    }  
    EEPROM.write(0,formId);  // write formatted eeprom flag
    lcd.clear();
  }
  
  Wire.begin();   // init I2C for external EEPROM (24LC512)
  lcd.setCursor(0,0); lcd.print(sSysVer);
  lcd.setCursor(0,1); lcd.print("Self test ");
  
  // when the checksum is not correct, the word table is not loaded or damaged (use loader script first)
  if (getChecksum()!=-14663) {lcd.print("FAIL! "); delay(6000); }
  
}

void loop() { bool yes; 
if (setupMode) 
  switch(setupItem) {
    case 1: showSetupInfo(); setupItem = 2; PIN.erase(); break;
    case 2: if (enterPin(selectedSeed)) setupItem = 3; break;
    case 3: if (checkPin(selectedSeed)) setupItem = 4; else {setupItem = 1; setupMode = false; }; break;
    case 4: if (yesNo("Change PIN?",yes)) 
      if (yes) setupItem = 5; else setupItem = 7; break;
    case 5: if (enterPin(selectedSeed)) setupItem = 6; break;
    case 6: if (savePinQ(selectedSeed)) setupItem = 7; break;
    case 7: 
      if (yesNo("Change name?",yes)) 
        if (yes) {strcpy(line.txt,seedName[selectedSeed]); line.end(); lcd.clear(); setupItem = 8; } 
        else setupItem = 10; break;  
    case 8: 
      if (editSeedName(selectedSeed)) setupItem = 9; break;
    case 9: 
      if (saveNameQ(selectedSeed)) setupItem = 10; break; 
    case 10: 
      if (yesNo("Change words?",yes)) 
        if (yes) {setupItem = 11; } else {setupItem = 1; menuItem = 1; setupMode = false; }; 
      break;
    case 11: 
      if (showWords(selectedSeed, wordNum)) {//memcpy(line.txt,sword[wordNum],wordLen); line.txt[wordLen]='\0';
          getWord(sword[wordNum]).toCharArray(line.txt,wordLen+1); line.end(); setupItem = 12; };
      if (lcd_key == btnLEFT) {setupItem = 13; lcd.clear(); }; break;  
    case 12: 
      if (editSecureWord(selectedSeed, wordNum)) setupItem = 11; break;
    case 13: 
      if (saveWordsQ(selectedSeed)) {setupItem = 1; menuItem = 1; setupMode = false; lcd.clear();}; break;  
  }
else {
  switch (menuItem) {
    case 1:
      if (runtime < 18) {
          if (message(sSysVer, sAuthor)) menuItem = 2; }
      else {    
          if (message(sSysVer, seedName[selectedSeed])) menuItem = 2; }
    break;
    case 2:
      if (selectSeed(selectedSeed)) {menuItem = 3; PIN.erase(); } ;
    break;  
    case 3:
      if (yesNo("Show BIP39 seed?",yes)) {lcd.clear(); 
        if (yes) menuItem = 22; else menuItem = 4; };
    break; 
    case 4:
      if (yesNo("Forgotten PIN?",yes)) {lcd.clear();
        if (yes) menuItem = 31; else menuItem = 5; }
    break; 
    case 5:
      if (yesNo("Setup?",yes)) {lcd.clear(); 
        if (yes) {setupItem = 1; setupMode = true; } else menuItem = 6; }
    break;    

#ifdef LCD  // LCD display only    
    case 6:
      if (yesNo("LCD control?",yes)) {lcd.clear(); 
        if (yes) menuItem = 61; else menuItem = 1; }
    break;    
#endif

#ifdef OLED  // OLED display only    
    case 6:
      lcd.clear(); menuItem = 1;
    break;    
#endif

    case 22: if (enterPin(selectedSeed)) menuItem = 23; break;
    case 23: if (checkPin(selectedSeed)) menuItem = 24; else menuItem = 3; break;
    case 24: if (showWords(selectedSeed, wordNum)) menuItem = 1; break;

    case 31: // restore access when is PIN loss
      if (delayedAcess()) {menuItem = 32; // reset PIN to default
          hashPin(&sha256, selectedSeed, defPin); savePinHash(selectedSeed); };        
      if (lcd_key == btnNONE && lastKeyPress == btnLEFT) {
          lastKeyPress = btnNONE; menuItem = 1; lcd.clear(); countDown = defaultPinRecoveryDelay; };   
    break;

    case 32: //
      if (message("PIN has been set","to 123")) menuItem=1;
    break;

    case 61:  // Adjust LCD contrast
      cursorOn = false; 
      lcd.setCursor(0,0); lcd.print("Set LCD contrast");
      lcd.setCursor(0,1); lcd.print("Up / Down");
      if (lcd_key == btnUP)   { LCDcontrast = change(LCDcontrast,150,1); analogWrite(pwmout, LCDcontrast); };
      if (lcd_key == btnDOWN) { LCDcontrast = change(LCDcontrast,0,-1); analogWrite(pwmout, LCDcontrast); };
      if (lcd_key == btnNONE && lastKeyPress == btnSELECT) { lastKeyPress = btnNONE; lcd.clear(); menuItem=1; EEPROM.write(addrContr,LCDcontrast);};  
    break; 
  };  
};

scanKey(lcd_key, lastKeyPress, nonePressTime, keyPressTime);
if ((millis()/100)%100 != runtime%100) {runtime++; mainLoop(); };
//if (millis()/100 > runtime) { runtime++; mainLoop(); };
}

/*
// converts boolean to text
char *toOnOff(boolean state){
  if (state) return "on"; else return "off";
}

// Convert unsigned long value to d-digit decimal string in local buffer      
char *u2s(unsigned long x,unsigned d)
{  static char b[16];
   char *p;
   unsigned digits = 0;
   unsigned long t = x;

   do ++digits; while (t /= 10);
   // if (digits > d) d = digits; // uncomment to allow more digits than spec'd
   *(p = b + d) = '\0';
   do *--p = x % 10 + '0'; while (x /= 10);
   while (p != b) *--p = ' ';
   return b;
}
 
// LCD back light control
void setBackLight(bool on){
  if (on) analogWrite(10, LCDcontrast); else analogWrite(10, 0); 
}
*/

