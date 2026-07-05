/*
  ============================================================
  ESP32 GSM RÖLE KONTROL SİSTEMİ (KARAKTER SORUNU DÜZELTİLDİ)
  ============================================================
*/

#include <DHT.h>
#include <EEPROM.h>
#include <HardwareSerial.h>

// ========== GSM PİNLERİ ==========
#define GSM_RX        15
#define GSM_TX        16

// ========== RÖLE PİNLERİ ==========
#define RELE1_PIN     25
#define RELE2_PIN     26
#define RELE3_PIN     32
#define RELE4_PIN     33

// ========== DHT ==========
#define DHT_PIN       23
#define DHT_TYPE      DHT11

// ========== ADMIN ==========
#define ADMIN1        "+905416932009"
#define ADMIN_PASSWORD "123456"

// ========== RÖLE LOJİĞİ ==========
#define ROLE_AC       HIGH
#define ROLE_KAPAT    LOW

// ========== EEPROM ==========
#define EEPROM_SIZE        512
#define EEPROM_MAGIC       77
#define EEPROM_RELAY_ADDR  100
#define MAX_ADMINS         6
#define PHONE_LEN          20

// ========== NESNELER ==========
HardwareSerial gsm(2);
DHT dht(DHT_PIN, DHT_TYPE);

// ========== GLOBAL DEĞİŞKENLER ==========
const uint8_t relayPins[4] = { RELE1_PIN, RELE2_PIN, RELE3_PIN, RELE4_PIN };
bool relayStates[4] = { false, false, false, false };
char adminNumbers[MAX_ADMINS][PHONE_LEN];
unsigned long lastGsmCheck = 0;
unsigned long lastNetworkCheck = 0;
bool callActive = false;
bool forceResetDone = false;

// ============================================================
// NUMARA NORMALİZASYON
// ============================================================

void normalizePhone(const char *src, char *dst, size_t dstSize) {
  char digits[PHONE_LEN];
  uint8_t d = 0;
  
  memset(dst, 0, dstSize);
  memset(digits, 0, sizeof(digits));
  
  for (uint8_t i = 0; src[i] != '\0' && d < sizeof(digits) - 1; i++) {
    if (src[i] >= '0' && src[i] <= '9') {
      digits[d++] = src[i];
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
  else if (d == 13 && digits[0] == '9' && digits[1] == '0') {
    snprintf(dst, dstSize, "+%s", digits);
  }
  else {
    strncpy(dst, src, dstSize - 1);
  }
  
  dst[dstSize - 1] = '\0';
}

bool isAdminNumber(const char *number) {
  char norm[PHONE_LEN];
  normalizePhone(number, norm, sizeof(norm));
  
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i][0] != '\0' && strcmp(adminNumbers[i], norm) == 0) {
      return true;
    }
  }
  return false;
}

// ============================================================
// EEPROM: ADMIN LİSTESİ
// ============================================================

void resetAdminList() {
  Serial.println("Admin listesi sifirlaniyor...");
  memset(adminNumbers, 0, sizeof(adminNumbers));
  
  normalizePhone(ADMIN1, adminNumbers[0], PHONE_LEN);
  Serial.printf("Admin 1: %s\n", adminNumbers[0]);
  
  EEPROM.write(0, EEPROM_MAGIC);
  int addr = 1;
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    for (uint8_t j = 0; j < PHONE_LEN; j++) {
      EEPROM.write(addr++, adminNumbers[i][j]);
    }
  }
  EEPROM.commit();
}

void loadAdmins() {
  EEPROM.begin(EEPROM_SIZE);
  
  if (!forceResetDone) {
    resetAdminList();
    forceResetDone = true;
    return;
  }
  
  if (EEPROM.read(0) != EEPROM_MAGIC) {
    resetAdminList();
    return;
  }
  
  int addr = 1;
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    for (uint8_t j = 0; j < PHONE_LEN; j++) {
      adminNumbers[i][j] = EEPROM.read(addr++);
    }
    adminNumbers[i][PHONE_LEN - 1] = '\0';
  }
  Serial.println("Admin listesi yuklendi.");
}

