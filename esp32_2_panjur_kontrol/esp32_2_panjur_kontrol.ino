/*
  ============================================================
  ESP32 #2 - 4 RÖLE (3sn TETİKLEME) - MUTFAK
  ============================================================
  
  📌 YENİ BLYNK BİLGİLERİ:
     Template ID: TMPL6wT4w1xHf
     Template Name: Quickstart TemplateCopy
     Auth Token: 26tnkIhziSuDFLSMNiebTavjpeL_Givr
  
  📌 BLYNK PİNLERİ:
     V13 → Röle 1 (GPIO25) - 3sn tetikleme
     V14 → Röle 2 (GPIO26) - 3sn tetikleme
     V15 → Röle 3 (GPIO32) - 3sn tetikleme
     V16 → Röle 4 (GPIO33) - 3sn tetikleme
  ============================================================
*/

// ============================================================
// BLYNK AYARLARI (YENİ!)
// ============================================================
#define BLYNK_TEMPLATE_ID "TMPL6wT4w1xHf"
#define BLYNK_TEMPLATE_NAME "Quickstart TemplateCopy"
#define BLYNK_AUTH_TOKEN "26tnkIhziSuDFLSMNiebTavjpeL_Givr"

#define BLYNK_PRINT Serial

// ============================================================
// KÜTÜPHANELER
// ============================================================
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <WiFiManager.h>

// ============================================================
// ESP32 PIN TANIMLAMALARI
// ============================================================
#define RELAY1_PIN 25
#define RELAY2_PIN 26
#define RELAY3_PIN 32
#define RELAY4_PIN 33

#define RELAY_ON HIGH
#define RELAY_OFF LOW
#define RELAY_TIMER 3000

// ============================================================
// WIFIMANAGER
// ============================================================
WiFiManager wm;

// ============================================================
// DEĞİŞKENLER
// ============================================================
bool relayStates[4] = {false, false, false, false};
bool relayTimers[4] = {false, false, false, false};
unsigned long relayTimerStart[4] = {0, 0, 0, 0};

// ============================================================
// RÖLE FONKSİYONLARI
// ============================================================

void setRelay(int index, bool state) {
  if (index < 0 || index > 3) return;
  
  switch(index) {
    case 0: digitalWrite(RELAY1_PIN, state ? RELAY_ON : RELAY_OFF); break;
    case 1: digitalWrite(RELAY2_PIN, state ? RELAY_ON : RELAY_OFF); break;
    case 2: digitalWrite(RELAY3_PIN, state ? RELAY_ON : RELAY_OFF); break;
    case 3: digitalWrite(RELAY4_PIN, state ? RELAY_ON : RELAY_OFF); break;
  }
  
  relayStates[index] = state;
  
  Serial.print("Mutfak Röle ");
  Serial.print(index + 1);
  Serial.print(" -> ");
  Serial.println(state ? "AÇIK (HIGH)" : "KAPALI (LOW)");
}

void checkRelayTimers() {
  for (int i = 0; i < 4; i++) {
    if (relayTimers[i] && (millis() - relayTimerStart[i] >= RELAY_TIMER)) {
      setRelay(i, false);
      relayTimers[i] = false;
      Serial.print("Mutfak Röle ");
      Serial.print(i + 1);
      Serial.println(" kapandı (3sn)");
    }
  }
}

// ============================================================
// BLYNK WIDGET CALLBACKS
// ============================================================

BLYNK_WRITE(V13) {
  int value = param.asInt();
  if (value == 1) {
    setRelay(0, true);
    relayTimers[0] = true;
    relayTimerStart[0] = millis();
    Serial.println("🔘 V13 - Mutfak Röle 1 (3sn)");
  }
}

BLYNK_WRITE(V14) {
  int value = param.asInt();
  if (value == 1) {
    setRelay(1, true);
    relayTimers[1] = true;
    relayTimerStart[1] = millis();
    Serial.println("🔘 V14 - Mutfak Röle 2 (3sn)");
  }
}

BLYNK_WRITE(V15) {
  int value = param.asInt();
  if (value == 1) {
    setRelay(2, true);
    relayTimers[2] = true;
    relayTimerStart[2] = millis();
    Serial.println("🔘 V15 - Mutfak Röle 3 (3sn)");
  }
}

BLYNK_WRITE(V16) {
  int value = param.asInt();
  if (value == 1) {
    setRelay(3, true);
    relayTimers[3] = true;
    relayTimerStart[3] = millis();
    Serial.println("🔘 V16 - Mutfak Röle 4 (3sn)");
  }
}

// ============================================================
// WIFI MANAGER
// ============================================================

void setupWiFiManager() {
  Serial.println("");
  Serial.println("========================================");
  Serial.println("📡 WiFi Manager Başlatılıyor...");
  Serial.println("========================================");
  Serial.println("📌 AP Adı: fatihontacautomation2");
  Serial.println("📌 AP Şifresi: 05416932009");
  Serial.println("📌 Bağlanıp 192.168.4.1 adresine git");
  Serial.println("📌 SADECE WiFi bilgilerini gir!");
  Serial.println("========================================");
  Serial.println("");
  
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(30);
  
  if (!wm.autoConnect("fatihontacautomation2", "05416932009")) {
    Serial.println("❌ WiFi Manager bağlanamadı!");
    ESP.restart();
  }
  
  Serial.println("");
  Serial.println("========================================");
  Serial.println("✅ WiFi bağlandı!");
  Serial.print("📡 IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("========================================");
  Serial.println("");
}

void connectBlynk() {
  Serial.print("Blynk bağlanıyor...");
  Blynk.config(BLYNK_AUTH_TOKEN, "blynk.cloud", 80);
  
  if (Blynk.connect()) {
    Serial.println(" ✅ Bağlandı!");
  } else {
    Serial.println(" ❌ Bağlanamadı!");
  }
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  
  digitalWrite(RELAY1_PIN, RELAY_OFF);
  digitalWrite(RELAY2_PIN, RELAY_OFF);
  digitalWrite(RELAY3_PIN, RELAY_OFF);
  digitalWrite(RELAY4_PIN, RELAY_OFF);
  
  Serial.println("");
  Serial.println("========================================");
  Serial.println("🍳 ESP32 #2 - MUTFAK (3sn TETİKLEME)");
  Serial.println("========================================");
  Serial.println("📌 Template ID: TMPL6wT4w1xHf");
  Serial.println("📌 Auth Token: 26tnkIhziSuDFLSMNiebTavjpeL_Givr");
  Serial.println("📌 AP: fatihontacautomation2");
  Serial.println("========================================");
  Serial.println("");
  
  setupWiFiManager();
  connectBlynk();
  
  Serial.println("");
  Serial.println("========================================");
  if (Blynk.connected()) {
    Serial.println("✅ MUTFAK SİSTEMİ HAZIR!");
  } else {
    Serial.println("⚠️ SADECE RÖLELER ÇALIŞIYOR!");
  }
  Serial.println("========================================");
  Serial.println("");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi kesildi!");
    WiFi.disconnect();
    WiFi.reconnect();
    delay(5000);
    return;
  }
  
  if (Blynk.connected()) {
    Blynk.run();
  } else {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 10000) {
      lastReconnect = millis();
      Serial.println("⚠️ Blynk yeniden bağlanıyor...");
      Blynk.connect();
    }
  }
  
  checkRelayTimers();
  
  delay(50);
}