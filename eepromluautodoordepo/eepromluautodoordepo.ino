#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ===== KEYPAD =====
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {5,6,7,8};
byte colPins[COLS] = {9,10,11,12};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ===== PINLER =====
#define RELAY 4
#define BUZZER 3

// ===== BUFFER =====
char input[20];
byte inputIndex = 0;

char password[20] = "1234";

const char masterAdd[] = "05416932009";
const char masterDelete[] = "*05416932009*";

// ===== ZAMAN =====
unsigned long lockStart = 0;
bool locked = false;
int wrongCount = 0;

// ===== EKRAN ZAMAN AŞIMI =====
unsigned long lastActivity = 0;
const unsigned long screenTimeout = 10000; 
bool screenOff = false;

enum Mode { NORMAL, ADD_MODE, DELETE_MODE };
Mode mode = NORMAL;

// ===== EEPROM FONKSIYONLARI =====
void savePasswordToEEPROM() {
  for (int i = 0; i < 20; i++) {
    EEPROM.write(i, password[i]);
  }
}

void loadPasswordFromEEPROM() {
  for (int i = 0; i < 20; i++) {
    password[i] = EEPROM.read(i);
  }

  if (password[0] == 0xFF || password[0] == '\0') {
    strcpy(password, "1234");
  }
}

void clearInput() {
  memset(input, 0, sizeof(input));
  inputIndex = 0;
}

void setup() {
  lcd.begin(16,2);
  lcd.backlight();

  pinMode(RELAY, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(RELAY, HIGH);

  loadPasswordFromEEPROM();   // <<< EKLENDI

  lcd.print("Sifre gir:");
  lastActivity = millis();
}

void loop() {

  if (!screenOff && millis() - lastActivity > screenTimeout) {
    lcd.noBacklight();
    screenOff = true;
  }

  if (locked && millis() - lockStart >= 5000) {
    locked = false;
    wrongCount = 0;
    lcd.clear();
    lcd.print("Sifre gir:");
  }

  if (locked) return;

  char key = keypad.getKey();
  if (!key) return;

  lastActivity = millis();
  if (screenOff) {
    lcd.backlight();
    screenOff = false;
  }

  if (key == 'C') {
    if (inputIndex > 0) {
      inputIndex--;
      input[inputIndex] = '\0';
      lcd.setCursor(0,1);
      lcd.print("                ");
      lcd.setCursor(0,1);
      lcd.print(input);
    }
    return;
  }

  if (mode == NORMAL) {

    if (key == '#') {

      if (strcmp(input, password) == 0 && strlen(password) > 0) {

        lcd.clear();
        lcd.print("Sifre dogru");

        digitalWrite(RELAY, LOW);
        digitalWrite(BUZZER, HIGH);
        delay(1800);
        digitalWrite(RELAY, HIGH);
        digitalWrite(BUZZER, LOW);

        wrongCount = 0;

        clearInput();
        delay(1000);
        lcd.clear();
        lcd.print("Sifre gir:");
        return;

      } else {

        wrongCount++;
        lcd.clear();
        lcd.print("Sifre yanlis");

        if (wrongCount >= 3) {
          lcd.clear();
          lcd.print("dunya bostur lo");
          locked = true;
          lockStart = millis();
          clearInput();
          return;
        }

        clearInput();
        delay(1000);
        lcd.clear();
        lcd.print("Sifre gir:");
        return;
      }
    }

    if (inputIndex < sizeof(input) - 1) {
      input[inputIndex++] = key;
      input[inputIndex] = '\0';
      lcd.setCursor(0,1);
      lcd.print(input);
    }

    if (strcmp(input, masterAdd) == 0) {
      mode = ADD_MODE;
      clearInput();
      lcd.clear();
      lcd.print("Yeni sifre:");
    }

    if (strcmp(input, masterDelete) == 0) {
      mode = DELETE_MODE;
      clearInput();
      lcd.clear();
      lcd.print("Silinecek:");
    }
  }

  else if (mode == ADD_MODE) {

    if (key == '#') {
      strcpy(password, input);
      savePasswordToEEPROM();   // <<< EKLENDI

      lcd.clear();
      lcd.print("Sifre kaydedildi");
      delay(1500);
      mode = NORMAL;
      clearInput();
      lcd.clear();
      lcd.print("Sifre gir:");
      return;
    }

    if (inputIndex < sizeof(input) - 1) {
      input[inputIndex++] = key;
      input[inputIndex] = '\0';
      lcd.setCursor(0,1);
      lcd.print(input);
    }
  }

  else if (mode == DELETE_MODE) {

    if (key == '#') {

      if (strcmp(input, password) == 0) {
        password[0] = '\0';
        savePasswordToEEPROM();   // <<< EKLENDI
        lcd.clear();
        lcd.print("Sifre silindi");
      } else {
        lcd.clear();
        lcd.print("Yanlis sifre");
      }

      delay(1500);
      mode = NORMAL;
      clearInput();
      lcd.clear();
      lcd.print("Sifre gir:");
      return;
    }

    if (inputIndex < sizeof(input) - 1) {
      input[inputIndex++] = key;
      input[inputIndex] = '\0';
      lcd.setCursor(0,1);
      lcd.print(input);
    }
  }
}
