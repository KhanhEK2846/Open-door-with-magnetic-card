#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <EEPROM.h>
#include <MFRC522.h>
#include <Servo.h>
#include <LowPower.h>

#define RST_PIN 9
#define SS_PIN 10
#define speakerPin 8
#define NOTE 4978
#define servoPin 7
#define buttonPin 6
#define wakeUpPin 2
#define System 4
#define ForceOpen 3


LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo servo;

int UID[4], i;
//const int OUID[4] = { 85, 197, 5, 173 };

int Count_Wrong = 0;
int Cur_Count_Wrong = 0;
bool Welcome = true;  //Flag thÃ´ng bÃ¡o láº§n Ä‘áº§u sá»­ dá»¥ng
bool Scan = true;
int KT_ID = 0;                     //Kiá»ƒm tra káº¿t quáº£ check tháº»
unsigned long Wrong_Time_Sur = 0;  //Thá»i gian chá» giá»¯a 2 láº§n quÃ©t tháº»
unsigned long Open_Time = 0;
bool Alert_Sig = false;  //TÃ­n hiá»‡u cáº£nh bÃ¡o xÃ¢m nháº­p
int Key_Master;          //
int Owner_Key;
bool KT_Owner_Key;
int pos = 0;  // biáº¿n pos dÃ¹ng Ä‘á»ƒ lÆ°u tá»a Ä‘á»™ cÃ¡c Servo

void setup() {
  lcd.init();
  servo.attach(servoPin);
  pinMode(System, OUTPUT);
  digitalWrite(System, HIGH);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(wakeUpPin, INPUT_PULLUP);
  pinMode(ForceOpen, INPUT_PULLUP);

  Key_Master = EEPROM.read(4);
  Owner_Key = EEPROM.read(9);
  servo.write(pos);  //Äáº·t má»‘c ban Ä‘áº§u cho servo
  /*Serial.begin(9600);
  while (!Serial)
    ;*/
  SPI.begin();
  mfrc522.PCD_Init();
  delay(4);

  //mfrc522.PCD_DumpVersionToSerial();
}

void loop() {

  Check_RFID();  //Kiá»ƒm tra giao tiáº¿p giá»¯a RFID vá»›i Arduino
  Nap();
  Notify();  //In ra thÃ´ng bÃ¡o
  Force_Open_Door();
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!mfrc522.PICC_IsNewCardPresent()) {
    Reset_Wrong();  //Sau 1 thá»i gian khÃ´ng quÃ©t thÃ¬ reset vá» ban Ä‘áº§u.
    return;
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  mfrc522.PICC_HaltA();  //Dá»«ng Ä‘á»c tháº»
  //mfrc522.PICC_DumpToSerial(&(mfrc522.uid));

  if (Owner_Key != 1) {  //Chá»n tháº» ID Owner
    Init_Owner_Key();
    Welcome = true;
    return;
  }

  if (Key_Master != 1) {  //Chá»n tháº» ID master
    Init_Master_Key();
    Scan = true;
    return;
  }

  CheckCard();

  lcd.clear();
  lcd.backlight();


  if (KT_ID == 0) {
    if (!digitalRead(buttonPin))
      Change_ID_Master();
    else
      OpenDoor();
  } else {
    if (Count_Wrong >= 3)
      Alert_Wrong();
    else
      TryAgain();
    KT_ID = 0;
  }



  Scan = true;
}

void Nap() {

  if (Owner_Key == 1 && Key_Master == 1) {
    LowPower.idle(SLEEP_250MS, ADC_ON, TIMER2_ON, TIMER1_ON, TIMER0_ON, SPI_ON, USART0_ON, TWI_ON);
  }
}


void Init_Master_Key() {
  for (byte i = 0; i < mfrc522.uid.size; ++i) {
    int tmp;
    tmp = mfrc522.uid.uidByte[i];
    EEPROM.write(i, tmp);
    delay(5);
  }
  EEPROM.write(4, 1);
  delay(5);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Verifying...");
  Key_Master = 1;
  delay(3000);
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Successful");
  delay(1000);
  Clear_ID();
}

void Init_Owner_Key() {
  for (byte i = 0; i < mfrc522.uid.size; ++i) {
    int tmp;
    tmp = mfrc522.uid.uidByte[i];
    EEPROM.write(i + 5, tmp);
    delay(5);
  }
  EEPROM.write(9, 1);
  delay(5);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Verifying...");
  Owner_Key = 1;
  delay(3000);
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Successful");
  delay(1000);
  Clear_ID();
}

void Check_RFID() {
  int R_S = 0;
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  if ((v == 0x00) || (v == 0xFF)) {
    lcd.clear();
    lcd.backlight();
    lcd.setCursor(1, 0);
    lcd.print("Communication");
    lcd.setCursor(5, 1);
    lcd.print("Failure");
    while (1) {
      v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
      if ((v != 0x00) && (v != 0xFF) && digitalRead(ForceOpen)) {
        lcd.clear();
        lcd.backlight();
        lcd.setCursor(1, 0);
        lcd.print("Reconnecting...");
        delay(4000);
        digitalWrite(System, LOW);
      }
      Force_Open_Door();
      if (!digitalRead(buttonPin)) {
        while (!digitalRead(buttonPin)) {}
        ++R_S;
        if (R_S == 7) {
          Reset_System();
        }
        if (R_S == 8) {
          Disable_Reset_System();
        }
        delay(500);
      }
    }
  }
}

