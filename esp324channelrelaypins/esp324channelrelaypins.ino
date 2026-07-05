#include <Arduino.h>

static const bool RELAY_ACTIVE_HIGH = true;

// Tahmini 4 röle pinleri
static const uint8_t RELAY_PINS[4] = {32, 33, 25, 26};

// Taranacak güvenli ESP32 çıkış pinleri
static const uint8_t FULL_SCAN_PINS[] = {
  25, 26, 32, 33
};

bool relayStates[4] = {false, false, false, false};

uint8_t relaySignal(bool on) {
  if (RELAY_ACTIVE_HIGH) {
    return on ? HIGH : LOW;
  } else {
    return on ? LOW : HIGH;
  }
}

const char* relayStateText(bool on) {
  return on ? "ACIK" : "KAPALI";
}

void setRelay(uint8_t relayIndex, bool on) {
  if (relayIndex >= 4) return;

  digitalWrite(RELAY_PINS[relayIndex], relaySignal(on));

  Serial.print("Role ");
  Serial.print(relayIndex + 1);
  Serial.print(" -> GPIO");
  Serial.print(RELAY_PINS[relayIndex]);
  Serial.print(" -> ");
  Serial.println(relayStateText(on));
}

void autoRelayTest() {
  Serial.println();
  Serial.println("=== OTOMATIK ROLE TESTI BASLADI ===");

  for (uint8_t i = 0; i < 4; i++) {
    Serial.print("Simdi Role ");
    Serial.print(i + 1);
    Serial.print(" test ediliyor. Pin: GPIO");
    Serial.println(RELAY_PINS[i]);

    setRelay(i, true);
    delay(1500);
    setRelay(i, false);
    delay(700);
  }

  Serial.println("=== OTOMATIK ROLE TESTI BITTI ===");
  Serial.println();
}

void fullScanPins() {
  Serial.println("=== TAM GPIO TARAMASI BASLADI ===");
  Serial.println("Her pin tek tek aktif edilir ve Serial Monitor'a yazdirilir.");
  Serial.println("Not: GPIO34, 35, 36, 39 giristir. GPIO6-11 flash icin kullanildigindan taranmiyor.");
  Serial.println();

  for (uint8_t i = 0; i < sizeof(FULL_SCAN_PINS); i++) {
    uint8_t pin = FULL_SCAN_PINS[i];
    pinMode(pin, OUTPUT);
    digitalWrite(pin, relaySignal(false));
  }

  for (uint8_t i = 0; i < sizeof(FULL_SCAN_PINS); i++) {
    uint8_t pin = FULL_SCAN_PINS[i];

    Serial.print("[FULL SCAN] GPIO");
    Serial.print(pin);
    Serial.println(" -> ACIK");
    digitalWrite(pin, relaySignal(true));
    delay(900);

    Serial.print("[FULL SCAN] GPIO");
    Serial.print(pin);
    Serial.println(" -> KAPALI");
    digitalWrite(pin, relaySignal(false));
    delay(350);
  }

  Serial.println();
  Serial.println("=== TAM GPIO TARAMASI BITTI ===");
  Serial.println();
}

void printHelp() {
  Serial.println("Komutlar:");
  Serial.println("1 -> Role 1 AC/KAPAT");
  Serial.println("2 -> Role 2 AC/KAPAT");
  Serial.println("3 -> Role 3 AC/KAPAT");
  Serial.println("4 -> Role 4 AC/KAPAT");
  Serial.println("f -> Tum GPIO taramasini tekrar baslat");
  Serial.println("h -> Yardimi yazdir");
  Serial.println();
}

void toggleRelay(uint8_t relayIndex) {
  relayStates[relayIndex] = !relayStates[relayIndex];
  setRelay(relayIndex, relayStates[relayIndex]);
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("ESP32 4 Kanal Role Pin Test Basladi");
  Serial.println("Serial Monitor hizi: 115200");
  Serial.println("Acilista otomatik role testi ve tam GPIO taramasi yapilacak.");
  Serial.println();

  for (uint8_t i = 0; i < 4; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], relaySignal(false));
  }

  printHelp();
  autoRelayTest();
  delay(1000);
  fullScanPins();
}

void loop() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  if (cmd == '1') {
    toggleRelay(0);
  } else if (cmd == '2') {
    toggleRelay(1);
  } else if (cmd == '3') {
    toggleRelay(2);
  } else if (cmd == '4') {
    toggleRelay(3);
  } else if (cmd == 'f' || cmd == 'F') {
    fullScanPins();
  } else if (cmd == 'h' || cmd == 'H') {
    printHelp();
  }
}
