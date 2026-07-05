#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"

// === LCD AYAR ===
LiquidCrystal_I2C lcd(0x27, 16, 2);

// === DHT11 AYAR ===
#define DHTPIN_ORTAM 5     // Ortam sıcaklığı sensörü pini
#define DHTPIN_SU    18    // Su sıcaklığı sensörü pini (örnek, sen kendi pinini yaz)
#define DHTTYPE DHT11

DHT dhtOrt(DHTPIN_ORTAM, DHTTYPE);
DHT dhtSu(DHTPIN_SU, DHTTYPE);

// === RÖLELER ===
#define RELAY_KADEME1 12
#define RELAY_KADEME2 26
#define RELAY_POMPA   27
#define RELAY_VANA    33

// === GİRİŞLER ===
#define POT_SU 34
#define POT_ODA 35
#define SU_SWITCH 32   // basınç 5V gelirse su var

void setup() {
  // Röle çıkışları
  pinMode(RELAY_KADEME1, OUTPUT);
  pinMode(RELAY_KADEME2, OUTPUT);
  pinMode(RELAY_POMPA, OUTPUT);
  pinMode(RELAY_VANA, OUTPUT);

  digitalWrite(RELAY_KADEME1, LOW);
  digitalWrite(RELAY_KADEME2, LOW);
  digitalWrite(RELAY_POMPA, LOW);
  digitalWrite(RELAY_VANA, LOW);

  // Girişler
  pinMode(POT_SU, INPUT);
  pinMode(POT_ODA, INPUT);
  pinMode(SU_SWITCH, INPUT);

  // LCD başlat
  Wire.begin(21, 22); // ESP32 SDA=21, SCL=22
  lcd.init();
  lcd.backlight();

  // DHT sensörleri başlat
  dhtOrt.begin();
  dhtSu.begin();
}

void loop() {
  // === POT DEĞERLERİ ===
  int potSu = analogRead(POT_SU);
  int potOda = analogRead(POT_ODA);

  int hedefSu = map(potSu, 0, 4095, 0, 80);
  int hedefOda = map(potOda, 0, 4095, 0, 40);

  // === SICAKLIK ÖLÇÜMÜ ===
  float ortamTemp = dhtOrt.readTemperature();
  float suTemp    = dhtSu.readTemperature();

  if (isnan(ortamTemp)) ortamTemp = -99; // hata için özel değer
  if (isnan(suTemp))    suTemp    = -99;

  // === SU VAR MI? ===
  int suVar = digitalRead(SU_SWITCH);

  if (suVar == LOW) {
    digitalWrite(RELAY_VANA, HIGH);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  SU YOK !!!  ");
    lcd.setCursor(0, 1);
    lcd.print("DOLUM BEKLENIYOR");

    digitalWrite(RELAY_KADEME1, LOW);
    digitalWrite(RELAY_KADEME2, LOW);

    delay(500);
    return;
  }

  // Su var
  digitalWrite(RELAY_VANA, LOW);
  digitalWrite(RELAY_POMPA, HIGH);

  // === REZISTANS KONTROLU ===
  if (ortamTemp > hedefOda + 5) {
    digitalWrite(RELAY_KADEME1, LOW);
    digitalWrite(RELAY_KADEME2, LOW);
  }

  if (suTemp < hedefSu / 2) {
    digitalWrite(RELAY_KADEME1, HIGH);
    digitalWrite(RELAY_KADEME2, HIGH);
  } else if (suTemp < hedefSu - 10) {
    digitalWrite(RELAY_KADEME1, HIGH);
    digitalWrite(RELAY_KADEME2, LOW);
  } else if (suTemp > hedefSu + 5) {
    digitalWrite(RELAY_KADEME1, LOW);
    digitalWrite(RELAY_KADEME2, LOW);
  }

  // === LCD GÖRÜNTÜ ===
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Su:");
  if (suTemp == -99) lcd.print("--");
  else {
    lcd.print(suTemp, 1);
    lcd.print((char)223);
  }
  lcd.print(" H:");
  lcd.print(hedefSu);

  lcd.setCursor(0, 1);
  lcd.print("Ort:");
  if (ortamTemp == -99) lcd.print("--");
  else {
    lcd.print(ortamTemp, 1);
    lcd.print((char)223);
  }
  lcd.print(" O.H:");
  lcd.print(hedefOda);

  delay(2000); // DHT için en az 2 saniye bekleme
}