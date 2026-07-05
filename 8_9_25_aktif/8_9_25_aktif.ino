

///////////////////////////////////////////////
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <AsyncTelegram2.h>
#include <DHT.h>
#include <PZEM004Tv30.h>

#define RELAY_CONTROL_PIN 25
#define RELAY_SHUTDOWN_PIN 32
#define DHTPIN 4
#define DHTTYPE DHT11
#define ZMPT_PIN 34
#define LOW_LEVEL_PIN 18
#define HIGH_LEVEL_PIN 19

DHT dht(DHTPIN, DHTTYPE);
WiFiClientSecure secured_client;
AsyncTelegram2 bot(secured_client);
PZEM004Tv30 pzem(Serial2, 16, 17);

// Telegram bot bilgileri
 const char* BOT_TOKEN = "8220396957:AAHiIFbeLBQqMeGfnphNIM6amPgmjQqeXeU";
  const int64_t GROUP_CHAT_ID = -1002971875406;
// Wi-Fi bilgileri
 const char* ssid = "FiberHGW_ZT655T";
  const char* password = "3Dyx4a7CXyDx";

bool pumpOn = false;
bool lowLevelHandled = false;
bool highLevelHandled = false;
unsigned long lastLowLevelActionTime = 0;
unsigned long lastHighLevelActionTime = 0;
int lowLevelActionCount = 0;
int highLevelActionCount = 0;
float zmptVref = 1.65;
static bool lastPumpState = false;

float readZMPT_RMS() {
  unsigned long t0 = millis();
  double acc = 0;
  uint32_t n = 0;
  while (millis() - t0 < 100) {
    int raw = analogRead(ZMPT_PIN);
    float v = (raw / 4095.0f) * 3.3f;
    float ac = v - zmptVref;
    acc += ac * ac;
    n++;
    delayMicroseconds(500);
  }
  float vrms = sqrt(acc / (double)n);
  float lineVrms = vrms * 220.0f / 0.4f;
  return lineVrms;
}

String getElectricMeasurements() {
  float voltage = pzem.voltage();
  float current = pzem.current();
  float power = pzem.power();
  float energy = pzem.energy();
  float frequency = pzem.frequency();
  float pf = pzem.pf();

  String s = "⚡ ELEKTRİK ÖLÇÜMLERİ:\n";
  s += "• Voltaj: " + String(voltage) + " V\n";
  s += "• Akım: " + String(current) + " A\n";
  s += "• Güç: " + String(power) + " W\n";
  s += "• Cosφ: " + String(pf) + "\n";
  s += "• Frekans: " + String(frequency) + " Hz\n";
  s += "• Enerji: " + String(energy) + " Wh\n";
  return s;
}

void sendToGroup(const String& text) {
  TBMessage msg;
  msg.chatId = GROUP_CHAT_ID;
  bot.sendMessage(msg, text);
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi bağlandı.");
}

void pulseRelay(int pin, int& count, unsigned long& lastTime, bool& handled, const String& message) {
  if (count < 3 && millis() - lastTime > 15000) {
    digitalWrite(pin, HIGH);
    delay(1000);
    digitalWrite(pin, LOW);
    count++;
    lastTime = millis();
    if (count == 1) sendToGroup(message);
    if (count >= 3) handled = true;
  }
}

void floatSwitchControl() {
  if (digitalRead(LOW_LEVEL_PIN) == HIGH && !lowLevelHandled) {
    highLevelHandled = false;
    pulseRelay(RELAY_CONTROL_PIN, lowLevelActionCount, lastLowLevelActionTime, lowLevelHandled,
               "💧 Su seviyesi azaldı, pompa geçici olarak çalıştırıldı.");
  }
  else if (digitalRead(HIGH_LEVEL_PIN) == HIGH && !highLevelHandled) {
    lowLevelHandled = false;
    pulseRelay(RELAY_SHUTDOWN_PIN, highLevelActionCount, lastHighLevelActionTime, highLevelHandled,
               "🛑 Su doldu, pompa geçici olarak kapatıldı.");
  }
  else {
    lowLevelActionCount = 0;
    highLevelActionCount = 0;
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  dht.begin();
  setupWiFi();

  pinMode(RELAY_CONTROL_PIN, OUTPUT);
  pinMode(RELAY_SHUTDOWN_PIN, OUTPUT);
  pinMode(LOW_LEVEL_PIN, INPUT);
  pinMode(HIGH_LEVEL_PIN, INPUT);

  digitalWrite(RELAY_CONTROL_PIN, LOW);
  digitalWrite(RELAY_SHUTDOWN_PIN, LOW);

  secured_client.setInsecure();
  bot.setTelegramToken(BOT_TOKEN);
  bot.setUpdateTime(1000);
  bot.begin();

  sendToGroup("🔌 Cihaz açıldı ve Telegram'a bağlandı.");
}

void loop() {
  floatSwitchControl();

  float current = pzem.current();
  bool currentPumpState = current > 1.0;

  if (currentPumpState != lastPumpState) {
    if (currentPumpState) {
      sendToGroup("⚡ Pompa çalıştı (Akım: " + String(current, 2) + " A)");
    } else {
      sendToGroup("⛔ Pompa kapandı (Akım: " + String(current, 2) + " A)");
    }
    lastPumpState = currentPumpState;
  }

  TBMessage msg;
  if (bot.getNewMessage(msg)) {
    if (msg.chatId != GROUP_CHAT_ID) return;

    String t = msg.text;
    t.toLowerCase();

    if (t == "/durum" || t == "durum") {
      float h = dht.readHumidity();
      float tC = dht.readTemperature();
      String pumpStatus = current > 1.0 ? "AÇIK" : "KAPALI";

      String s = "🔎 Durum:\n"
                 "• Pompa: " + pumpStatus + "\n"
                 "• Sıcaklık: " + String(isnan(tC)? -99 : tC) + " °C\n"
                 "• Nem: " + String(isnan(h)? -1 : h) + " %\n"
                 "• WiFi RSSI: " + String(WiFi.RSSI()) + " dBm\n\n";
      s += getElectricMeasurements();
      sendToGroup(s);
    }
    else if (t == "pompa aç") {
      digitalWrite(RELAY_CONTROL_PIN, HIGH);
      delay(1000);
      digitalWrite(RELAY_CONTROL_PIN, LOW);
      pumpOn = true;
      sendToGroup("✅ Pompa manuel olarak AÇILDI.");
    }
    else if (t == "pompa kapat") {
      digitalWrite(RELAY_SHUTDOWN_PIN, HIGH);
      delay(1000);
      digitalWrite(RELAY_SHUTDOWN_PIN, LOW);
      pumpOn = false;
      sendToGroup("⛔ Pompa manuel olarak KAPATILDI.");
    }
    else {
      sendToGroup("📋 Komutlar:\n"
                  "• durum → Sistem bilgilerini göster\n"
                  "• pompa aç → Pompayı manuel olarak çalıştır\n"
                  "• pompa kapat → Pompayı manuel olarak durdur\n"
                  "• Pompa durumu akım sensörü ile doğrulanmaktadır.");
    }
  }
}