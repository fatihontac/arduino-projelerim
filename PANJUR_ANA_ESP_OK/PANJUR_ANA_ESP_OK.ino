/*
  ============================================================
  ESP32 #1 - PANJUR KONTROL (SALON)
  ============================================================
  
  📌 SADECE WiFi bilgileri WiFiManager'den girilir!
  📌 Blynk Template ID ve Token SABİT!
  
  📌 BLYNK PİNLERİ:
     V9  → Röle 1 (GPIO25) - BUTON (Basılı tut → AÇIK, Bırak → KAPALI)
     V10 → Röle 2 (GPIO26) - BUTON (Basılı tut → AÇIK, Bırak → KAPALI)
     V11 → Röle 3 (GPIO32) - BUTON (Basınca 3sn AÇIK, sonra KAPANIR)
     V12 → Röle 4 (GPIO33) - BUTON (Basınca 3sn AÇIK, sonra KAPANIR)
  
  📌 RÖLE AKTİFLİK: HIGH aktif (HIGH=Çeker, LOW=Bırakır)
  ============================================================
*/

// ============================================================
// BLYNK AYARLARI (SABİT!)
// ============================================================
#define BLYNK_TEMPLATE_ID "TMPL6643iV697"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "ZkOKjV_wg3mhwXw48-1Ujl-OZIDFcwLF"

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
#define RELAY1_PIN 25   // V9  - BUTON (Basılı tut)
#define RELAY2_PIN 26   // V10 - BUTON (Basılı tut)
#define RELAY3_PIN 32   // V11 - BUTON (3sn tetikleme)
#define RELAY4_PIN 33   // V12 - BUTON (3sn tetikleme)

// ============================================================
// RÖLE AKTİFLİK (HIGH AKTİF!)
// ============================================================
#define RELAY_ON HIGH
#define RELAY_OFF LOW
#define RELAY_TIMER 3000  // 3 saniye

// ============================================================
// WIFIMANAGER - SADECE WiFi İÇİN!
// ============================================================
WiFiManager wm;

// ============================================================
// DEĞİŞKENLER
// ============================================================
bool relayStates[4] = {false, false, false, false};

// Röle 3 ve 4 için timer (V11 ve V12)
bool relayTimers[2] = {false, false};
unsigned long relayTimerStart[2] = {0, 0};

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
  
  // Blynk widget'larını güncelle
  if (Blynk.connected()) {
    if (index == 0) Blynk.virtualWrite(V9, state);
    if (index == 1) Blynk.virtualWrite(V10, state);
  }
  
  Serial.print("Röle ");
  Serial.print(index + 1);
  Serial.print(" (GPIO");
  if (index == 0) Serial.print(RELAY1_PIN);
  else if (index == 1) Serial.print(RELAY2_PIN);
  else if (index == 2) Serial.print(RELAY3_PIN);
  else if (index == 3) Serial.print(RELAY4_PIN);
  Serial.print(") -> ");
  Serial.println(state ? "AÇIK (HIGH)" : "KAPALI (LOW)");
}

void checkRelayTimers() {
  for (int i = 0; i < 2; i++) {
    if (relayTimers[i] && (millis() - relayTimerStart[i] >= RELAY_TIMER)) {
      setRelay(i + 2, false);  // Röle 3 ve 4
      relayTimers[i] = false;
      Serial.print("Röle ");
      Serial.print(i + 3);
      Serial.println(" kapandı (3sn)");
    }
  }
}

// ============================================================
// BLYNK WIDGET CALLBACKS
// ============================================================

// ============================================================
// V9 - Röle 1 (BUTON - Basılı tut AÇIK, Bırak KAPALI)
// ============================================================
BLYNK_WRITE(V9) {
  int value = param.asInt();  // 1=basılı, 0=bırakıldı
  setRelay(0, value == 1);
}

// ============================================================
// V10 - Röle 2 (BUTON - Basılı tut AÇIK, Bırak KAPALI)
// ============================================================
BLYNK_WRITE(V10) {
  int value = param.asInt();  // 1=basılı, 0=bırakıldı
  setRelay(1, value == 1);
}

// ============================================================
// V11 - Röle 3 (BUTON - Basınca 3sn AÇIK, sonra KAPANIR)
// ============================================================
BLYNK_WRITE(V11) {
  int value = param.asInt();  // 1=basıldı, 0=bırakıldı
  
  if (value == 1) {
    // Basıldı → Röle 3'ü 3sn aç
    setRelay(2, true);
    relayTimers[0] = true;
    relayTimerStart[0] = millis();
    Serial.println("🔘 V11 BUTONUNA BASILDI (Röle 3 - 3sn çalışacak)");
  } else {
    // Bırakıldı → timer devam eder, 3sn sonra kapanır
    Serial.println("🔘 V11 BUTONU BIRAKILDI (3sn sonra kapanacak)");
  }
}

