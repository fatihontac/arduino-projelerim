#include <SoftwareSerial.h>

SoftwareSerial mySerial(9, 8); // RX, TX (V3)

#define M0 7
#define M1 6 

#define RELAY1 10
#define RELAY2 11

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);

  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  digitalWrite(M0, LOW);
  digitalWrite(M1, HIGH);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);

  // Röleler başlangıçta HIGH (açık) olabilir, senin devrene göre ayarla
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
}

void loop() {
  if (Serial.available()) {
    mySerial.write(Serial.read());
  }

  if (mySerial.available()) {
    String cmd = mySerial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "OFF1") {
      digitalWrite(RELAY1, LOW); // Röle1 kapat
      Serial.println("Röle1 LOW oldu");
    }
    if (cmd == "OFF2") {
      digitalWrite(RELAY2, LOW); // Röle2 kapat
      Serial.println("Röle2 LOW oldu");
    }
  }
}
