/*
# Özet: Çalışma Prensibi ve Çözülen Hatalar

## 📋 ÇALIŞMA PRENSİBİ

### Açılış ve Başlangıç
1. **GSM Modül Başlatma**: STARTUP_PIN 9 üzerinden SIM800C modülü 5 saniye boyunca start edilir
2. **Admin Listesi Yükleme**: EEPROM'dan admin numaraları okunur, kayıt yoksa varsayılan adminler (ADMIN1, ADMIN2, ADMIN3) yüklenir
3. **Modül Konfigürasyonu**: SMS modu, CLI (arayan numara gösterme), DTMF algılama aktif edilir
4. **Bildirim**: Kayıtlı tüm adminlere "Sistem açıldı" SMS'i gider

### Normal İşletim - İki Ana Fonksiyon

#### 1. SMS KOMUTLARI (Admin yetkisi gerektirir)
| Komut | İşlev |
|-------|-------|
| `AC` veya `R1 AC` | Röle 1 AÇ |
| `KAPAT` veya `R1 KAPAT` | Röle 1 KAPAT |
| `HEPSI AC` | Tüm röleler AÇ |
| `HEPSI KAPAT` | Tüm röleler KAPAT |
| `DURUM` | Röle durumları + sıcaklık/nem SMS'i |
| `ADMIN EKLE 123456 +905XXXXXXXXX` | Yeni admin ekle |
| `ADMIN SIL 123456 +905XXXXXXXXX` | Admin sil (son admin silinemez) |
| `ADMINLER` | Admin listesini SMS gönder |

#### 2. ARAMA ve DTMF KONTROLÜ (Admin ararsa)
| DTMF Tuşu | İşlev |
|-----------|-------|
| 1 | R1 AÇ |
| 2 | R1 KAPAT |
| 3 | R2 AÇ |
| 4 | R2 KAPAT |
| 5 | R3 AÇ |
| 6 | R3 KAPAT |
| 7 | R4 AÇ |
| 8 | R4 KAPAT |
| * | Tüm röleler AÇ |
| 0 | Tüm röleler KAPAT |
| 9 | Seri porta durum yaz |
| # | Aramayı sonlandır |

### Donanım Bağlantıları
```
Pin 2  → GSM TX
Pin 3  → GSM RX  
Pin 4  → Röle 1 (HIGH=AÇ, LOW=KAPAT)
Pin 5  → Röle 2
Pin 6  → Röle 3
Pin 7  → Röle 4
Pin 9  → SIM800C Power Key
Pin 11 → DHT11
```

---

## 🔧 ÇÖZÜLEN HATALAR

### Hata 1: Uzun Süreli Çalışmada Aramaların Meşgule Düşmesi/Kapanması
**Nedeni:** GSM modülü 13+ saat çalıştıktan sonra AT komutlarına yanıt vermemeye başlıyordu

**Çözüm:**
- **GSM Watchdog Mekanizması** eklendi (`GSM_WATCHDOG_MS = 30000ms`)
- `isGSMAlive()` fonksiyonu ile her 30 saniyede bir modül canlılığı kontrol ediliyor
- Yanıt gelmezse `resetGSMModule()` ile modül otomatik resetleniyor
- `sendATWithResponse()` fonksiyonu ile komutların başarılı olup olmadığı kontrol ediliyor
- Tüm GSM işlemlerinde `lastGsmActivityMs` zaman damgası güncelleniyor

### Hata 2: Kayıtlı Admin Numaradan SMS'te "Yetkisiz Numara" Hatası
**Nedeni:** EEPROM okuma/yazma sırasında senkronizasyon sorunu ve normalize edilmiş numaralar ile karşılaştırma hatası

**Çözüm:**
- `EEPROM.write()` yerine `EEPROM.update()` kullanıldı (sadece değişen byte'lar yazılır)
- Admin kaydetme/silme sonrası `saveAdmins()` çağrılmadan önce liste tamamen güncelleniyor
- Yükleme sırasında `EEPROM.read(0) == EEPROM_MAGIC` kontrolü iyileştirildi
- `handleAdminCommand()` içinde yetki kontrolü en başa alındı

### Hata 3: Uzun Enerji Kesintisi Sonrası Kod Yüklemeden Çalışmama
**Nedeni:** GSM modülü bekleme süresi yetersizdi ve modül hazır olmadan AT komutları gönderiliyordu

**Çözüm:**
```
ESKİ:                                   YENİ:
digitalWrite(STARTUP_PIN, HIGH);        digitalWrite(STARTUP_PIN, HIGH);
delay(5000);                            delay(3000);
digitalWrite(STARTUP_PIN, LOW);         digitalWrite(STARTUP_PIN, LOW);
                                        delay(5000);  // ← 5 saniye bekleme eklendi
                                        
                                        // Modül hazır olana kadar bekle
                                        for (int i = 0; i < 3 && !isGSMAlive(); i++) {
                                          delay(2000);
                                        }
```

### Ek İyileştirmeler
1. **Daha iyi hata yönetimi**: SMS göndermeden önce GSM bağlantısı kontrol ediliyor
2. **String güvenliği**: Tüm buffer işlemlerinde taşma kontrolleri artırıldı
3. **Serial debug**: Hata durumlarında daha detaylı log çıktıları eklendi

---

## ✅ TEST ÖNERİLERİ

1. **Serial Monitör çıktılarını izleyin** (9600 baud):
   - "GSM modulu yanit vermiyor, resetleniyor..." mesajını görürseniz watchdog çalışıyor demektir
   - "Admin listesi EEPROM'a kaydedildi" mesajı EEPROM yazma başarılı demektir

2. **Uzun süreli test**: Cihazı 24-48 saat çalıştırıp periyodik olarak arama yaparak test edin

3. **Enerji kesintisi testi**: Cihazı 10 dakika fişten çekip tekrar takın, otomatik çalışıp çalışmadığını kontrol edin
*/

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
#define ADMIN2 ""
#define ADMIN3 ""