// ============================================================
// V12 - Röle 4 (BUTON - Basınca 3sn AÇIK, sonra KAPANIR)
// ============================================================
BLYNK_WRITE(V12) {
  int value = param.asInt();  // 1=basıldı, 0=bırakıldı
  
  if (value == 1) {
    // Basıldı → Röle 4'ü 3sn aç
    setRelay(3, true);
    relayTimers[1] = true;
    relayTimerStart[1] = millis();
    Serial.println("🔘 V12 BUTONUNA BASILDI (Röle 4 - 3sn çalışacak)");
  } else {
    // Bırakıldı → timer devam eder, 3sn sonra kapanır
    Serial.println("🔘 V12 BUTONU BIRAKILDI (3sn sonra kapanacak)");
  }
}

// ============================================================
// WIFI MANAGER (SADECE WİFİ)
// ============================================================

void setupWiFiManager() {
  Serial.println("");
  Serial.println("========================================");
  Serial.println("📡 WiFi Manager Başlatılıyor...");
  Serial.println("========================================");
  Serial.println("📌 AP Adı: fatihontacautomation");
  Serial.println("📌 AP Şifresi: 05416932009");
  Serial.println("📌 Bağlanıp 192.168.4.1 adresine git");
  Serial.println("📌 SADECE WiFi bilgilerini gir!");
  Serial.println("📌 Blynk bilgileri SABİT (kodda tanımlı)");
  Serial.println("========================================");
  Serial.println("");
  
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(30);
  
  if (!wm.autoConnect("fatihontacautomation", "05416932009")) {
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

// ============================================================
// BLYNK BAĞLANTISI (SABİT TOKEN!)
// ============================================================

void connectBlynk() {
  Serial.print("Blynk bağlanıyor...");
  Blynk.config(BLYNK_AUTH_TOKEN, "blynk.cloud", 80);
  
  if (Blynk.connect()) {
    Serial.println(" ✅ Bağlandı!");
    Blynk.virtualWrite(V9, relayStates[0]);
    Blynk.virtualWrite(V10, relayStates[1]);
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
  
  // Röle pinlerini OUTPUT olarak ayarla
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  
  // Başlangıçta tüm röleler kapalı (LOW)
  digitalWrite(RELAY1_PIN, RELAY_OFF);
  digitalWrite(RELAY2_PIN, RELAY_OFF);
  digitalWrite(RELAY3_PIN, RELAY_OFF);
  digitalWrite(RELAY4_PIN, RELAY_OFF);
  
  Serial.println("");
  Serial.println("========================================");
  Serial.println("🏠 ESP32 #1 - PANJUR KONTROL (SALON)");
  Serial.println("========================================");
  Serial.println("📌 Blynk Template ID: TMPL6643iV697 (SABİT)");
  Serial.println("📌 Blynk Auth Token: ZkOKjV_... (SABİT)");
  Serial.println("📌 AP: fatihontacautomation");
  Serial.println("📌 AP Şifre: 05416932009");
  Serial.println("========================================");
  Serial.println("📌 RÖLE PİNLERİ (HIGH AKTİF):");
  Serial.println("   GPIO25 → Röle 1 (V9 - BUTON)");
  Serial.println("   GPIO26 → Röle 2 (V10 - BUTON)");
  Serial.println("   GPIO32 → Röle 3 (V11 - BUTON - 3sn)");
  Serial.println("   GPIO33 → Röle 4 (V12 - BUTON - 3sn)");
  Serial.println("========================================");
  Serial.println("📌 BUTON MANTIĞI:");
  Serial.println("   V9:  Basılı tut → AÇIK, Bırak → KAPALI");
  Serial.println("   V10: Basılı tut → AÇIK, Bırak → KAPALI");
  Serial.println("   V11: Basınca 3sn AÇIK, sonra KAPANIR");
  Serial.println("   V12: Basınca 3sn AÇIK, sonra KAPANIR");
  Serial.println("========================================");
  Serial.println("");
  
  setupWiFiManager();
  connectBlynk();
  
  Serial.println("");
  Serial.println("========================================");
  if (Blynk.connected()) {
    Serial.println("✅ SİSTEM HAZIR!");
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
    Serial.println("⚠️ WiFi kesildi! Yeniden bağlanılıyor...");
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