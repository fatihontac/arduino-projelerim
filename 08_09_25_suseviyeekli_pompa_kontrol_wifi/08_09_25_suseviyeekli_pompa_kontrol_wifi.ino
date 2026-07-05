//SON DENENEN KOD SU SEVİYE RÖLELERİ EKLENDİ RM-ÖLELER KONROL
// EDİLDİ SADECE TETİKLEME İÇİN ÖALIŞIYOR UYGUN KOD


#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <AsyncTelegram2.h>
#include <DHT.h>
#include <PZEM004Tv30.h>

// --- Donanım Tanımları ---
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
PZEM004Tv30 pzem  (Serial2, 16, 17);

// Telegram bot bilgileri
const char* BOT_TOKEN = "8220396957:AAHiIFbeLBQqMeGfnphNIM6amPgmjQqeXeU";
const int64_t GROUP_CHAT_ID = -1002971875406;

// Wi-Fi bilgileri
const char* ssid = "FiberHGW_ZT655T";
const char* password = "3Dyx4a7CXyDx";

bool pumpOn = false;
bool gridHasPower = true;
bool lowLevelHandled = false;
bool highLevelHandled = false;
unsigned long lastPowerChangeMs = 0;
unsigned long lastLowLevelActionTime = 0;
unsigned long lastHighLevelActionTime = 0;
int lowLevelActionCount = 0;
int highLevelActionCount = 0;
const unsigned long powerDebounceMs = 8000;
float zmptVref = 1.65;

String relayStatusStr() { return pumpOn ? "AÇIK" : "KAPALI"; }

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
    digitalWrite(pin, HIGH);   // Röleyi geçici olarak aç
    delay(1000);
    digitalWrite(pin, LOW);    // Röleyi tekrar kapat
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

String getPZEMStatus() {
  float voltage = pzem.voltage();
  float current = pzem.current();
  float power = pzem.power();
  float energy = pzem.energy();
  float frequency = pzem.frequency();
  float pf = pzem.pf();

  String s = "🔌 PZEM Ölçümleri:\n";
  s += "• Voltaj: " + String(voltage) + " V\n";
  s += "• Akım: " + String(current) + " A\n";
  s += "• Güç: " + String(power) + " W\n";
  s += "• Cosφ: " + String(pf) + "\n";
  s += "• Frekans: " + String(frequency) + " Hz\n";
  s += "• Enerji: " + String(energy) + " Wh\n";
  return s;
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

  digitalWrite(RELAY_CONTROL_PIN, LOW);     // Röleler başlangıçta kapalı
  digitalWrite(RELAY_SHUTDOWN_PIN, LOW);

  secured_client.setInsecure();
  bot.setTelegramToken(BOT_TOKEN);
  bot.setUpdateTime(1000);
  bot.begin();

  sendToGroup("🔌 Cihaz açıldı ve Telegram'a bağlandı.");
}

void loop() {
  float v = readZMPT_RMS();
  bool powerNow = (v > 120.0f);
  unsigned long now = millis();

  if (powerNow != gridHasPower && (now - lastPowerChangeMs) > powerDebounceMs) {
    gridHasPower = powerNow;
    lastPowerChangeMs = now;
    if (gridHasPower) {
      sendToGroup("✅ Şebeke elektriği VAR (" + String((int)v) + "V~)");
    } else {
      sendToGroup("⚠️ Şebeke elektriği YOK (" + String((int)v) + "V~)");
      if (pumpOn) {
        digitalWrite(RELAY_CONTROL_PIN, LOW);
        pumpOn = false;
        sendToGroup("🛑 Elektrik yokken pompa güvenlik nedeniyle KAPATILDI.");
      }
    }
  }

  floatSwitchControl();

  TBMessage msg;
  if (bot.getNewMessage(msg)) {
    if (msg.chatId != GROUP_CHAT_ID) return;

    String t = msg.text;
    t.toLowerCase();

    if (t == "/durum" || t == "durum") {
      float h = dht.readHumidity();
      float tC = dht.readTemperature();
      String volt = String((int)readZMPT_RMS());
      String grid = gridHasPower ? "VAR" : "YOK";

      String s = "🔎 Durum:\n"
                 "• Pompa: " + relayStatusStr() + "\n"
                 "• Sıcaklık: " + String(isnan(tC)? -99 : tC) + " °C\n"
                 "• Nem: " + String(isnan(h)? -1 : h) + " %\n"
                 "• Şebeke: " + grid + " (" + volt + "V~)\n"
                 "• WiFi RSSI: " + String(WiFi.RSSI()) + " dBm\n\n";
      s += getPZEMStatus();
      sendToGroup(s);
    }

    else if (t == "/pompa_ac" || t == "pompa aç" || t == "pompa ac") {
      if (!gridHasPower) {
        sendToGroup("⚠️ Şebeke elektriği YOK. Pompa çalıştırılmadı.");
      } else if (pumpOn) {
        sendToGroup("ℹ️ Pompa zaten AÇIK.");
      } else {
        digitalWrite(RELAY_CONTROL_PIN, HIGH);
        pumpOn = true;
        sendToGroup("✅ Pompa manuel olarak AÇILDI.");
      }
    }

    else if (t == "/pompa_kapat" || t == "pompa kapat") {
      if (!pumpOn) {
        sendToGroup("ℹ️ Pompa zaten KAPALI.");
      } else {
        digitalWrite(RELAY_CONTROL_PIN, LOW);
        pumpOn = false;
        sendToGroup("🟦 Pompa manuel olarak KAPATILDI.");
      }
    }

    else {
      sendToGroup("📋 Komutlar:\n"
                  "• /durum → Sistem bilgilerini göster\n"
                  "• /pompa_ac veya 'pompa aç' → Pompayı aç\n"
                  "• /pompa_kapat veya 'pompa kapat' → Pompayı kapat");
    }
  }
}
