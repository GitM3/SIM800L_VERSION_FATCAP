#include "config.h"
 
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include "SCD30.h"
#include "ArduinoJson.h"
#include <Seeed_HM330X.h>
#include <SensirionI2CSen5x.h>


TinyGsm        modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient  mqtt(client);

SensirionI2CSen5x sen55;
void external_state(bool in){ //switches relay ON/OFF
    if(in) digitalWrite(RELAY_PIN, HIGH);
    else digitalWrite(RELAY_PIN,LOW);
}

String senor_json_data(){
    scd30.initialize();
    scd30.setAutoSelfCalibration(1);

    StaticJsonDocument<448> doc; //300+8+128 + 12, see: https://arduinojson.org/v6/assistant/
    doc["ID"] = DEVICE_ID;
    //scd30 data
    float result[3] = {0};
    if (scd30.isAvailable()) {
        scd30.getCarbonDioxideConcentration(result);   
        doc["carbon_dioxide"] = result[0];
        doc["Temperature"] = result[1];
        doc["Humidity"] = result[2];   
    }
    //scd30 data end
    //SEN55 data start
    error = sen55.readMeasuredValues(
        massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
        massConcentrationPm10p0, ambientHumidity, ambientTemperature, vocIndex,
        noxIndex);

    if (error) {
        Serial.print("Error trying to execute readMeasuredValues(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else {
        doc["MassConcentrationPm1p0"]=massConcentrationPm1p0;
        doc["MassConcentrationPm2p5"]=massConcentrationPm2p5;
        doc["MassConcentrationPm4p0"]=massConcentrationPm4p0;
        doc["MassConcentrationPm10p0"]=massConcentrationPm10p0;
        if (isnan(ambientHumidity)) {
            doc["AmbientHumidity"]="n/a";
        } else {
            doc["AmbientHumidity"]=(ambientHumidity);
        }
        if (isnan(ambientTemperature)) {
            doc["AmbientTemperature"]="n/a";
        } else {
            doc["AmbientTemperature"]=ambientTemperature;
        }
        if (isnan(vocIndex)) {
            doc["VocIndex"]="n/a";
        } else {
            doc["VocIndex"]=vocIndex;
        }
        if (isnan(noxIndex)) {
            doc["NoxIndex"]="n/a";
        } else {
            doc["NoxIndex"]=noxIndex;
        }
    }
    
    //SEN55 data end
    
    String data;
    serializeJson(doc,data);

    return data;
}

void sensor_setup(){
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(BATTERY_PIN, INPUT);
    Wire.begin();
    sen55.begin(Wire);
    error = sen55.deviceReset();
    if (error) {
        Serial.print("Error trying to execute deviceReset(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }
    error = sen55.startMeasurement();
    if (error) {
        Serial.print("Error executing startMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }
}


void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg;
  for(int i = 0; i < len; i++) {
    char c = (char)payload[i];
    msg += c;
  }

  Serial.print("Recived ");
  Serial.println(msg);
}

boolean network_connect(){
  if (!modem.isNetworkConnected()) {
    SerialMon.println("Connecting to network");
    if (!modem.waitForNetwork(180000L, true)) {
      return false;
    }
  }
  if (!modem.isGprsConnected()) {
    SerialMon.println("Connecting to "+ String(APN));
    if (!modem.gprsConnect(APN,"","")) {
      return false;
    }
  }
  if(!mqtt.connected()){
    SerialMon.println("Connecting to "+ String(BROKER));
    if (!mqtt.connect(TOKEN)) {
      return false;
    }
    mqtt.subscribe(SUBSCRIBE_TOPIC);
    Serial.println("CONNECTED");
  }
  return true;
}

void setup() {
  SerialMon.begin(115200);
  delay(1000);
  sensor_setup();
  TinyGsmAutoBaud(SerialAT, 9600, 115200);
  delay(6000);
  SerialMon.println("Initializing modem...");
  modem.restart((char*)14);
  mqtt.setServer(BROKER, PORT);
  mqtt.setCallback(mqttCallback);
}

enum MAINSTATE{WARMUP,SEND};
MAINSTATE main_state = MAINSTATE::WARMUP;
unsigned long int timerr = 0;
void loop() {
  if(!network_connect()) return;
  
  mqtt.loop();
  switch (main_state)
  {
    case MAINSTATE::WARMUP:{
      if(timerr > millis()){
        main_state = MAINSTATE::SEND;
        external_state(1);
        timerr = millis() + WARMUP_INTERVAL;
      }
      break;
    }
    case MAINSTATE::SEND:{
      if(timerr > millis()){
        mqtt.publish(PUBLISH_TOPIC,senor_json_data().c_str());
        main_state= MAINSTATE::WARMUP;
        external_state(0);
        timerr = millis() + READING_INTERVAL;
      }
      break;
    }
  }
} 