/*
 * 08_DtmfRelayControl.ino - DTMF Ton ile Röle Kontrolü
 *
 * Arduino'nun SIM kartını arayın → tuşlara basarak röleleri kontrol edin.
 *
 * DTMF Tuş Haritası:
 *   1 → Röle 1 AÇ        2 → Röle 1 KAPAT
 *   3 → Röle 2 AÇ        4 → Röle 2 KAPAT
 *   5 → Röle 3 AÇ        6 → Röle 3 KAPAT
 *   7 → Röle 4 AÇ        8 → Röle 4 KAPAT
 *   * → HEPSİ AÇ         0 → HEPSİ KAPAT
 *   # → Aramayı Kapat    9 → Durum (Serial'e yaz)
 *
 * Donanım - Pin Bağlantıları:
 *   Arduino Pin 2  →  SIM800C TX
 *   Arduino Pin 3  →  SIM800C RX
 *   Arduino Pin 9  →  SIM800C Boot/Power Key
 *   Arduino Pin 4  →  Röle 1 (IN1)
 *   Arduino Pin 5  →  Röle 2 (IN2)
 *   Arduino Pin 6  →  Röle 3 (IN3)
 *   Arduino Pin 7  →  Röle 4 (IN4)
 *
 * GitHub: https://github.com/hiberblsm/arduino-uno-gsm-shield
 * Geliştirici: Hiber Bilişim - https://www.hiber.com.tr
 */

#include "SIM800C.h"

// ============================================================
//  YAPILANDIRMA
// ============================================================

#define R1_PIN  4
#define R2_PIN  5
#define R3_PIN  6
#define R4_PIN  7

// Röle shield tipi:
//   RELAY_ACTIVE_HIGH → pin HIGH = röle AÇIK  (varsayılan)
//   Yorum satırı yap  → Active-LOW moda geçer (pin LOW = röle AÇIK)
#define RELAY_ACTIVE_HIGH

// Yetkili telefon numarası (güvenlik için)
// Boş bırakırsan → herkesten gelen arama kabul edilir
// Dolu olursa  → sadece bu numaradan gelen arama yanıtlanır
#define YETKILI  ""         // örn: "+905XXXXXXXXX"

// Maksimum çağrı süresi (ms) — dolunca otomatik kapar
#define MAKS_CAGRI_MS   120000UL    // 120 saniye

// RING gelip CLIP gelmezse kaç ms sonra yine de yanıtla (YETKILI boşsa)
#define CLIP_TIMEOUT_MS   4000UL

// ============================================================

#ifdef RELAY_ACTIVE_HIGH
  #define ROL_AC    HIGH
  #define ROL_KAPAT LOW
#else
  #define ROL_AC    LOW
  #define ROL_KAPAT HIGH
#endif

SIM800C gsm;

const uint8_t ROLE_PINLERI[4] = {R1_PIN, R2_PIN, R3_PIN, R4_PIN};
bool rolAcik[4] = {false, false, false, false};

// Çağrı durum makinesi
enum CallState : uint8_t { IDLE, RINGING, IN_CALL };
CallState callState     = IDLE;
unsigned long callTimer = 0;
bool clipGeldi          = false;

// ============================================================
//  RÖLE FONKSİYONLARI
// ============================================================

void roleInit() {
    for (uint8_t i = 0; i < 4; i++) {
        pinMode(ROLE_PINLERI[i], OUTPUT);
        digitalWrite(ROLE_PINLERI[i], ROL_KAPAT);
        rolAcik[i] = false;
    }
}

void roleSet(uint8_t idx, bool ac) {
    if (idx >= 4) return;
    digitalWrite(ROLE_PINLERI[idx], ac ? ROL_AC : ROL_KAPAT);
    rolAcik[idx] = ac;
    Serial.print(F("  [ROLE] "));
    Serial.print(idx + 1);
    Serial.println(ac ? F(" ACILDI") : F(" KAPATILDI"));
}

void durumYazdir() {
    Serial.println(F("  --- DURUM ---"));
    for (uint8_t i = 0; i < 4; i++) {
        Serial.print(F("  R"));
        Serial.print(i + 1);
        Serial.print(F(": "));
        Serial.println(rolAcik[i] ? F("ACIK") : F("KAPALI"));
    }
}

// ============================================================
//  DTMF TUŞUNU IŞLE
// ============================================================

void dtmfIsle(char ton) {
    Serial.print(F("[DTMF] Tus: "));
    Serial.println(ton);

    switch (ton) {
        case '1': roleSet(0, true);  break;  // R1 AÇ
        case '2': roleSet(0, false); break;  // R1 KAPAT
        case '3': roleSet(1, true);  break;  // R2 AÇ
        case '4': roleSet(1, false); break;  // R2 KAPAT
        case '5': roleSet(2, true);  break;  // R3 AÇ
        case '6': roleSet(2, false); break;  // R3 KAPAT
        case '7': roleSet(3, true);  break;  // R4 AÇ
        case '8': roleSet(3, false); break;  // R4 KAPAT
        case '*': for (uint8_t i = 0; i < 4; i++) roleSet(i, true);  break; // HEPSI AÇ
        case '0': for (uint8_t i = 0; i < 4; i++) roleSet(i, false); break; // HEPSI KAPAT
        case '9': durumYazdir(); break;
        case '#':
            Serial.println(F("  [DTMF] # → Arama kapatiliyor"));
            gsm.callHangup();
            callState = IDLE;
            break;
        default:
            Serial.println(F("  [DTMF] Tanimsiz tus"));
            break;
    }
}