void printAdmins() {
  Serial.println("=== ADMIN LISTESI ===");
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i][0] != '\0') {
      Serial.printf("%d: [%s]\n", i + 1, adminNumbers[i]);
    }
  }
}

bool addAdmin(const char *number) {
  char normalized[PHONE_LEN];
  normalizePhone(number, normalized, sizeof(normalized));
  
  if (isAdminNumber(normalized)) return false;
  
  for (uint8_t i = 0; i < MAX_ADMINS; i++) {
    if (adminNumbers[i][0] == '\0') {
      strncpy(adminNumbers[i], normalized, PHONE_LEN - 1);
      
      EEPROM.write(0, EEPROM_MAGIC);
      int addr = 1;
      for (uint8_t j = 0; j < MAX_ADMINS; j++) {
        for (uint8_t k = 0; k < PHONE_LEN; k++) {
          EEPROM.write(addr++, adminNumbers[j][k]);
        }
      }
      EEPROM.commit();
      return true;
    }
  }
  return false;
}

// ============================================================
// RÖLE FONKSİYONLARI
// ============================================================

void relayInit() {
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], ROLE_KAPAT);
  }
  
  byte stateByte = EEPROM.read(EEPROM_RELAY_ADDR);
  for (uint8_t i = 0; i < 4; i++) {
    relayStates[i] = bitRead(stateByte, i);
    digitalWrite(relayPins[i], relayStates[i] ? ROLE_AC : ROLE_KAPAT);
  }
}

void setRelay(uint8_t index, bool on) {
  if (index >= 4) return;
  relayStates[index] = on;
  digitalWrite(relayPins[index], on ? ROLE_AC : ROLE_KAPAT);
  
  byte stateByte = 0;
  for (uint8_t i = 0; i < 4; i++) {
    if (relayStates[i]) stateByte |= (1 << i);
  }
  EEPROM.write(EEPROM_RELAY_ADDR, stateByte);
  EEPROM.commit();
  
  Serial.printf("Role %d -> %s\n", index + 1, on ? "ACIK" : "KAPALI");
}

void setAllRelays(bool on) {
  for (uint8_t i = 0; i < 4; i++) setRelay(i, on);
}

// ============================================================
// DHT FONKSİYONLARI
// ============================================================

void readDHT(float &temp, float &hum) {
  temp = dht.readTemperature();
  hum = dht.readHumidity();
  if (isnan(temp)) temp = 0;
  if (isnan(hum)) hum = 0;
}

// ============================================================
// GSM FONKSİYONLARI
// ============================================================

void sendAT(const char *cmd, unsigned long waitMs = 1000) {
  Serial.printf("AT >> %s\n", cmd);
  gsm.println(cmd);
  delay(waitMs);
}

bool gsmWaitResponse(const char *target, unsigned long timeout) {
  unsigned long start = millis();
  String resp = "";
  while (millis() - start < timeout) {
    while (gsm.available()) {
      resp += (char)gsm.read();
      if (resp.indexOf(target) != -1) return true;
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
      resp += (char)gsm.read();
      if (resp.indexOf("+CREG: 0,1") != -1 || resp.indexOf("+CREG: 0,5") != -1) {
        return true;
      }
    }
  }
  return false;
}

void gsmInitialize() {
  Serial.println(F("GSM baslatiliyor..."));
  delay(3000);
  
  for (uint8_t i = 0; i < 5; i++) {
    if (gsmTest()) {
      Serial.println(F("GSM cevap verdi, ayarlar yapiliyor..."));
      sendAT("ATE0");
      sendAT("AT+CMGF=1");
      sendAT("AT+CNMI=1,2,0,0,0");
      sendAT("AT+CLIP=1");
      sendAT("AT+DDET=1");
      delay(2000);
      if (gsmNetworkOK()) {
        Serial.println(F("GSM HAZIR"));
        return;
      }
    }
    delay(2000);
  }
  Serial.println(F("GSM BASLATILAMADI"));
}

void sendSMS(const char *number, const char *message) {
  gsm.println(F("AT+CMGF=1"));
  delay(500);
  gsm.print(F("AT+CMGS=\""));
  gsm.print(number);
  gsm.println(F("\""));
  delay(500);
  
  // Mesajı gönder - Türkçe karakter sorununu önlemek için
  gsm.print(message);
  delay(500);
  gsm.write(26);
  delay(5000);
  Serial.printf("SMS gonderildi: %s\n", number);
}

