#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// I2C adresin büyük ihtimalle 0x27 veya 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Wire.begin(21, 22);       // SDA = 21, SCL = 22
  Wire.setClock(100000);   // I2C yavaslat (SART)

  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("ESP32 LCD TEST");

  lcd.setCursor(0, 1);
  lcd.print("Basliyor...");
  delay(2000);
}

void loop() {
  static int sayac = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CALISIYOR");

  lcd.setCursor(0, 1);
  lcd.print("Sayac: ");
  lcd.print(sayac);

  sayac++;
  delay(1000);
}