#define ADMIN_PASSWORD "123456"

#define MAX_ADMINS 6
#define PHONE_LEN 16
#define EEPROM_MAGIC 77
#define DHT_MIN_INTERVAL_MS 2500UL

#define CLIP_TIMEOUT_MS 4000UL
#define MAX_CALL_MS 120000UL
#define GSM_WATCHDOG_MS 30000UL  // GSM watchdog süresi

#define GSM_LINE_LEN 96
#define SMS_BODY_LEN 96
#define SMS_TEXT_LEN 160

#define ROLE_AC HIGH
#define ROLE_KAPAT LOW

SoftwareSerial gsm(GSM_RX, GSM_TX);
DHT dht(DHT_PIN, DHT_TYPE);

const uint8_t relayPins[4] = {RELE1_PIN, RELE2_PIN, RELE3_PIN, RELE4_PIN};
bool relayStates[4] = {false, false, false, false};

char gsmLine[GSM_LINE_LEN];
uint8_t gsmLineLen = 0;
char lastSmsSender[PHONE_LEN];
char adminNumbers[MAX_ADMINS][PHONE_LEN];

unsigned long lastDhtReadMs = 0;
float lastValidTemperature = NAN;
float lastValidHumidity = NAN;
unsigned long lastGsmActivityMs = 0;
bool gsmNeedsReset = false;

