#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define RELAY1_PIN 5
#define RELAY2_PIN 6

SoftwareSerial gsm(7, 8); // RX, TX

String incomingSMS = "";
String senderNumber = "";

const int maxNumbers = 5;
const int numberLength = 11;

void setup() {
  Serial.begin(9600);     // Seri monitör
  gsm.begin(9600);        // GSM modül haberleşmesi

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);
  dht.begin();

  delay(3000);            // Autobaud için bekleme
  Serial.println("GSM sistemi başlatıldı.");

  gsm.println("ATE0");    // Echo kapatma
  delay(500);
  gsm.println("AT");      // Test komutu
  delay(500);
  gsm.println("AT+CMGF=1"); // Metin modu
  delay(500);
  gsm.println("AT+CNMI=1,2,0,0,0"); // Yeni SMS bildirimi
}

void loop() {
  while (gsm.available()) {
    char c = gsm.read();
    Serial.print(c);
    incomingSMS += c;
  }

  if (incomingSMS.indexOf("+CMT:") != -1) {
    parseSMS(incomingSMS);
    incomingSMS = "";
  }
}

void parseSMS(String sms) {
  int numStart = sms.indexOf("\"") + 1;
  int numEnd = sms.indexOf("\"", numStart);
  senderNumber = sms.substring(numStart, numEnd);

  String message = sms.substring(sms.lastIndexOf("\n") + 1);
  message.trim();
  message.toUpperCase();

  Serial.println("Gelen SMS: " + message);
  Serial.println("Gönderen Numara: " + senderNumber);

  if (message.startsWith("EKLE:")) {
    String newNum = message.substring(5);
    newNum.trim();
    addNumber(newNum);
    sendSMS(senderNumber, "Numara eklendi.");
  } else if (message.startsWith("SIL:")) {
    String delNum = message.substring(4);
    delNum.trim();
    removeNumber(delNum);
    sendSMS(senderNumber, "Numara silindi.");
  } else if (isAuthorized(senderNumber)) {
    if (message == "AC") {
      digitalWrite(RELAY1_PIN, HIGH);
      delay(3000);
      digitalWrite(RELAY1_PIN, LOW);
      sendSMS(senderNumber, "Pompa açıldı.");
    } else if (message == "KAPAT") {
      digitalWrite(RELAY2_PIN, HIGH);
      delay(3000);
      digitalWrite(RELAY2_PIN, LOW);
      sendSMS(senderNumber, "Pompa kapatıldı.");
    } else if (message == "DURUM") {
      float temp = dht.readTemperature();
      float hum = dht.readHumidity();
      String status = "Sıcaklık: " + String(temp) + "°C\nNem: " + String(hum) + "%";
      sendSMS(senderNumber, status);
    } else {
      sendSMS(senderNumber, "Geçersiz komut.");
    }
  } else {
    sendSMS(senderNumber, "Yetkisiz numara.");
  }
}

void sendSMS(String number, String text) {
  gsm.println("AT+CMGS=\"" + number + "\"");
  delay(100);
  gsm.print(text);
  delay(100);
  gsm.write(26); // Ctrl+Z
  delay(1000);
}

bool isAuthorized(String num) {
  for (int i = 0; i < maxNumbers; i++) {
    char stored[numberLength];
    for (int j = 0; j < numberLength; j++) {
      stored[j] = EEPROM.read(i * numberLength + j);
    }
    String storedNum = String(stored);
    storedNum.trim();
    if (storedNum == num) return true;
  }
  return false;
}

void addNumber(String num) {
  for (int i = 0; i < maxNumbers; i++) {
    char stored[numberLength];
    for (int j = 0; j < numberLength; j++) {
      stored[j] = EEPROM.read(i * numberLength + j);
    }
    String storedNum = String(stored);
    storedNum.trim();
    if (storedNum == "") {
      for (int j = 0; j < numberLength; j++) {
        EEPROM.write(i * numberLength + j, j < num.length() ? num[j] : '\0');
      }
      break;
    }
  }
}

void removeNumber(String num) {
  for (int i = 0; i < maxNumbers; i++) {
    char stored[numberLength];
    for (int j = 0; j < numberLength; j++) {
      stored[j] = EEPROM.read(i * numberLength + j);
    }
    String storedNum = String(stored);
    storedNum.trim();
    if (storedNum == num) {
      for (int j = 0; j < numberLength; j++) {
        EEPROM.write(i * numberLength + j, '\0');
      }
      break;
    }
  }
}