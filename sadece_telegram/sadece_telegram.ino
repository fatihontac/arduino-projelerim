/*
  ============================================================
  ESP8266 TELEGRAM + RÖLE KONTROL (BLYNK YOK!)
  ============================================================
  
  SADECE Telegram ile röle kontrolü
  Manyetik switch kapı takibi
  ============================================================
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// ========== WiFi AYARLARI ==========
const char* ssid = "FiberHGW_ZT655T";
const char* password = "3Dyx4a7CXyDx";

// ========== TELEGRAM AYARLARI ==========
#define BOT_TOKEN "8974288272:AAFFt6oVqCcjIqjyhVRhRa02fOYVBfJHpRI"
#define CHAT_ID "1229956787"

// ========== PIN TANIMLAMALARI ==========
#define RELAY1_PIN 16
#define RELAY2_PIN 14
#define RELAY3_PIN 12
#define RELAY4_PIN 13
#define MAGNETIC_PIN 15

#define RELAY_ON HIGH
#define RELAY_OFF LOW
#define RELAY_TIMER 3000

// ========== RÖLE PIN DİZİSİ ==========
const int relayPins[4] = {RELAY1_PIN, RELAY2_PIN, RELAY3_PIN, RELAY4_PIN};

// ========== DEĞİŞKENLER ==========
bool relayStates[4] = {false, false, false, false};
bool relayTimers[3] = {false, false, false};
unsigned long relayTimerStart[3] = {0, 0, 0};
bool previousDoorState = HIGH;
unsigned long lastTelegramCheck = 0;
const unsigned long TELEGRAM_CHECK_INTERVAL = 3000;
unsigned long lastMillis = 0;

WiFiClientSecure client;

// ========== TELEGRAM FONKSİYONLARI ==========

bool sendTelegramMessage(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi bağlı değil!");
    return false;
  }
  
  HTTPClient http;
  client.setInsecure();
  client.setTimeout(5000);
  
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
  
  String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/sendMessage?chat_id=" + String(CHAT_ID) + "&text=" + encodedMessage;
  
  Serial.print("📤 Gönderiliyor: ");
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

String getTelegramMessage() {
  if (WiFi.status() != WL_CONNECTED) return "";
  
  HTTPClient http;
  client.setInsecure();
  client.setTimeout(5000);
  
  String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/getUpdates?offset=-1";
  
  http.begin(client, url);
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    String payload = http.getString();
    http.end();
    
    // JSON parse etmeden basit arama
    int textIndex = payload.indexOf("\"text\":\"");
    if (textIndex > 0) {
      int start = textIndex + 8;
      int end = payload.indexOf("\"", start);
      if (end > start) {
        String text = payload.substring(start, end);
        
        // Update ID'yi bul ve temizle
        int updateIndex = payload.indexOf("\"update_id\":");
        if (updateIndex > 0) {
          int idStart = updateIndex + 12;
          int idEnd = payload.indexOf(",", idStart);
          if (idEnd > idStart) {
            String updateId = payload.substring(idStart, idEnd);
            String clearUrl = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/getUpdates?offset=" + updateId;
            HTTPClient clearHttp;
            clearHttp.begin(client, clearUrl);
            clearHttp.GET();
            clearHttp.end();
          }
        }
        
        return text;
      }
    }
  } else {
    http.end();
  }
  return "";
}

// ========== RÖLE FONKSİYONLARI ==========

void setRelay(int index, bool state) {
  if (index < 0 || index > 3) return;
  digitalWrite(relayPins[index], state ? RELAY_ON : RELAY_OFF);
  relayStates[index] = state;
  Serial.print("Röle ");
  Serial.print(index + 1);
  Serial.println(state ? " AÇIK" : " KAPALI");
}

void checkRelayTimers() {
  for (int i = 0; i < 3; i++) {
    if (relayTimers[i] && (millis() - relayTimerStart[i] >= RELAY_TIMER)) {
      setRelay(i + 1, false);
      relayTimers[i] = false;
      Serial.print("Röle ");
      Serial.print(i + 2);
      Serial.println(" kapandı (3sn)");
    }
  }
}

void checkDoorStatus() {
  int currentState = digitalRead(MAGNETIC_PIN);
  if (currentState != previousDoorState) {
    previousDoorState = currentState;
    if (currentState == HIGH) {
      Serial.println("🚪 KAPI AÇILDI!");
      sendTelegramMessage("🚪🔓 KAPI AÇILDI!");
    } else {
      Serial.println("🚪 KAPI KAPANDI!");
      sendTelegramMessage("🚪🔒 KAPI KAPANDI!");
    }
  }
}

void handleTelegramCommands() {
  if (millis() - lastTelegramCheck < TELEGRAM_CHECK_INTERVAL) return;
  lastTelegramCheck = millis();
  
  String message = getTelegramMessage();
  if (message == "") return;
  
  message.toLowerCase();
  Serial.print("📩 Komut: ");
  Serial.println(message);
  
  if (message == "/durum" || message == "durum") {
    String statusMsg = "📊 PANJUR DURUMU\n";
    statusMsg += "------------------------\n";
    
    for (int i = 0; i < 4; i++) {
      statusMsg += relayStates[i] ? "✅" : "❌";
      statusMsg += " Röle " + String(i + 1) + ": ";
      statusMsg += relayStates[i] ? "AÇIK\n" : "KAPALI\n";
    }
    
    int doorState = digitalRead(MAGNETIC_PIN);
    statusMsg += doorState == HIGH ? "🚪 Kapı: AÇIK" : "🚪 Kapı: KAPALI";
    
    sendTelegramMessage(statusMsg);
  }
  else if (message == "/ac" || message == "ac") {
    for (int i = 0; i < 4; i++) {
      setRelay(i, true);
    }
    sendTelegramMessage("🔓 Tüm röleler AÇILDI!");
  }
  else if (message == "/kapat" || message == "kapat") {
    for (int i = 0; i < 4; i++) {
      setRelay(i, false);
    }
    sendTelegramMessage("🔒 Tüm röleler KAPANDI!");
  }
  else if (message == "/start") {
    sendTelegramMessage("🤖 Panjur Kontrol Sistemi Aktif!\n\nKomutlar:\n/durum - Durum bilgisi\n/ac - Tüm röleleri aç\n/kapat - Tüm röleleri kapat");
  }
  else if (message == "/1ac" || message == "1ac") {
    setRelay(0, true);
    sendTelegramMessage("✅ Röle 1 AÇILDI!");
  }
  else if (message == "/1kapat" || message == "1kapat") {
    setRelay(0, false);
    sendTelegramMessage("❌ Röle 1 KAPANDI!");
  }
  else if (message == "/2ac" || message == "2ac") {
    setRelay(1, true);
    sendTelegramMessage("✅ Röle 2 AÇILDI!");
  }
  else if (message == "/2kapat" || message == "2kapat") {
    setRelay(1, false);
    sendTelegramMessage("❌ Röle 2 KAPANDI!");
  }
  else if (message == "/3ac" || message == "3ac") {
    setRelay(2, true);
    sendTelegramMessage("✅ Röle 3 AÇILDI!");
  }
  else if (message == "/3kapat" || message == "3kapat") {
    setRelay(2, false);
    sendTelegramMessage("❌ Röle 3 KAPANDI!");
  }
  else if (message == "/4ac" || message == "4ac") {
    setRelay(3, true);
    sendTelegramMessage("✅ Röle 4 AÇILDI!");
  }
  else if (message == "/4kapat" || message == "4kapat") {
    setRelay(3, false);
    sendTelegramMessage("❌ Röle 4 KAPANDI!");
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("");
  Serial.println("========================================");
  Serial.println("ESP8266 TELEGRAM + RÖLE KONTROL");
  Serial.println("========================================");
  
  // Pinler
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  pinMode(MAGNETIC_PIN, INPUT_PULLUP);
  
  digitalWrite(RELAY1_PIN, RELAY_OFF);
  digitalWrite(RELAY2_PIN, RELAY_OFF);
  digitalWrite(RELAY3_PIN, RELAY_OFF);
  digitalWrite(RELAY4_PIN, RELAY_OFF);
  
  // WiFi
  Serial.print("WiFi'ye bağlanılıyor");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int deneme = 0;
  while (WiFi.status() != WL_CONNECTED && deneme < 30) {
    delay(500);
    Serial.print(".");
    deneme++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("✅ WiFi bağlandı! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("❌ WiFi bağlanamadı!");
    while(1) { delay(1000); }
  }
  
  previousDoorState = digitalRead(MAGNETIC_PIN);
  
  // Telegram başlangıç
  delay(2000);
  Serial.println("📤 Telegram başlangıç mesajları gönderiliyor...");
  
  sendTelegramMessage("🤖 Panjur Kontrol Sistemi Başlatıldı!");
  delay(1000);
  sendTelegramMessage("📡 Telegram aktif!");
  delay(1000);
  
  if (previousDoorState == HIGH) {
    sendTelegramMessage("🚪 Kapı AÇIK durumda!");
  } else {
    sendTelegramMessage("🚪 Kapı KAPALI durumda!");
  }
  
  Serial.println("");
  Serial.println("✅ Sistem hazir!");
  Serial.println("");
  Serial.println("📌 Telegram Komutları:");
  Serial.println("  /durum   - Durum bilgisi");
  Serial.println("  /ac      - Tüm röleleri aç");
  Serial.println("  /kapat   - Tüm röleleri kapat");
  Serial.println("  /1ac     - Röle 1 aç");
  Serial.println("  /1kapat  - Röle 1 kapat");
  Serial.println("  /2ac     - Röle 2 aç");
  Serial.println("  /2kapat  - Röle 2 kapat");
  Serial.println("  /3ac     - Röle 3 aç");
  Serial.println("  /3kapat  - Röle 3 kapat");
  Serial.println("  /4ac     - Röle 4 aç");
  Serial.println("  /4kapat  - Röle 4 kapat");
  Serial.println("========================================");
  Serial.println("");
}

// ========== LOOP ==========
void loop() {
  checkRelayTimers();
  checkDoorStatus();
  handleTelegramCommands();
  
  // Her 10 saniyede bir canlı olduğunu göster
  if (millis() - lastMillis > 10000) {
    lastMillis = millis();
    Serial.println("🔄 Sistem çalışıyor...");
  }
  
  delay(50);
}