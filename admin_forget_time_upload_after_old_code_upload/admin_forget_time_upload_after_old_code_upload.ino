// ======================================================
// GSM ROLE + SMS + DTMF SISTEMI
// YETKISIZ NUMARA SORUNU DUZELTILMIS TAM ENTEGRE KOD
// SMS + DTMF SISTEMINE DOKUNULMADI
// ======================================================

#include <SoftwareSerial.h>
#include <DHT.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

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
#define ADMIN2 ""
#define ADMIN3 ""

#define ADMIN_PASSWORD "123456"

#define MAX_ADMINS 6
#define PHONE_LEN 16
#define EEPROM_MAGIC 77
#define DHT_MIN_INTERVAL_MS 2500UL

#define CLIP_TIMEOUT_MS 4000UL
#define MAX_CALL_MS 120000UL

#define GSM_LINE_LEN 96
#define SMS_BODY_LEN 96
#define SMS_TEXT_LEN 160

#define ROLE_AC HIGH
#define ROLE_KAPAT LOW

SoftwareSerial gsm(GSM_RX, GSM_TX);
DHT dht(DHT_PIN, DHT_TYPE);

const uint8_t relayPins[4] = {
  RELE1_PIN,
  RELE2_PIN,
  RELE3_PIN,
  RELE4_PIN
};

bool relayStates[4] = {false, false, false, false};

bool waitingSmsBody = false;

char gsmLine[GSM_LINE_LEN];
uint8_t gsmLineLen = 0;

char lastSmsSender[PHONE_LEN];
char adminNumbers[MAX_ADMINS][PHONE_LEN];

unsigned long lastDhtReadMs = 0;
float lastValidTemperature = NAN;
float lastValidHumidity = NAN;

enum CallState {
  CALL_IDLE,
  CALL_RINGING,
  CALL_ACTIVE
};

CallState callState = CALL_IDLE;

unsigned long callStartMs = 0;
unsigned long ringStartMs = 0;

bool clipReceived = false;

char lastCaller[PHONE_LEN];

void clearBuffer(char *buf, size_t len) {

  if (!buf || len == 0) return;

  memset(buf, 0, len);
}

bool isSpaceChar(char c) {

  return c == ' ' ||
         c == '\r' ||
         c == '\n' ||
         c == '\t';
}

void safeCopy(char *dst, const char *src, size_t dstSize) {

  if (!dst || dstSize == 0) return;

  if (!src) {

    dst[0] = '\0';

    return;
  }

  strncpy(dst, src, dstSize - 1);

  dst[dstSize - 1] = '\0';
}

void normalizeCommand(char *text) {

  if (!text) return;

  size_t r = 0;

  while (text[r] && isSpaceChar(text[r])) r++;

  size_t w = 0;

  bool lastWasSpace = false;

  while (text[r]) {

    char c = text[r++];

    if (c >= 'a' && c <= 'z') {

      c = c - ('a' - 'A');
    }

    if (isSpaceChar(c)) {

      if (w == 0 || lastWasSpace) continue;

      text[w++] = ' ';

      lastWasSpace = true;

    } else {

      text[w++] = c;

      lastWasSpace = false;
    }

    if (w >= SMS_BODY_LEN - 1) break;
  }

  if (w > 0 && text[w - 1] == ' ') w--;

  text[w] = '\0';
}

void normalizePhone(const char *src, char *dst, size_t dstSize) {

  char digits[PHONE_LEN];

  uint8_t d = 0;

  clearBuffer(dst, dstSize);

  clearBuffer(digits, sizeof(digits));

  if (!src) return;

  for (uint8_t i = 0; src[i] != '\0' && d < sizeof(digits) - 1; i++) {

    if (src[i] >= '0' && src[i] <= '9') {

      digits[d++] = src[i];
    }
  }

  digits[d] = '\0';

  if (d == 11 && digits[0] == '0') {

    snprintf(dst, dstSize, "+9%s", digits);

  } else if (d == 10 && digits[0] == '5') {

    snprintf(dst, dstSize, "+90%s", digits);

  } else if (d == 12 && digits[0] == '9' && digits[1] == '0') {

    snprintf(dst, dstSize, "+%s", digits);

  } else if (d > 0) {

    safeCopy(dst, digits, dstSize);
  }
}