// ============================================================
//  SETUP
// ============================================================

void setup() {
    roleInit();   // Önce röleleri kapat — güç açılışında tetiklenmesin
    Serial.begin(9600);
    delay(500);

    Serial.println(F("\n=============================="));
    Serial.println(F("  08 - DTMF Role Kontrol"));
    Serial.println(F("  Hiber Bilisim - hiber.com.tr"));
    Serial.println(F("=============================="));
    Serial.println(F("  1/2=R1  3/4=R2  5/6=R3  7/8=R4"));
    Serial.println(F("  * =HepsiAc  0=HepsiKapat  #=Kapat"));

    // Modem başlat
    Serial.println(F("\nModem baslatiliyor..."));
    gsm.hardReset();

    // SIM kart
    Serial.print(F("  SIM  : "));
    {
        uint8_t i;
        for (i = 0; i < 20; i++) {
            if (gsm.getSIMStatus() == "READY") break;
            delay(1000);
        }
        Serial.println(i < 20 ? F("READY") : F("HATA!"));
        if (i >= 20) { while (1); }
    }

    // Şebeke kaydı
    Serial.print(F("  Sebeke: "));
    for (uint8_t i = 0; i < 30; i++) {
        if (gsm.isRegistered()) break;
        delay(1000);
    }
    Serial.println(gsm.getOperator());

    // Sinyal
    int rssi = gsm.getSignalQuality();
    Serial.print(F("  RSSI : "));
    Serial.print(rssi);
    Serial.println(rssi == 99 ? F("  (sinyal yok!)") : F("  OK"));

    // DTMF ve arayan ID açık
    gsm.callSetCLIP(true);
    gsm.callSetDTMF(true);

    Serial.println(F("\n[HAZIR] Arama bekleniyor..."));
    if (strlen(YETKILI) > 3) {
        Serial.print(F("  Yetkili: "));
        Serial.println(F(YETKILI));
    } else {
        Serial.println(F("  (Yetki yok - herkese acik)"));
    }
}

// ============================================================
//  LOOP
// ============================================================

void loop() {
    char event[8];
    char detail[20];

    if (gsm.callPoll(event, sizeof(event), detail, sizeof(detail))) {

        // ---- RING — gelen çağrı ----
        if (!strcmp(event, "RING")) {
            if (callState == IDLE) {
                callState = RINGING;
                callTimer = millis();
                clipGeldi = false;
                Serial.println(F("\n[RING] Gelen arama..."));
            }
        }

        // ---- CLIP — arayan numara ----
        else if (!strcmp(event, "CLIP")) {
            if (callState == RINGING) {
                clipGeldi = true;
                Serial.print(F("  Arayan: "));
                Serial.println(detail);

                // Yetki kontrolü
                bool yetkili = (strlen(YETKILI) == 0 || !strcmp(detail, YETKILI));
                if (yetkili) {
                    gsm.callAnswer();
                    callState = IN_CALL;
                    callTimer = millis();
                    Serial.println(F("[CALL] Yanıtlandı — DTMF bekleniyor"));
                } else {
                    Serial.println(F("[RED] Yetkisiz numara, yanıtlanmadı"));
                    // Aramayı yoksay, IDLE'a dön
                    callState = IDLE;
                }
            }
        }

        // ---- DTMF — tuş basıldı ----
        else if (!strcmp(event, "DTMF")) {
            if (callState == IN_CALL) {
                dtmfIsle(detail[0]);
            }
        }

        // ---- HANGUP — karşı taraf kapattı ----
        else if (!strcmp(event, "HANGUP")) {
            if (callState != IDLE) {
                Serial.println(F("[HANG] Arama kapandi"));
                callState = IDLE;
            }
        }
    }

    // RINGING: CLIP gelmeden timeout — YETKILI boşsa yine de yanıtla
    if (callState == RINGING && !clipGeldi &&
        (millis() - callTimer > CLIP_TIMEOUT_MS)) {
        if (strlen(YETKILI) == 0) {
            Serial.println(F("  (CLIP gelmedi, yine de yanitlaniyor)"));
            gsm.callAnswer();
            callState = IN_CALL;
            callTimer = millis();
        } else {
            // Yetkili numara bekleniyor ama CLIP gelmedi
            Serial.println(F("  (CLIP gelmedi, yetkili mod → yoksayildi)"));
            callState = IDLE;
        }
    }

    // IN_CALL: maksimum süre doldu → otomatik kapat
    if (callState == IN_CALL &&
        (millis() - callTimer > MAKS_CAGRI_MS)) {
        Serial.println(F("[AUTO] Maks sure doldu, arama kapatiliyor"));
        gsm.callHangup();
        callState = IDLE;
    }
}