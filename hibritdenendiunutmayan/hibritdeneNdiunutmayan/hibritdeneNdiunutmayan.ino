#include <SoftwareSerial.h>
#include <DHT.h>
#include <EEPROM.h>
#include <avr/wdt.h>

#define GSM_RX 2
#define GSM_TX 3

#define DHT_PIN 11
#define DHT_TYPE DHT11

#define RELE1_PIN 4
#define RELE2_PIN 5
#define RELE3_PIN 6
#define RELE4_PIN 7

#define STARTUP_PIN 9

#define ADMIN1 "+905416932009"

#define ROLE_AC HIGH
#define ROLE_KAPAT LOW

#define MAX_ADMINS 6
#define PHONE_LEN 16

#define EEPROM_MAGIC 77
#define EEPROM_RELAY_ADDR 120

SoftwareSerial gsm(GSM_RX, GSM_TX);
DHT dht(DHT_PIN, DHT_TYPE);

const uint8_t relayPins[4] = {
  RELE1_PIN,
  RELE2_PIN,
  RELE3_PIN,
  RELE4_PIN
};

bool relayStates[4] = {false, false, false, false};

char adminNumbers[MAX_ADMINS][PHONE_LEN];

unsigned long lastGsmCheck = 0;
unsigned long lastNetworkCheck = 0;

void clearBuffer(char *buf, size_t len) {
  memset(buf, 0, len);
}

void normalizePhone(const char *src, char *dst, size_t dstSize) {

  char digits[PHONE_LEN];

  uint8_t d = 0;

  clearBuffer(dst, dstSize);
  clearBuffer(digits, sizeof(digits));

  for (uint8_t i = 0; src[i] != '\0'; i++) {

    if (src[i] >= '0' && src[i] <= '9') {

      if (d < sizeof(digits) - 1) {
        digits[d++] = src[i];
      }
    }
  }

  digits[d] = '\0';

  if (d == 11 && digits[0] == '0') {
    snprintf(dst, dstSize, "+9%s", digits);
  }
  else if (d == 10 && digits[0] == '5') {
    snprintf(dst, dstSize, "+90%s", digits);
  }
  else if (d == 12 && digits[0] == '9' && digits[1] == '0') {
    snprintf(dst, dstSize, "+%s", digits);
  }
}

bool isAdminNumber(const char *number) {

  char normalized[PHONE_LEN];

  normalizePhone(number, normalized, sizeof(normalized));

  for (uint8_t i = 0; i < MAX_ADMINS; i++) {

    if (strcmp(adminNumbers[i], normalized) == 0) {
      return true;
    }
  }

  return false;
}

void saveAdmins() {

  EEPROM.update(0, EEPROM_MAGIC);

  int addr = 1;

  for (uint8_t i = 0; i < MAX_ADMINS; i++) {

    for (uint8_t j = 0; j < PHONE_LEN; j++) {

      EEPROM.update(addr++, adminNumbers[i][j]);
    }
  }
}

void setDefaultAdmins() {

  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    clearBuffer(adminNumbers[i], PHONE_LEN);
  }

  normalizePhone(ADMIN1, adminNumbers[0], PHONE_LEN);
}

void loadAdmins() {

  if (EEPROM.read(0) != EEPROM_MAGIC) {

    setDefaultAdmins();
    saveAdmins();

    return;
  }

  int addr = 1;

  for (uint8_t i = 0; i < MAX_ADMINS; i++) {

    for (uint8_t j = 0; j < PHONE_LEN; j++) {

      adminNumbers[i][j] = EEPROM.read(addr++);
    }

    adminNumbers[i][PHONE_LEN - 1] = '\0';
  }
}

void saveRelayStates() {

  byte stateByte = 0;

  for (uint8_t i = 0; i < 4; i++) {

    if (relayStates[i]) {
      stateByte |= (1 << i);
    }
  }

  EEPROM.update(EEPROM_RELAY_ADDR, stateByte);
}