enum CallState : uint8_t { CALL_IDLE, CALL_RINGING, CALL_ACTIVE };
CallState callState = CALL_IDLE;
unsigned long callStartMs = 0;
unsigned long ringStartMs = 0;
bool clipReceived = false;
char lastCaller[PHONE_LEN];
bool waitingSmsBody = false;

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
  size_t r = 0, w = 0;
  bool lastWasSpace = false;
  
  while (text[r] && isSpaceChar(text[r])) r++;
  
  while (text[r]) {
    char c = text[r++];
    if (c >= 'a' && c <= 'z') c -= 32;
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
  
  for (uint8_t i = 0; src[i] && d < sizeof(digits) - 1; i++) {
    if (src[i] >= '0' && src[i] <= '9') digits[d++] = src[i];
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
  const char *first = strchr(line, '"');
  if (!first) return false;
  const char *second = strchr(first + 1, '"');
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
    if (*p == ' ' || *p == '"') p++;
    else return *p;
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
  Serial.print(F("Role ")); Serial.print(index + 1);
  Serial.print(F(" -> ")); Serial.println(on ? F("ACIK") : F("KAPALI"));
}

void setAllRelays(bool on) {
  for (uint8_t i = 0; i < 4; i++) setRelay(i, on);
}

void printRelayStatusSerial() {
  Serial.println(F("---- ROLE DURUMU ----"));
  for (uint8_t i = 0; i < 4; i++) {
    Serial.print(F("R")); Serial.print(i + 1);
    Serial.print(F(": ")); Serial.println(relayStates[i] ? F("Acik") : F("Kapali"));
  }
}

bool readDHTFresh(float &sicaklik, float &nem) {
  unsigned long now = millis();
  if (lastDhtReadMs && (now - lastDhtReadMs) < DHT_MIN_INTERVAL_MS) {
    delay(DHT_MIN_INTERVAL_MS - (now - lastDhtReadMs));
  }
  
  for (uint8_t i = 0; i < 3; i++) {
    nem = dht.readHumidity();
    sicaklik = dht.readTemperature();
    lastDhtReadMs = millis();
    
    if (!isnan(sicaklik) && !isnan(nem)) {
      lastValidTemperature = sicaklik;
      lastValidHumidity = nem;
      Serial.print(F("DHT OK -> Sicaklik: ")); Serial.print(sicaklik);
      Serial.print(F(" C Nem: ")); Serial.print(nem); Serial.println(F(" %"));
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
  Serial.print(F("AT << ")); Serial.println(command);
  gsm.println(command);
  delay(waitMs);
  lastGsmActivityMs = millis();
}

bool sendATWithResponse(const __FlashStringHelper *command, const char *expected, unsigned int timeoutMs) {
  gsm.println(command);
  unsigned long start = millis();
  char buffer[32];
  uint8_t idx = 0;
  
  while (millis() - start < timeoutMs) {
    if (gsm.available()) {
      char c = gsm.read();
      if (c == '\n') {
        buffer[idx] = '\0';
        if (strstr(buffer, expected)) {
          lastGsmActivityMs = millis();
          return true;
        }
        idx = 0;
      } else if (c != '\r' && idx < sizeof(buffer) - 1) {
        buffer[idx++] = c;
      }
    }
  }
  return false;
}

void resetGSMModule() {
  Serial.println(F("*** GSM modulu resetleniyor ***"));
  digitalWrite(STARTUP_PIN, HIGH);
  delay(2000);
  digitalWrite(STARTUP_PIN, LOW);
  delay(5000);
  
  // Yeniden başlatma komutları
  sendAT(F("AT"), 1000);
  sendAT(F("ATE0"), 1000);
  sendAT(F("AT+CMGF=1"), 1000);
  sendAT(F("AT+CNMI=1,2,0,0,0"), 1000);
  sendAT(F("AT+CLIP=1"), 1000);
  sendAT(F("AT+DDET=1"), 1000);
  
  gsmNeedsReset = false;
  lastGsmActivityMs = millis();
  Serial.println(F("*** GSM modulu resetlendi ***"));
}

bool isGSMAlive() {
  return sendATWithResponse(F("AT"), "OK", 2000);
}

void sendSMS(const char *number, const char *message) {
  if (!isGSMAlive()) {
    Serial.println(F("GSM modulu cevap vermiyor"));
    gsmNeedsReset = true;
    return;
  }
  
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

void sendSMSFlash(const char *number, const __FlashStringHelper *message) {
  char buffer[SMS_TEXT_LEN];
  strcpy_P(buffer, (PGM_P)message);
  sendSMS(number, buffer);
}

bool isAdminNumber(const char *number) {
  char normalized[PHONE_LEN];
  normalizePhone(number, normalized, sizeof(normalized));
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i][0] && strcmp(adminNumbers[i], normalized) == 0) return true;
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
    if (adminNumbers[i][0]) count++;
  }
  return count;
}

int8_t findAdminIndex(const char *number) {
  char normalized[PHONE_LEN];
  normalizePhone(number, normalized, sizeof(normalized));
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i][0] && strcmp(adminNumbers[i], normalized) == 0) return i;
  }
  return -1;
}

bool removeAdminNumber(const char *number) {
  int8_t idx = findAdminIndex(number);
  if (idx < 0 || countAdmins() <= 1) return false;
  
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
  Serial.println(F("Admin listesi EEPROM'a kaydedildi"));
}

void setDefaultAdmins() {
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    clearBuffer(adminNumbers[i], PHONE_LEN);
  }
  normalizePhone(ADMIN1, adminNumbers[0], PHONE_LEN);
  if (ADMIN2[0]) normalizePhone(ADMIN2, adminNumbers[1], PHONE_LEN);
  if (ADMIN3[0]) normalizePhone(ADMIN3, adminNumbers[2], PHONE_LEN);
}

