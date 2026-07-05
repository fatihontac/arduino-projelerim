#include <LoRa_E22.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(9, 8);
LoRa_E22 E22(&mySerial);

#define M0 7
#define M1 6
#define INPUT_PIN_D2 2
#define INPUT_PIN_D5 5

struct veriler {   
  char deger[15];
} data;

bool isSending = false;
unsigned long sendStartTime = 0;
int lastState = -1;  // 0=LOW (AÇ), 1=HIGH (KAPAT)

void setup() {
  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  digitalWrite(M0, LOW);
  digitalWrite(M1, LOW);
  
  pinMode(INPUT_PIN_D2, INPUT);
  pinMode(INPUT_PIN_D5, INPUT);

  Serial.begin(9600);
  E22.begin();
  delay(500);

  Serial.println("=====================================");
  Serial.println("        VERICI HAZIR");
  Serial.println("=====================================");
  Serial.println("D5=5V (ÖNCELİKLİ) -> KAPAT (HIGH)");
  Serial.println("D2=5V ve D5=0V   -> AÇ (LOW)");
  Serial.println("Her 5 sn'de 1, 2 sn sinyal gönderir");
  Serial.println("=====================================");
}

void loop() {
  bool d2State = digitalRead(INPUT_PIN_D2);
  bool d5State = digitalRead(INPUT_PIN_D5);
  unsigned long currentTime = millis();

  // === KARAR MEKANİZMASI ===
  int currentState = 1;  // Varsayılan: KAPALI (HIGH)
  
  if (d5State == HIGH) {
    currentState = 1;  // D5=5V ise KAPAT (HIGH) - ÖNCELİKLİ!
  }
  else if (d2State == HIGH) {
    currentState = 0;  // D2=5V ve D5=0V ise AÇ (LOW)
  }
  else {
    currentState = 1;  // Her ikisi de 0V ise KAPAT (HIGH)
  }

  // === 5 SANİYEDE 1, 2 SANİYE GÖNDERİM ===
  if (currentState != lastState) {
    // Durum değişti, hemen gönderime başla
    isSending = true;
    sendStartTime = currentTime;
    lastState = currentState;
    Serial.print("Durum değişti -> ");
    Serial.println(currentState == 0 ? "AÇ (LOW)" : "KAPAT (HIGH)");
  }

  if (isSending && (currentTime - sendStartTime >= 5000)) {
    // 5 saniye geçti, gönderimi durdur
    isSending = false;
    Serial.println("Gönderim periyodu tamamlandı, 5 sn bekleniyor...");
  }

  if (isSending) {
    // 2 saniye boyunca sürekli gönder
    unsigned long pulseStart = currentTime;
    while (millis() - pulseStart < 2000) {
      if (currentState == 0) {
        sprintf(data.deger, "RELAY_LOW");
        ResponseStatus rs = E22.sendFixedMessage(0, 2, 18, &data, sizeof(veriler));
        Serial.print("Gönder: AÇ (LOW) - ");
        Serial.println(rs.getResponseDescription());
      } else {
        sprintf(data.deger, "RELAY_HIGH");
        ResponseStatus rs = E22.sendFixedMessage(0, 2, 18, &data, sizeof(veriler));
        Serial.print("Gönder: KAPAT (HIGH) - ");
        Serial.println(rs.getResponseDescription());
      }
      delay(100);
    }
    // Gönderim zamanını güncelle
    sendStartTime = currentTime;
  }

  // === DEBUG: Pin durumlarını göster ===
  static unsigned long lastDebugTime = 0;
  if (currentTime - lastDebugTime >= 10000) {
    Serial.print("D2=");
    Serial.print(d2State ? "5V" : "0V");
    Serial.print(" | D5=");
    Serial.print(d5State ? "5V" : "0V");
    Serial.print(" | Karar: ");
    Serial.println(currentState == 0 ? "AÇ (LOW)" : "KAPAT (HIGH)");
    lastDebugTime = currentTime;
  }

  delay(50);
}