//Verici
#include <LoRa_E22.h>
#include <SoftwareSerial.h>
SoftwareSerial mySerial(9, 8);
LoRa_E22 E22(&mySerial);
#define M0 7
#define M1 6
struct veriler {   
 char deger[15];
} data;
void setup() {
  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  digitalWrite(M0, LOW);
  digitalWrite(M1, LOW);
  Serial.begin(9600);
  E22.begin();
  delay(500);
}
void loop() {
  sprintf(data.deger,"Merhaba");
  ResponseStatus rs = E22.sendFixedMessage(0, 2, 18, &data, sizeof(veriler));
  Serial.println(rs.getResponseDescription());
  delay(1000);
}