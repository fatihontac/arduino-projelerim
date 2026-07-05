#include <SoftwareSerial.h>
SoftwareSerial gsm(4, 5); // RX, TX

void setup() {
  Serial.begin(9600);
  gsm.begin(9600);
  delay(2000);
  Serial.println("AT testi basladi...");
  gsm.println("AT");
}

void loop() {
  if (gsm.available()) {
    Serial.write(gsm.read());
  }
}