void loadRelayStates() {

  byte stateByte = EEPROM.read(EEPROM_RELAY_ADDR);

  for (uint8_t i = 0; i < 4; i++) {

    relayStates[i] = bitRead(stateByte, i);

    digitalWrite(relayPins[i], relayStates[i] ? ROLE_AC : ROLE_KAPAT);
  }
}

void relayInit() {

  for (uint8_t i = 0; i < 4; i++) {

    pinMode(relayPins[i], OUTPUT);

    digitalWrite(relayPins[i], ROLE_KAPAT);
  }

  loadRelayStates();
}

void setRelay(uint8_t index, bool on) {

  if (index >= 4) return;

  digitalWrite(relayPins[index], on ? ROLE_AC : ROLE_KAPAT);

  relayStates[index] = on;

  saveRelayStates();

  Serial.print("ROLE ");
  Serial.print(index + 1);
  Serial.print(" -> ");
  Serial.println(on ? "ACIK" : "KAPALI");
}

void setAllRelays(bool on) {

  for (uint8_t i = 0; i < 4; i++) {

    setRelay(i, on);
  }
}

void sendAT(const __FlashStringHelper *cmd, unsigned long waitMs) {

  gsm.println(cmd);

  delay(waitMs);
}

bool gsmWaitResponse(const char *target, unsigned long timeout) {

  unsigned long start = millis();

  String resp = "";

  while (millis() - start < timeout) {

    while (gsm.available()) {

      char c = gsm.read();

      resp += c;

      if (resp.indexOf(target) != -1) {
        return true;
      }
    }
  }

  return false;
}

bool gsmTest() {

  while (gsm.available()) gsm.read();

  gsm.println(F("AT"));

  return gsmWaitResponse("OK", 3000);
}

bool gsmNetworkOK() {

  while (gsm.available()) gsm.read();

  gsm.println(F("AT+CREG?"));

  unsigned long start = millis();

  String resp = "";

  while (millis() - start < 5000) {

    while (gsm.available()) {

      char c = gsm.read();

      resp += c;

      if (resp.indexOf("+CREG: 0,1") != -1 ||
          resp.indexOf("+CREG: 0,5") != -1) {

        return true;
      }
    }
  }

  return false;
}

void gsmPowerToggle() {

  Serial.println(F("SIM800C BASLATILIYOR"));

  digitalWrite(STARTUP_PIN, HIGH);

  delay(2500);

  digitalWrite(STARTUP_PIN, LOW);

  delay(10000);
}

void gsmInitialize() {

  uint8_t tries = 0;

  while (tries < 5) {

    gsmPowerToggle();

    if (gsmTest()) {

      sendAT(F("ATE0"), 1000);
      sendAT(F("AT+CMGF=1"), 1000);
      sendAT(F("AT+CNMI=1,2,0,0,0"), 1000);
      sendAT(F("AT+CLIP=1"), 1000);
      sendAT(F("AT+DDET=1"), 1000);

      delay(3000);

      if (gsmNetworkOK()) {

        Serial.println(F("GSM HAZIR"));

        return;
      }
    }

    tries++;
  }

  Serial.println(F("GSM BASLATILAMADI"));

  wdt_enable(WDTO_2S);

  while (1) {
  }
}

void sendSMS(const char *number, const char *message) {

  gsm.println(F("AT+CMGF=1"));

  delay(500);

  gsm.print(F("AT+CMGS=\""));
  gsm.print(number);
  gsm.println(F("\""));

  delay(500);

  gsm.print(message);

  delay(500);

  gsm.write(26);

  delay(5000);
}

void sendStatusSMS(const char *number) {

  char msg[160];

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  snprintf(
    msg,
    sizeof(msg),
    "R1:%s R2:%s\nR3:%s R4:%s\nSicaklik:%.1f C\nNem:%.1f%%",
    relayStates[0] ? "Acik" : "Kapali",
    relayStates[1] ? "Acik" : "Kapali",
    relayStates[2] ? "Acik" : "Kapali",
    relayStates[3] ? "Acik" : "Kapali",
    t,
    h
  );

  sendSMS(number, msg);
}

