#include <SoftwareSerial.h>
SoftwareSerial mySerial(1, 0); // RX, TX
int IC_ON = 5;
void setup() {
 pinMode(IC_ON, OUTPUT);
 digitalWrite(IC_ON, HIGH);
 // Open serial communications and wait for port to open:
 Serial.begin(9600);
 while (!Serial) {
 ; // wait for serial port to connect. Needed for native USB port only
 }
 // set the data rate for the SoftwareSerial port
 mySerial.begin(9600);
}
void loop() { // run over and over
 if (mySerial.available()) {
 Serial.write(mySerial.read());
 delay(1);
 }
 delay(1);
 if (Serial.available()) {
 mySerial.write(Serial.read());
 delay(1);
 }
}