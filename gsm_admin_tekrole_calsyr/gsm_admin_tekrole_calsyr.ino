//admin kaydetmek için admin 123456 +90541xxxx. admin kayıt var tekröle herşey çalışıyor.röle 6 otomatik gsm başlatma.
#include <SoftwareSerial.h>
#include <DHT.h>
#include <EEPROM.h>

#define GSM_RX 7
#define GSM_TX 8
#define DHT_PIN 11
#define DHT_TYPE DHT11

#define RELE1_PIN 4
#define STARTUP_PIN 6

#define ADMIN1 "+905416932009"
#define ADMIN2 "+90542XXXXXXX"
#define ADMIN3 "+90543XXXXXXX"

#define ADMIN_PASSWORD "123456"

#define MAX_ADMINS 6
#define PHONE_LEN 16
#define EEPROM_MAGIC 77

SoftwareSerial gsm(GSM_RX, GSM_TX);
DHT dht(DHT_PIN, DHT_TYPE);

bool rele1State = false;
bool waitingSmsBody = false;
bool callActive = false;

unsigned long callStartTime = 0;

String gsmLine = "";
String lastSmsSender = "";
String adminNumbers[MAX_ADMINS];

void setup() {
  Serial.begin(9600);
  gsm.begin(9600);

  pinMode(RELE1_PIN, OUTPUT);
  pinMode(STARTUP_PIN, OUTPUT);

  digitalWrite(RELE1_PIN, LOW);

  digitalWrite(STARTUP_PIN, HIGH);
  delay(5000);
  digitalWrite(STARTUP_PIN, LOW);

  dht.begin();
  loadAdmins();

  Serial.println("==== GSM Sulama Sistemi Basladi ====");
  printAdmins();

  sendAT("AT", 1000);
  sendAT("AT+CMGF=1", 1000);
  sendAT("AT+CNMI=1,2,0,0,0", 1000);
  sendAT("AT+CLIP=1", 1000);
  sendAT("AT+DDET=1", 1000);

  delay(2000);
  sendSMSAllAdmins("Sistem acildi");
}

void loop() {
  readGSM();

  if (callActive && millis() - callStartTime > 20000) {
    sendAT("ATH", 500);
    callActive = false;
    Serial.println("Arama kapatildi");
  }
}

void readGSM() {
  while (gsm.available()) {
    char c = gsm.read();

    if (c == '\r') continue;

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

  if (upperLine.indexOf("RING") >= 0) {
    Serial.println("Arama geliyor");
    sendAT("ATA", 500);
    callActive = true;
    callStartTime = millis();
    return;
  }

  if (upperLine.indexOf("NO CARRIER") >= 0) {
    callActive = false;
    Serial.println("Arama sonlandi");
    return;
  }

  if (callActive) {
    if (upperLine.indexOf("DTMF: 1") >= 0 || upperLine.indexOf("DTMF=1") >= 0 || upperLine.indexOf("DTMF 1") >= 0) {
      digitalWrite(RELE1_PIN, HIGH);
      rele1State = true;
      Serial.println("DTMF 1 -> Role acildi");
    }
    else if (upperLine.indexOf("DTMF: 2") >= 0 || upperLine.indexOf("DTMF=2") >= 0 || upperLine.indexOf("DTMF 2") >= 0) {
      digitalWrite(RELE1_PIN, LOW);
      rele1State = false;
      Serial.println("DTMF 2 -> Role kapandi");
    }
  }

  if (upperLine.startsWith("+CMT:")) {
    lastSmsSender = normalizePhone(getSenderNumber(line));
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

  if (smsUpper.startsWith("ADMIN ")) {
    handleAdminCommand(sender, sms);
    return;
  }

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

    String durum = "Role: ";
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

void handleAdminCommand(String sender, String sms) {
  if (!isAdminNumber(sender)) {
    sendSMS(sender, "Yetkisiz numara");
    return;
  }

  int firstSpace = sms.indexOf(' ');
  int secondSpace = sms.indexOf(' ', firstSpace + 1);

  if (firstSpace < 0 || secondSpace < 0) {
    sendSMS(sender, "Format: admin 123456 +905416932009");
    return;
  }

  String password = sms.substring(firstSpace + 1, secondSpace);
  password.trim();

  String newNumber = sms.substring(secondSpace + 1);
  newNumber.trim();
  newNumber = normalizePhone(newNumber);

  if (password != ADMIN_PASSWORD) {
    sendSMS(sender, "Admin sifresi hatali");
    return;
  }

  if (!isValidPhone(newNumber)) {
    sendSMS(sender, "Numara gecersiz");
    return;
  }

  if (isAdminNumber(newNumber)) {
    sendSMS(sender, "Numara zaten admin");
    return;
  }

  if (addAdminNumber(newNumber)) {
    saveAdmins();
    Serial.print("Yeni admin eklendi >> ");
    Serial.println(newNumber);
    sendSMS(sender, "Admin eklendi: " + newNumber);
  } else {
    sendSMS(sender, "Admin listesi dolu");
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
  for (int i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i].length() > 0) {
      sendSMS(adminNumbers[i], message);
    }
  }
}

bool isAdminNumber(String number) {
  String normalized = normalizePhone(number);

  for (int i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i].length() > 0 && adminNumbers[i] == normalized) {
      return true;
    }
  }
  return false;
}