void processSMS(String sender, String sms) {

  sms.toUpperCase();

  if (!isAdminNumber(sender.c_str())) {

    sendSMS(sender.c_str(), "Yetkisiz numara");

    return;
  }

  if (sms == "R1 AC") {
    setRelay(0, true);
    sendSMS(sender.c_str(), "Role1 Acildi");
  }
  else if (sms == "R1 KAPAT") {
    setRelay(0, false);
    sendSMS(sender.c_str(), "Role1 Kapandi");
  }
  else if (sms == "R2 AC") {
    setRelay(1, true);
    sendSMS(sender.c_str(), "Role2 Acildi");
  }
  else if (sms == "R2 KAPAT") {
    setRelay(1, false);
    sendSMS(sender.c_str(), "Role2 Kapandi");
  }
  else if (sms == "R3 AC") {
    setRelay(2, true);
    sendSMS(sender.c_str(), "Role3 Acildi");
  }
  else if (sms == "R3 KAPAT") {
    setRelay(2, false);
    sendSMS(sender.c_str(), "Role3 Kapandi");
  }
  else if (sms == "R4 AC") {
    setRelay(3, true);
    sendSMS(sender.c_str(), "Role4 Acildi");
  }
  else if (sms == "R4 KAPAT") {
    setRelay(3, false);
    sendSMS(sender.c_str(), "Role4 Kapandi");
  }
  else if (sms == "HEPSI AC") {
    setAllRelays(true);
    sendSMS(sender.c_str(), "Tum roleler acildi");
  }
  else if (sms == "HEPSI KAPAT") {
    setAllRelays(false);
    sendSMS(sender.c_str(), "Tum roleler kapandi");
  }
  else if (sms == "DURUM") {
    sendStatusSMS(sender.c_str());
  }
}

void handleDTMF(char tone) {

  switch (tone) {

    case '1': setRelay(0, true); break;
    case '2': setRelay(0, false); break;
    case '3': setRelay(1, true); break;
    case '4': setRelay(1, false); break;
    case '5': setRelay(2, true); break;
    case '6': setRelay(2, false); break;
    case '7': setRelay(3, true); break;
    case '8': setRelay(3, false); break;
    case '*': setAllRelays(true); break;
    case '0': setAllRelays(false); break;
  }
}

void readGSM() {

  if (!gsm.available()) return;

  String line = gsm.readStringUntil('\n');

  line.trim();

  if (line.startsWith("+CMT:")) {

    int first = line.indexOf('\"');
    int second = line.indexOf('\"', first + 1);

    String sender = line.substring(first + 1, second);

    String sms = gsm.readStringUntil('\n');

    sms.trim();

    processSMS(sender, sms);
  }

  if (line.startsWith("+DTMF:")) {

    char tone = line.charAt(line.length() - 1);

    handleDTMF(tone);
  }
}

void setup() {

  wdt_disable();

  Serial.begin(9600);

  gsm.begin(9600);

  dht.begin();

  relayInit();

  pinMode(STARTUP_PIN, OUTPUT);

  digitalWrite(STARTUP_PIN, LOW);

  loadAdmins();

  gsmInitialize();

  sendSMS(ADMIN1, "Sistem acildi");

  wdt_enable(WDTO_8S);
}

void loop() {

  wdt_reset();

  readGSM();

  if (millis() - lastGsmCheck > 60000UL) {

    lastGsmCheck = millis();

    if (!gsmTest()) {

      gsmInitialize();
    }
  }

  if (millis() - lastNetworkCheck > 300000UL) {

    lastNetworkCheck = millis();

    if (!gsmNetworkOK()) {

      gsmInitialize();
    }
  }
}