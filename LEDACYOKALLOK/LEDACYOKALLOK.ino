#include <SoftwareSerial.h>
 
#define GSM_RX 4
#define GSM_TX 5
#define LED_PIN 8
#define PHONE_NUMBER "+905416932009" // SMS ve arama numaran
#define SMS_TEXT "Sistem calisiyor!" // Gönderilecek SMS

SoftwareSerial gsm(GSM_RX, GSM_TX);

void setup() {
  Serial.begin(9600);
  gsm.begin(9600);
  pinMode(LED_PIN, OUTPUT);

  Serial.println("==== GSM Test Basliyor ====");
  delay(2000);

  // 1️⃣ AT testi
  Serial.println("AT testi...");
  sendCommand("AT", 2000);

  // 2️⃣ Şebeke kontrolü
  Serial.println("Sim kart ve sebekeye bakiliyor...");
  sendCommand("AT+CREG?", 2000);

  // 3️⃣ SMS moduna geç
  Serial.println("SMS moduna geciliyor...");
  sendCommand("AT+CMGF=1", 2000);

  // 4️⃣ SMS gönder
  Serial.println("SMS gonderiliyor...");
  gsm.print("AT+CMGS=\""); 
  gsm.print(PHONE_NUMBER); 
  gsm.println("\"");
  delay(500);
  gsm.print(SMS_TEXT);
  delay(500);
  gsm.write(26); // CTRL+Z
  delay(5000);
  readResponse();

  // 5️⃣ Arama yap
  Serial.println("Arama yapiliyor...");
  gsm.print("ATD");
  gsm.print(PHONE_NUMBER);
  gsm.println(";");
  delay(15000);
  gsm.println("ATH"); // Aramayı kapat
  readResponse();

  Serial.println("==== GSM Test Tamamlandi ====");
}

void loop() {
  // Seri monitörden gelen komutlar
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "LEDAC") {
      digitalWrite(LED_PIN, HIGH);
      delay(10000);
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED 5 saniye yandi.");
    } else {
      gsm.println(cmd); // diğer komutlar GSM’e
    }
  }

  // GSM’den gelen verileri seri monitöre yaz
  if (gsm.available()) {
    String resp = gsm.readString();
    Serial.print(resp);

    // Gelen aramayı yakala
    if (resp.indexOf("RING") != -1) {
      Serial.println("Gelen arama algilandi, cevap veriliyor...");
      gsm.println("ATA"); // Cevapla
      delay(20000);        // 20 saniye konuş
      gsm.println("ATH");  // Kapat
      Serial.println("Arama kapatildi.");
    }
  }
}

// Fonksiyon: AT komutunu gönder ve yanıtı oku
void sendCommand(String cmd, unsigned long timeout) {
  gsm.println(cmd);
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (gsm.available()) {
      Serial.write(gsm.read());
    }
  }
}

// Fonksiyon: GSM’den gelen tüm yanıtları oku
void readResponse() {
  delay(500);
  while (gsm.available()) {
    Serial.write(gsm.read());
  }
}