bool addAdminNumber(String number) {
  for (int i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i].length() == 0) {
      adminNumbers[i] = number;
      return true;
    }
  }
  return false;
}

void loadAdmins() {
  if (EEPROM.read(0) != EEPROM_MAGIC) {
    setDefaultAdmins();
    saveAdmins();
    return;
  }

  int addr = 1;
  for (int i = 0; i < MAX_ADMINS; i++) {
    char buf[PHONE_LEN];
    for (int j = 0; j < PHONE_LEN; j++) {
      buf[j] = char(EEPROM.read(addr++));
    }
    buf[PHONE_LEN - 1] = '\0';
    adminNumbers[i] = String(buf);
    adminNumbers[i].trim();
  }

  if (adminNumbers[0].length() == 0) {
    setDefaultAdmins();
    saveAdmins();
  }
}

void saveAdmins() {
  EEPROM.write(0, EEPROM_MAGIC);

  int addr = 1;
  for (int i = 0; i < MAX_ADMINS; i++) {
    char buf[PHONE_LEN];
    for (int j = 0; j < PHONE_LEN; j++) {
      buf[j] = 0;
    }

    adminNumbers[i].toCharArray(buf, PHONE_LEN);

    for (int j = 0; j < PHONE_LEN; j++) {
      EEPROM.write(addr++, buf[j]);
    }
  }
}

void setDefaultAdmins() {
  for (int i = 0; i < MAX_ADMINS; i++) {
    adminNumbers[i] = "";
  }

  adminNumbers[0] = normalizePhone(ADMIN1);
  adminNumbers[1] = normalizePhone(ADMIN2);
  adminNumbers[2] = normalizePhone(ADMIN3);
}

void printAdmins() {
  Serial.println("Admin listesi:");
  for (int i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i].length() > 0) {
      Serial.print(i + 1);
      Serial.print(" -> ");
      Serial.println(adminNumbers[i]);
    }
  }
}

String normalizePhone(String number) {
  String n = number;
  n.trim();

  String cleaned = "";
  for (int i = 0; i < n.length(); i++) {
    char c = n.charAt(i);
    if ((c >= '0' && c <= '9') || (c == '+' && cleaned.length() == 0)) {
      cleaned += c;
    }
  }

  if (cleaned.startsWith("0")) {
    cleaned = "+9" + cleaned;
  }
  else if (cleaned.startsWith("5") && cleaned.length() == 10) {
    cleaned = "+90" + cleaned;
  }
  else if (cleaned.startsWith("90") && cleaned.length() == 12) {
    cleaned = "+" + cleaned;
  }

  return cleaned;
}

bool isValidPhone(String number) {
  String n = normalizePhone(number);
  return n.startsWith("+90") && n.length() == 13;
}

String getSenderNumber(String header) {
  int firstQuote = header.indexOf('\"');
  if (firstQuote < 0) return "";

  int secondQuote = header.indexOf('\"', firstQuote + 1);
  if (secondQuote < 0) return "";

  return header.substring(firstQuote + 1, secondQuote);
}
