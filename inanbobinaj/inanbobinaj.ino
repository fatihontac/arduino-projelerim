#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ================= LCD =================
#define LCD_ADDRESS 0x27   // Eğer çalışmazsa 0x3F dene
LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

// ================= DHT =================
#define DHTPIN 4       // DHT11 DATA pini D4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Wire.begin();        // Nano için I2C başlat
  lcd.begin(16, 2);    // init yerine begin daha stabil
  lcd.backlight();
  dht.begin();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LUTFEN BEKLEYIN");
  lcd.setCursor(0, 1);
  lcd.print("SISTEM BASLIYOR");
  delay(3000);
  lcd.clear();
}

void loop() {

  // === Firma Bilgisi ===
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("INAN BOBINAJ");
  lcd.setCursor(0, 1);
  lcd.print("TEL:5370604220");
  delay(5000);

  // === DHT Okuma ===
  float sicaklik = dht.readTemperature();
  float nem = dht.readHumidity();

  lcd.clear();

  if (isnan(sicaklik) || isnan(nem)) {
    lcd.setCursor(0, 0);
    lcd.print("SENSOR HATASI");
    lcd.setCursor(0, 1);
    lcd.print("TEKRAR DENEYIN");
  } else {

    String tempStr = "SICAKLIK:" + String((int)sicaklik) + "C";
    int pos = (16 - tempStr.length()) / 2;
    lcd.setCursor(pos, 0);
    lcd.print(tempStr);

    String humStr = "NEM:" + String((int)nem) + "%";
    pos = (16 - humStr.length()) / 2;
    lcd.setCursor(pos, 1);
    lcd.print(humStr);
  }

  delay(4000);
}