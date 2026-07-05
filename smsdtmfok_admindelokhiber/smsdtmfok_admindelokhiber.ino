/*Şu an sistemin çalışma prensibi şöyle:

Açılışta Arduino, SIM800C modülünü STARTUP_PIN 9 üzerinden başlatıyor, ardından EEPROM’daki admin listesini yüklüyor. Eğer EEPROM’da kayıt yoksa kod içindeki varsayılan adminleri (ADMIN1, varsa ADMIN2, ADMIN3) yerleştiriyor. Sonra SMS modu, arayan numara gösterme ve DTMF algılama açılıyor. En sonda kayıtlı adminlere Sistem acildi SMS’i gidiyor.

Normal çalışmada sistem iki şeyi sürekli dinliyor: gelen SMS’leri ve gelen aramaları. SMS gelirse önce gönderen numara okunuyor, numara normalize ediliyor (053..., 5..., 90..., +90... biçimleri tek formata çevriliyor), sonra mesaj komut olarak işleniyor. Admin değilse röle komutları reddediliyor. Admin ise şu komutlar çalışıyor:

AC, KAPAT, R1 AC, R2 KAPAT gibi röle komutları
HEPSI AC, HEPSI KAPAT
DURUM -> röle durumları + DHT11 sıcaklık/nem SMS’i
ADMIN EKLE 123456 +905...
ADMIN SIL 123456 +905...
ADMINLER
Admin ekleme/silme olduğunda liste RAM’de güncelleniyor ve hemen EEPROM’a kaydediliyor; yani elektrik kesilse de kaybolmuyor. Son adminin silinmesine izin verilmiyor.

Arama tarafında, biri aradığında sistem CLIP ile numarayı alıyor. Numara adminse çağrıyı cevaplıyor, değilse kapatıyor. Cevaplanan çağrıda DTMF tuşları röleleri kontrol ediyor:

1/2 -> R1 aç/kapat
3/4 -> R2 aç/kapat
5/6 -> R3 aç/kapat
7/8 -> R4 aç/kapat
* -> hepsini aç
0 -> hepsini kapat
9 -> seri portta durum yaz
# -> aramayı kapat
Donanım mantığı şu an şu şekilde ayarlı:

2 -> GSM TX
3 -> GSM RX
4,5,6,7 -> röleler
9 -> SIM800C power key
11 -> DHT11
Röle mantığı da şu an:

HIGH = AÇ
LOW = KAPAT  */
  
  
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

// Bu kartta:
// HIGH = AC
// LOW  = KAPAT
#define ROLE_AC HIGH
#define ROLE_KAPAT LOW

SoftwareSerial gsm(GSM_RX, GSM_TX);
DHT dht(DHT_PIN, DHT_TYPE);

const uint8_t relayPins[4] = {RELE1_PIN, RELE2_PIN, RELE3_PIN, RELE4_PIN};

bool relayStates[4] = {false, false, false, false};
bool waitingSmsBody = false;

char gsmLine[GSM_LINE_LEN];
uint8_t gsmLineLen = 0;

char lastSmsSender[PHONE_LEN];
char adminNumbers[MAX_ADMINS][PHONE_LEN];

unsigned long lastDhtReadMs = 0;
float lastValidTemperature = NAN;
float lastValidHumidity = NAN;

enum CallState : uint8_t { CALL_IDLE, CALL_RINGING, CALL_ACTIVE };
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
  return c == ' ' || c == '\r' || c == '\n' || c == '\t';
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
  return (strncmp(norm, "+90", 3) == 0 && strlen(norm) == 13);
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

void printRelayStatusSerial() {
  Serial.println(F("---- ROLE DURUMU ----"));
  for (uint8_t i = 0; i < 4; i++) {
    Serial.print(F("R"));
    Serial.print(i + 1);
    Serial.print(F(": "));
    Serial.println(relayStates[i] ? F("Acik") : F("Kapali"));
  }
}

