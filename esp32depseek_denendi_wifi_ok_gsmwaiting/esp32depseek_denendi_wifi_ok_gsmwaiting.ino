/*
  ============================================================
  ESP32 ENTEGRE SİSTEM: GSM + Blynk + WiFi Manager + DTMF + SMS
  ============================================================
  
  ÖZELLİKLER:
  1. WiFi varsa → Blynk üzerinden röle kontrolü
  2. WiFi yoksa → GSM üzerinden SMS/DTMF kontrolü
  3. WiFi şifresi dışarıdan girilebilir (WiFi Manager)
  4. Blynk Token dışarıdan girilebilir
  5. Admin listesi EEPROM'da saklanır
  6. DHT22 sıcaklık/nem ölçümü (DHT11 de çalışır)
  7. DTMF ile arama sırasında röle kontrolü
  8. Otomatik GSM watchdog
  
  ESP32 BAĞLANTILARI:
  GPIO15 → GSM TX (SIM800C RX)
  GPIO16 → GSM RX (SIM800C TX)
  GPIO17 → GSM STARTUP PIN (Power Key)
  GPIO18 → Röle 1
  GPIO19 → Röle 2
  GPIO21 → Röle 3
  GPIO22 → Röle 4
  GPIO23 → DHT11/DHT22 DATA
  
  NOT: SIM800C 5V ise MUTLAKA seviye dönüştürücü kullanın!
  
  Blynk App Ayarları:
  - V1: Röle 1 Kontrolü (BUTTON - SWITCH mod)
  - V2: Röle 2 Kontrolü
  - V3: Röle 3 Kontrolü
  - V4: Röle 4 Kontrolü
  - V5: Tüm Röleleri AÇ (BUTTON - PUSH mod)
  - V6: Tüm Röleleri KAPAT (BUTTON - PUSH mod)
  - V7: Durum ve Sıcaklık Gösterimi (LABEL)
  - V8: Röle Durumları (LABEL)
  ============================================================
*/
// ========== BLYNK 2.0 ZORUNLU TANIMLAMALARI ==========
#define BLYNK_TEMPLATE_ID "TXXXXXXXXXX"    // Blynk Console'dan alın
#define BLYNK_TEMPLATE_NAME "GSM Role Kontrol"
#define BLYNK_AUTH_TOKEN "your_blynk_token"  // Veya WiFi Manager ile girilecek
// ========== KÜTÜPHANELER ==========
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <DHT.h>
#include <EEPROM.h>
#include <HardwareSerial.h>

// ========== BLYNK AYARLARI ==========
WiFiManager wifiManager;
char blynk_token[34] = "YOUR_BLYNK_TOKEN";

// Blynk Sanal Pin Tanımlamaları
#define BLYNK_VPIN_RELAY1     V0
#define BLYNK_VPIN_RELAY2     V2
#define BLYNK_VPIN_RELAY3     V3
#define BLYNK_VPIN_RELAY4     V4
#define BLYNK_VPIN_ALL_ON     V5
#define BLYNK_VPIN_ALL_OFF    V6
#define BLYNK_VPIN_STATUS     V7
#define BLYNK_VPIN_RELAY_STATES V8

// ========== GSM PIN TANIMLAMALARI (ESP32) ==========
#define GSM_RX 16           // ESP32 RX pin - GSM TX'e bağlan
#define GSM_TX 15           // ESP32 TX pin - GSM RX'e bağlan
#define STARTUP_PIN 17      // GSM modülü power key pin

// ========== RÖLE PIN TANIMLAMALARI ==========
#define RELE1_PIN 25
#define RELE2_PIN 26
#define RELE3_PIN 32
#define RELE4_PIN 33

// ========== DHT PIN TANIMLAMASI ==========
#define DHT_PIN 23
#define DHT_TYPE DHT22      // DHT11 kullanıyorsanız DHT11 yapın

