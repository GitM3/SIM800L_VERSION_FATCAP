#include "config.h"

#include <hardware/pio.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include "SCD30.h"
#include "ArduinoJson.h"
#include <Seeed_HM330X.h>
#include <SensirionI2CSen5x.h>


TinyGsm       modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient  mqtt(client);

int batt_mv(){
 return analogRead(BATTERY_PIN)*(3300/4095) *((1+2.7)/2.7); 
}

SensirionI2CSen5x sen55;
void external_state(bool in){ //switches relay ON/OFF
    if(in) digitalWrite(RELAY_PIN, HIGH);
    else digitalWrite(RELAY_PIN,LOW);
}

String senor_json_data(){
    scd30.initialize();
    scd30.setAutoSelfCalibration(1);

    StaticJsonDocument<442> doc; //300+8+128 +8
    //batt
    doc["Battery_voltage"] = batt_mv();
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
    s_error = sen55.readMeasuredValues(
        massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
        massConcentrationPm10p0, ambientHumidity, ambientTemperature, vocIndex,
        noxIndex);

    if (s_error) {
        Serial.print("s_error trying to execute readMeasuredValues(): ");
        errorToString(s_error, errorMessage, 256);
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
    Wire.setSDA(SDA);
    Wire.setSCL(SCL);
    Wire.begin();
    sen55.begin(Wire);
    s_error = sen55.deviceReset();
    if (s_error) {
        Serial.print("Error trying to execute deviceReset(): ");
        errorToString(s_error, errorMessage, 256);
        Serial.println(errorMessage);
    }
    s_error = sen55.startMeasurement();
    if (s_error) {
        Serial.print("Error executing startMeasurement(): ");
        errorToString(s_error, errorMessage, 256);
        Serial.println(errorMessage);
    }
    Serial.println("init success");
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
    if (!mqtt.connect("SADASKHKHDADASD5514615616")) {
      return false;
    }
    mqtt.subscribe(SUBSCRIBE_TOPIC);
    Serial.println("CONNECTED");
  }
  return true;
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BATTERY_PIN, INPUT);
  pinMode(RST,OUTPUT);
  digitalWrite(RST,0);
  delay(1000);
  digitalWrite(RST,1);
  delay(1000);
  SerialMon.begin(115200);
  delay(4000);
  Serial2.setTX(TX);
  Serial2.setRX(RX);
  // sensor_setup();
  TinyGsmAutoBaud(SerialAT, 9600, 115200);
  delay(6000);
  SerialMon.println("Initializing modem...");
  modem.init();
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
      if(timerr < millis()){
        main_state = MAINSTATE::SEND;
        external_state(1);
        delay(1000);
        sensor_setup();
        timerr = millis() + WARMUP_INTERVAL;
        Serial.println("warming up");
      }
      break;
    }
    case MAINSTATE::SEND:{
      if(timerr < millis()){
        Serial.println("sending data");
        mqtt.publish(PUBLISH_TOPIC,senor_json_data().c_str());
        main_state= MAINSTATE::WARMUP;
        external_state(0);
        timerr = millis() + READING_INTERVAL;
        Serial.println("turning off sensors");
      }
      break;
    }
  }
} 