bool readDHTFresh(float &sicaklik, float &nem) {
  unsigned long now = millis();

  if (lastDhtReadMs != 0 && (now - lastDhtReadMs) < DHT_MIN_INTERVAL_MS) {
    delay(DHT_MIN_INTERVAL_MS - (now - lastDhtReadMs));
  }

  for (uint8_t i = 0; i < 3; i++) {
    nem = dht.readHumidity();
    sicaklik = dht.readTemperature();
    lastDhtReadMs = millis();

    if (!isnan(sicaklik) && !isnan(nem)) {
      lastValidTemperature = sicaklik;
      lastValidHumidity = nem;

      Serial.print(F("DHT OK -> Sicaklik: "));
      Serial.print(sicaklik);
      Serial.print(F(" C  Nem: "));
      Serial.print(nem);
      Serial.println(F(" %"));
      return true;
    }

    delay(DHT_MIN_INTERVAL_MS);
  }

  if (!isnan(lastValidTemperature) && !isnan(lastValidHumidity)) {
    sicaklik = lastValidTemperature;
    nem = lastValidHumidity;
    Serial.println(F("DHT yeni veri veremedi, son gecerli deger kullanildi"));
    return true;
  }

  Serial.println(F("DHT okuma hatasi"));
  return false;
}

void sendAT(const __FlashStringHelper *command, unsigned int waitMs) {
  Serial.print(F("AT << "));
  Serial.println(command);
  gsm.println(command);
  delay(waitMs);
}

void sendSMS(const char *number, const char *message) {
  sendAT(F("AT+CMGF=1"), 500);

  Serial.print(F("AT << AT+CMGS=\""));
  Serial.print(number);
  Serial.println(F("\""));
  gsm.print(F("AT+CMGS=\""));
  gsm.print(number);
  gsm.println(F("\""));
  delay(500);

  Serial.print(F("SMS << "));
  Serial.println(message);
  gsm.print(message);
  delay(500);

  Serial.println(F("AT << CTRL+Z"));
  gsm.write(26);
  delay(4000);
}

void sendSMSFlash(const char *number, const __FlashStringHelper *message) {
  sendAT(F("AT+CMGF=1"), 500);

  Serial.print(F("AT << AT+CMGS=\""));
  Serial.print(number);
  Serial.println(F("\""));
  gsm.print(F("AT+CMGS=\""));
  gsm.print(number);
  gsm.println(F("\""));
  delay(500);

  Serial.print(F("SMS << "));
  Serial.println(message);
  gsm.print(message);
  delay(500);

  Serial.println(F("AT << CTRL+Z"));
  gsm.write(26);
  delay(4000);
}

bool isAdminNumber(const char *number) {
  char normalized[PHONE_LEN];
  normalizePhone(number, normalized, sizeof(normalized));

  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i][0] != '\0' && strcmp(adminNumbers[i], normalized) == 0) {
      return true;
    }
  }
  return false;
}

bool addAdminNumber(const char *number) {
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i][0] == '\0') {
      safeCopy(adminNumbers[i], number, PHONE_LEN);
      return true;
    }
  }
  return false;
}

uint8_t countAdmins() {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i][0] != '\0') count++;
  }
  return count;
}

int8_t findAdminIndex(const char *number) {
  char normalized[PHONE_LEN];
  normalizePhone(number, normalized, sizeof(normalized));

  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i][0] != '\0' && strcmp(adminNumbers[i], normalized) == 0) {
      return i;
    }
  }
  return -1;
}

bool removeAdminNumber(const char *number) {
  int8_t idx = findAdminIndex(number);
  if (idx < 0) return false;

  if (countAdmins() <= 1) {
    return false;
  }

  for (uint8_t i = idx; i < MAX_ADMINS - 1; i++) {
    safeCopy(adminNumbers[i], adminNumbers[i + 1], PHONE_LEN);
  }

  clearBuffer(adminNumbers[MAX_ADMINS - 1], PHONE_LEN);
  return true;
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
  if (EEPROM.read(0) != EEPROM_MAGIC) {
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
  }

  if (adminNumbers[0][0] == '\0') {
    setDefaultAdmins();
    saveAdmins();
  }
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

void sendSMSAllAdmins(const __FlashStringHelper *message) {
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i][0] != '\0' && isValidPhone(adminNumbers[i])) {
      sendSMSFlash(adminNumbers[i], message);
    }
  }
}

void sendAdminListSMS(const char *number) {
  char msg[SMS_TEXT_LEN];
  char line[32];

  clearBuffer(msg, sizeof(msg));
  safeCopy(msg, "Adminlar:\n", sizeof(msg));

  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i][0] != '\0') {
      snprintf(line, sizeof(line), "%u:%s\n", i + 1, adminNumbers[i]);

      size_t used = strlen(msg);
      size_t freeSpace = sizeof(msg) - used - 1;
      if (freeSpace > 0) {
        strncat(msg, line, freeSpace);
      }
    }
  }

  sendSMS(number, msg);
}

