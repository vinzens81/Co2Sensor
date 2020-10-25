# Co2Senser

# Libs
You need to install some Libs in Arduino editor in order to compile it

- Adafruit GFY Library
- Adafruit NeoPixel
- Adafruit SGP30 Sensor
- Adafruit SSD1306
- ESP8266 Influxdb

# Configs
In order to have the Board send Data to your Influxdb, you need to setup Wifi and influx Credentials in the Code. Please fill the `""` according to your setup.

Here the Code snippet you need to Change:
```
  53
  54 //
  55 // Wifi definition
  56 //
  57 #define WIFI_SSID "" // Never Publish this values
  58 #define WIFI_PASSWORD "" // Never Publish this values
  59 //
  60 // Wifi definition End
  61 //
  62
  63 //
  64 // Influx DB Definition
  65 //
  66 #define INFLUXDB_URL "" // Never Publish this values
  67 #define INFLUXDB_TOKEN "" // Never Publish this values
  68 #define INFLUXDB_ORG "" // Never Publish this values
  69 #define INFLUXDB_BUCKET "" // Never Publish this values
  70 #define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
  71 //
  72 // Influx DB Definition End
  73 //
  74

```

Never ever leak this information as it will grant access to your Wifi and Influx DB!
