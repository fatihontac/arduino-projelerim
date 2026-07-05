/*
 * 01_SerialTest.ino
 * Arduino UNO + SIM800C — Serial / AT Komut Testi
 * 
 * Bu sketch SIM800C modülünün temel fonksiyonlarını test eder:
 *   - AT komut yanıtı
 *   - IMEI / IMSI bilgileri
 *   - Sinyal gücü (CSQ)
 *   - SIM kart durumu
 *   - Operatör bilgisi
 *   - Network kayıt durumu
 * 
 * Bağlantılar:
 *   Arduino Pin 2 -> SIM800C TX
 *   Arduino Pin 3 -> SIM800C RX
 *   Arduino Pin 9 -> SIM800C Boot/Power Key
 * 
 * Serial Monitor: 9600 baud
 */

#include <SIM800C.h>

SIM800C gsm(2, 3, 9);  // TX, RX, Boot

void printSeparator() {
    Serial.println(F("========================================"));
}

void setup() {
    Serial.begin(9600);
    while (!Serial);

    printSeparator();
    Serial.println(F("  Arduino UNO + SIM800C Serial Test"));
    Serial.println(F("  HiberSoft GSM Shield Library"));
    printSeparator();
    Serial.println();

    // Modemi başlat
    Serial.println(F("[TEST] Modem baslatiliyor..."));
    if (!gsm.begin()) {
        Serial.println(F("[FAIL] Modem baslatma hatasi!"));
        Serial.println(F("  -> Kablo baglantilarini kontrol edin"));
        Serial.println(F("  -> SIM800C guc kaynagini kontrol edin"));
        while (1) delay(1000);
    }
    Serial.println(F("[PASS] Modem hazir"));
    Serial.println();

    // ===== TEST 1: IMEI =====
    Serial.print(F("[TEST] IMEI: "));
    String imei = gsm.getIMEI();
    if (imei.length() > 0) {
        Serial.println(imei);
        Serial.println(F("[PASS] IMEI okundu"));
    } else {
        Serial.println(F("[FAIL] IMEI okunamadi"));
    }
    Serial.println();

    // ===== TEST 2: IMSI =====
    Serial.print(F("[TEST] IMSI: "));
    String imsi = gsm.getIMSI();
    if (imsi.length() > 0) {
        Serial.println(imsi);
        Serial.println(F("[PASS] IMSI okundu"));
    } else {
        Serial.println(F("[FAIL] IMSI okunamadi (SIM kart var mi?)"));
    }
    Serial.println();

    // ===== TEST 3: SIM Durumu =====
    Serial.print(F("[TEST] SIM Durumu: "));
    String simStatus = gsm.getSIMStatus();
    Serial.println(simStatus);
    if (simStatus == "READY") {
        Serial.println(F("[PASS] SIM kart hazir"));
    } else {
        Serial.print(F("[WARN] SIM kart durumu: "));
        Serial.println(simStatus);
    }
    Serial.println();

    // ===== TEST 4: Sinyal Gücü =====
    Serial.print(F("[TEST] Sinyal Gucu (CSQ): "));
    int csq = gsm.getSignalQuality();
    Serial.println(csq);
    if (csq > 0 && csq < 99) {
        int rssi = -113 + (csq * 2);
        Serial.print(F("  RSSI: "));
        Serial.print(rssi);
        Serial.println(F(" dBm"));

        if (csq > 20)      Serial.println(F("  Kalite: MUKEMMEL"));
        else if (csq > 15)  Serial.println(F("  Kalite: IYI"));
        else if (csq > 10)  Serial.println(F("  Kalite: ORTA"));
        else                 Serial.println(F("  Kalite: ZAYIF"));

        Serial.println(F("[PASS] Sinyal olculdu"));
    } else {
        Serial.println(F("[FAIL] Sinyal alinamiyor"));
    }
    Serial.println();

    // ===== TEST 5: Operatör =====
    Serial.print(F("[TEST] Operator: "));
    String op = gsm.getOperator();
    if (op.length() > 0) {
        Serial.println(op);
        Serial.println(F("[PASS] Operator bilgisi alindi"));
    } else {
        Serial.println(F("[FAIL] Operator bilgisi alinamadi"));
    }
    Serial.println();

    // ===== TEST 6: Network Kayıt =====
    Serial.print(F("[TEST] Network Kayit: "));
    if (gsm.isRegistered()) {
        Serial.println(F("KAYITLI"));
        Serial.println(F("[PASS] Network kaydi basarili"));
    } else {
        Serial.println(F("KAYITSIZ"));
        Serial.println(F("[FAIL] Network kaydi yok"));
    }
    Serial.println();

    // ===== SONUÇ =====
    printSeparator();
    Serial.println(F("  Tum testler tamamlandi!"));
    printSeparator();
}

void loop() {
    // Serial Monitor'dan AT komutu gönderme (interaktif mod)
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.length() > 0) {
            Serial.print(F("> "));
            Serial.println(cmd);
            String resp = gsm.sendAT(cmd, 5000);
            Serial.println(resp);
        }
    }
}