void answerCall() {
  Serial.println(F("Yetkili arama, cevap veriliyor"));
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

void handleCallTimeouts() {
  unsigned long now = millis();

  if (callState == CALL_RINGING && !clipReceived && (now - ringStartMs > CLIP_TIMEOUT_MS)) {
    Serial.println(F("CLIP zaman asimi, arama kapatiliyor"));
    hangupCall();
  }

  if (callState == CALL_ACTIVE && (now - callStartMs > MAX_CALL_MS)) {
    Serial.println(F("Maksimum arama suresi doldu"));
    hangupCall();
  }
}

void sendStatusSMS(const char *number) {
  char statusMsg[SMS_TEXT_LEN];
  char tempBuf[12];
  char humBuf[12];
  float sicaklik = NAN;
  float nem = NAN;

  bool okundu = readDHTFresh(sicaklik, nem);

  if (okundu) {
    dtostrf(sicaklik, 0, 1, tempBuf);
    dtostrf(nem, 0, 1, humBuf);

    snprintf_P(
      statusMsg,
      sizeof(statusMsg),
      PSTR("R1:%s R2:%s\nR3:%s R4:%s\nSicaklik:%s C\nNem:%s %%"),
      relayStates[0] ? "Acik" : "Kapali",
      relayStates[1] ? "Acik" : "Kapali",
      relayStates[2] ? "Acik" : "Kapali",
      relayStates[3] ? "Acik" : "Kapali",
      tempBuf,
      humBuf
    );
  } else {
    snprintf_P(
      statusMsg,
      sizeof(statusMsg),
      PSTR("R1:%s R2:%s\nR3:%s R4:%s\nSicaklik:Hata\nNem:Hata"),
      relayStates[0] ? "Acik" : "Kapali",
      relayStates[1] ? "Acik" : "Kapali",
      relayStates[2] ? "Acik" : "Kapali",
      relayStates[3] ? "Acik" : "Kapali"
    );
  }

  sendSMS(number, statusMsg);
}

void handleAdminCommand(const char *sender, char *sms) {
  if (!isAdminNumber(sender)) {
    sendSMSFlash(sender, F("Yetkisiz numara"));
    return;
  }

  char work[SMS_BODY_LEN];
  safeCopy(work, sms, sizeof(work));
  normalizeCommand(work);

  if (strcmp_P(work, PSTR("ADMINLER")) == 0) {
    sendAdminListSMS(sender);
    return;
  }

  char *savePtr = NULL;
  char *t1 = strtok_r(work, " ", &savePtr);
  char *t2 = strtok_r(NULL, " ", &savePtr);
  char *t3 = strtok_r(NULL, " ", &savePtr);
  char *t4 = strtok_r(NULL, " ", &savePtr);

  char *password = NULL;
  char *targetNumber = NULL;
  bool isDelete = false;

  if (!t1 || strcmp_P(t1, PSTR("ADMIN")) != 0) {
    sendSMSFlash(sender, F("Format: ADMIN EKLE 123456 +905..."));
    return;
  }

  if (t2 && strcmp_P(t2, PSTR("EKLE")) == 0) {
    password = t3;
    targetNumber = t4;
    isDelete = false;
  }
  else if (t2 && strcmp_P(t2, PSTR("SIL")) == 0) {
    password = t3;
    targetNumber = t4;
    isDelete = true;
  }
  else {
    password = t2;
    targetNumber = t3;
    isDelete = false;
  }

  if (!password || !targetNumber) {
    sendSMSFlash(sender, F("Format: ADMIN EKLE 123456 +905..."));
    return;
  }

  if (strcmp(password, ADMIN_PASSWORD) != 0) {
    sendSMSFlash(sender, F("Admin sifresi hatali"));
    return;
  }

  char normalized[PHONE_LEN];
  normalizePhone(targetNumber, normalized, sizeof(normalized));

  if (!isValidPhone(normalized)) {
    sendSMSFlash(sender, F("Numara gecersiz"));
    return;
  }

  if (isDelete) {
    if (!isAdminNumber(normalized)) {
      sendSMSFlash(sender, F("Numara admin degil"));
      return;
    }

    if (countAdmins() <= 1) {
      sendSMSFlash(sender, F("Son admin silinemez"));
      return;
    }

    if (removeAdminNumber(normalized)) {
      char msg[48];
      saveAdmins();
      snprintf(msg, sizeof(msg), "Admin silindi: %s", normalized);
      sendSMS(sender, msg);
    } else {
      sendSMSFlash(sender, F("Admin silinemedi"));
    }

    return;
  }

  if (isAdminNumber(normalized)) {
    sendSMSFlash(sender, F("Numara zaten admin"));
    return;
  }

  if (addAdminNumber(normalized)) {
    char msg[48];
    saveAdmins();
    snprintf(msg, sizeof(msg), "Admin eklendi: %s", normalized);
    sendSMS(sender, msg);
  } else {
    sendSMSFlash(sender, F("Admin listesi dolu"));
  }
}

void handleDTMF(char tone) {
  Serial.print(F("DTMF >> "));
  Serial.println(tone);

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
    case '9': printRelayStatusSerial(); break;
    case '#':
      Serial.println(F("Arama DTMF ile kapatildi"));
      hangupCall();
      break;
    default:
      Serial.println(F("Tanimsiz DTMF tusu"));
      break;
  }
}