// ========== ADMIN AYARLARI ==========
#define ADMIN1 "+905416932009"
#define ADMIN2 ""
#define ADMIN3 ""
#define ADMIN_PASSWORD "123456"

// ========== SABİTLER ==========
#define MAX_ADMINS 6
#define PHONE_LEN 16
#define EEPROM_MAGIC 77
#define DHT_MIN_INTERVAL_MS 2500UL
#define CLIP_TIMEOUT_MS 4000UL
#define MAX_CALL_MS 120000UL
#define GSM_WATCHDOG_MS 30000UL
#define GSM_LINE_LEN 96
#define SMS_BODY_LEN 96
#define SMS_TEXT_LEN 160
#define BLYNK_RECONNECT_INTERVAL 30000

// Röle mantığı (HIGH = AÇ, LOW = KAPAT)
#define ROLE_AC HIGH
#define ROLE_KAPAT LOW

// ========== NESNELER ==========
HardwareSerial gsm(2);      // ESP32'nin 2. donanımsal serial portu
DHT dht(DHT_PIN, DHT_TYPE);

// ========== DEĞİŞKENLER ==========
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
unsigned long lastBlynkAttempt = 0;
bool blynkConnected = false;

enum CallState : uint8_t { CALL_IDLE, CALL_RINGING, CALL_ACTIVE };
CallState callState = CALL_IDLE;
unsigned long callStartMs = 0;
unsigned long ringStartMs = 0;
bool clipReceived = false;
char lastCaller[PHONE_LEN];
bool waitingSmsBody = false;

// ========== BLYNK WIDGET CALLBACKS ==========
void updateBlynkRelayStates() {
  if (!blynkConnected) return;
  Blynk.virtualWrite(BLYNK_VPIN_RELAY1, relayStates[0]);
  Blynk.virtualWrite(BLYNK_VPIN_RELAY2, relayStates[1]);
  Blynk.virtualWrite(BLYNK_VPIN_RELAY3, relayStates[2]);
  Blynk.virtualWrite(BLYNK_VPIN_RELAY4, relayStates[3]);
  
  char relayText[50];
  snprintf(relayText, sizeof(relayText), "R1:%s R2:%s R3:%s R4:%s",
    relayStates[0] ? "AÇ" : "KAP",
    relayStates[1] ? "AÇ" : "KAP",
    relayStates[2] ? "AÇ" : "KAP",
    relayStates[3] ? "AÇ" : "KAP");
  Blynk.virtualWrite(BLYNK_VPIN_RELAY_STATES, relayText);
}

void updateBlynkStatus() {
  if (!blynkConnected) return;
  float temp, hum;
  char statusMsg[100];
  
  if (readDHTFresh(temp, hum)) {
    snprintf(statusMsg, sizeof(statusMsg), "Sıcaklık: %.1f°C\nNem: %.1f%%", temp, hum);
    Blynk.virtualWrite(BLYNK_VPIN_STATUS, statusMsg);
  } else {
    Blynk.virtualWrite(BLYNK_VPIN_STATUS, "Sıcaklık/Nem: Hata");
  }
}

BLYNK_WRITE(BLYNK_VPIN_RELAY1) { setRelay(0, param.asInt()); }
BLYNK_WRITE(BLYNK_VPIN_RELAY2) { setRelay(1, param.asInt()); }
BLYNK_WRITE(BLYNK_VPIN_RELAY3) { setRelay(2, param.asInt()); }
BLYNK_WRITE(BLYNK_VPIN_RELAY4) { setRelay(3, param.asInt()); }

BLYNK_WRITE(BLYNK_VPIN_ALL_ON) {
  if (param.asInt() == 1) {
    setAllRelays(true);
    Blynk.virtualWrite(BLYNK_VPIN_ALL_ON, 0);
  }
}

