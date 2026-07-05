//verici denendi çalışan kod
#include <LoRa_E22.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(9, 8);
LoRa_E22 E22(&mySerial);

#define M0 7
#define M1 6
#define INPUT_PIN_D2 2   // 5V kontrol pini (Röle LOW)
#define INPUT_PIN_D5 5   // 5V kontrol pini (Röle HIGH)

struct veriler {   
  char deger[15];
} data;

int lastState = -1;  // Son gönderilen durum (0=LOW, 1=HIGH)

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

  Serial.println("=== VERICI HAZIR ===");
  Serial.println("D2=5V  -> Alıcı Pin11 LOW  (Röle çeker)");
  Serial.println("D5=5V  -> Alıcı Pin11 HIGH (Röle bırakır)");
  Serial.println("Her 5 dakikada 1 gönderim");
  Serial.println("=====================");
}

void loop() {
  static unsigned long lastSendTime = 0;
  unsigned long currentTime = millis();
  
  // D2 ve D5 pinlerini oku
  bool d2State = digitalRead(INPUT_PIN_D2);
  bool d5State = digitalRead(INPUT_PIN_D5);
  
  int currentState = -1;
  
  // Öncelik: D2=5V ise LOW gönder
  if (d2State == HIGH) {
    currentState = 0;  // LOW gönder
  }
  // Değilse D5=5V ise HIGH gönder
  else if (d5State == HIGH) {
    currentState = 1;  // HIGH gönder
  }
  
  // Durum değişti VEYA 5 dakika (500 ms) geçtiyse gönder
  if (currentState != -1 && (currentState != lastState || (currentTime - lastSendTime >= 300000))) {
    
    if (currentState == 0) {
      sprintf(data.deger, "RELAY_LOW");
      Serial.println("Gönder: RELAY_LOW (D2=5V) -> Alıcı Pin11 LOW");
    } else {
      sprintf(data.deger, "RELAY_HIGH");
      Serial.println("Gönder: RELAY_HIGH (D5=5V) -> Alıcı Pin11 HIGH");
    }
    
    ResponseStatus rs = E22.sendFixedMessage(0, 2, 18, &data, sizeof(veriler));
    Serial.print("Durum: ");
    Serial.println(rs.getResponseDescription());
    
    lastState = currentState;
    lastSendTime = currentTime;
  }
  
  // Debug: Pin durumlarını göster (her 10 saniyede)
  static unsigned long lastDebugTime = 0;
  if (currentTime - lastDebugTime >= 10000) {
    Serial.print("D2=");
    Serial.print(d2State ? "5V" : "0V");
    Serial.print(" | D5=");
    Serial.print(d5State ? "5V" : "0V");
    Serial.print(" | Son gönderim: ");
    Serial.println(lastState == 0 ? "LOW" : (lastState == 1 ? "HIGH" : "Hiç"));
    lastDebugTime = currentTime;
  }
  
  delay(100);
}