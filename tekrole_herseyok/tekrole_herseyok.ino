#include <SoftwareSerial.h>
#include <DHT.h>

#define GSM_RX 7
#define GSM_TX 8
#define DHT_PIN 11
#define DHT_TYPE DHT11

#define RELE1_PIN 4
#define STARTUP_PIN 6

#define ADMIN1 "+905416932009"
#define ADMIN2 "+90542XXXXXXX"
#define ADMIN3 "+90543XXXXXXX"

SoftwareSerial gsm(GSM_RX, GSM_TX);
DHT dht(DHT_PIN, DHT_TYPE);

bool rele1State = false;
bool waitingSmsBody = false;

String gsmLine = "";
String lastSmsSender = "";

void setup() {
  Serial.begin(9600);
  gsm.begin(9600);

  pinMode(RELE1_PIN, OUTPUT);
  pinMode(STARTUP_PIN, OUTPUT);

  // Role mantigi:
  // HIGH = role acik
  // LOW  = role kapali
  digitalWrite(RELE1_PIN, LOW);

  // Startup pini:
  // Ilk acilista 5 saniye HIGH, sonra LOW
  digitalWrite(STARTUP_PIN, HIGH);
  delay(5000);
  digitalWrite(STARTUP_PIN, LOW);

  dht.begin();

  Serial.println("==== GSM Sulama Sistemi Basladi ====");

  sendAT("AT", 1000);
  sendAT("AT+CMGF=1", 1000);
  sendAT("AT+CNMI=1,2,0,0,0", 1000);
  sendAT("AT+CLIP=1", 1000);

  delay(2000);
  sendSMSAllAdmins("Sistem acildi");
}

void loop() {
  readGSM();
}

void readGSM() {
  while (gsm.available()) {
    char c = gsm.read();

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      gsmLine.trim();

      if (gsmLine.length() > 0) {
        Serial.print("GSM >> ");
        Serial.println(gsmLine);
        processGSMLine(gsmLine);
      }

      gsmLine = "";
    } else {
      gsmLine += c;
    }
  }
}

void processGSMLine(String line) {
  String upperLine = line;
  upperLine.toUpperCase();

  if (upperLine.startsWith("+CMT:")) {
    lastSmsSender = getSenderNumber(line);
    waitingSmsBody = true;

    Serial.println("----- YENI SMS BASLIGI -----");
    Serial.print("SMS Gonderen >> ");
    Serial.println(lastSmsSender);
    return;
  }

  if (waitingSmsBody) {
    waitingSmsBody = false;
    handleSMS(lastSmsSender, line);
  }
}

void handleSMS(String sender, String smsText) {
  String sms = smsText;
  sms.trim();

  Serial.println("----- YENI SMS -----");
  Serial.print("Gonderen: ");
  Serial.println(sender);
  Serial.print("Icerik: ");
  Serial.println(sms);

  String smsUpper = sms;
  smsUpper.toUpperCase();

  if (!isAdminNumber(sender)) {
    Serial.println("Yetkisiz numara");
    sendSMS(sender, "Yetkisiz numara");
    return;
  }

  if (smsUpper == "AC") {
    digitalWrite(RELE1_PIN, HIGH);
    rele1State = true;
    Serial.println("Role -> HIGH (ACIK)");
    sendSMS(sender, "Role acildi");
  }
  else if (smsUpper == "KAPAT") {
    digitalWrite(RELE1_PIN, LOW);
    rele1State = false;
    Serial.println("Role -> LOW (KAPALI)");
    sendSMS(sender, "Role kapandi");
  }
  else if (smsUpper == "DURUM") {
    float sicaklik = dht.readTemperature();
    float nem = dht.readHumidity();

    String durum = "";
    durum += "Role: ";
    durum += (rele1State ? "Acik" : "Kapali");
    durum += "\nSicaklik: ";
    durum += isnan(sicaklik) ? String("Hata") : String(sicaklik, 1);
    durum += " C";
    durum += "\nNem: ";
    durum += isnan(nem) ? String("Hata") : String(nem, 1);
    durum += " %";

    Serial.println("DURUM komutu alindi");
    sendSMS(sender, durum);
  }
  else {
    Serial.println("Hatali komut");
    sendSMS(sender, "Hatali komut");
  }
}

void sendAT(String command, int waitMs) {
  Serial.print("AT << ");
  Serial.println(command);
  gsm.println(command);
  delay(waitMs);
}

void sendSMS(String number, String message) {
  sendAT("AT+CMGF=1", 500);

  Serial.print("AT << AT+CMGS=\"");
  Serial.print(number);
  Serial.println("\"");
  gsm.print("AT+CMGS=\"");
  gsm.print(number);
  gsm.println("\"");
  delay(500);

  Serial.print("SMS << ");
  Serial.println(message);
  gsm.print(message);
  delay(500);

  Serial.println("AT << CTRL+Z");
  gsm.write(26);
  delay(4000);
}

void sendSMSAllAdmins(String message) {
  sendSMS(ADMIN1, message);
  sendSMS(ADMIN2, message);
  sendSMS(ADMIN3, message);
}

bool isAdminNumber(String number) {
  return number == ADMIN1 || number == ADMIN2 || number == ADMIN3;
}

String getSenderNumber(String header) {
  int firstQuote = header.indexOf('\"');
  if (firstQuote < 0) return "";

  int secondQuote = header.indexOf('\"', firstQuote + 1);
  if (secondQuote < 0) return "";

  return header.substring(firstQuote + 1, secondQuote);
}
