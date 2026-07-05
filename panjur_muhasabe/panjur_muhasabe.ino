/*
  ============================================================
  ESP8266 BLYNK RÖLE KONTROL
  ============================================================
  
  Blynk App ile 4 röleyi kontrol et:
  V4 → Röle 1 (Kalıcı Aç/Kapa)
  V5 → Röle 2 (3 saniye tetikleme)
  V6 → Röle 3 (3 saniye tetikleme)
  V7 → Röle 4 (3 saniye tetikleme)
  
  ESP8266 GPIO Pinleri:
  GPIO16 (D0) → Röle 1
  GPIO14 (D5) → Röle 2
  GPIO12 (D6) → Röle 3
  GPIO13 (D7) → Röle 4
  GPIO15 (D8) → Manyetik Switch (opsiyonel)
  ============================================================
*/

// ========== BLYNK AYARLARI ==========
#define BLYNK_TEMPLATE_ID "TMPL69e3IoQ_3"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "XyLuEgFrfHV5PVAcLQPkkk8tX9maSpuf"

// ========== KÜTÜPHANELER ==========
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

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

// ========== DEĞİŞKENLER ==========
bool relayStates[4] = {false, false, false, false};
bool relayTimers[3] = {false, false, false};
unsigned long relayTimerStart[3] = {0, 0, 0};
bool previousDoorState = HIGH;

// ========== FONKSİYON PROTOTİPLERİ ==========
void updateBlynkRelayStates();
void checkRelayTimers();
void checkDoorStatus();

// ========== BLYNK WIDGET CALLBACKS ==========

// V4 - Röle 1 (Kalıcı Açık/Kapalı)
BLYNK_WRITE(V4) {
  int value = param.asInt();
  if (value == 1) {
    digitalWrite(RELAY1_PIN, RELAY_ON);
    relayStates[0] = true;
    Serial.println("Röle 1 AÇIK (kalıcı)");
  } else {
    digitalWrite(RELAY1_PIN, RELAY_OFF);
    relayStates[0] = false;
    Serial.println("Röle 1 KAPALI (kalıcı)");
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
    Serial.print("Kapı durumu değişti: ");
    Serial.println(currentState == HIGH ? "AÇIK" : "KAPALI");
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
  Serial.println("ESP8266 BLYNK RÖLE KONTROL");
  Serial.println("========================================");
  Serial.print("WiFi: ");
  Serial.println(ssid);
  Serial.print("Blynk Token: ");
  Serial.println(BLYNK_AUTH_TOKEN);
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
    Serial.print("WiFi bağlandı! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("WiFi bağlanamadı!");
  }
  
  // Blynk Bağlantısı
  Serial.print("Blynk'e bağlanılıyor...");
  Blynk.config(BLYNK_AUTH_TOKEN);
  
  if (Blynk.connect()) {
    Serial.println(" Bağlandı!");
    updateBlynkRelayStates();
    updateDoorStatus();
  } else {
    Serial.println(" Bağlanamadı!");
  }
  
  previousDoorState = digitalRead(MAGNETIC_PIN);
  
  Serial.println("");
  Serial.println("Sistem hazir!");
  Serial.println("Blynk App'ten röleleri kontrol edebilirsiniz:");
  Serial.println("  V4: Röle 1 (Kalıcı Aç/Kapa)");
  Serial.println("  V5: Röle 2 (3sn tetikleme)");
  Serial.println("  V6: Röle 3 (3sn tetikleme)");
  Serial.println("  V7: Röle 4 (3sn tetikleme)");
  Serial.println("========================================");
  Serial.println("");
}

// ========== LOOP ==========
void loop() {
  Blynk.run();
  checkRelayTimers();
  checkDoorStatus();
  
  // Blynk bağlantısı koptuysa yeniden bağlan
  if (!Blynk.connected()) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 5000) {
      lastReconnect = millis();
      Serial.println("Blynk bağlantısı kesildi, yeniden bağlanılıyor...");
      Blynk.connect();
    }
  }
  
  delay(50);
}