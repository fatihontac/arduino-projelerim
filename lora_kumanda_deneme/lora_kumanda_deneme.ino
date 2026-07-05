#include <SoftwareSerial.h>

SoftwareSerial mySerial(9, 8); // RX, TX (V3)

#define M0 7
#define M1 6 

#define IN1 2   // Kumanda giriş pini
#define IN2 3   // Kumanda giriş pini

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);

  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  digitalWrite(M0, LOW);
  digitalWrite(M1, HIGH);

  pinMode(IN1, INPUT);
  pinMode(IN2, INPUT);
}

void loop() {
  if (Serial.available()) {
    mySerial.write(Serial.read());
  }

  if (mySerial.available()) {
    Serial.write(mySerial.read());
  }

  // Kumanda girişlerini kontrol et
  if (digitalRead(IN1) == HIGH) {
    mySerial.println("OFF1"); // Röle1 LOW komutu
  }
  if (digitalRead(IN2) == HIGH) {
    mySerial.println("OFF2"); // Röle2 LOW komutu
  }
}
