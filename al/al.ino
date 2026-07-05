#include <LoRa_E22.h>
#include <SoftwareSerial.h>  // ← Doğru yazım!

SoftwareSerial mySerial(9, 8);  // ← Doğru yazım!
LoRa_E22 E22(&mySerial);

#define M0 7
#define M1 6
#define RELAY_PIN 11

#define SIFRE "asdf0987654321"

struct veriler {
  char sifre[20];
  char komut[10];
} data;

unsigned long lastSignalTime = 0;
bool relayState = false;

void setup() {
  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  digitalWrite(M0, LOW);
  digitalWrite(M1, LOW);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Başlangıçta kapalı

  Serial.begin(9600);
  E22.begin();
  delay(500);

  Serial.println("=====================================");
  Serial.println("        ALICI HAZIR");
  Serial.println("=====================================");
  Serial.println("Röle: Pin 11 (LOW=Kapalı, HIGH=Açık)");
  Serial.println("Şifre: asdf0987654321");
  Serial.println("60 sn sinyal gelmezse otomatik kapatır!");
  Serial.println("=====================================");
}

void loop() {
  unsigned long currentTime = millis();

  while (E22.available() > 1) {
    ResponseStructContainer rsc = E22.receiveMessage(sizeof(veriler));
    struct veriler data = *(veriler*)rsc.data;

    String gelenSifre = String(data.sifre);
    String gelenKomut = String(data.komut);
    gelenSifre.trim();
    gelenKomut.trim();

    Serial.print("Gelen Şifre: ");
    Serial.print(gelenSifre);
    Serial.print(" | Komut: ");
    Serial.println(gelenKomut);

    if (gelenSifre == SIFRE) {
      Serial.println("✅ Şifre DOĞRU!");

      if (gelenKomut == "ON") {
        Serial.println(">>> Röle AÇ (HIGH)");
        digitalWrite(RELAY_PIN, HIGH);
        relayState = true;
        lastSignalTime = currentTime;
      }
      else if (gelenKomut == "OFF") {
        Serial.println(">>> Röle KAPAT (LOW)");
        digitalWrite(RELAY_PIN, LOW);
        relayState = false;
        lastSignalTime = currentTime;
      }
    } else {
      Serial.println("❌ Şifre YANLIŞ!");
    }

    rsc.close();
  }

  // 60 sn sinyal gelmezse kapat
  if (relayState && (currentTime - lastSignalTime >= 60000)) {
    Serial.println(">>> ⚠️ 60 SN SİNYAL GELMEDİ! Röle KAPATILIYOR");
    digitalWrite(RELAY_PIN, LOW);
    relayState = false;
  }

  delay(50);
}