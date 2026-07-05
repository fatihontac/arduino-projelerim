/*
 * 07_SmsRelayControl.ino - SMS ile Röle Kontrolü
 *
 * SIM800C GSM Shield + 4 Kanallı Röle Shield
 *
 * SMS Komutları:
 *   R1 AC        → Röle 1 aç   (veya: ROLE1 AC)
 *   R1 KAPAT     → Röle 1 kapat (veya: ROLE1 KAPAT)
 *   R2 AC / R2 KAPAT
 *   R3 AC / R3 KAPAT
 *   R4 AC / R4 KAPAT
 *   HEPSI AC     → 4 röleyi birden aç
 *   HEPSI KAPAT  → 4 röleyi birden kapat
 *   DURUM        → Tüm röle durumunu SMS ile bildir
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
 * Github: https://github.com/hiberblsm/arduino-uno-gsm-shield
 * Geliştirici: Hiber Bilişim - https://www.hiber.com.tr
 */

#include "SIM800C.h"

// ============================================================
//  YAPILANDIRMA - sadece burası değiştirilir
// ============================================================

// Röle pin numaraları
#define R1_PIN  4
#define R2_PIN  5
#define R3_PIN  6
#define R4_PIN  7

// Röle shield tipi:
//   RELAY_ACTIVE_HIGH (varsayılan) → pin HIGH olduğunda röle AÇILIR
//   Bu satırı silin veya yorum yapın → Active-LOW moda geçer (pin LOW = röle AÇIK)
#define RELAY_ACTIVE_HIGH

// Yetkili telefon numarası (güvenlik)
// Boş bırakırsan → herkesten komut kabul edilir
// Dolu olursa → sadece bu numaradan gelen SMS işleme alınır
#define YETKILI  "+905416932009"     // örn: "+905XXXXXXXXX"

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

// ============================================================
//  SMS FONKSİYONLARI
// ============================================================

void durumSmsSend(const char *numara) {
    char msg[80];
    snprintf(msg, sizeof(msg),
        "Durum:\nR1:%s R2:%s\nR3:%s R4:%s",
        rolAcik[0] ? "AC " : "KPL",
        rolAcik[1] ? "AC " : "KPL",
        rolAcik[2] ? "AC " : "KPL",
        rolAcik[3] ? "AC " : "KPL"
    );
    gsm.smsSend(numara, msg);
    Serial.println(F("  [SMS] Durum gonderildi"));
}

