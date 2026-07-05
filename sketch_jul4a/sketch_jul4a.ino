/*
  ============================================================
  ESP8266 BLYNK + TELEGRAM + MANYETİK SWITCH
  ============================================================
  
  ÖZELLİKLER:
  1. Blynk App ile 4 röle kontrolü
  2. Manyetik switch (GPIO15) kapı takibi
  3. Kapı açıldığında Telegram bildirimi
  4. Telegram'dan /durum komutu ile bilgi alma
  
  ESP8266 GPIO Pinleri:
  GPIO16 (D0) → Röle 1
  GPIO14 (D5) → Röle 2
  GPIO12 (D6) → Röle 3
  GPIO13 (D7) → Röle 4
  GPIO15 (D8) → Manyetik Switch (Kapı sensörü)
  ============================================================
*/

// ========== BLYNK AYARLARI ==========
#define BLYNK_TEMPLATE_ID "TMPL69e3IoQ_3"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "XyLuEgFrfHV5PVAcLQPkkk8tX9maSpuf"

// ========== TELEGRAM AYARLARI ==========
#define BOT_TOKEN "8974288272:AAFFt6oVqCcjIqjyhVRhRa02fOYVBfJHpRI"
#define CHAT_ID "1229956787"

// ========== KÜTÜPHANELER ==========
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

// ========== WiFi AYARLARI ==========
const char* ssid = "FiberHGW_ZT655T";
const char* password = "3Dyx4a7CXyDx";

// ========== PIN TANIMLAMALARI ==========
#define RELAY1_PIN 16    // GPIO16 (D0) - Röle 1
#define RELAY2_PIN 14    // GPIO14 (D5) - Röle 2
#define RELAY3_PIN 12    // GPIO12 (D6) - Röle 3
#define RELAY4_PIN 13    // GPIO13 (D7) - Röle 4
#define MAGNETIC_PIN 15  // GPIO15 (D8) - Manyetik switch

#define RELAY_ON HIGH
#define RELAY_OFF LOW
#define RELAY_TIMER 3000  // 3 saniye

// ========== RÖLE PIN DİZİSİ (handleTelegramCommands için) ==========
const int relayPins[4] = {RELAY1_PIN, RELAY2_PIN, RELAY3_PIN, RELAY4_PIN};

// ========== DEĞİŞKENLER ==========
bool relayStates[4] = {false, false, false, false};
bool relayTimers[3] = {false, false, false};
unsigned long relayTimerStart[3] = {0, 0, 0};
bool previousDoorState = HIGH;
unsigned long lastTelegramCheck = 0;
const unsigned long TELEGRAM_CHECK_INTERVAL = 2000;

WiFiClient client;

// ========== FONKSİYON PROTOTİPLERİ ==========
void updateBlynkRelayStates();
void checkRelayTimers();
void checkDoorStatus();
String getTelegramMessage();
void sendTelegramMessage(String message);
void handleTelegramCommands();

// ========== TELEGRAM FONKSİYONLARI ==========

// Telegram'dan gelen mesajları oku
String getTelegramMessage() {
  String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/getUpdates?offset=-1";
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    String payload = http.getString();
    http.end();
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    if (doc["ok"] == true && doc["result"][0]["message"]["text"].as<String>() != "") {
      String text = doc["result"][0]["message"]["text"].as<String>();
      int updateId = doc["result"][0]["update_id"].as<int>();
      
      // Mesajı işlendik olarak işaretle
      String clearUrl = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/getUpdates?offset=" + String(updateId + 1);
      HTTPClient clearHttp;
      clearHttp.begin(client, clearUrl);
      clearHttp.GET();
      clearHttp.end();
      
      return text;
    }
    http.end();
  }
  return "";
}

// Telegram'a mesaj gönder
void sendTelegramMessage(String message) {
  // Türkçe karakterleri URL encode et
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
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = http.POST("");
  http.end();
}

// ========== TELEGRAM KOMUT İŞLEYİCİ ==========
void handleTelegramCommands() {
  if (millis() - lastTelegramCheck < TELEGRAM_CHECK_INTERVAL) {
    return;
  }
  lastTelegramCheck = millis();
  
  String message = getTelegramMessage();
  if (message == "") return;
  
  message.toLowerCase();
  Serial.print("Telegram komutu: ");
  Serial.println(message);
  
  if (message == "/durum" || message == "durum") {
    String statusMsg = "📊 **PANJUR DURUMU**\n";
    statusMsg += "------------------------\n";
    
    for (int i = 0; i < 4; i++) {
      statusMsg += relayStates[i] ? "✅" : "❌";
      statusMsg += " Röle ";
      statusMsg += String(i + 1);
      statusMsg += relayStates[i] ? ": AÇIK\n" : ": KAPALI\n";
    }
    
    int doorState = digitalRead(MAGNETIC_PIN);
    statusMsg += doorState == HIGH ? "🚪 Kapı: AÇIK" : "🚪 Kapı: KAPALI";
    
    sendTelegramMessage(statusMsg);
    
  } else if (message == "/ac" || message == "ac") {
    for (int i = 0; i < 4; i++) {
      digitalWrite(relayPins[i], RELAY_ON);
      relayStates[i] = true;
    }
    updateBlynkRelayStates();
    sendTelegramMessage("🔓 Tüm röleler AÇILDI!");
    
  } else if (message == "/kapat" || message == "kapat") {
    for (int i = 0; i < 4; i++) {
      digitalWrite(relayPins[i], RELAY_OFF);
      relayStates[i] = false;
    }
    updateBlynkRelayStates();
    sendTelegramMessage("🔒 Tüm röleler KAPANDI!");
    
  } else if (message == "/start") {
    sendTelegramMessage("🤖 Merhaba! Panjur Kontrol Sistemi aktif.\n\nKomutlar:\n/durum - Durum bilgisi\n/ac - Tüm röleleri aç\n/kapat - Tüm röleleri kapat");
  }
}

