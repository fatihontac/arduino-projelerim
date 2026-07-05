#include <LoRa_E22.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(9, 8);
LoRa_E22 E22(&mySerial);

#define M0 7
#define M1 6
#define RELAY_PIN 11   // Röle çıkışı

struct veriler {
  char deger[15];
} data;

unsigned long lastSignalTime = 0;
bool relayState = false;  // Röle durumu (false=kapalı, true=açık)

void setup() {
  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  digitalWrite(M0, LOW);
  digitalWrite(M1, LOW);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Başlangıçta röle kapalı

  Serial.begin(9600);
  E22.begin();
  delay(500);

  Serial.println("=== ALICI HAZIR ===");
  Serial.println("Röle: Pin 11");
  Serial.println("RELAY_ON  -> Röle çeker (LOW)");
  Serial.println("1 dakika sinyal gelmezse röle kapanır");
  Serial.println("====================");
}

void loop() {
  unsigned long currentTime = millis();

  // === GELEN MESAJI KONTROL ET ===
  while (E22.available() > 1) {
    ResponseStructContainer rsc = E22.receiveMessage(sizeof(veriler));
    struct veriler data = *(veriler*)rsc.data;

    String gelenMesaj = String(data.deger);
    gelenMesaj.trim();

    Serial.print("Gelen Mesaj: ");
    Serial.println(gelenMesaj);

    if (gelenMesaj == "RELAY_ON") {
      Serial.println(">>> Röle ÇEK (LOW)");
      digitalWrite(RELAY_PIN, LOW);
      relayState = true;
      lastSignalTime = currentTime;  // Sinyal zamanını güncelle
    } else {
      Serial.println(">>> Bilinmeyen komut!");
    }

    rsc.close();
  }

  // === GÜVENLİK ZAMANLAYICISI: 1 DAKİKA SİNYAL GELMEZSE KAPAT ===
  if (relayState && (currentTime - lastSignalTime >= 60000)) {
    Serial.println(">>> 1 DAKİKA SİNYAL GELMEDİ! Röle KAPAT (HIGH)");
    digitalWrite(RELAY_PIN, HIGH);
    relayState = false;
  }

  // Debug: Her 10 saniyede durum yaz
  static unsigned long lastDebugTime = 0;
  if (currentTime - lastDebugTime >= 10000) {
    Serial.print("Röle: ");
    Serial.print(relayState ? "AÇIK" : "KAPALI");
    if (relayState) {
      Serial.print(" | Sinyal üzerinden: ");
      Serial.print((currentTime - lastSignalTime) / 1000);
      Serial.print(" sn");
    }
    Serial.println();
    lastDebugTime = currentTime;
  }

  delay(50);
}