void komutIsle(const char *gonderen, const char *komut) {
    Serial.print(F("[SMS ALINDI] "));
    Serial.print(gonderen);
    Serial.print(F(" -> \""));
    Serial.print(komut);
    Serial.println('"');

    // Yetki kontrolü
    if (strlen(YETKILI) > 3 && strcmp(gonderen, YETKILI) != 0) {
        Serial.println(F("  [RED] Yetkisiz numara, komut yoksayildi"));
        return;
    }

    static const char ROK[]  PROGMEM = "OK";
    const char *yanit = NULL;

    // Komut eşleştirme (hem kısa R1/R2 hem uzun ROLE1/ROLE2 destekli)
    if      (!strcmp_P(komut, PSTR("R1 AC"))      || !strcmp_P(komut, PSTR("ROLE1 AC")))    { roleSet(0, true);  yanit = "Role 1 ACILDI"; }
    else if (!strcmp_P(komut, PSTR("R1 KAPAT"))   || !strcmp_P(komut, PSTR("ROLE1 KAPAT"))) { roleSet(0, false); yanit = "Role 1 KAPATILDI"; }
    else if (!strcmp_P(komut, PSTR("R2 AC"))      || !strcmp_P(komut, PSTR("ROLE2 AC")))    { roleSet(1, true);  yanit = "Role 2 ACILDI"; }
    else if (!strcmp_P(komut, PSTR("R2 KAPAT"))   || !strcmp_P(komut, PSTR("ROLE2 KAPAT"))) { roleSet(1, false); yanit = "Role 2 KAPATILDI"; }
    else if (!strcmp_P(komut, PSTR("R3 AC"))      || !strcmp_P(komut, PSTR("ROLE3 AC")))    { roleSet(2, true);  yanit = "Role 3 ACILDI"; }
    else if (!strcmp_P(komut, PSTR("R3 KAPAT"))   || !strcmp_P(komut, PSTR("ROLE3 KAPAT"))) { roleSet(2, false); yanit = "Role 3 KAPATILDI"; }
    else if (!strcmp_P(komut, PSTR("R4 AC"))      || !strcmp_P(komut, PSTR("ROLE4 AC")))    { roleSet(3, true);  yanit = "Role 4 ACILDI"; }
    else if (!strcmp_P(komut, PSTR("R4 KAPAT"))   || !strcmp_P(komut, PSTR("ROLE4 KAPAT"))) { roleSet(3, false); yanit = "Role 4 KAPATILDI"; }
    else if (!strcmp_P(komut, PSTR("HEPSI AC")))    { for (uint8_t i = 0; i < 4; i++) roleSet(i, true);  yanit = "Tum roller ACILDI"; }
    else if (!strcmp_P(komut, PSTR("HEPSI KAPAT"))) { for (uint8_t i = 0; i < 4; i++) roleSet(i, false); yanit = "Tum roller KAPATILDI"; }
    else if (!strcmp_P(komut, PSTR("DURUM")))       { durumSmsSend(gonderen); return; }
    else                                            { yanit = "Bilinmeyen komut";
                                                      Serial.print(F("  [WARN] ")); Serial.println(komut); }

    // Onay SMS'i gönder
    if (yanit && strlen(gonderen) > 3) {
        gsm.smsSend(gonderen, yanit);
        Serial.print(F("  [SMS] Yanit: ")); Serial.println(yanit);
    }
}

// ============================================================
//  SETUP
// ============================================================

void setup() {
    roleInit();   // Önce röleleri kapat! (güç açılışında röle tetiklenmesin)
    Serial.begin(9600);
    delay(500);

    Serial.println(F("\n=============================="));
    Serial.println(F("  07 - SMS Role Kontrol"));
    Serial.println(F("  Hiber Bilisim - hiber.com.tr"));
    Serial.println(F("=============================="));
    Serial.println(F("Komutlar:  R1 AC / R1 KAPAT"));
    Serial.println(F("           R2 AC / R2 KAPAT"));
    Serial.println(F("           R3 AC / R3 KAPAT"));
    Serial.println(F("           R4 AC / R4 KAPAT"));
    Serial.println(F("           HEPSI AC / HEPSI KAPAT"));
    Serial.println(F("           DURUM"));

    // Modem başlat
    Serial.println(F("\nModem baslatiliyor..."));
    gsm.hardReset();

    // SIM kart hazır mı?
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

    // Eski SMS'leri temizle, yoksa SIM hafızası doluyor
    gsm.smsDeleteAll();

    // Gelen SMS bildirimi (+CMT: URC) aç
    gsm.smsSetIncoming(true);

    Serial.println(F("\n[HAZIR] SMS komut bekleniyor..."));
    if (strlen(YETKILI) > 3) {
        Serial.print(F("  Yetkili: "));
        Serial.println(F(YETKILI));
    } else {
        Serial.println(F("  (Yetki yok - herkese acik)"));
    }
}

// ============================================================
//  LOOP — heap kullanmaz, sonsuz çalışır
// ============================================================

void loop() {
    char gonderen[20];   // telefon numarası (+905XXXXXXXXX)
    char komut[32];      // mesaj gövdesi (büyük harf)

    // smsPoll: +CMT: URC gelirse true döner, char[] tamponlara yazar
    if (gsm.smsPoll(gonderen, sizeof(gonderen), komut, sizeof(komut))) {
        komutIsle(gonderen, komut);
        delay(500);
        gsm.smsDeleteAll();   // SIM hafızasını temizle
    }
}