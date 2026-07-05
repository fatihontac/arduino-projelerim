#include <SoftwareSerial.h>
#include <DHT.h>

#define GSM_RX 4
#define GSM_TX 5
#define DHT_PIN A7
#define DHT_TYPE DHT11

#define RELE1_PIN 8
#define RELE2_PIN 9
#define TERMIC_PIN 7

// Admin numara (SMS atılacak)
#define ADMIN1 "+905416932009"
#define ADMIN2 "+90542XXXXXXX"
#define ADMIN3 "+90543XXXXXXX"

SoftwareSerial gsm(GSM_RX, GSM_TX);
DHT dht(DHT_PIN, DHT_TYPE);

// Röle durum değişkenleri
bool rele1State = false;
bool rele2State = false;

// Arama kontrol
unsigned long callStartTime = 0;
bool callActive = false;

// Gelen veri buffer
String gsmBuffer = "";

void setup() {
  Serial.begin(9600);
  gsm.begin(9600);

  pinMode(RELE1_PIN, OUTPUT);
  pinMode(RELE2_PIN, OUTPUT);
  pinMode(TERMIC_PIN, INPUT_PULLUP);

  dht.begin();

  digitalWrite(RELE1_PIN, LOW);
  digitalWrite(RELE2_PIN, LOW);

  Serial.println("==== GSM Sulama Sistemi Başladı ====");

  // DTMF tuşlarını aç
  gsm.println("AT+DDET=1");
  delay(500);
}

void loop() {
  readGSM();
  checkTermic();
}

// GSM'den gelenleri satır satır oku
void readGSM() {
  while (gsm.available()) {
    char c = gsm.read();
    gsmBuffer += c;
    if (c == '\n') { // satır sonu geldi, işlem yap
      gsmBuffer.trim();
      if (gsmBuffer.length() > 0) processGSMLine(gsmBuffer);
      gsmBuffer = "";
    }
  }
}

// Satır satır GSM verisi işleme
void processGSMLine(String line) {
  line.toUpperCase();

  // Arama geldi
  if (line.indexOf("RING") >= 0) {
    gsm.println("ATA"); // aramayı aç
    callStartTime = millis();
    callActive = true;
    Serial.println("Arama başladı...");
    return;
  }

  // Arama açıkken DTMF tuşlarını kontrol et
  if (callActive) {
    if (millis() - callStartTime > 20000) {
      gsm.println("ATH"); // aramayı kapat
      callActive = false;
      Serial.println("Arama kapatıldı (20 sn)");
    } else {
      if (line.indexOf("DTMF: 1") >= 0 || line.indexOf("DTMF=1") >= 0 || line.indexOf("DTMF 1") >= 0) {
        digitalWrite(RELE1_PIN, HIGH); rele1State = true;
        Serial.println("DTMF 1: Röle1 Açıldı");
      }
      else if (line.indexOf("DTMF: 2") >= 0 || line.indexOf("DTMF=2") >= 0 || line.indexOf("DTMF 2") >= 0) {
        digitalWrite(RELE1_PIN, LOW); rele1State = false;
        Serial.println("DTMF 2: Röle1 Kapandı");
      }
      else if (line.indexOf("DTMF: 3") >= 0 || line.indexOf("DTMF=3") >= 0 || line.indexOf("DTMF 3") >= 0) {
        digitalWrite(RELE2_PIN, HIGH); rele2State = true;
        Serial.println("DTMF 3: Röle2 Açıldı");
      }
      else if (line.indexOf("DTMF: 4") >= 0 || line.indexOf("DTMF=4") >= 0 || line.indexOf("DTMF 4") >= 0) {
        digitalWrite(RELE2_PIN, LOW); rele2State = false;
        Serial.println("DTMF 4: Röle2 Kapandı");
      }
    }
    return;
  }

  // SMS geldi
  if (line.indexOf("+CMT:") >= 0) {
    delay(500);
    String sms = gsm.readStringUntil('\n');
    sms.trim();
    sms.toUpperCase();

    String sender = getSenderNumber(line);

    if (sms.indexOf("RELE1AC") >= 0) {
      digitalWrite(RELE1_PIN, HIGH);
      rele1State = true;
      sendSMS(sender, "Röle1 Açıldı");
    }
    else if (sms.indexOf("RELE1KAPAT") >= 0) {
      digitalWrite(RELE1_PIN, LOW);
      rele1State = false;
      sendSMS(sender, "Röle1 Kapandı");
    }
    else if (sms.indexOf("RELE2AC") >= 0) {
      digitalWrite(RELE2_PIN, HIGH);
      rele2State = true;
      sendSMS(sender, "Röle2 Açıldı");
    }
    else if (sms.indexOf("RELE2KAPAT") >= 0) {
      digitalWrite(RELE2_PIN, LOW);
      rele2State = false;
      sendSMS(sender, "Röle2 Kapandı");
    }
    else if (sms.indexOf("DURUM") >= 0) {
      float sicaklik = dht.readTemperature();
      float nem = dht.readHumidity();
      String durumMsg = "Röle1: " + String(rele1State ? "Açık" : "Kapalı") +
                        "\nRöle2: " + String(rele2State ? "Açık" : "Kapalı") +
                        "\nSıcaklık: " + String(sicaklik) + "C" +
                        "\nNem: " + String(nem) + "%";
      sendSMS(sender, durumMsg);
    }
    else {
      sendSMS(sender, "Hatalı komut!");
    }
  }
}

// Termik kontrol
void checkTermic() {
  static bool termicFlag = false;
  if (digitalRead(TERMIC_PIN) == LOW && !termicFlag) {
    termicFlag = true;
    sendSMS(ADMIN1, "Termik atıldı!");
    sendSMS(ADMIN2, "Termik atıldı!");
    sendSMS(ADMIN3, "Termik atıldı!");
  } else if (digitalRead(TERMIC_PIN) == HIGH) {
    termicFlag = false;
  }
}

// SMS gönder
void sendSMS(String num, String msg) {
  gsm.println("AT+CMGF=1");
  delay(500);
  gsm.print("AT+CMGS=\""); gsm.print(num); gsm.println("\"");
  delay(500);
  gsm.print(msg);
  delay(500);
  gsm.write(26); // CTRL+Z
  delay(3000);
}

// SMS atan numarayı al
String getSenderNumber(String smsHeader) {
  int start = smsHeader.indexOf("\"")+1;
  int end = smsHeader.indexOf("\"", start);
  return smsHeader.substring(start, end);
}