// ========== BLYNK WIDGET CALLBACKS ==========

// V4 - Röle 1 (Kalıcı Açık/Kapalı)
BLYNK_WRITE(V4) {
  int value = param.asInt();
  if (value == 1) {
    digitalWrite(RELAY1_PIN, RELAY_ON);
    relayStates[0] = true;
    Serial.println("Röle 1 AÇIK (kalıcı)");
    sendTelegramMessage("✅ Röle 1 AÇILDI (Kalıcı)");
  } else {
    digitalWrite(RELAY1_PIN, RELAY_OFF);
    relayStates[0] = false;
    Serial.println("Röle 1 KAPALI (kalıcı)");
    sendTelegramMessage("❌ Röle 1 KAPANDI (Kalıcı)");
  }
  updateBlynkRelayStates();
}

// V5 - Röle 2 (3 saniye tetikleme)
BLYNK_WRITE(V5) {
  if (param.asInt() == 1) {
    digitalWrite(RELAY2_PIN, RELAY_ON);
    relayStates[1] = true;
    relayTimers[0] = true;
    relayTimerStart[0] = millis();
    Serial.println("Röle 2 tetiklendi (3 saniye)");
    sendTelegramMessage("⚡ Röle 2 TETİKLENDI (3sn)");
    updateBlynkRelayStates();
    Blynk.virtualWrite(V5, 0);  // Butonu sıfırla
  }
}

// V6 - Röle 3 (3 saniye tetikleme)
BLYNK_WRITE(V6) {
  if (param.asInt() == 1) {
    digitalWrite(RELAY3_PIN, RELAY_ON);
    relayStates[2] = true;
    relayTimers[1] = true;
    relayTimerStart[1] = millis();
    Serial.println("Röle 3 tetiklendi (3 saniye)");
    sendTelegramMessage("⚡ Röle 3 TETİKLENDI (3sn)");
    updateBlynkRelayStates();
    Blynk.virtualWrite(V6, 0);  // Butonu sıfırla
  }
}

// V7 - Röle 4 (3 saniye tetikleme)
BLYNK_WRITE(V7) {
  if (param.asInt() == 1) {
    digitalWrite(RELAY4_PIN, RELAY_ON);
    relayStates[3] = true;
    relayTimers[2] = true;
    relayTimerStart[2] = millis();
    Serial.println("Röle 4 tetiklendi (3 saniye)");
    sendTelegramMessage("⚡ Röle 4 TETİKLENDI (3sn)");
    updateBlynkRelayStates();
    Blynk.virtualWrite(V7, 0);  // Butonu sıfırla
  }
}

// ========== BLYNK GÜNCELLEME FONKSİYONLARI ==========
void updateBlynkRelayStates() {
  Blynk.virtualWrite(V0, relayStates[0]);
  Blynk.virtualWrite(V1, relayStates[1]);
  Blynk.virtualWrite(V2, relayStates[2]);
  Blynk.virtualWrite(V3, relayStates[3]);
  
  char relayText[50];
  snprintf(relayText, sizeof(relayText), "R1:%s R2:%s R3:%s R4:%s",
    relayStates[0] ? "AÇ" : "KAP",
    relayStates[1] ? "AÇ" : "KAP",
    relayStates[2] ? "AÇ" : "KAP",
    relayStates[3] ? "AÇ" : "KAP");
  Blynk.virtualWrite(V8, relayText);
}

void updateDoorStatus() {
  int doorState = digitalRead(MAGNETIC_PIN);
  Blynk.virtualWrite(V9, doorState == HIGH ? 1 : 0);
  Blynk.virtualWrite(V10, doorState == HIGH ? "AÇIK" : "KAPALI");
}