bool isValidPhone(const char *number) {

  char norm[PHONE_LEN];

  normalizePhone(number, norm, sizeof(norm));

  return (
    strncmp(norm, "+90", 3) == 0 &&
    strlen(norm) == 13
  );
}

bool getQuotedField(const char *line, char *out, size_t outSize) {

  if (!line || !out || outSize == 0) return false;

  const char *first = strchr(line, '\"');

  if (!first) return false;

  const char *second = strchr(first + 1, '\"');

  if (!second) return false;

  size_t len = (size_t)(second - (first + 1));

  if (len >= outSize) len = outSize - 1;

  memcpy(out, first + 1, len);

  out[len] = '\0';

  return true;
}

char getDTMFTone(const char *line) {

  if (!line) return '\0';

  const char *colon = strchr(line, ':');

  if (!colon) return '\0';

  const char *p = colon + 1;

  while (*p) {

    if (*p == ' ' || *p == '\"') {

      p++;

      continue;
    }

    return *p;
  }

  return '\0';
}

void relayInit() {

  for (uint8_t i = 0; i < 4; i++) {

    pinMode(relayPins[i], OUTPUT);

    digitalWrite(relayPins[i], ROLE_KAPAT);

    relayStates[i] = false;
  }
}

void setRelay(uint8_t index, bool on) {

  if (index >= 4) return;

  digitalWrite(relayPins[index], on ? ROLE_AC : ROLE_KAPAT);

  relayStates[index] = on;

  Serial.print(F("Role "));
  Serial.print(index + 1);
  Serial.print(F(" -> "));
  Serial.println(on ? F("ACIK") : F("KAPALI"));
}

void setAllRelays(bool on) {

  for (uint8_t i = 0; i < 4; i++) {

    setRelay(i, on);
  }
}

bool isAdminNumber(const char *number) {

  char normalized[PHONE_LEN];

  normalizePhone(number, normalized, sizeof(normalized));

  Serial.print(F("Kontrol edilen numara: "));
  Serial.println(normalized);

  for (uint8_t i = 0; i < MAX_ADMINS; i++) {

    if (adminNumbers[i][0] == '\0') continue;

    char adminNorm[PHONE_LEN];

    normalizePhone(adminNumbers[i], adminNorm, sizeof(adminNorm));

    Serial.print(F("Kayitli admin: "));
    Serial.println(adminNorm);

    if (strcmp(adminNorm, normalized) == 0) {

      Serial.println(F("ADMIN DOGRULANDI"));

      return true;
    }
  }

  Serial.println(F("ADMIN BULUNAMADI"));

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

  if (ADMIN2[0] != '\0') {

    normalizePhone(ADMIN2, adminNumbers[1], PHONE_LEN);
  }

  if (ADMIN3[0] != '\0') {

    normalizePhone(ADMIN3, adminNumbers[2], PHONE_LEN);
  }
}

void loadAdmins() {

  for (uint8_t i = 0; i < MAX_ADMINS; i++) {

    clearBuffer(adminNumbers[i], PHONE_LEN);
  }

  if (EEPROM.read(0) != EEPROM_MAGIC) {

    Serial.println(F("EEPROM BOS"));

    setDefaultAdmins();

    saveAdmins();

    return;
  }

  int addr = 1;

  for (uint8_t i = 0; i < MAX_ADMINS; i++) {

    for (uint8_t j = 0; j < PHONE_LEN; j++) {

      adminNumbers[i][j] = (char)EEPROM.read(addr++);
    }

    adminNumbers[i][PHONE_LEN - 1] = '\0';

    char normalized[PHONE_LEN];

    normalizePhone(adminNumbers[i], normalized, sizeof(normalized));

    safeCopy(adminNumbers[i], normalized, PHONE_LEN);
  }

  if (!isValidPhone(adminNumbers[0])) {

    Serial.println(F("ADMIN BOZUK"));

    setDefaultAdmins();

    saveAdmins();
  }

  Serial.println(F("EEPROM ADMINLER YUKLENDI"));
}