// ============================================================
// SMS KOMUT İŞLEYİCİ (KARAKTER SORUNU DÜZELTİLDİ)
// ============================================================

// SMS içindeki bozuk karakterleri temizle
String cleanSmsText(String raw) {
  String cleaned = "";
  for (uint8_t i = 0; i < raw.length(); i++) {
    char c = raw[i];
    // Sadece geçerli karakterleri al (A-Z, 0-9, bosluk, nokta)
    if ((c >= 'A' && c <= 'Z') || 
        (c >= 'a' && c <= 'z') || 
        (c >= '0' && c <= '9') || 
        c == ' ' || c == '.' || c == '_' || c == '-' || c == '+' ||
        c == 'R' || c == 'A' || c == 'C' || c == 'K' || c == 'P' || c == 'T' ||
        c == 'H' || c == 'E' || c == 'S' || c == 'I' || c == 'D' || c == 'U' ||
        c == 'M' || c == 'N' || c == 'L') {
      cleaned += c;
    }
    else if (c >= 'A' && c <= 'Z') {
      cleaned += c;
    }
  }
  cleaned.toUpperCase();
  return cleaned;
}

void processSMS(String sender, String sms) {
  Serial.println("\n========== YENI SMS ==========");
  Serial.printf("Gonderen: [%s]\n", sender.c_str());
  Serial.printf("Orijinal SMS: [%s]\n", sms.c_str());
  
  // SMS'i temizle
  String cleanSms = cleanSmsText(sms);
  Serial.printf("Temizlenmis SMS: [%s]\n", cleanSms.c_str());
  
  // Admin kontrolü
  bool isAdmin = isAdminNumber(sender.c_str());
  
  if (!isAdmin) {
    sendSMS(sender.c_str(), "Yetkisiz numara! Admin degilsiniz.");
    return;
  }
  
  cleanSms.trim();
  
  // ADMIN EKLE
  if (cleanSms.startsWith("ADMIN EKLE")) {
    int firstSpace = cleanSms.indexOf(' ', 11);
    if (firstSpace > 0) {
      String password = cleanSms.substring(11, firstSpace);
      String newNumber = cleanSms.substring(firstSpace + 1);
      newNumber.trim();
      
      if (password == ADMIN_PASSWORD) {
        if (addAdmin(newNumber.c_str())) {
          sendSMS(sender.c_str(), "Admin eklendi");
          printAdmins();
        } else {
          sendSMS(sender.c_str(), "Admin eklenemedi!");
        }
      } else {
        sendSMS(sender.c_str(), "Sifre hatali!");
      }
    }
    return;
  }
  
  // ADMIN LISTESI
  if (cleanSms == "ADMINLER") {
    String list = "Adminler:\n";
    for (uint8_t i = 0; i < MAX_ADMINS; i++) {
      if (adminNumbers[i][0] != '\0') {
        list += String(i + 1) + ": " + String(adminNumbers[i]) + "\n";
      }
    }
    sendSMS(sender.c_str(), list.c_str());
    return;
  }
  
  // RÖLE KOMUTLARI
  if (cleanSms == "R1 AC" || cleanSms == "AC" || cleanSms == "R1AC") {
    setRelay(0, true);
    sendSMS(sender.c_str(), "R1 ACIK");
  }
  else if (cleanSms == "R1 KAPAT" || cleanSms == "KAPAT" || cleanSms == "R1KAPAT") {
    setRelay(0, false);
    sendSMS(sender.c_str(), "R1 KAPALI");
  }
  else if (cleanSms == "R2 AC" || cleanSms == "R2AC") {
    setRelay(1, true);
    sendSMS(sender.c_str(), "R2 ACIK");
  }
  else if (cleanSms == "R2 KAPAT" || cleanSms == "R2KAPAT") {
    setRelay(1, false);
    sendSMS(sender.c_str(), "R2 KAPALI");
  }
  else if (cleanSms == "R3 AC" || cleanSms == "R3AC") {
    setRelay(2, true);
    sendSMS(sender.c_str(), "R3 ACIK");
  }
  else if (cleanSms == "R3 KAPAT" || cleanSms == "R3KAPAT") {
    setRelay(2, false);
    sendSMS(sender.c_str(), "R3 KAPALI");
  }
  else if (cleanSms == "R4 AC" || cleanSms == "R4AC") {
    setRelay(3, true);
    sendSMS(sender.c_str(), "R4 ACIK");
  }
  else if (cleanSms == "R4 KAPAT" || cleanSms == "R4KAPAT") {
    setRelay(3, false);
    sendSMS(sender.c_str(), "R4 KAPALI");
  }
  else if (cleanSms == "HEPSI AC" || cleanSms == "HEPSIAC") {
    setAllRelays(true);
    sendSMS(sender.c_str(), "TUM ROLER ACIK");
  }
  else if (cleanSms == "HEPSI KAPAT" || cleanSms == "HEPSIKAPAT") {
    setAllRelays(false);
    sendSMS(sender.c_str(), "TUM ROLER KAPALI");
  }
  else if (cleanSms == "DURUM") {
    float temp, hum;
    readDHT(temp, hum);
    char msg[200];
    snprintf(msg, sizeof(msg),
      "R1:%s R2:%s R3:%s R4:%s\nSicaklik:%.1f C\nNem:%.1f%%",
      relayStates[0] ? "Acik" : "Kapali",
      relayStates[1] ? "Acik" : "Kapali",
      relayStates[2] ? "Acik" : "Kapali",
      relayStates[3] ? "Acik" : "Kapali",
      temp, hum);
    sendSMS(sender.c_str(), msg);
  }
  else {
    sendSMS(sender.c_str(), "Hatali komut!\nKomutlar: R1 AC, R1 KAPAT, DURUM");
  }
}

