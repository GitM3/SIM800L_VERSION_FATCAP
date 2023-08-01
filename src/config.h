#include <Arduino.h>


#define TINY_GSM_MODEM_SIM868
#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_DEBUG SerialMon

#define DEVICE_ID "SENSOR STATION XXXX" //Update
#define PUBLISH_TOPIC "0001/data"      //Update

#define SUBSCRIBE_TOPIC "0001/data"
#define SUBSCRIBE_RESPOND_TOPIC "0001/data"

#define APN "Vodacom"
#define BROKER "test.mosquitto.org" //update
#define PORT 1883
#define TOKEN "secret"
#define APP_VERSION "ccfccnadblapdp7slkg0-v1"

#define RELAY_PIN 0
#define BATTERY_PIN 0
#define READING_INTERVAL 30000
#define WARMUP_INTERVAL 2*1000*60

float_t batt_v = 0;
float_t err = 0;
float massConcentrationPm1p0;
float massConcentrationPm2p5;
float massConcentrationPm4p0;
float massConcentrationPm10p0;
float ambientHumidity;
float ambientTemperature;
float vocIndex;
float noxIndex;

uint16_t error;
char errorMessage[256];