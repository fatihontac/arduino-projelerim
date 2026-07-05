#include <SoftwareSerial.h>
#include <EEPROM.h>

SoftwareSerial sim900(0, 1); // RX, TX
const int pumpOpenPin = 5;
const int pumpClosePin = 6;

const int maxNumbers = 5;
const int numberLength = 13; // "+905XXXXXXXXX"
const String adminPassword = "12345"; // Şifre burada tanımlanır

void setup() {
  pinMode(pumpOpenPin, OUTPUT);
  pinMode(pumpClosePin, OUTPUT);
  digitalWrite(pumpOpenPin, LOW);
  digitalWrite(pumpClosePin, LOW);

  Serial.begin(9600);
  sim900.begin(9600);

  sim900.println("AT+CMGF=1");
  delay(1000);
  sim900.println("AT+CNMI=1,2,0,0,0");
  delay(1000);

  Serial.println("Sistem hazır. SMS bekleniyor...");
}

void loop() {
  if (sim900.available()) {
    String sms = sim900.readString();
    String sender = parseSender(sms);
    String message = parseMessage(sms);

    Serial.println("Gönderen: " + sender);
    Serial.println("Mesaj: " + message);

    if (message.startsWith("EKLE:")) {
      int eqIndex = message.indexOf('=');
      String pass = message.substring(5, eqIndex);
      String newNumber = message.substring(eqIndex + 1);
      if (pass == adminPassword) {
        addNumber(newNumber);
        sendSMS(sender, "Numara eklendi: " + newNumber);
      } else {
        sendSMS(sender, "Hatalı şifre. Numara eklenmedi.");
      }
    } else if (message.startsWith("SIL:")) {
      int eqIndex = message.indexOf('=');
      String pass = message.substring(4, eqIndex);
      String delNumber = message.substring(eqIndex + 1);
      if (pass == adminPassword) {
        removeNumber(delNumber);
        sendSMS(sender, "Numara silindi: " + delNumber);
      } else {
        sendSMS(sender, "Hatalı şifre. Numara silinmedi.");
      }
    } else if (isAuthorized(sender)) {
      if (message.indexOf("AC") >= 0) {
        digitalWrite(pumpOpenPin, HIGH);
        delay(4000);
        digitalWrite(pumpOpenPin, LOW);
        sendSMS(sender, "Pompa açıldı (5. pine 4 saniye +5V).");
      } else if (message.indexOf("KAPA") >= 0) {
        digitalWrite(pumpClosePin, HIGH);
        delay(4000);
        digitalWrite(pumpClosePin, LOW);
        sendSMS(sender, "Pompa kapatıldı (6. pine 4 saniye +5V).");
      }
    } else {
      sendSMS(sender, "Yetkisiz numara.");
    }
  }
}

bool isAuthorized(String num) {
  for (int i = 0; i < maxNumbers; i++) {
    String stored = readNumber(i);
    if (stored == num) return true;
  }
  return false;
}

void addNumber(String num) {
  for (int i = 0; i < maxNumbers; i++) {
    String stored = readNumber(i);
    if (stored == "") {
      writeNumber(i, num);
      return;
    }
  }
}

void removeNumber(String num) {
  for (int i = 0; i < maxNumbers; i++) {
    String stored = readNumber(i);
    if (stored == num) {
      clearNumber(i);
      return;
    }
  }
}

String readNumber(int index) {
  String num = "";
  for (int i = 0; i < numberLength; i++) {
    char c = EEPROM.read(index * numberLength + i);
    if (c != 0xFF) num += c;
  }
  return num;
}

void writeNumber(int index, String num) {
  for (int i = 0; i < numberLength; i++) {
    EEPROM.write(index * numberLength + i, i < num.length() ? num[i] : 0xFF);
  }
}

void clearNumber(int index) {
  for (int i = 0; i < numberLength; i++) {
    EEPROM.write(index * numberLength + i, 0xFF);
  }
}

void sendSMS(String to, String msg) {
  sim900.println("AT+CMGS=\"" + to + "\"");
  delay(1000);
  sim900.print(msg);
  sim900.write(26); // Ctrl+Z
  delay(3000);
}

String parseSender(String sms) {
  int start = sms.indexOf("+90");
  return sms.substring(start, start + numberLength);
}

String parseMessage(String sms) {
  int bodyStart = sms.indexOf("\n", sms.indexOf("\n") + 1);
  String msg = sms.substring(bodyStart + 1);
  msg.trim();
  return msg;
}