void handleSMS(const char *sender, char *smsText) {
  normalizeCommand(smsText);

  Serial.println(F("----- YENI SMS -----"));
  Serial.print(F("Gonderen: "));
  Serial.println(sender);
  Serial.print(F("Icerik: "));
  Serial.println(smsText);

  if (strcmp_P(smsText, PSTR("ADMINLER")) == 0 || strncmp_P(smsText, PSTR("ADMIN "), 6) == 0) {
    handleAdminCommand(sender, smsText);
    return;
  }

  if (!isAdminNumber(sender)) {
    Serial.println(F("Yetkisiz numara"));
    sendSMSFlash(sender, F("Yetkisiz numara"));
    return;
  }

  if (strcmp_P(smsText, PSTR("AC")) == 0 || strcmp_P(smsText, PSTR("R1 AC")) == 0 || strcmp_P(smsText, PSTR("ROLE1 AC")) == 0) {
    setRelay(0, true);
    sendSMSFlash(sender, F("Role 1 acildi"));
  }
  else if (strcmp_P(smsText, PSTR("KAPAT")) == 0 || strcmp_P(smsText, PSTR("R1 KAPAT")) == 0 || strcmp_P(smsText, PSTR("ROLE1 KAPAT")) == 0) {
    setRelay(0, false);
    sendSMSFlash(sender, F("Role 1 kapandi"));
  }
  else if (strcmp_P(smsText, PSTR("R2 AC")) == 0 || strcmp_P(smsText, PSTR("ROLE2 AC")) == 0) {
    setRelay(1, true);
    sendSMSFlash(sender, F("Role 2 acildi"));
  }
  else if (strcmp_P(smsText, PSTR("R2 KAPAT")) == 0 || strcmp_P(smsText, PSTR("ROLE2 KAPAT")) == 0) {
    setRelay(1, false);
    sendSMSFlash(sender, F("Role 2 kapandi"));
  }
  else if (strcmp_P(smsText, PSTR("R3 AC")) == 0 || strcmp_P(smsText, PSTR("ROLE3 AC")) == 0) {
    setRelay(2, true);
    sendSMSFlash(sender, F("Role 3 acildi"));
  }
  else if (strcmp_P(smsText, PSTR("R3 KAPAT")) == 0 || strcmp_P(smsText, PSTR("ROLE3 KAPAT")) == 0) {
    setRelay(2, false);
    sendSMSFlash(sender, F("Role 3 kapandi"));
  }
  else if (strcmp_P(smsText, PSTR("R4 AC")) == 0 || strcmp_P(smsText, PSTR("ROLE4 AC")) == 0) {
    setRelay(3, true);
    sendSMSFlash(sender, F("Role 4 acildi"));
  }
  else if (strcmp_P(smsText, PSTR("R4 KAPAT")) == 0 || strcmp_P(smsText, PSTR("ROLE4 KAPAT")) == 0) {
    setRelay(3, false);
    sendSMSFlash(sender, F("Role 4 kapandi"));
  }
  else if (strcmp_P(smsText, PSTR("HEPSI AC")) == 0) {
    setAllRelays(true);
    sendSMSFlash(sender, F("Tum roleler acildi"));
  }
  else if (strcmp_P(smsText, PSTR("HEPSI KAPAT")) == 0) {
    setAllRelays(false);
    sendSMSFlash(sender, F("Tum roleler kapandi"));
  }
  else if (strcmp_P(smsText, PSTR("DURUM")) == 0) {
    sendStatusSMS(sender);
  }
  else {
    Serial.println(F("Hatali komut"));
    sendSMSFlash(sender, F("Hatali komut"));
  }
}

