#include <LoRa_E22.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(9, 8);
LoRa_E22 E22(&mySerial);

#define M0 7
#define M1 6
#define BUTTON_PIN 2  // Buton girişi (GND ile tetiklenir)

struct veriler {   
  char deger[15];
} data;

bool lastState = HIGH;  // Butonun son durumu

void setup() {
  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  digitalWrite(M0, LOW);
  digitalWrite(M1, LOW);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Buton pini

  Serial.begin(9600);
  E22.begin();
  delay(500);

  Serial.println("=== VERICI HAZIR ===");
  Serial.println("Pin 2'ye GND verince komut gider");
  Serial.println("Gönderilen: ROLE_ON veya ROLE_OFF");
  Serial.println("=====================");
}

void loop() {
  bool buttonState = digitalRead(BUTTON_PIN);

  // Buton durumu değişti mi?
  if (buttonState != lastState) {
    if (buttonState == LOW) {  // Butona basıldı (GND verildi)
      sprintf(data.deger, "ROLE_ON");
      ResponseStatus rs = E22.sendFixedMessage(0, 2, 18, &data, sizeof(veriler));
      Serial.print("Gönder: ROLE_ON - ");
      Serial.println(rs.getResponseDescription());
    } else {  // Buton bırakıldı
      sprintf(data.deger, "ROLE_OFF");
      ResponseStatus rs = E22.sendFixedMessage(0, 2, 18, &data, sizeof(veriler));
      Serial.print("Gönder: ROLE_OFF - ");
      Serial.println(rs.getResponseDescription());
    }
    lastState = buttonState;
    delay(100);  // Debounce
  }

  // Gelen mesajları kontrol et (debug için)
  while (E22.available() > 1) {
    ResponseStructContainer rsc = E22.receiveMessage(sizeof(veriler));
    struct veriler gelen = *(veriler*)rsc.data;
    Serial.print("Gelen: ");
    Serial.println(gelen.deger);
    rsc.close();
  }
}