void Disable_Reset_System() {
  EEPROM.write(4, 1);
  delay(5);
  EEPROM.write(9, 1);
  delay(5);
}

void Reset_System() {
  EEPROM.write(4, 255);
  delay(5);
  EEPROM.write(9, 255);
  delay(5);
}

void Change_ID_Master() {
  Count_Wrong = 0;
  if (KT_Owner_Key) {
    EEPROM.write(4, 255);
    delay(5);
    lcd.setCursor(5, 0);
    lcd.print("Reset");
    lcd.setCursor(3, 1);
    lcd.print("ID Master");
    delay(1500);
    lcd.clear();
    lcd.setCursor(3, 0);
    Key_Master = 255;
    Welcome = true;
    lcd.print("Successful");
    delay(750);
    KT_Owner_Key = false;
  } else {
    lcd.clear();
    lcd.setCursor(5, 0);
    lcd.print("Invalid");
    lcd.setCursor(5, 1);
    lcd.print("ID card");
    delay(750);
  }
}

void Notify() {
  if (Owner_Key != 1 && Welcome) {
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("Select a");
    lcd.setCursor(4, 1);
    lcd.print("ID Owner");
    Welcome = false;
  }
  if (Key_Master != 1 && Welcome) {
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("Select a");
    lcd.setCursor(4, 1);
    lcd.print("ID Master");
    Welcome = false;
  }
  if (Owner_Key == 1 && Key_Master == 1 && Count_Wrong == 0 && Scan) {
    lcd.clear();
    lcd.noBacklight();
    lcd.setCursor(4, 0);
    lcd.print("Scan card");
    lcd.setCursor(3, 1);
    lcd.print("to open door");
    Scan = false;
  }
}


void CheckCard() {

  for (byte i = 0; i < mfrc522.uid.size; ++i) {
    UID[i] = mfrc522.uid.uidByte[i];
  }

  for (int i = 0; i < mfrc522.uid.size; ++i) {  //Kiem tra Owner_Key
    int tmp = EEPROM.read(i + 5);
    if (tmp != UID[i]) {
      KT_ID = 1;
      break;
    }
  }

  if (KT_ID == 0) {  //phat hien owner key
    KT_Owner_Key = true;
    Clear_ID();
    return;
  } else
    KT_ID = 0;

  for (int i = 0; i < mfrc522.uid.size; ++i) {  //Kiem tra Master_key
    int tmp = EEPROM.read(i);
    if (tmp != UID[i]) {
      KT_ID = 1;
      break;
    }
  }
  Clear_ID();
}


void OpenDoor() {
  noTone(speakerPin);
  Count_Wrong = 0;
  Alert_Sig = false;
  lcd.setCursor(2, 0);
  lcd.print("Door is open");
  servo.write(90);
  delay(2000);
  lcd.noBacklight();

  attachInterrupt(0, wakeUp, LOW);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  detachInterrupt(0);
  lcd.backlight();

  CloseDoor();
}

void Force_Open_Door() {
  if (!digitalRead(ForceOpen)) {
    unsigned long t = millis();
    unsigned long c_t = 0;
    while (!digitalRead(ForceOpen)) {
      c_t = (unsigned long)(millis() - t);
      if ((c_t > 1000) && !digitalRead(ForceOpen)) break;
    }
    if ((c_t < 1000))
      return;
    lcd.clear();
    lcd.backlight();
    OpenDoor();
    Scan = true;
  }
}

void wakeUp() {
  // Nothing here
}


void TryAgain() {
  ++Count_Wrong;
  lcd.setCursor(3, 0);
  lcd.print("Try Again!");
  lcd.setCursor(2, 1);
  lcd.print(4 - Count_Wrong);
  lcd.setCursor(4, 1);
  lcd.print(" more tries");
}

void Alert_Wrong() {
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("ATTENTION !!!");
  lcd.setCursor(0, 1);
  lcd.print("Detect intruders");
  Alert_Sig = true;
  tone(speakerPin, NOTE);
}

void CloseDoor() {
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("Door is close");
  servo.write(pos);
  delay(1000);
}

void Reset_Wrong() {
  if (Alert_Sig) {  //Náº¿u cÃ³ tÃ­n hiá»‡u Alert thÃ¬ khÃ´ng cháº¡y hÃ m nÃ y
    Wrong_Time_Sur = 0;
    return;
  }
  if (Count_Wrong == 0)  //Náº¿u khÃ´ng cÃ³ bÃ¡o lá»—i thÃ¬ khÃ´ng cháº¡y hÃ m nÃ y
    return;

  if (Cur_Count_Wrong != Count_Wrong) {  //Náº¿u ID khÃ´ng há»£p lá»‡ (Sá»‘ láº§n sai > 0)
    Wrong_Time_Sur = millis();           //Äáº·t thá»i gian chá» cho láº§n quÃ©t láº¡i tiáº¿p theo
    Cur_Count_Wrong = Count_Wrong;
  }
  if ((unsigned long)(millis() - Wrong_Time_Sur) > 300000) {
    Count_Wrong = 0;
    Wrong_Time_Sur = 0;
    Cur_Count_Wrong = 0;
  }
}


void Clear_ID() {
  for (int i = 0; i < mfrc522.uid.size; ++i)
    UID[i] = 0;
}