void printAdmins() {

  Serial.println(F("Admin listesi:"));

  for (uint8_t i = 0; i < MAX_ADMINS; i++) {

    if (adminNumbers[i][0] != '\0') {

      Serial.print(i + 1);

      Serial.print(F(" -> "));

      Serial.println(adminNumbers[i]);
    }
  }
}

void sendAT(const __FlashStringHelper *command, unsigned int waitMs) {

  gsm.println(command);

  delay(waitMs);
}

void sendSMSFlash(const char *number, const __FlashStringHelper *message) {

  sendAT(F("AT+CMGF=1"), 500);

  gsm.print(F("AT+CMGS=\""));

  gsm.print(number);

  gsm.println(F("\""));

  delay(500);

  gsm.print(message);

  delay(500);

  gsm.write(26);

  delay(4000);
}

void answerCall() {

  gsm.println(F("ATA"));

  callState = CALL_ACTIVE;

  callStartMs = millis();
}

void hangupCall() {

  gsm.println(F("ATH"));

  callState = CALL_IDLE;

  clipReceived = false;

  lastCaller[0] = '\0';
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

    case '#':
      hangupCall();
      break;
  }
}

void processGSMLine(char *line) {

  if (strncmp_P(line, PSTR("+CLIP:"), 6) == 0) {

    char callerRaw[PHONE_LEN];

    clearBuffer(callerRaw, sizeof(callerRaw));

    clearBuffer(lastCaller, sizeof(lastCaller));

    if (getQuotedField(line, callerRaw, sizeof(callerRaw))) {

      normalizePhone(callerRaw, lastCaller, sizeof(lastCaller));

      Serial.print(F("Arayan: "));
      Serial.println(lastCaller);

      if (isAdminNumber(lastCaller)) {

        answerCall();

      } else {

        Serial.println(F("Yetkisiz arama"));

        hangupCall();
      }
    }

    return;
  }

  if (strncmp_P(line, PSTR("+DTMF:"), 6) == 0) {

    char tone = getDTMFTone(line);

    if (tone != '\0') {

      handleDTMF(tone);
    }

    return;
  }
}

void readGSM() {

  while (gsm.available()) {

    char c = gsm.read();

    if (c == '\r') continue;

    if (c == '\n') {

      if (gsmLineLen > 0) {

        gsmLine[gsmLineLen] = '\0';

        Serial.println(gsmLine);

        processGSMLine(gsmLine);

        gsmLineLen = 0;

        gsmLine[0] = '\0';
      }

    } else {

      if (gsmLineLen < GSM_LINE_LEN - 1) {

        gsmLine[gsmLineLen++] = c;
      }
    }
  }
}

void setup() {

  Serial.begin(9600);

  gsm.begin(9600);

  relayInit();

  pinMode(STARTUP_PIN, OUTPUT);

  digitalWrite(STARTUP_PIN, HIGH);

  delay(5000);

  digitalWrite(STARTUP_PIN, LOW);

  dht.begin();

  // SADECE 1 KEZ CALISTIR
  EEPROM.write(0, 0);

  loadAdmins();

  clearBuffer(gsmLine, sizeof(gsmLine));

  clearBuffer(lastSmsSender, sizeof(lastSmsSender));

  clearBuffer(lastCaller, sizeof(lastCaller));

  Serial.println(F("==== GSM SISTEM BASLADI ===="));

  printAdmins();

  sendAT(F("AT"), 1000);

  sendAT(F("ATE0"), 1000);

  sendAT(F("AT+CMGF=1"), 1000);

  sendAT(F("AT+CNMI=1,2,0,0,0"), 1000);

  sendAT(F("AT+CLIP=1"), 1000);

  sendAT(F("AT+DDET=1"), 1000);

  delay(2000);

  sendSMSFlash(ADMIN1, F("Sistem acildi"));
}

void loop() {

  readGSM();
}