// ============================================================
// DTMF İŞLEYİCİ
// ============================================================

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
    case '#': gsm.println("ATH"); break;
  }
}

// ============================================================
// GSM VERİ OKUMA
// ============================================================

void readGSM() {
  if (!gsm.available()) return;
  
  String line = gsm.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;
  
  Serial.printf("GSM << %s\n", line.c_str());
  
  if (line.startsWith("+CMT:")) {
    int first = line.indexOf('"');
    int second = line.indexOf('"', first + 1);
    if (first < 0 || second < 0) return;
    String sender = line.substring(first + 1, second);
    String sms = gsm.readStringUntil('\n');
    sms.trim();
    processSMS(sender, sms);
    return;
  }
  
  if (line.startsWith("+DTMF:")) {
    char tone = line.charAt(line.length() - 1);
    handleDTMF(tone);
    return;
  }
  
  if (line.startsWith("+CLIP:")) {
    int first = line.indexOf('"');
    int second = line.indexOf('"', first + 1);
    if (first >= 0 && second > first) {
      String caller = line.substring(first + 1, second);
      char callerNorm[PHONE_LEN];
      normalizePhone(caller.c_str(), callerNorm, sizeof(callerNorm));
      if (isAdminNumber(callerNorm)) {
        gsm.println("ATA");
      } else {
        gsm.println("ATH");
      }
    }
    return;
  }
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println(F("\n========================================"));
  Serial.println(F("=== TELEFON ILE KONTROL SISTEMI ==="));
  Serial.println(F("========================================\n"));
  
  relayInit();
  gsm.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);
  delay(1000);
  dht.begin();
  
  loadAdmins();
  
  Serial.println("\n=== ADMIN LISTESI ===");
  printAdmins();
  
  gsmInitialize();
  
  // YENI AÇILIŞ MESAJI - Istenen formatta
  if (adminNumbers[0][0] != '\0') {
    sendSMS(adminNumbers[0], "TELEFON ILE KONTROL SISTEMI ACILDI, +905416932009");
  }
  
  lastGsmCheck = millis();
  lastNetworkCheck = millis();
  
  Serial.println(F("\nSistem hazir."));
  Serial.println(F("Komutlar: R1 AC, R1 KAPAT, DURUM"));
}

// ============================================================
// LOOP
// ============================================================

void loop() {
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
  
  delay(10);
}