BLYNK_WRITE(BLYNK_VPIN_ALL_OFF) {
  if (param.asInt() == 1) {
    setAllRelays(false);
    Blynk.virtualWrite(BLYNK_VPIN_ALL_OFF, 0);
  }
}

// ========== WIFI YÖNETİMİ ==========
void setupWiFiManager() {
  Serial.println(F("\n=== WiFi Manager Başlatılıyor ==="));
  
  WiFiManagerParameter custom_blynk_token("blynk", "Blynk Auth Token", blynk_token, 34);
  wifiManager.addParameter(&custom_blynk_token);
  
  if (!wifiManager.startConfigPortal("ESP32_GSM_Role")) {
    Serial.println(F("WiFi Manager bağlantı kurulamadı, GSM modunda çalışılacak"));
    blynkConnected = false;
    return;
  }
  
  strcpy(blynk_token, custom_blynk_token.getValue());
  Serial.print(F("Blynk Token: "));
  Serial.println(blynk_token);
  
  Serial.println(F("WiFi bağlantısı başarılı!"));
  Serial.print(F("IP Adresi: "));
  Serial.println(WiFi.localIP());
  
  if (strlen(blynk_token) > 5 && strcmp(blynk_token, "YOUR_BLYNK_TOKEN") != 0) {
    Blynk.config(blynk_token);
    if (Blynk.connect()) {
      blynkConnected = true;
      Serial.println(F("Blynk bağlantısı başarılı!"));
      updateBlynkRelayStates();
      updateBlynkStatus();
    }
  }
}

void handleBlynk() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!blynkConnected && millis() - lastBlynkAttempt > BLYNK_RECONNECT_INTERVAL) {
      lastBlynkAttempt = millis();
      if (strlen(blynk_token) > 5 && strcmp(blynk_token, "YOUR_BLYNK_TOKEN") != 0) {
        Blynk.config(blynk_token);
        blynkConnected = Blynk.connect();
        if (blynkConnected) updateBlynkRelayStates();
      }
    } else if (blynkConnected) {
      Blynk.run();
    }
  } else if (blynkConnected) {
    blynkConnected = false;
    Serial.println(F("WiFi bağlantısı kesildi, GSM moduna geçiliyor"));
  }
}

// ========== RÖLE FONKSİYONLARI ==========
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
  Serial.printf("Röle %d -> %s\n", index + 1, on ? "AÇIK" : "KAPALI");
  updateBlynkRelayStates();
}

void setAllRelays(bool on) {
  for (uint8_t i = 0; i < 4; i++) setRelay(i, on);
}

// ========== DHT FONKSİYONLARI ==========
bool readDHTFresh(float &temp, float &hum) {
  unsigned long now = millis();
  if (lastDhtReadMs && (now - lastDhtReadMs) < DHT_MIN_INTERVAL_MS) {
    delay(DHT_MIN_INTERVAL_MS - (now - lastDhtReadMs));
  }
  
  for (uint8_t i = 0; i < 3; i++) {
    hum = dht.readHumidity();
    temp = dht.readTemperature();
    lastDhtReadMs = millis();
    
    if (!isnan(temp) && !isnan(hum)) {
      lastValidTemperature = temp;
      lastValidHumidity = hum;
      Serial.printf("DHT OK -> Sıcaklık: %.1f C Nem: %.1f %%\n", temp, hum);
      return true;
    }
    delay(DHT_MIN_INTERVAL_MS);
  }
  
  if (!isnan(lastValidTemperature) && !isnan(lastValidHumidity)) {
    temp = lastValidTemperature;
    hum = lastValidHumidity;
    Serial.println(F("DHT yeni veri yok, son geçerli değer kullanıldı"));
    return true;
  }
  
  Serial.println(F("DHT okuma hatası"));
  return false;
}

// ========== GSM FONKSİYONLARI ==========
void sendAT(const char* cmd, unsigned int waitMs = 1000) {
  Serial.printf("AT << %s\n", cmd);
  gsm.println(cmd);
  delay(waitMs);
  lastGsmActivityMs = millis();
}

