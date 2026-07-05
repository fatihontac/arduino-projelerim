/***********************
 This sketch controls an electronic circuit that can sense 
 the environment using a sensor (electronic components that convert 
 real-world measurements into electrical signals). The device 
 processes the information from the sensors with the behavior 
 described in the sketch. The device will then be able to interact 
 with the world by using actuators, electronic components that can 
 convert an electric signal into physical action.
 
 EETechFix
***********************/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>

#define SCREEN_WIDTH 128 // dis play width, in pixels
#define SCREEN_HEIGHT 64 // display height, in pixels
#define OLED_RESET     4
#define SCREEN_ADDRESS 0x3C // OLED

#define DHTPIN 2     // DHT11 sensor is connected to pin 2
#define DHTTYPE DHT11   // DHT 11

DHT dht(DHTPIN, DHTTYPE);

const int sensorPin = A0;
const float alpha = 0.95; // Low Pass Filter alpha (0 - 1 )
float sensorVal;
float psiVal;
float barVal;
float temperature;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(9600);
  while (!Serial);

  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1); // Don't proceed, wait forever
  }
  display.clearDisplay();
  display.display();
}

void loop() {
  sensorVal = (float)analogRead(sensorPin); // Read pressure sensor val (A0)
  psiVal = (sensorVal / 1024.0) * 4.6; // calculate psi value
  psiVal = (psiVal - 0.4784) / 0.0401; // x=(y-b)/m
  barVal = psiToBar(psiVal); // Convert psi to bar

  temperature = dht.readTemperature(); // Read temperature from DHT11

  Serial.print("raw_adc "); Serial.print(sensorVal, 0);
  Serial.print(", bar "); Serial.println(barVal, 2);
  Serial.print(", temperature "); Serial.println(temperature, 1);

  updateOLED(barVal, temperature); // Pass bar and temperature values to updateOLED function

  delay(200);
}

float psiToBar(float psi) {
  return psi * 0.0689476;
}

void updateOLED(float barVal, float temperature) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);

  // Display "fatihontac" at the top
  display.setCursor(0, 0);
  display.print("fatihontac");

  // Display temperature
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print("Temperature: ");
  display.print(temperature, 1);
  display.print(" C");

  // Display bar value
  display.setTextSize(2);
  display.setCursor(0, 40);
  display.print(barVal, 2);
  display.print(" bar");

  display.display();
}