// ========== RÖLE TIMER KONTROL ==========
void checkRelayTimers() {
  // Röle 2
  if (relayTimers[0] && (millis() - relayTimerStart[0] >= RELAY_TIMER)) {
    digitalWrite(RELAY2_PIN, RELAY_OFF);
    relayStates[1] = false;
    relayTimers[0] = false;
    Serial.println("Röle 2 kapandı (3sn doldu)");
    updateBlynkRelayStates();
  }
  
  // Röle 3
  if (relayTimers[1] && (millis() - relayTimerStart[1] >= RELAY_TIMER)) {
    digitalWrite(RELAY3_PIN, RELAY_OFF);
    relayStates[2] = false;
    relayTimers[1] = false;
    Serial.println("Röle 3 kapandı (3sn doldu)");
    updateBlynkRelayStates();
  }
  
  // Röle 4
  if (relayTimers[2] && (millis() - relayTimerStart[2] >= RELAY_TIMER)) {
    digitalWrite(RELAY4_PIN, RELAY_OFF);
    relayStates[3] = false;
    relayTimers[2] = false;
    Serial.println("Röle 4 kapandı (3sn doldu)");
    updateBlynkRelayStates();
  }
}

// ========== KAPI DURUMU KONTROL ==========
void checkDoorStatus() {
  int currentState = digitalRead(MAGNETIC_PIN);
  
  if (currentState != previousDoorState) {
    previousDoorState = currentState;
    updateDoorStatus();
    
    if (currentState == HIGH) {
      Serial.println("🚪 KAPI AÇILDI!");
      sendTelegramMessage("🚪🔓 KAPI AÇILDI!");
      Blynk.virtualWrite(V10, "AÇIK");
    } else {
      Serial.println("🚪 KAPI KAPANDI!");
      sendTelegramMessage("🚪🔒 KAPI KAPANDI!");
      Blynk.virtualWrite(V10, "KAPALI");
    }
  }
}

// ========== RÖLE BAŞLATMA ==========
void relayInit() {
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  pinMode(MAGNETIC_PIN, INPUT_PULLUP);
  
  digitalWrite(RELAY1_PIN, RELAY_OFF);
  digitalWrite(RELAY2_PIN, RELAY_OFF);
  digitalWrite(RELAY3_PIN, RELAY_OFF);
  digitalWrite(RELAY4_PIN, RELAY_OFF);
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  
  relayInit();
  
  Serial.println("");
  Serial.println("========================================");
  Serial.println("ESP8266 BLYNK + TELEGRAM + MANYETİK SWITCH");
  Serial.println("========================================");
  Serial.print("WiFi: ");
  Serial.println(ssid);
  Serial.print("Blynk Token: ");
  Serial.println(BLYNK_AUTH_TOKEN);
  Serial.print("Telegram Bot: @panjurkontrol_bot");
  Serial.println("");
  Serial.println("========================================");
  Serial.println("");
  
  // WiFi Bağlantısı
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
  }
  
  // Blynk Bağlantısı
  Serial.print("Blynk'e bağlanılıyor...");
  Blynk.config(BLYNK_AUTH_TOKEN);
  
  if (Blynk.connect()) {
    Serial.println(" ✅ Bağlandı!");
    updateBlynkRelayStates();
    updateDoorStatus();
  } else {
    Serial.println(" ❌ Bağlanamadı!");
  }
  
  previousDoorState = digitalRead(MAGNETIC_PIN);
  
  // Telegram başlangıç mesajı
  sendTelegramMessage("🤖 Panjur Kontrol Sistemi Başlatıldı!");
  sendTelegramMessage("📡 Blynk ve Telegram aktif!");
  sendTelegramMessage("📶 WiFi: " + String(ssid));
  
  if (previousDoorState == HIGH) {
    sendTelegramMessage("🚪 Kapı AÇIK durumda!");
  } else {
    sendTelegramMessage("🚪 Kapı KAPALI durumda!");
  }
  
  Serial.println("");
  Serial.println("✅ Sistem hazir!");
  Serial.println("");
  Serial.println("📌 Blynk App'ten röleleri kontrol edebilirsiniz:");
  Serial.println("  V4: Röle 1 (Kalıcı Aç/Kapa)");
  Serial.println("  V5: Röle 2 (3sn tetikleme)");
  Serial.println("  V6: Röle 3 (3sn tetikleme)");
  Serial.println("  V7: Röle 4 (3sn tetikleme)");
  Serial.println("");
  Serial.println("📌 Telegram Komutları:");
  Serial.println("  /durum  - Tüm durum bilgisi");
  Serial.println("  /ac     - Tüm röleleri aç");
  Serial.println("  /kapat  - Tüm röleleri kapat");
  Serial.println("  /start  - Yardım mesajı");
  Serial.println("");
  Serial.println("📌 Manyetik Switch (GPIO15):");
  Serial.println("  Kapı açıldığında Telegram bildirimi gönderir");
  Serial.println("========================================");
  Serial.println("");
}

// ========== LOOP ==========
void loop() {
  Blynk.run();
  checkRelayTimers();
  checkDoorStatus();
  handleTelegramCommands();
  
  // Blynk bağlantısı koptuysa yeniden bağlan
  if (!Blynk.connected()) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 5000) {
      lastReconnect = millis();
      Serial.println("⚠️ Blynk bağlantısı kesildi, yeniden bağlanılıyor...");
      Blynk.connect();
    }
  }
  
  delay(50);
}