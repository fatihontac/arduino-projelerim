#include <LoRa_E22.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(9, 8);
LoRa_E22 E22(&mySerial);

#define M0 7
#define M1 6
#define RELAY_PIN 11

struct veriler {
  char deger[15];
} data;

unsigned long lastSignalTime = 0;
bool relayState = false;  // false=kapalı, true=açık

void setup() {
  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  digitalWrite(M0, LOW);
  digitalWrite(M1, LOW);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // Başlangıçta KAPALI (HIGH)

  Serial.begin(9600);
  E22.begin();
  delay(500);

  Serial.println("=====================================");
  Serial.println("        ALICI HAZIR");
  Serial.println("=====================================");
  Serial.println("Röle: Pin 11");
  Serial.println("RELAY_LOW  -> LOW  (Röle AÇIK)");
  Serial.println("RELAY_HIGH -> HIGH (Röle KAPALI)");
  Serial.println("60 sn sinyal gelmezse OTOMATİK KAPATIR!");
  Serial.println("=====================================");
}

void loop() {
  unsigned long currentTime = millis();

  while (E22.available() > 1) {
    ResponseStructContainer rsc = E22.receiveMessage(sizeof(veriler));
    struct veriler data = *(veriler*)rsc.data;

    String gelenMesaj = String(data.deger);
    gelenMesaj.trim();

    Serial.print("Gelen Mesaj: ");
    Serial.println(gelenMesaj);

    if (gelenMesaj == "RELAY_LOW") {
      Serial.println(">>> Röle AÇ (LOW)");
      digitalWrite(RELAY_PIN, LOW);
      relayState = true;
      lastSignalTime = currentTime;
    }
    else if (gelenMesaj == "RELAY_HIGH") {
      Serial.println(">>> Röle KAPAT (HIGH)");
      digitalWrite(RELAY_PIN, HIGH);
      relayState = false;
      lastSignalTime = currentTime;
    }
    else {
      Serial.println(">>> Bilinmeyen komut!");
    }

    rsc.close();
  }

  // === GÜVENLİK ZAMANLAYICISI: 60 SN SİNYAL GELMEZSE KAPAT ===
  if (relayState && (currentTime - lastSignalTime >= 60000)) {
    Serial.println(">>> ⚠️ 60 SN SİNYAL GELMEDİ! Röle KAPAT (HIGH)");
    digitalWrite(RELAY_PIN, HIGH);
    relayState = false;
  }

  // === DEBUG: Her 10 saniyede durum göster ===
  static unsigned long lastDebugTime = 0;
  if (currentTime - lastDebugTime >= 10000) {
    Serial.print("Röle: ");
    Serial.print(relayState ? "AÇIK (LOW)" : "KAPALI (HIGH)");
    if (relayState) {
      Serial.print(" | Sinyal üzerinden: ");
      Serial.print((currentTime - lastSignalTime) / 1000);
      Serial.print(" sn / 60 sn");
    }
    Serial.println();
    lastDebugTime = currentTime;
  }

  delay(50);
}