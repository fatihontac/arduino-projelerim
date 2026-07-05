#include <SoftwareSerial.h>
#include <DHT.h>
//UNO 2 GSM7 DE = UNO 3 GSM8 DE
#define GSM_RX 7
#define GSM_TX 8
#define DHT_PIN A7
#define DHT_TYPE DHT11

#define RELE1_PIN 4
#define RELE2_PIN 5
#define TERMIC_PIN 12

#define ADMIN1 "+905416932009"
#define ADMIN2 "+90542XXXXXXX"
#define ADMIN3 "+90543XXXXXXX"

SoftwareSerial gsm(GSM_RX, GSM_TX);
DHT dht(DHT_PIN, DHT_TYPE);

bool rele1State = false;
bool rele2State = false;
bool waitingSmsBody = false;
bool termicAlarmSent = false;
String gsmLine = "";
String lastSmsSender = "";

void setup() {
  Serial.begin(9600);
  gsm.begin(9600);

  pinMode(RELE1_PIN, OUTPUT);
  pinMode(RELE2_PIN, OUTPUT);
  pinMode(TERMIC_PIN, INPUT_PULLUP);

  // Senin role kartina gore:
  // HIGH = role acik
  // LOW  = role kapali
  digitalWrite(RELE1_PIN, LOW);
  digitalWrite(RELE2_PIN, LOW);

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
  checkTermic();
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

  if (smsUpper == "RELE1AC") {
    digitalWrite(RELE1_PIN, HIGH);
    rele1State = true;
    Serial.println("Role1 -> HIGH (ACIK)");
    sendSMS(sender, "Role1 acildi");
  }
  else if (smsUpper == "RELE1KAPAT") {
    digitalWrite(RELE1_PIN, LOW);
    rele1State = false;
    Serial.println("Role1 -> LOW (KAPALI)");
    sendSMS(sender, "Role1 kapandi");
  }
  else if (smsUpper == "RELE2AC") {
    digitalWrite(RELE2_PIN, HIGH);
    rele2State = true;
    Serial.println("Role2 -> HIGH (ACIK)");
    sendSMS(sender, "Role2 acildi");
  }
  else if (smsUpper == "RELE2KAPAT") {
    digitalWrite(RELE2_PIN, LOW);
    rele2State = false;
    Serial.println("Role2 -> LOW (KAPALI)");
    sendSMS(sender, "Role2 kapandi");
  }
  else if (smsUpper == "DURUM") {
    float sicaklik = dht.readTemperature();
    float nem = dht.readHumidity();

    String durum = "";
    durum += "Role1: ";
    durum += (rele1State ? "Acik" : "Kapali");
    durum += "\nRole2: ";
    durum += (rele2State ? "Acik" : "Kapali");
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

void checkTermic() {
  int termicState = digitalRead(TERMIC_PIN);

  if (termicState == LOW && !termicAlarmSent) {
    termicAlarmSent = true;
    Serial.println("Termik atildi!");
    sendSMSAllAdmins("Termik atildi!");
  }

  if (termicState == HIGH) {
    termicAlarmSent = false;
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