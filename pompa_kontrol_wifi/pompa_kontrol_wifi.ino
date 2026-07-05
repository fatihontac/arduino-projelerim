#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <AsyncTelegram2.h>
#include <ArduinoJson.h>
#include <DHT.h>

// --- Donanım Tanımları ---
#define RELAY_PIN 26
#define DHTPIN    32
#define DHTTYPE   DHT11
#define ZMPT_PIN  34
#define RESET_BUTTON_PIN 33  // Wi-Fi sıfırlama için buton

DHT dht(DHTPIN, DHTTYPE);
WiFiClientSecure secured_client;
AsyncTelegram2 bot(secured_client);

// Telegram bot bilgileri
 const char* BOT_TOKEN = "8220396957:AAHiIFbeLBQqMeGfnphNIM6amPgmjQqeXeU"; // Bot token
//const int64_t GROUP_CHAT_ID = -1229956787;  // Grup chat ID
const int64_t GROUP_CHAT_ID = -1002971875406;
// Durum değişkenleri
bool pumpOn = false;
bool gridHasPower = true;
unsigned long lastPowerChangeMs = 0;
const unsigned long powerDebounceMs = 8000;

// ZMPT kalibrasyon parametreleri
float zmptSensitivity = 0.011;
float zmptVref = 1.65;

// Röle durumu metni
String relayStatusStr() { return pumpOn ? "AÇIK" : "KAPALI"; }

// RMS voltaj ölçümü
float readZMPT_RMS() {
  unsigned long t0 = millis();
  double acc = 0;
  uint32_t n = 0;
  while (millis() - t0 < 100) {
    int raw = analogRead(ZMPT_PIN);
    float v  = (raw / 4095.0f) * 3.3f;
    float ac = v - zmptVref;
    acc += ac * ac;
    n++;
    delayMicroseconds(500);
  }
  float vrms = sqrt(acc / (double)n);
  float lineVrms = vrms * 220.0f / 0.4f;
  return lineVrms;
}

// Telegram mesaj gönderimi
void sendToGroup(const String& text) {
  TBMessage msg;
  msg.chatId = GROUP_CHAT_ID;
  bot.sendMessage(msg, text);
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);  // Buton pin tanımı
  pumpOn = false;

  Serial.begin(115200);
  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFiManager wm;

  // Butona uzun basıldıysa Wi-Fi ayarlarını sıfırla
  unsigned long pressStart = millis();
  while (digitalRead(RESET_BUTTON_PIN) == LOW) {
    if (millis() - pressStart > 3000) {
      Serial.println("Wi-Fi ayarları sıfırlanıyor...");
      wm.resetSettings();
      delay(1000);
      break;
    }
  }

  bool ok = wm.autoConnect("PompaKontrol", "12345678");
  Serial.println(ok ? "WiFi bağlandı" : "WiFi başarısız");

  bot.setUpdateTime(1000);
  bot.setTelegramToken(BOT_TOKEN);

  sendToGroup("🔌 Cihaz açıldı ve Telegram'a bağlandı.");
  Serial.println("Bot hazır.");
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
        digitalWrite(RELAY_PIN, HIGH);
        pumpOn = false;
        sendToGroup("🛑 Elektrik yokken pompa güvenlik nedeniyle KAPATILDI.");
      }
    }
  }

  TBMessage msg;
  if (bot.getNewMessage(msg)) {
    if (msg.chatId != GROUP_CHAT_ID) return;

    String t = msg.text;
    t.toLowerCase();

    auto cevapDurum = [&]() {
      float h = dht.readHumidity();
      float tC = dht.readTemperature();
      String volt = String((int)readZMPT_RMS());
      String grid = gridHasPower ? "VAR" : "YOK";
      String s = "🔎 Durum:\n"
                 "• Pompa: " + relayStatusStr() + "\n"
                 "• Sıcaklık: " + String(isnan(tC)? -99 : tC) + " °C\n"
                 "• Nem: " + String(isnan(h)? -1 : h) + " %\n"
                 "• Şebeke: " + grid + " (" + volt + "V~)\n"
                 "• WiFi RSSI: " + String(WiFi.RSSI()) + " dBm";
      sendToGroup(s);
    };

    if (t == "/durum" || t == "durum") {
      cevapDurum();
    }
    else if (t == "/pompa_ac" || t == "pompa aç" || t == "pompa ac") {
      if (!gridHasPower) {
        sendToGroup("⚠️ Şebeke elektriği YOK. Pompa çalıştırılmadı.");
      } else if (pumpOn) {
        sendToGroup("ℹ️ Pompa zaten AÇIK.");
      } else {
        digitalWrite(RELAY_PIN, LOW);
        pumpOn = true;
        sendToGroup("✅ Pompa AÇILDI.");
      }
    }
    else if (t == "/pompa_kapat" || t == "pompa kapat") {
      if (!pumpOn) {
        sendToGroup("ℹ️ Pompa zaten KAPALI.");
      } else {
        digitalWrite(RELAY_PIN, HIGH);
        pumpOn = false;
        sendToGroup("🟦 Pompa KAPANDI.");
      }
    }
    else {
      sendToGroup("Komutlar: \n"
                  "• /pompa_ac  veya 'pompa aç'\n"
                  "• /pompa_kapat  veya 'pompa kapat'\n"
                  "• /durum");
    }
  }
}