bool isGSMAlive() {
  gsm.println("AT");
  unsigned long start = millis();
  while (millis() - start < 2000) {
    if (gsm.available()) {
      String response = gsm.readStringUntil('\n');
      if (response.indexOf("OK") >= 0) {
        lastGsmActivityMs = millis();
        return true;
      }
    }
  }
  return false;
}

void resetGSMModule() {
  Serial.println(F("*** GSM modülü resetleniyor ***"));
  digitalWrite(STARTUP_PIN, HIGH);
  delay(2000);
  digitalWrite(STARTUP_PIN, LOW);
  delay(5000);
  
  sendAT("AT", 1000);
  sendAT("ATE0", 1000);
  sendAT("AT+CMGF=1", 1000);
  sendAT("AT+CNMI=1,2,0,0,0", 1000);
  sendAT("AT+CLIP=1", 1000);
  sendAT("AT+DDET=1", 1000);
  
  gsmNeedsReset = false;
  lastGsmActivityMs = millis();
}

void sendSMS(const char* number, const char* message) {
  if (!isGSMAlive()) {
    Serial.println(F("GSM modülü cevap vermiyor"));
    gsmNeedsReset = true;
    return;
  }
  
  sendAT("AT+CMGF=1", 500);
  gsm.printf("AT+CMGS=\"%s\"\r\n", number);
  delay(500);
  gsm.print(message);
  delay(500);
  gsm.write(26);
  delay(4000);
}

void sendSMSAllAdmins(const char* message) {
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i][0] != '\0') {
      sendSMS(adminNumbers[i], message);
    }
  }
}

// ========== NORMALİZASYON FONKSİYONLARI ==========
void normalizePhone(const char* src, char* dst, size_t dstSize) {
  char digits[PHONE_LEN];
  uint8_t d = 0;
  
  memset(dst, 0, dstSize);
  memset(digits, 0, sizeof(digits));
  
  for (uint8_t i = 0; src[i] && d < sizeof(digits)-1; i++) {
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
    strncpy(dst, digits, dstSize);
  }
}

bool isAdminNumber(const char* number) {
  char normalized[PHONE_LEN];
  normalizePhone(number, normalized, sizeof(normalized));
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i][0] && strcmp(adminNumbers[i], normalized) == 0) return true;
  }
  return false;
}

// ========== ADMIN YÖNETİMİ ==========
void loadAdmins() {
  EEPROM.begin(512);
  if (EEPROM.read(0) != EEPROM_MAGIC) {
    memset(adminNumbers, 0, sizeof(adminNumbers));
    normalizePhone(ADMIN1, adminNumbers[0], PHONE_LEN);
    if (ADMIN2[0]) normalizePhone(ADMIN2, adminNumbers[1], PHONE_LEN);
    if (ADMIN3[0]) normalizePhone(ADMIN3, adminNumbers[2], PHONE_LEN);
    saveAdmins();
    return;
  }
  
  int addr = 1;
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    for (uint8_t j = 0; j < PHONE_LEN; j++) {
      adminNumbers[i][j] = EEPROM.read(addr++);
    }
    adminNumbers[i][PHONE_LEN-1] = '\0';
  }
}

void saveAdmins() {
  EEPROM.write(0, EEPROM_MAGIC);
  int addr = 1;
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    for (uint8_t j = 0; j < PHONE_LEN; j++) {
      EEPROM.write(addr++, adminNumbers[i][j]);
    }
  }
  EEPROM.commit();
  Serial.println(F("Admin listesi EEPROM'a kaydedildi"));
}

