/*
  ============================================================
  ESP32 KAPI SENSÖRÜ + TELEGRAM (TEKRAR MESAJ YOK)
  ============================================================
  
  ✅ Kapı açılınca 1 kere: "🚪🔓 KAPI AÇILDI!"
  ✅ Kapı kapanınca 1 kere: "🚪🔒 KAPI KAPANDI!"
  ✅ /durum: "Kapı AÇIK" veya "Kapı KAPALI" (1 kere)
  ============================================================
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <SPIFFS.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

// ============================================================
// PIN TANIMLAMALARI
// ============================================================
#define MAGNETIC_PIN 15

// ============================================================
// WiFiManager AYARLARI
// ============================================================
#define AP_NAME "fatihontacautomation"
#define AP_PASSWORD "05416932009"

// ============================================================
// EEPROM AYARLARI
// ============================================================
#define EEPROM_SIZE 10
#define BOOT_FLAG_ADDR 0

// ============================================================
// SABİTLER
// ============================================================
const unsigned long TELEGRAM_CHECK_INTERVAL = 3000;

// ============================================================
// NESNELER
// ============================================================
WiFiClientSecure client;
WiFiManager wifiManager;

// ============================================================
// DEĞİŞKENLER
// ============================================================
bool previousDoorState = HIGH;
bool doorIsOpen = false;
unsigned long lastTelegramCheck = 0;
unsigned long lastHeartbeat = 0;
bool firstBoot = true;

// Telegram bilgileri
char botToken[200] = "";
char chatIdStr[50] = "";
bool shouldSaveConfig = false;

// ✅ Son işlenen update_id'yi sakla (tekrar mesaj göndermemek için)
long lastUpdateId = 0;

// ============================================================
// FONKSİYON PROTOTİPLERİ
// ============================================================
bool sendTelegramMessage(String message);
String getTelegramMessage();

// ============================================================
// İLK AÇILIŞ KONTROLÜ
// ============================================================
bool isFirstBoot() {
  EEPROM.begin(EEPROM_SIZE);
  uint8_t bootFlag = EEPROM.read(BOOT_FLAG_ADDR);
  
  if (bootFlag == 0) {
    EEPROM.write(BOOT_FLAG_ADDR, 1);
    EEPROM.commit();
    EEPROM.end();
    Serial.println("✅ İlk açılış!");
    return true;
  }
  EEPROM.end();
  return false;
}

// ============================================================
// CONFIG KAYIT/YÜKLEME (SPIFFS)
// ============================================================
void saveConfigToSPIFFS() {
  Serial.println(F("📝 Config kaydediliyor..."));
  
  if (!SPIFFS.begin(true)) {
    Serial.println(F("❌ SPIFFS başlatılamadı!"));
    return;
  }
  
  DynamicJsonDocument json(256);
  json["bot_token"] = botToken;
  json["chat_id"] = chatIdStr;
  
  File configFile = SPIFFS.open("/telegram_config.json", "w");
  if (!configFile) {
    Serial.println(F("❌ Config dosyası açılamadı!"));
    return;
  }
  
  if (serializeJson(json, configFile)) {
    Serial.println(F("✅ Config kaydedildi!"));
  } else {
    Serial.println(F("❌ Config yazılamadı!"));
  }
  configFile.close();
}

void loadConfigFromSPIFFS() {
  Serial.println(F("📂 Config yükleniyor..."));
  
  if (!SPIFFS.begin(true)) {
    Serial.println(F("❌ SPIFFS başlatılamadı!"));
    return;
  }
  
  if (!SPIFFS.exists("/telegram_config.json")) {
    Serial.println(F("❌ Config dosyası bulunamadı!"));
    return;
  }
  
  File configFile = SPIFFS.open("/telegram_config.json", "r");
  if (!configFile) {
    Serial.println(F("❌ Config dosyası açılamadı!"));
    return;
  }
  
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size + 1]);
  configFile.readBytes(buf.get(), size);
  buf[size] = '\0';
  
  DynamicJsonDocument json(256);
  DeserializationError error = deserializeJson(json, buf.get());
  configFile.close();
  
  if (error) {
    Serial.println(F("❌ JSON parse hatası!"));
    return;
  }
  
  if (json.containsKey("bot_token")) {
    strcpy(botToken, json["bot_token"]);
    Serial.print("✅ Bot Token: ");
    Serial.println(botToken);
  }
  if (json.containsKey("chat_id")) {
    strcpy(chatIdStr, json["chat_id"]);
    Serial.print("✅ Chat ID: ");
    Serial.println(chatIdStr);
  }
}

void saveConfigCallback() {
  Serial.println(F("📌 Config kaydedilmeli"));
  shouldSaveConfig = true;
}

// ============================================================
// WIFI MANAGER
// ============================================================
void setupWiFiManager() {
  Serial.println(F("\n📡 WiFi Manager Başlatılıyor..."));
  
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setConnectTimeout(20);
  
  WiFiManagerParameter custom_token("token", "Telegram Bot Token", botToken, 200);
  WiFiManagerParameter custom_chatid("chatid", "Telegram Chat ID", chatIdStr, 50);
  
  wifiManager.addParameter(&custom_token);
  wifiManager.addParameter(&custom_chatid);
  wifiManager.setTitle("ESP32 Kapı Sensörü");
  
  bool connected = wifiManager.autoConnect(AP_NAME, AP_PASSWORD);
  
  if (connected) {
    Serial.println("✅ WiFi bağlandı!");
    Serial.print("📡 IP: ");
    Serial.println(WiFi.localIP());
    
    String newToken = String(custom_token.getValue());
    String newChatId = String(custom_chatid.getValue());
    
    if (newToken.length() > 0 && newChatId.length() > 0) {
      strcpy(botToken, newToken.c_str());
      strcpy(chatIdStr, newChatId.c_str());
      if (shouldSaveConfig) {
        saveConfigToSPIFFS();
        shouldSaveConfig = false;
      }
    }
  } else {
    Serial.println("❌ WiFi bağlanamadı! AP modunda...");
    Serial.println("📱 AP: " + String(AP_NAME));
    Serial.println("🔑 Şifre: " + String(AP_PASSWORD));
  }
}

// ============================================================
// TELEGRAM FONKSİYONLARI
// ============================================================
bool sendTelegramMessage(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi bağlı değil!");
    return false;
  }
  
  if (strlen(botToken) == 0 || strlen(chatIdStr) == 0) {
    Serial.println("⚠️ Telegram ayarları eksik!");
    return false;
  }
  
  HTTPClient http;
  client.setInsecure();
  client.setTimeout(10000);
  
  // Türkçe karakterleri encode et
  String encodedMessage = message;
  encodedMessage.replace(" ", "%20");
  encodedMessage.replace("ğ", "%C4%9F");
  encodedMessage.replace("ü", "%C3%BC");
  encodedMessage.replace("ş", "%C5%9F");
  encodedMessage.replace("ı", "%C4%B1");
  encodedMessage.replace("ö", "%C3%B6");
  encodedMessage.replace("ç", "%C3%A7");
  encodedMessage.replace("Ğ", "%C4%9E");
  encodedMessage.replace("Ü", "%C3%9C");
  encodedMessage.replace("Ş", "%C5%9E");
  encodedMessage.replace("İ", "%C4%B0");
  encodedMessage.replace("Ö", "%C3%96");
  encodedMessage.replace("Ç", "%C3%87");
  
  String url = "https://api.telegram.org/bot" + String(botToken) + "/sendMessage?chat_id=" + String(chatIdStr) + "&text=" + encodedMessage;
  
  Serial.print("📤 Telegram: ");
  Serial.println(message);
  
  http.begin(client, url);
  int httpCode = http.GET();
  http.end();
  
  if (httpCode == 200) {
    Serial.println("✅ Mesaj gönderildi!");
    return true;
  } else {
    Serial.print("❌ Hata! Kod: ");
    Serial.println(httpCode);
    return false;
  }
}

// ============================================================
// ✅ TELEGRAM MESAJ OKU (SADECE YENİ MESAJLAR)
// ============================================================
String getTelegramMessage() {
  if (WiFi.status() != WL_CONNECTED) return "";
  if (strlen(botToken) == 0 || strlen(chatIdStr) == 0) return "";
  
  HTTPClient http;
  client.setInsecure();
  client.setTimeout(5000);
  
  // Son update_id'den sonrasını iste
  String url = "https://api.telegram.org/bot" + String(botToken) + "/getUpdates?offset=" + String(lastUpdateId + 1);
  
  http.begin(client, url);
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    String payload = http.getString();
    http.end();
    
    // update_id'yi bul
    int updateIndex = payload.indexOf("\"update_id\":");
    if (updateIndex > 0) {
      int idStart = updateIndex + 12;
      int idEnd = payload.indexOf(",", idStart);
      if (idEnd > idStart) {
        long newUpdateId = payload.substring(idStart, idEnd).toInt();
        if (newUpdateId > lastUpdateId) {
          lastUpdateId = newUpdateId;  // ✅ Yeni update_id'yi kaydet
          
          // Text'i bul
          int textIndex = payload.indexOf("\"text\":\"");
          if (textIndex > 0) {
            int start = textIndex + 8;
            int end = payload.indexOf("\"", start);
            if (end > start) {
              return payload.substring(start, end);
            }
          }
        }
      }
    }
  } else {
    http.end();
  }
  return "";
}

// ============================================================
// KAPI DURUMU KONTROLÜ
// ============================================================
void checkDoorStatus() {
  int currentState = digitalRead(MAGNETIC_PIN);
  
  if (currentState != previousDoorState) {
    previousDoorState = currentState;
    doorIsOpen = (currentState == HIGH);
    
    if (currentState == HIGH) {
      Serial.println("🚪 KAPI AÇILDI!");
      sendTelegramMessage("🚪🔓 KAPI AÇILDI!");
    } else {
      Serial.println("🚪 KAPI KAPANDI!");
      sendTelegramMessage("🚪🔒 KAPI KAPANDI!");
    }
  }
}

// ============================================================
// TELEGRAM KOMUTLARI (SADECE /durum)
// ============================================================
void handleTelegramCommands() {
  if (millis() - lastTelegramCheck < TELEGRAM_CHECK_INTERVAL) return;
  lastTelegramCheck = millis();
  
  String message = getTelegramMessage();
  if (message == "") return;
  
  message.toLowerCase();
  message.trim();
  
  Serial.print("📩 Komut: ");
  Serial.println(message);
  
  if (message == "/durum" || message == "durum") {
    if (doorIsOpen) {
      sendTelegramMessage("🚪 Kapı AÇIK");
    } else {
      sendTelegramMessage("🚪 Kapı KAPALI");
    }
  }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("ESP32 KAPI SENSÖR + TELEGRAM");
  Serial.println("========================================");
  
  Serial.print("🔍 Reset: ");
  switch(esp_reset_reason()) {
    case ESP_RST_POWERON: Serial.println("Güç açılışı"); break;
    case ESP_RST_BROWNOUT: Serial.println("⚠️ GÜÇ DÜŞÜŞÜ!"); break;
    case ESP_RST_WDT: Serial.println("⚠️ Watchdog!"); break;
    default: Serial.println(esp_reset_reason()); break;
  }
  
  pinMode(MAGNETIC_PIN, INPUT_PULLUP);
  
  firstBoot = isFirstBoot();
  loadConfigFromSPIFFS();
  setupWiFiManager();
  
  previousDoorState = digitalRead(MAGNETIC_PIN);
  doorIsOpen = (previousDoorState == HIGH);
  
  Serial.print("🔍 Kapı: ");
  Serial.println(doorIsOpen ? "AÇIK" : "KAPALI");
  
  delay(2000);
  
  if (WiFi.status() == WL_CONNECTED && strlen(botToken) > 0 && strlen(chatIdStr) > 0) {
    Serial.println("📤 Başlangıç mesajı gönderiliyor...");
    sendTelegramMessage("🤖 Kapı Sensör Sistemi Başlatıldı!");
    delay(500);
    
    if (doorIsOpen) {
      sendTelegramMessage("🚪 Kapı AÇIK durumda!");
    } else {
      sendTelegramMessage("🚪 Kapı KAPALI durumda!");
    }
    delay(500);
    sendTelegramMessage("📌 /durum yazarak bilgi alın.");
  } else {
    Serial.println("⚠️ Telegram ayarları eksik! Web arayüzünden girin.");
    Serial.println("🌐 http://192.168.4.1");
  }
  
  Serial.println("");
  Serial.println("========================================");
  Serial.println(WiFi.status() == WL_CONNECTED ? "✅ WIFI BAĞLI" : "⚠️ AP MODU");
  Serial.println("========================================");
  Serial.println("📌 Telegram Komutu:");
  Serial.println("  /durum - Kapı AÇIK / KAPALI");
  Serial.println("========================================");
  Serial.println("");
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastWarning = 0;
    if (millis() - lastWarning > 30000) {
      lastWarning = millis();
      Serial.println("⚠️ WiFi yok! AP modunda...");
      Serial.println("📱 AP: " + String(AP_NAME));
      Serial.println("🔑 Şifre: " + String(AP_PASSWORD));
    }
    delay(100);
    return;
  }
  
  if (millis() - lastHeartbeat > 10000) {
    lastHeartbeat = millis();
    Serial.println("🔄 Sistem çalışıyor...");
  }
  
  checkDoorStatus();
  handleTelegramCommands();
  
  delay(50);
}