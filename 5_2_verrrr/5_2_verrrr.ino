//verici
#include <LoRa_E22.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(9, 8);
LoRa_E22 E22(&mySerial);

#define M0 7
#define M1 6
#define INPUT_PIN_D2 2   // 5V kontrol pini

struct veriler {   
  char deger[15];
} data;

bool lastD2State = LOW;
bool isSending = false;
unsigned long sendStartTime = 0;

void setup() {
  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  digitalWrite(M0, LOW);
  digitalWrite(M1, LOW);
  
  pinMode(INPUT_PIN_D2, INPUT);

  Serial.begin(9600);
  E22.begin();
  delay(500);

  Serial.println("=== VERICI HAZIR ===");
  Serial.println("D2=5V iken her 5 sn'de 1, 2 sn tetikleme sinyali");
  Serial.println("Gönderilen: RELAY_ON (2 sn boyunca)");
  Serial.println("=====================");
}

void loop() {
  bool d2State = digitalRead(INPUT_PIN_D2);
  unsigned long currentTime = millis();

  // D2=5V ve daha önce gönderim başlamadıysa
  if (d2State == HIGH && !isSending) {
    isSending = true;
    sendStartTime = currentTime;
    Serial.println("D2=5V algılandı, gönderim başladı.");
  }

  // D2=0V ise gönderimi durdur
  if (d2State == LOW && isSending) {
    isSending = false;
    Serial.println("D2=0V, gönderim durduruldu.");
  }

  // Gönderim aktifse ve 5 saniyede bir tetikle
  if (isSending) {
    if ((currentTime - sendStartTime) >= 5000) {  // 5 saniye oldu mu?
      
      Serial.println(">>> TETİKLEME SİNYALİ GÖNDER (2 sn)");
      
      // 2 saniye boyunca RELAY_ON gönder
      unsigned long pulseStart = currentTime;
      while (millis() - pulseStart < 2000) {
        sprintf(data.deger, "RELAY_ON");
        ResponseStatus rs = E22.sendFixedMessage(0, 2, 18, &data, sizeof(veriler));
        Serial.print("Gönder: RELAY_ON - ");
        Serial.println(rs.getResponseDescription());
        delay(100);  // Kısa aralıklarla tekrarla (güvenlik için)
      }
      
      sendStartTime = currentTime;  // Zamanlayıcıyı sıfırla
      Serial.println("Tetikleme tamamlandı, 5 sn bekleniyor...");
    }
  }

  // Debug: Pin durumunu göster
  static unsigned long lastDebugTime = 0;
  if (currentTime - lastDebugTime >= 10000) {
    Serial.print("D2=");
    Serial.print(d2State ? "5V" : "0V");
    Serial.print(" | Aktif=");
    Serial.println(isSending ? "Evet" : "Hayır");
    lastDebugTime = currentTime;
  }

  delay(50);
}