#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/* ===== PINLER ===== */
#define RELAY_45_1 25   // 4.5 kW
#define RELAY_45_2 26   // 4.5 kW
#define RELAY_3KW   32  // 3 kW
#define RELAY_POMPA 33  // Pompa

#define DS_WATER 5
#define DS_ROOM  21

#define POT_WATER 34
#define POT_ROOM  35

LiquidCrystal_I2C lcd(0x27, 16, 2);

OneWire owWater(DS_WATER);
OneWire owRoom(DS_ROOM);
DallasTemperature waterSensor(&owWater);
DallasTemperature roomSensor(&owRoom);

bool odaIzni = true;

/* ===== RÖLE FONKSİYONLARI ===== */
void tumIsitmaKapat() {
  digitalWrite(RELAY_45_1, LOW);
  digitalWrite(RELAY_45_2, LOW);
  digitalWrite(RELAY_3KW, LOW);
}

void rezistans3kW(bool ac) {
  digitalWrite(RELAY_3KW, ac ? HIGH : LOW);
}

void rezistans45kW(bool ac) {
  digitalWrite(RELAY_45_1, ac ? HIGH : LOW);
  digitalWrite(RELAY_45_2, ac ? HIGH : LOW);
}

void pompaAc() {
  digitalWrite(RELAY_POMPA, HIGH);
}

/* ===== SETUP ===== */
void setup() {
  pinMode(RELAY_45_1, OUTPUT);
  pinMode(RELAY_45_2, OUTPUT);
  pinMode(RELAY_3KW, OUTPUT);
  pinMode(RELAY_POMPA, OUTPUT);

  tumIsitmaKapat();
  pompaAc();   // 🔴 POMPA HER ZAMAN AÇIK

  Wire.begin(23, 22);
  lcd.init();
  lcd.backlight();

  waterSensor.begin();
  roomSensor.begin();

  analogReadResolution(10);
}

/* ===== LOOP ===== */
void loop() {

  waterSensor.requestTemperatures();
  roomSensor.requestTemperatures();

  float suTemp  = waterSensor.getTempCByIndex(0);
  float odaTemp = roomSensor.getTempCByIndex(0);

  int hedefSu  = map(analogRead(POT_WATER), 0, 1023, 0, 100);
  int hedefOda = map(analogRead(POT_ROOM),  0, 1023, 0, 100);

  /* ===== HEDEF 0 DURUMU ===== */
  if (hedefSu == 0 && hedefOda == 0) {
    tumIsitmaKapat();
    lcdGuncelle(suTemp, odaTemp, hedefSu, hedefOda);
    delay(200);
    return;
  }

  /* ===== ODA KONTROLÜ ===== */
  if (odaTemp >= hedefOda + 2) {
    odaIzni = false;
    tumIsitmaKapat();
  }
  if (odaTemp < hedefOda) {
    odaIzni = true;
  }

  /* ===== SU KONTROLÜ ===== */
  if (odaIzni && hedefSu > 0) {

    float yariHedef = hedefSu / 2.0;

    if (suTemp < yariHedef) {
      rezistans3kW(true);
      rezistans45kW(true);
    }
    else if (suTemp < hedefSu - 10) {
      rezistans3kW(true);
      rezistans45kW(false);
    }
    else if (suTemp >= hedefSu + 3) {
      rezistans3kW(false);
      rezistans45kW(false);
    }
    else if (suTemp < hedefSu) {
      rezistans3kW(true);
      rezistans45kW(false);
    }
  }

  lcdGuncelle(suTemp, odaTemp, hedefSu, hedefOda);
  delay(200);
}

/* ===== LCD ===== */
void lcdGuncelle(float su, float oda, int hSu, int hOda) {
  lcd.setCursor(0,0);
  lcd.print("Su:");
  lcd.print(su,0);
  lcd.print((char)223);
  lcd.print(" O:");
  lcd.print(oda,0);
  lcd.print((char)223);
  lcd.print(" ");

  lcd.setCursor(0,1);
  lcd.print("HSu:");
  lcd.print(hSu);
  lcd.print((char)223);
  lcd.print(" HO:");
  lcd.print(hOda);
  lcd.print((char)223);
  lcd.print(" ");
}
