#include <WiFi.h>
#include <RemoteXY.h>
#include <DHT.h>
#include <time.h>

// RemoteXY ayarları
#define REMOTEXY_MODE__ESP32CORE_WIFI_POINT
#define REMOTEXY_WIFI_SSID "FiberHGW_ZT655T"
#define REMOTEXY_WIFI_PASSWORD "3Dyx4a7CXyDx"
#define REMOTEXY_SERVER_PORT 6377



// RemoteXY arayüz tanımı (RemoteXY sitesinden oluşturulmuş)
#pragma pack(push, 1)
uint8_t RemoteXY_CONF[] = {
  255,9,0,8,0,192,0,19,0,0,0,107,111,109,98,105,32,107,111,110,
  116,114,111,108,0,31,1,106,200,1,1,9,0,2,4,174,44,22,0,2,
  26,31,31,79,78,0,79,70,70,0,2,55,147,44,22,0,2,26,31,31,
  79,78,0,79,70,70,0,4,7,47,96,21,128,2,26,13,6,119,91,20,
  77,14,94,26,2,3,147,44,22,0,2,26,31,31,79,78,0,79,70,70,
  0,2,53,175,44,22,0,2,26,31,31,79,78,0,79,70,70,0,2,6,
  4,98,33,0,1,26,31,31,79,78,0,79,70,70,0,71,7,75,41,41,
  56,0,1,24,135,0,0,0,0,0,0,200,66,0,0,160,65,0,0,32,
  65,0,0,0,64,24,0,71,55,75,42,42,56,0,6,24,135,0,0,0,
  0,0,0,200,66,0,0,160,65,0,0,32,65,0,0,0,64,24,0
};

struct {
  uint8_t switch_01; // Manuel kombi açma
  uint8_t switch_02; // Otomatik mod
  int8_t slider_01;  // Hedef sıcaklık
  uint8_t ZAMANLAYICI; // Saat
  uint8_t editTime_01_minute;
  uint8_t editTime_01_second;
  uint8_t switch_03; // Röle 1
  uint8_t switch_04; // Röle 2
  uint8_t switch_05; // Röle 3
  float SICAKLIK;
  float NEM;
  uint8_t connect_flag;
} RemoteXY;
#pragma pack(pop)

// DHT11 tanımı
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Röle pinleri
#define RELAY_KOMBI 22
#define RELAY_1     23
#define RELAY_2     21
#define RELAY_3     19

void setup() {
  RemoteXY_Init();
  dht.begin();

  pinMode(RELAY_KOMBI, OUTPUT);
  pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);
  pinMode(RELAY_3, OUTPUT);

  digitalWrite(RELAY_KOMBI, LOW);
  digitalWrite(RELAY_1, LOW);
  digitalWrite(RELAY_2, LOW);
  digitalWrite(RELAY_3, LOW);

  // NTP saat senkronizasyonu (Türkiye GMT+3)
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

void loop() {
  RemoteXY_Handler();

  // Sıcaklık ve nem okuma
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  if (!isnan(temp)) RemoteXY.SICAKLIK = temp;
  if (!isnan(hum)) RemoteXY.NEM = hum;

  // Saat bilgisi
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);
  int hourNow = timeinfo->tm_hour;
  int minuteNow = timeinfo->tm_min;
  int secondNow = timeinfo->tm_sec;

  // Kombi kontrol mantığı
  bool saatUygun = (hourNow == RemoteXY.ZAMANLAYICI &&
                    minuteNow == RemoteXY.editTime_01_minute &&
                    abs(secondNow - RemoteXY.editTime_01_second) <= 5);

  bool soguk = !isnan(temp) && temp < (RemoteXY.slider_01 - 5);
  bool sicak = !isnan(temp) && temp > (RemoteXY.slider_01 + 5);

  if (RemoteXY.switch_01 == 1) {
    digitalWrite(RELAY_KOMBI, HIGH); // Manuel açık
  } else if (RemoteXY.switch_02 == 1) {
    if (saatUygun) {
      if (soguk) digitalWrite(RELAY_KOMBI, HIGH);
      else if (sicak) digitalWrite(RELAY_KOMBI, LOW);
    } else {
      digitalWrite(RELAY_KOMBI, LOW);
    }
  } else {
    digitalWrite(RELAY_KOMBI, LOW);
  }

  // Diğer röleler
  digitalWrite(RELAY_1, RemoteXY.switch_03 ? HIGH : LOW);
  digitalWrite(RELAY_2, RemoteXY.switch_04 ? HIGH : LOW);
  digitalWrite(RELAY_3, RemoteXY.switch_05 ? HIGH : LOW);

  RemoteXY_delay(2000);
}