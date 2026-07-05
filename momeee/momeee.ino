#include <LoRa_E22.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(9, 8);
LoRa_E22 E22(&mySerial);

#define M0 7
#define M1 6
#define INPUT_PIN_D2 2   // 5V kontrol pini (Röle AÇ)
#define INPUT_PIN_D5 5   // 5V kontrol pini (Röle KAPAT)

struct veriler {   
  char komut[10];
} data;

bool isSendingOn = false;
bool isSendingOff = false;
unsigned long sendStartTime = 0;

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
  Serial.println("        VERICI HAZIR (ŞİFRESİZ)");
  Serial.println("=====================================");
  Serial.println("D2=5V -> Her 5 sn'de 1, 2 sn AÇ sinyali");
  Serial.println("D5=5V -> Her 5 sn'de 1, 2 sn KAPAT sinyali");
  Serial.println("=====================================");
}

void loop() {
  bool d2State = digitalRead(INPUT_PIN_D2);
  bool d5State = digitalRead(INPUT_PIN_D5);
  unsigned long currentTime = millis();

  // === D2=5V (Röle AÇ) ===
  if (d2State == HIGH) {
    isSendingOff = false;
    if (!isSendingOn) {
      isSendingOn = true;
      sendStartTime = currentTime;
      Serial.println("D2=5V -> AÇMA sinyali başladı.");
    }
  } else {
    if (isSendingOn) {
      isSendingOn = false;
      Serial.println("D2=0V -> AÇMA sinyali durduruldu.");
    }
  }

  // === D5=5V (Röle KAPAT) ===
  if (d5State == HIGH) {
    isSendingOn = false;
    if (!isSendingOff) {
      isSendingOff = true;
      sendStartTime = currentTime;
      Serial.println("D5=5V -> KAPATMA sinyali başladı.");
    }
  } else {
    if (isSendingOff) {
      isSendingOff = false;
      Serial.println("D5=0V -> KAPATMA sinyali durduruldu.");
    }
  }

  // === AÇMA SİNYALİ GÖNDER (Her 5 sn'de 1, 2 sn boyunca) ===
  if (isSendingOn && (currentTime - sendStartTime) >= 5000) {
    Serial.println(">>> AÇMA SİNYALİ GÖNDER (2 sn)");
    
    unsigned long pulseStart = currentTime;
    while (millis() - pulseStart < 2000) {
      strcpy(data.komut, "ON");
      
      ResponseStatus rs = E22.sendFixedMessage(1, 18, 22, &data, sizeof(veriler));
      Serial.print("Gönder: ON - ");
      Serial.println(rs.getResponseDescription());
      delay(100);
    }
    sendStartTime = currentTime;
  }

  // === KAPATMA SİNYALİ GÖNDER (Her 5 sn'de 1, 2 sn boyunca) ===
  if (isSendingOff && (currentTime - sendStartTime) >= 5000) {
    Serial.println(">>> KAPATMA SİNYALİ GÖNDER (2 sn)");
    
    unsigned long pulseStart = currentTime;
    while (millis() - pulseStart < 2000) {
      strcpy(data.komut, "OFF");
      
      ResponseStatus rs = E22.sendFixedMessage(1, 18, 22, &data, sizeof(veriler));
      Serial.print("Gönder: OFF - ");
      Serial.println(rs.getResponseDescription());
      delay(100);
    }
    sendStartTime = currentTime;
  }

  delay(50);
}