void loadAdmins() {
  if (EEPROM.read(0) != EEPROM_MAGIC) {
    Serial.println(F("EEPROM'da kayit yok, varsayilan adminler yukleniyor"));
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
    if (adminNumbers[i][0]) {
      Serial.print(i + 1); Serial.print(F(" -> ")); Serial.println(adminNumbers[i]);
    }
  }
}

void sendSMSAllAdmins(const __FlashStringHelper *message) {
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i][0] && isValidPhone(adminNumbers[i])) {
      sendSMSFlash(adminNumbers[i], message);
    }
  }
}

void sendAdminListSMS(const char *number) {
  char msg[SMS_TEXT_LEN] = "Adminlar:\n";
  char line[32];
  
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i][0]) {
      snprintf(line, sizeof(line), "%u:%s\n", i + 1, adminNumbers[i]);
      strncat(msg, line, sizeof(msg) - strlen(msg) - 1);
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
  char tempBuf[12], humBuf[12];
  float sicaklik = NAN, nem = NAN;
  
  if (readDHTFresh(sicaklik, nem)) {
    dtostrf(sicaklik, 0, 1, tempBuf);
    dtostrf(nem, 0, 1, humBuf);
    snprintf_P(statusMsg, sizeof(statusMsg),
      PSTR("R1:%s R2:%s\nR3:%s R4:%s\nSicaklik:%s C\nNem:%s %%"),
      relayStates[0] ? "Acik" : "Kapali", relayStates[1] ? "Acik" : "Kapali",
      relayStates[2] ? "Acik" : "Kapali", relayStates[3] ? "Acik" : "Kapali",
      tempBuf, humBuf);
  } else {
    snprintf_P(statusMsg, sizeof(statusMsg),
      PSTR("R1:%s R2:%s\nR3:%s R4:%s\nSicaklik:Hata\nNem:Hata"),
      relayStates[0] ? "Acik" : "Kapali", relayStates[1] ? "Acik" : "Kapali",
      relayStates[2] ? "Acik" : "Kapali", relayStates[3] ? "Acik" : "Kapali");
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
  
  if (!t1 || strcmp_P(t1, PSTR("ADMIN")) != 0) {
    sendSMSFlash(sender, F("Format: ADMIN EKLE 123456 +905..."));
    return;
  }
  
  bool isDelete = (t2 && strcmp_P(t2, PSTR("SIL")) == 0);
  char *password = isDelete ? t3 : t2;
  char *targetNumber = isDelete ? t4 : t3;
  
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
  } else {
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
}

void handleDTMF(char tone) {
  Serial.print(F("DTMF >> ")); Serial.println(tone);
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
    case '#': hangupCall(); break;
  }
}

void handleSMS(const char *sender, char *smsText) {
  normalizeCommand(smsText);
  Serial.println(F("----- YENI SMS -----"));
  Serial.print(F("Gonderen: ")); Serial.println(sender);
  Serial.print(F("Icerik: ")); Serial.println(smsText);
  
  if (strcmp_P(smsText, PSTR("ADMINLER")) == 0 || strncmp_P(smsText, PSTR("ADMIN "), 6) == 0) {
    handleAdminCommand(sender, smsText);
    return;
  }
  
  if (!isAdminNumber(sender)) {
    Serial.println(F("Yetkisiz numara"));
    sendSMSFlash(sender, F("Yetkisiz numara"));
    return;
  }
  
  if (strcmp_P(smsText, PSTR("AC")) == 0 || strcmp_P(smsText, PSTR("R1 AC")) == 0) {
    setRelay(0, true);
    sendSMSFlash(sender, F("Role 1 acildi"));
  } else if (strcmp_P(smsText, PSTR("KAPAT")) == 0 || strcmp_P(smsText, PSTR("R1 KAPAT")) == 0) {
    setRelay(0, false);
    sendSMSFlash(sender, F("Role 1 kapandi"));
  } else if (strcmp_P(smsText, PSTR("R2 AC")) == 0) {
    setRelay(1, true);
    sendSMSFlash(sender, F("Role 2 acildi"));
  } else if (strcmp_P(smsText, PSTR("R2 KAPAT")) == 0) {
    setRelay(1, false);
    sendSMSFlash(sender, F("Role 2 kapandi"));
  } else if (strcmp_P(smsText, PSTR("R3 AC")) == 0) {
    setRelay(2, true);
    sendSMSFlash(sender, F("Role 3 acildi"));
  } else if (strcmp_P(smsText, PSTR("R3 KAPAT")) == 0) {
    setRelay(2, false);
    sendSMSFlash(sender, F("Role 3 kapandi"));
  } else if (strcmp_P(smsText, PSTR("R4 AC")) == 0) {
    setRelay(3, true);
    sendSMSFlash(sender, F("Role 4 acildi"));
  } else if (strcmp_P(smsText, PSTR("R4 KAPAT")) == 0) {
    setRelay(3, false);
    sendSMSFlash(sender, F("Role 4 kapandi"));
  } else if (strcmp_P(smsText, PSTR("HEPSI AC")) == 0) {
    setAllRelays(true);
    sendSMSFlash(sender, F("Tum roleler acildi"));
  } else if (strcmp_P(smsText, PSTR("HEPSI KAPAT")) == 0) {
    setAllRelays(false);
    sendSMSFlash(sender, F("Tum roleler kapandi"));
  } else if (strcmp_P(smsText, PSTR("DURUM")) == 0) {
    sendStatusSMS(sender);
  } else {
    sendSMSFlash(sender, F("Hatali komut"));
  }
}

