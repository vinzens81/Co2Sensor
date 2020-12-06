#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include "MHZ19.h"
#include <Arduino.h>
#include <SoftwareSerial.h>
#include "FS.h"



#define DHTPIN 3
#define RX_PIN D7
#define TX_PIN D6
#define DHTTYPE    DHT11
MHZ19 myMHZ19;
SoftwareSerial mySerial(RX_PIN, TX_PIN);
#define BAUDRATE 9600                                      // Native to the sensor (do not change)

DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;


unsigned long getDataTimer = 0;


#if defined(ESP32)
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"
#elif defined(ESP8266)
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#include "settings.h"



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


InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

Point sensor("Co2_stats");

int sensorResetCounter = 0;
int histSensor1 = 0;
int histSensor2 = 0;
int histSensor3 = 0;
int histCounter = 0;
int sendDataToInfluxCounter = 0;

void verifyRange(int range);

void setup() {
  // Setup Serial
  Serial.begin(115200);
  //Setup Display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.display();
  display.clearDisplay();
  displaySimpleText("Setup Wifi");

  //Setup Wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to wifi");
  int i = 1;
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    displaySimpleText("Setup Wifi " + String(i));
    i++;
    delay(100);
    if (i > 100) {
      displaySimpleText("Cannot Connect to Wifi!");
      delay(5000);

    }
  }
  Serial.println();

  //Setup Influx Client
  sensor.addTag("device", DEVICE);
  displaySimpleText("Sync time...");

  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  if (client.validateConnection()) {
    displaySimpleText("Connecting Influx: " + client.getServerUrl());
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    displaySimpleText("Connecting Influx failed!");
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  delay(500);
  pixels.begin();
  mySerial.begin(BAUDRATE);                               // Uno example: Begin Stream with MHZ19 baudrate

  myMHZ19.begin(mySerial);
  myMHZ19.setRange(2000);
  myMHZ19.calibrateZero();
  myMHZ19.setSpan(2000);
  myMHZ19.autoCalibration(false);

  setupHumiditySensor();

}


void setupHumiditySensor() {
  dht.begin();
  Serial.println(F("DHTxx Unified Sensor Example"));
  // Print temperature sensor details.
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  Serial.println(F("------------------------------------"));
  Serial.println(F("Temperature Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("°C"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("°C"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("°C"));
  Serial.println(F("------------------------------------"));
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  Serial.println(F("Humidity Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("%"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("%"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("%"));
  Serial.println(F("------------------------------------"));
  // Set delay between sensor readings based on sensor details.
  delayMS = sensor.min_delay / 1000;

}


void displaySimpleText(String text) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(text);
  display.display();

}


void loop() {
  delay(2000);
  double co2ppm;
  co2ppm = readValuesCo2ppm();
  Serial.print("Co2: "); Serial.println(co2ppm);

  sensors_event_t event;
  double humidity;
  humidity = getHumidity(event);
  Serial.print("Humidity: "); Serial.println(humidity);
  double temperatur;
  temperatur = getTemperatur(event);
  Serial.print("Temperatur: "); Serial.println(temperatur);

  int ppm = int(co2ppm);
  String text = "";
  switch (ppm) {
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

  writeInflux(co2ppm, temperatur, humidity);
  displayText(co2ppm, temperatur, humidity, text);
  if (WiFi.status() != WL_CONNECTED) {
    ReconnectWifi();
  }

}

double getHumidity(sensors_event_t event) {
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Error reading humidity!"));
  }
  else {
    return (event.relative_humidity);
  }
}

void ReconnectWifi() {
  int wifi_retry = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_retry < 5 ) {
    wifi_retry++;
    Serial.println("WiFi not connected. Try to reconnect");
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(100);
  }
  if (wifi_retry >= 5) {
    Serial.println("\nReboot");
    ESP.restart();
  }
}

double getTemperatur(sensors_event_t event) {
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println(F("Error reading temperature!"));
  }
  else {
    return (event.temperature);
  }
}

void writeInflux(double co2ppm, double temperatur, double humidity) {
  sendDataToInfluxCounter++;
  if (sendDataToInfluxCounter == 10 ) {
    sendDataToInfluxCounter = 0;
    sensor.clearFields();
    sensor.addField("co2", co2ppm);
    sensor.addField("temp", temperatur);
    sensor.addField("hum", humidity);
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


void displayText(double co2ppm, double temperatur, double humidity, String text) {
  String title = DEVICE;
  title.remove(9);
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.drawLine(0, 15, 128, 15, WHITE);
  display.drawLine(0, 16, 128, 16, WHITE);
  display.setCursor(5, 0);
  display.println(title);
  display.setTextSize(1);
  display.setCursor(1, 20);
  display.println("CO2: " + String(co2ppm));
  display.setCursor(1, 28);
  display.println("Temp: " + String(int(temperatur)) + " humid: " + String(int(humidity)));
  display.setTextSize(3);
  display.setCursor(0, 40);
  display.println(text);
  display.display();
}


double readValuesCo2ppm() {
  if (millis() - getDataTimer >= 2000)
  {
    //    double adjustedCO2 = myMHZ19.getCO2Raw();
    //    adjustedCO2 = 6.60435861e+15 * exp(-8.78661228e-04 * adjustedCO2);      // Exponential equation for Raw & CO2 relationship
    //    double foo = (adjustedCO2);
    //    //Serial.print(foo);
    //    getDataTimer = millis();

    int CO2;
    CO2 = myMHZ19.getCO2();
    if (CO2 == 0) {
      Serial.println("\nReboot");
      displaySimpleText("Sensor Filure, neet to reset");
      delay(1000);
      ESP.restart();
    }

    if (isValueNotChaning(CO2)) {
      Serial.println("\nReboot");
      displaySimpleText("Sensor Filure, neet to reset");
      delay(1000);
      ESP.restart();      
    }
    return (CO2);
  }
}

boolean isValueNotChaning(int CO2){
  histCounter++;
  if (histCounter == 10) {
    histCounter = 0;
    histSensor3 = histSensor2;
    histSensor2 = histSensor1;
    histSensor1 = CO2;
    Serial.print("Readings: co2:");
    Serial.print(CO2);
    Serial.print(", old1: ");
    Serial.print(histSensor1);
    Serial.print(", old2: ");
    Serial.print(histSensor2);
    Serial.print(", old3: ");
    Serial.print(histSensor3);
    Serial.println();
    if (CO2 == histSensor1 && histSensor1 == histSensor2 && histSensor2 == histSensor3) {
      return true;
    }
  }
  return false;
}

void verifyRange(int range)
{
  Serial.println("Requesting new range.");

  myMHZ19.setRange(range);                             // request new range write

  if (myMHZ19.getRange() == range)                     // Send command to device to return it's range value.
    Serial.println("Range successfully applied.");   // Success

  else
    Serial.println("Failed to apply range.");        // Failed
}
