#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include "Adafruit_SGP30.h"
#if defined(ESP32)
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"
#elif defined(ESP8266)
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#define DEVICE "Sensor_Joeran"
#endif
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

//
// Sensor Definition
//
Adafruit_SGP30 sgp;
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
  const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
  const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
  return absoluteHumidityScaled;
}
//
// Sensor Definition End
//

//
// LED
//
#define PIN 15
#define NUMPIXELS 1
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
//
// LED end
//

//
// Display definition
//
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
//
// Display definition end
//


//
// Wifi definition
//
#define WIFI_SSID "" // Never Publish this values
#define WIFI_PASSWORD "" // Never Publish this values
//
// Wifi definition End
//

//
// Influx DB Definition
//
#define INFLUXDB_URL "" // Never Publish this values
#define INFLUXDB_TOKEN "" // Never Publish this values
#define INFLUXDB_ORG "" // Never Publish this values
#define INFLUXDB_BUCKET "" // Never Publish this values
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
//
// Influx DB Definition End
//


InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

Point sensor("Co2_stats");

int sensorResetCounter = 0;
int sendDataToInfluxCounter = 0;

void setup() {
  // Setup Serial
  Serial.begin(115200);

  //Setup Wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  //Setup Influx Client
  sensor.addTag("device", DEVICE);
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  //Setup Display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.display();
  display.clearDisplay();
  pixels.begin();

  //Setup CO2 sensor
  if (! sgp.begin()) {
    Serial.println("Sensor not found :(");
    while (1);
  }
  Serial.print("Found SGP30 serial #");
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);

}

void loop() {
  readValuesFromSensor();

  String text = "";
  switch (sgp.eCO2) {
    case 0 ... 500:
      pixels.setPixelColor(0, pixels.Color(0, 0, 255));
      text = "Super";
      break;
    case 501 ... 1000:
      pixels.setPixelColor(0, pixels.Color(0, 255, 0));
      text = "Good";
      break;
    case 1001 ... 1500:
      pixels.setPixelColor(0, pixels.Color(255, 128, 0));
      text = "Moderate";
      break;
    default:
      pixels.setPixelColor(0, pixels.Color(255, 0, 0));
      text = "Critical";
      break;
  }
  pixels.show();
  
  displayText(sgp.eCO2, sgp.TVOC, text);

  writeInflux(sgp.eCO2, sgp.TVOC);

  recalibrateSensor();

  delay(1000);

}

void recalibrateSensor() {
  sensorResetCounter++;
  if (sensorResetCounter == 30) {
    sensorResetCounter = 0;

    uint16_t TVOC_base, eCO2_base;
    if (! sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
      Serial.println("Failed to get baseline readings");
      return;
    }
    //Serial.print("****Baseline values: eCO2: 0x"); Serial.print(eCO2_base, HEX);
    //Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base, HEX);
  }
}

void readValuesFromSensor() {
  if (! sgp.IAQmeasure()) {
    Serial.println("Measurement failed");
    return;
  }

  // Uncomment for debugging
  //Serial.print("TVOC "); Serial.print(sgp.TVOC); Serial.print(" ppb ");
  //Serial.print("eCO2 "); Serial.print(sgp.eCO2); Serial.println(" ppm");

  if (! sgp.IAQmeasureRaw()) {
    Serial.println("Raw Measurement failed");
    return;
  }
  // Uncomment for debugging
  //Serial.print("Raw H2 "); Serial.print(sgp.rawH2); Serial.print(" \t");
  //Serial.print("Raw Ethanol "); Serial.print(sgp.rawEthanol); Serial.println("");
}

void writeInflux(int co2, int TVOC) {
  sendDataToInfluxCounter++;
  if (sendDataToInfluxCounter == 10 ) {
    sendDataToInfluxCounter = 0;
    sensor.clearFields();
    sensor.addField("eCO2", co2);
    sensor.addField("VOC", TVOC);
    Serial.print("Writing: "); Serial.println(sensor.toLineProtocol());
    if ((WiFi.RSSI() == 0) && (wifiMulti.run() != WL_CONNECTED)) {
      Serial.println("Wifi connection lost");
    }
    if (!client.writePoint(sensor)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());
    }
  }
}


void displayText(int co2, int voc, String text) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.drawLine(0, 15, 128, 15, WHITE);
  display.drawLine(0, 16, 128, 16, WHITE);
  display.setCursor(5, 0);
  display.println(F("Air Quali"));
  display.setTextSize(1);
  display.setCursor(1, 20);
  display.println("eCO2: " + String(co2));
  display.setCursor(1, 28);
  display.println("VOC: " + String(voc));
  display.setTextSize(3);
  display.setCursor(0, 40);
  display.println(text);
  display.display();
}