void processGSMLine(char *line) {
  if (strncmp_P(line, PSTR("+CMT:"), 5) == 0) {
    char senderRaw[PHONE_LEN];
    clearBuffer(senderRaw, sizeof(senderRaw));
    clearBuffer(lastSmsSender, sizeof(lastSmsSender));

    if (getQuotedField(line, senderRaw, sizeof(senderRaw))) {
      normalizePhone(senderRaw, lastSmsSender, sizeof(lastSmsSender));
    }

    waitingSmsBody = true;

    Serial.println(F("----- YENI SMS BASLIGI -----"));
    Serial.print(F("SMS Gonderen >> "));
    Serial.println(lastSmsSender);
    return;
  }

  if (waitingSmsBody) {
    waitingSmsBody = false;
    handleSMS(lastSmsSender, line);
    return;
  }

  if (strcmp_P(line, PSTR("RING")) == 0) {
    if (callState == CALL_IDLE) {
      callState = CALL_RINGING;
      ringStartMs = millis();
      clipReceived = false;
      lastCaller[0] = '\0';
      Serial.println(F("Gelen arama algilandi"));
    }
    return;
  }

  if (strncmp_P(line, PSTR("+CLIP:"), 6) == 0) {
    char callerRaw[PHONE_LEN];
    clearBuffer(callerRaw, sizeof(callerRaw));
    clearBuffer(lastCaller, sizeof(lastCaller));

    if (getQuotedField(line, callerRaw, sizeof(callerRaw))) {
      normalizePhone(callerRaw, lastCaller, sizeof(lastCaller));
      clipReceived = true;

      Serial.print(F("Arayan numara >> "));
      Serial.println(lastCaller);

      if (callState == CALL_RINGING) {
        if (isAdminNumber(lastCaller)) {
          answerCall();
        } else {
          Serial.println(F("Yetkisiz arama, kapatiliyor"));
          hangupCall();
        }
      }
    }
    return;
  }

  if (strncmp_P(line, PSTR("+DTMF:"), 6) == 0) {
    if (callState == CALL_ACTIVE) {
      char tone = getDTMFTone(line);
      if (tone != '\0') {
        handleDTMF(tone);
      }
    }
    return;
  }

  if (strcmp_P(line, PSTR("NO CARRIER")) == 0 ||
      strcmp_P(line, PSTR("BUSY")) == 0 ||
      strcmp_P(line, PSTR("NO ANSWER")) == 0) {
    Serial.println(F("Arama sonlandi"));
    callState = CALL_IDLE;
    clipReceived = false;
    lastCaller[0] = '\0';
  }
}

void readGSM() {
  while (gsm.available()) {
    char c = gsm.read();

    if (c == '\r') continue;

    if (c == '\n') {
      if (gsmLineLen > 0) {
        gsmLine[gsmLineLen] = '\0';
        Serial.print(F("GSM >> "));
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
  loadAdmins();

  clearBuffer(gsmLine, sizeof(gsmLine));
  clearBuffer(lastSmsSender, sizeof(lastSmsSender));
  clearBuffer(lastCaller, sizeof(lastCaller));

  Serial.println(F("==== GSM Sulama Sistemi Basladi ===="));
  printAdmins();

  sendAT(F("AT"), 1000);
  sendAT(F("ATE0"), 1000);
  sendAT(F("AT+CMGF=1"), 1000);
  sendAT(F("AT+CNMI=1,2,0,0,0"), 1000);
  sendAT(F("AT+CLIP=1"), 1000);
  sendAT(F("AT+DDET=1"), 1000);

  delay(2000);
  sendSMSAllAdmins(F("Sistem acildi"));
}

void loop() {
  readGSM();
  handleCallTimeouts();
}