// ========== DURUM SMS ==========
void sendStatusSMS(const char* number) {
  char statusMsg[SMS_TEXT_LEN];
  float temp, hum;
  
  if (readDHTFresh(temp, hum)) {
    snprintf(statusMsg, sizeof(statusMsg),
      "R1:%s R2:%s R3:%s R4:%s\nSıcaklık:%.1f C\nNem:%.1f %%",
      relayStates[0]?"AÇ":"KAP", relayStates[1]?"AÇ":"KAP",
      relayStates[2]?"AÇ":"KAP", relayStates[3]?"AÇ":"KAP", temp, hum);
  } else {
    snprintf(statusMsg, sizeof(statusMsg),
      "R1:%s R2:%s R3:%s R4:%s\nSıcaklık:Hata\nNem:Hata",
      relayStates[0]?"AÇ":"KAP", relayStates[1]?"AÇ":"KAP",
      relayStates[2]?"AÇ":"KAP", relayStates[3]?"AÇ":"KAP");
  }
  sendSMS(number, statusMsg);
}

// ========== SMS KOMUT İŞLEYİCİ ==========
void handleSMS(const char* sender, char* smsText) {
  Serial.printf("SMS Gönderen: %s\nİçerik: %s\n", sender, smsText);
  
  if (!isAdminNumber(sender)) {
    sendSMS(sender, "Yetkisiz numara");
    return;
  }
  
  // Komutları büyük harfe çevir
  for (char* p = smsText; *p; p++) *p = toupper(*p);
  
  if (strstr(smsText, "R1 AÇ") || strstr(smsText, "AC")) {
    setRelay(0, true);
    sendSMS(sender, "Röle 1 açıldı");
  } else if (strstr(smsText, "R1 KAPAT") || strstr(smsText, "KAPAT")) {
    setRelay(0, false);
    sendSMS(sender, "Röle 1 kapandı");
  } else if (strstr(smsText, "R2 AÇ")) {
    setRelay(1, true);
    sendSMS(sender, "Röle 2 açıldı");
  } else if (strstr(smsText, "R2 KAPAT")) {
    setRelay(1, false);
    sendSMS(sender, "Röle 2 kapandı");
  } else if (strstr(smsText, "R3 AÇ")) {
    setRelay(2, true);
    sendSMS(sender, "Röle 3 açıldı");
  } else if (strstr(smsText, "R3 KAPAT")) {
    setRelay(2, false);
    sendSMS(sender, "Röle 3 kapandı");
  } else if (strstr(smsText, "R4 AÇ")) {
    setRelay(3, true);
    sendSMS(sender, "Röle 4 açıldı");
  } else if (strstr(smsText, "R4 KAPAT")) {
    setRelay(3, false);
    sendSMS(sender, "Röle 4 kapandı");
  } else if (strstr(smsText, "HEPSİ AÇ")) {
    setAllRelays(true);
    sendSMS(sender, "Tüm röleler açıldı");
  } else if (strstr(smsText, "HEPSİ KAPAT")) {
    setAllRelays(false);
    sendSMS(sender, "Tüm röleler kapandı");
  } else if (strstr(smsText, "DURUM")) {
    sendStatusSMS(sender);
  } else {
    sendSMS(sender, "Hatalı komut");
  }
}

// ========== DTMF İŞLEYİCİ ==========
void handleDTMF(char tone) {
  Serial.printf("DTMF >> %c\n", tone);
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
    case '#': hangupCall(); break;
  }
}

// ========== ARAMA FONKSİYONLARI ==========
void answerCall() {
  Serial.println(F("Yetkili arama, cevap veriliyor"));
  gsm.println("ATA");
  callState = CALL_ACTIVE;
  callStartMs = millis();
}

void hangupCall() {
  gsm.println("ATH");
  callState = CALL_IDLE;
  clipReceived = false;
  lastCaller[0] = '\0';
}

