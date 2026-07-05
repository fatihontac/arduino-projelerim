#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/* ===== PINLER ===== */
#define RELAY_45_1 25
#define RELAY_45_2 26
#define RELAY_30_1 32
#define RELAY_30_2 33

#define DS_WATER 5
#define DS_ROOM  21

#define POT_WATER 34
#define POT_ROOM  35

#define MODE_PIN 27   // HIGH = EKO , LOW = KON

/* ===== LCD ===== */
LiquidCrystal_I2C lcd(0x27, 16, 2);

/* ===== DS18B20 ===== */
OneWire owWater(DS_WATER);
OneWire owRoom(DS_ROOM);
DallasTemperature waterSensor(&owWater);
DallasTemperature roomSensor(&owRoom);

/* ===== YARDIMCI FONKSİYONLAR ===== */
void allOff() {
  digitalWrite(RELAY_45_1, LOW);
  digitalWrite(RELAY_45_2, LOW);
  digitalWrite(RELAY_30_1, LOW);
  digitalWrite(RELAY_30_2, LOW);
}

void relay45(bool on) {
  digitalWrite(RELAY_45_1, on ? HIGH : LOW);
  digitalWrite(RELAY_45_2, on ? HIGH : LOW);
}

void relay30(bool on) {
  digitalWrite(RELAY_30_1, on ? HIGH : LOW);
  digitalWrite(RELAY_30_2, on ? HIGH : LOW);
}

/* ===== SETUP ===== */
void setup() {
  pinMode(RELAY_45_1, OUTPUT);
  pinMode(RELAY_45_2, OUTPUT);
  pinMode(RELAY_30_1, OUTPUT);
  pinMode(RELAY_30_2, OUTPUT);
  allOff();

  pinMode(MODE_PIN, INPUT_PULLDOWN);

  Wire.begin(23, 22);
  lcd.init();
  lcd.backlight();

  waterSensor.begin();
  roomSensor.begin();
}

/* ===== LOOP ===== */
void loop() {
  waterSensor.requestTemperatures();
  roomSensor.requestTemperatures();

  float waterTemp = waterSensor.getTempCByIndex(0);
  float roomTemp  = roomSensor.getTempCByIndex(0);

  if (waterTemp < -50 || roomTemp < -50) {
    allOff();
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("SENSOR HATASI");
    delay(1000);
    return;
  }

  int targetWater = map(analogRead(POT_WATER), 0, 4095, 20, 80);
  int targetRoom  = map(analogRead(POT_ROOM),  0, 4095, 15, 35);

  bool ecoMode = digitalRead(MODE_PIN); // HIGH = EKO

  /* ===== ODA ÖNCELİĞİ ===== */
  if (roomTemp >= targetRoom + 2.0) {
    allOff();
  } else {

    /* ===== SU SICAKLIĞI KONTROLÜ ===== */
    if (waterTemp < targetWater / 2.0) {
      if (ecoMode) {
        relay45(true);
        relay30(false);
      } else {
        relay45(true);
        relay30(true);
      }
    }
    else if (waterTemp < targetWater + 3.0) {
      if (ecoMode) {
        relay45(false);
        relay30(true);
      } else {
        relay45(true);
        relay30(false);
      }
    }
    else {
      allOff();
    }
  }

  /* ===== LCD ===== */
  lcd.setCursor(0,0);
  lcd.print("Su:");
  lcd.print(waterTemp,0);
  lcd.print((char)223);
  lcd.print(" O:");
  lcd.print(roomTemp,0);
  lcd.print((char)223);

  lcd.setCursor(12,0);
  lcd.print(ecoMode ? "EKO" : "KON");

  lcd.setCursor(0,1);
  lcd.print("HSu:");
  lcd.print(targetWater);
  lcd.print((char)223);
  lcd.print(" HO:");
  lcd.print(targetRoom);
  lcd.print((char)223);

  delay(1000);
}