void processGSMLine(char *line) {
  if (strncmp_P(line, PSTR("+CMT:"), 5) == 0) {
    char senderRaw[PHONE_LEN];
    if (getQuotedField(line, senderRaw, sizeof(senderRaw))) {
      normalizePhone(senderRaw, lastSmsSender, sizeof(lastSmsSender));
    }
    waitingSmsBody = true;
    return;
  }
  
  if (waitingSmsBody) {
    waitingSmsBody = false;
    handleSMS(lastSmsSender, line);
    return;
  }
  
  if (strcmp_P(line, PSTR("RING")) == 0 && callState == CALL_IDLE) {
    callState = CALL_RINGING;
    ringStartMs = millis();
    clipReceived = false;
    lastCaller[0] = '\0';
    return;
  }
  
  if (strncmp_P(line, PSTR("+CLIP:"), 6) == 0) {
    char callerRaw[PHONE_LEN];
    if (getQuotedField(line, callerRaw, sizeof(callerRaw))) {
      normalizePhone(callerRaw, lastCaller, sizeof(lastCaller));
      clipReceived = true;
      if (callState == CALL_RINGING) {
        if (isAdminNumber(lastCaller)) answerCall();
        else hangupCall();
      }
    }
    return;
  }
  
  if (strncmp_P(line, PSTR("+DTMF:"), 6) == 0 && callState == CALL_ACTIVE) {
    char tone = getDTMFTone(line);
    if (tone) handleDTMF(tone);
    return;
  }
  
  if (strcmp_P(line, PSTR("NO CARRIER")) == 0 || strcmp_P(line, PSTR("BUSY")) == 0) {
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
        processGSMLine(gsmLine);
        gsmLineLen = 0;
      }
    } else if (gsmLineLen < GSM_LINE_LEN - 1) {
      gsmLine[gsmLineLen++] = c;
    }
    lastGsmActivityMs = millis();
  }
}

void setup() {
  Serial.begin(9600);
  gsm.begin(9600);
  relayInit();
  
  pinMode(STARTUP_PIN, OUTPUT);
  digitalWrite(STARTUP_PIN, HIGH);
  delay(3000);
  digitalWrite(STARTUP_PIN, LOW);
  delay(5000);
  
  dht.begin();
  loadAdmins();
  
  Serial.println(F("==== GSM Sulama Sistemi Basladi ===="));
  printAdmins();
  
  // GSM modülünü başlat
  for (int i = 0; i < 3 && !isGSMAlive(); i++) {
    Serial.println(F("GSM modulu bekleniyor..."));
    delay(2000);
  }
  
  sendAT(F("AT"), 1000);
  sendAT(F("ATE0"), 1000);
  sendAT(F("AT+CMGF=1"), 1000);
  sendAT(F("AT+CNMI=1,2,0,0,0"), 1000);
  sendAT(F("AT+CLIP=1"), 1000);
  sendAT(F("AT+DDET=1"), 1000);
  
  delay(2000);
  sendSMSAllAdmins(F("Sistem acildi"));
  lastGsmActivityMs = millis();
}

void loop() {
  // GSM watchdog kontrolü
  if (millis() - lastGsmActivityMs > GSM_WATCHDOG_MS) {
    if (!isGSMAlive()) {
      Serial.println(F("GSM modulu yanit vermiyor, resetleniyor..."));
      resetGSMModule();
    } else {
      lastGsmActivityMs = millis();
    }
  }
  
  readGSM();
  handleCallTimeouts();
}