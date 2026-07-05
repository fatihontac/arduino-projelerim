#define BLYNK_TEMPLATE_ID   "TMPL609h2xxlP"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN    "RnaZLezpzt4j5ltLBuvvY4Pg5f9402w1"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

/* ----------- WİFİ ve BLYNK ----------- */
char ssid[] = "FiberHGW_ZTK3AY-2.4G-ext";
char pass[] = "XfsfEU4cDsCz";
char auth[] = BLYNK_AUTH_TOKEN;

/* ----------- RÖLE PİNLERİ ----------- */
#define RELAY1 21   // V0
#define RELAY2 18   // V1
#define RELAY3 13   // V4

unsigned long lastWifiCheck = 0;

/* ----------- WİFİ BAĞLANTI SİSTEMİ ----------- */
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  Serial.print("WiFi baglaniyor");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi baglandi");
}

/* ----------- SETUP ----------- */
void setup() {
  Serial.begin(115200);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);

  // Röleler başlangıçta kapalı
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  digitalWrite(RELAY3, HIGH);

  setupWiFi();
  Blynk.config(auth);
  Blynk.connect();
}

/* ----------- BLYNK BUTONLARI (PUSH / MOMENTARY) ----------- */
BLYNK_WRITE(V0) {
  int val = param.asInt();              // 1 = basılı, 0 = bırakıldı
  digitalWrite(RELAY1, val ? LOW : HIGH);
}

BLYNK_WRITE(V1) {
  int val = param.asInt();
  digitalWrite(RELAY2, val ? LOW : HIGH);
}

BLYNK_WRITE(V4) {
  int val = param.asInt();
  digitalWrite(RELAY3, val ? LOW : HIGH);
}

/* ----------- LOOP ----------- */
void loop() {

  // WiFi + Blynk kontrolü (5 saniye)
  if (millis() - lastWifiCheck > 5000) {
    lastWifiCheck = millis();

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi bagli degil, tekrar baglaniliyor...");
      WiFi.disconnect();
      WiFi.begin(ssid, pass);
    }

    if (!Blynk.connected()) {
      Serial.println("Blynk bagli degil, tekrar baglaniliyor...");
      Blynk.connect();
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }
}
