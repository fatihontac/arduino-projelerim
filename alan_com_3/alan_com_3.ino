#include <LoRa_E22.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(9, 8);
LoRa_E22 E22(&mySerial);

#define M0 7
#define M1 6
#define RELAY_PIN 11  // Röle çıkışı (HIGH ile çeker)

struct veriler {
  char deger[15];
} data;

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
  Serial.println("Röle: Pin 9 (HIGH aktif)");
  Serial.println("Beklenen komutlar: ROLE_ON, ROLE_OFF");
  Serial.println("====================");
}

void loop() {
  while (E22.available() > 1) {
    ResponseStructContainer rsc = E22.receiveMessage(sizeof(veriler));
    struct veriler data = *(veriler*)rsc.data;

    String gelenMesaj = String(data.deger);
    gelenMesaj.trim();

    Serial.print("Gelen Mesaj: ");
    Serial.println(gelenMesaj);

    // === RÖLE KONTROL ===
    if (gelenMesaj == "ROLE_ON") {
      Serial.println(">>> Röle TETİKLENİYOR (HIGH)");
      digitalWrite(RELAY_PIN, HIGH);   // Röle çeker
      delay(1000);                     // 1 saniye bekle
      digitalWrite(RELAY_PIN, LOW);    // Röle bırakır
      Serial.println(">>> Röle bıraktı (LOW)");
    }
    else if (gelenMesaj == "ROLE_OFF") {
      Serial.println(">>> Röle KAPAT (LOW)");
      digitalWrite(RELAY_PIN, LOW);    // Röle pasif
    }
    else {
      Serial.println(">>> Bilinmeyen komut!");
    }

    rsc.close();
  }
}