// ========== GSM SATIR İŞLEME ==========
void processGSMLine(char* line) {
  if (strncmp(line, "+CMT:", 5) == 0) {
    char* start = strchr(line, '"');
    if (start) {
      char* end = strchr(start + 1, '"');
      if (end) {
        *end = '\0';
        normalizePhone(start + 1, lastSmsSender, sizeof(lastSmsSender));
        waitingSmsBody = true;
      }
    }
    return;
  }
  
  if (waitingSmsBody) {
    waitingSmsBody = false;
    handleSMS(lastSmsSender, line);
    return;
  }
  
  if (strcmp(line, "RING") == 0 && callState == CALL_IDLE) {
    callState = CALL_RINGING;
    ringStartMs = millis();
    clipReceived = false;
    return;
  }
  
  if (strncmp(line, "+CLIP:", 6) == 0) {
    char* start = strchr(line, '"');
    if (start) {
      char* end = strchr(start + 1, '"');
      if (end) {
        *end = '\0';
        normalizePhone(start + 1, lastCaller, sizeof(lastCaller));
        clipReceived = true;
        if (callState == CALL_RINGING) {
          if (isAdminNumber(lastCaller)) answerCall();
          else hangupCall();
        }
      }
    }
    return;
  }
  
  if (strncmp(line, "+DTMF:", 6) == 0 && callState == CALL_ACTIVE) {
    char* colon = strchr(line, ':');
    if (colon) {
      for (char* p = colon + 1; *p; p++) {
        if (*p >= '0' && *p <= '9') { handleDTMF(*p); break; }
        else if (*p == '*' || *p == '#') { handleDTMF(*p); break; }
      }
    }
    return;
  }
  
  if (strcmp(line, "NO CARRIER") == 0 || strcmp(line, "BUSY") == 0) {
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

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n\n========================================"));
  Serial.println(F("=== ESP32 ENTEGRE SİSTEM BAŞLIYOR ==="));
  Serial.println(F("========================================"));
  
  // Röle pinlerini başlat
  relayInit();
  
  // GSM STARTUP pin
  pinMode(STARTUP_PIN, OUTPUT);
  digitalWrite(STARTUP_PIN, HIGH);
  delay(3000);
  digitalWrite(STARTUP_PIN, LOW);
  delay(5000);
  
  // GSM Hardware Serial başlat
  gsm.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);
  
  // DHT sensör
  dht.begin();
  
  // Adminleri yükle
  loadAdmins();
  
  // GSM modülünü yapılandır
  for (int i = 0; i < 3 && !isGSMAlive(); i++) {
    Serial.println(F("GSM modülü bekleniyor..."));
    delay(2000);
  }
  
  sendAT("AT");
  sendAT("ATE0");
  sendAT("AT+CMGF=1");
  sendAT("AT+CNMI=1,2,0,0,0");
  sendAT("AT+CLIP=1");
  sendAT("AT+DDET=1");
  
  // WiFi Manager
  setupWiFiManager();
  
  delay(2000);
  sendSMSAllAdmins("ESP32 Sistem acildi - GSM + Blynk modu aktif");
  if (blynkConnected) sendSMSAllAdmins("Blynk baglantisi aktif");
  
  lastGsmActivityMs = millis();
}

// ========== LOOP ==========
void loop() {
  handleBlynk();
  
  static unsigned long lastStatusUpdate = 0;
  if (millis() - lastStatusUpdate > 10000) {
    lastStatusUpdate = millis();
    updateBlynkStatus();
  }
  
  if (millis() - lastGsmActivityMs > GSM_WATCHDOG_MS) {
    if (!isGSMAlive()) {
      Serial.println(F("GSM modülü yanıt vermiyor, resetleniyor..."));
      resetGSMModule();
    } else {
      lastGsmActivityMs = millis();
    }
  }
  
  readGSM();
  
  // Call timeouts
  unsigned long now = millis();
  if (callState == CALL_RINGING && !clipReceived && (now - ringStartMs > CLIP_TIMEOUT_MS)) {
    hangupCall();
  }
  if (callState == CALL_ACTIVE && (now - callStartMs > MAX_CALL_MS)) {
    hangupCall();
  }
}