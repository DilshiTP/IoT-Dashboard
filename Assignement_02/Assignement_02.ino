#include <PubSubClient.h>
#include <WiFi.h> 
#include <RTClib.h>            //to get the current time
#include "DHTesp.h"            //to measure the temperature and humidity
#include <ESP32Servo.h>

#define Buzz 15                 // buzzer connected to the pin D15 of ESP32
int fre = 200;                    // buzzer frequency - can get from the dashboard , set default as 200Hz
const int DHT_PIN = 13;        // DHT sensor connected to the pin D13 of ESP32
const int servoPin = 18;       // servo motor connected to the pin D18 of ESP32
int sensor =34;                // LDR sensor connected to the pin D34 of ESP32
char State_Main = '0';       //main switch of scheduler status
int re_day = 0;                //remaining days of scheduler

const float GAMMA = 0.7;       // to calculate the light intensity from the analog read
const float RL10 = 50;         // to calculate the light intensity from the analog read

WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHTesp dhtSensor;
RTC_DS1307 rtc;
Servo servo;

char tempAr[6];            //arrays to collect the sensor readings-temperature,humidity,intensity
char humiAr[6];
char inteAr[6];


void setup() {
  Serial.begin(115200);
  setupWifi();
  setupMqtt();
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  servo.attach(servoPin, 500, 2400);

}

void loop() {
  if(!mqttClient.connected()){
    connectToBroker();
  }
  mqttClient.loop();

  //Publish temperatue,humidity and intensity to the node red dashboard

  updateTemperature();
  //Serial.print("Temperature "); Serial.println(tempAr);
  mqttClient.publish("Temperature_ENTC", tempAr);
  delay(1000);
  updateHumidity();
  //Serial.print("Humidity "); Serial.println(humiAr);
  mqttClient.publish("Humidity_ENTC", humiAr);
  delay(1000);
  updateIntensity();
  //Serial.print("Intensity "); Serial.println(inteAr);
  mqttClient.publish("Intensity_ENTC", inteAr);
  delay(1000);

}

void setupWifi(){
   WiFi.begin("Wokwi-GUEST", "");

   while (WiFi.status() != WL_CONNECTED){
     delay(500);
     Serial.print(".");
   }

   Serial.println("");
   Serial.println("WiFi connected");
   Serial.println("IP address: ");
   Serial.println(WiFi.localIP());
}

void setupMqtt(){
  mqttClient.setServer("test.mosquitto.org", 1883);
  mqttClient.setCallback(receiveCallback);
}

void connectToBroker(){
  while(!mqttClient.connected()){
    Serial.print("Attempting MQTT connection...");
    if(mqttClient.connect("ESP32-4567289826389")){
      Serial.println("connected");
      mqttClient.subscribe("Main-buzzer");               //subscribe each topics from nodered
      mqttClient.subscribe("A_1");                       //scheduler switch 1
      mqttClient.subscribe("A_2");                       //scheduler switch 2
      mqttClient.subscribe("A_3");                       //scheduler switch 3
      mqttClient.subscribe("Shade_ENTC");                //shade contoller
      mqttClient.subscribe("A1_T");                      //scheduler set time 1
      mqttClient.subscribe("A2_T");                      //scheduler set time 2
      mqttClient.subscribe("A3_T");                      //scheduler set time 3
      mqttClient.subscribe("Sch");                       //scheduler main switch
      mqttClient.subscribe("Day");                       //remaining days
      mqttClient.subscribe("FHz");                       //frequency of the buzzer
    
    
    }else{
      Serial.print("failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

//function to get the temperature values from the DHT sensor
void updateTemperature(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature,2).toCharArray(tempAr,6);
}

//function to get the humidity values from the DHT sensor
void updateHumidity(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.humidity,2).toCharArray(humiAr,6);
}

//function to get the intensity values from the DHT sensor
void updateIntensity(){
  int analogValue = analogRead(sensor);
  //float voltage = analogValue / 1024. * 5;         
  //float resistance = 2000 * voltage / (1 - voltage / 5);
  //float lux = 10*pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, (1 / GAMMA)); //convert analogread to lux value
  String(analogValue).toCharArray(inteAr,6);      //analogread max value = 4062, minimum value = 32
}

//Subscribe
void receiveCallback(char* topic,byte* payload, unsigned int length){
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");


  String msg = String((char)payload[0]);        
  Serial.print((char)payload[0]);
  for(int i= 1;i<length;i++){
    Serial.print((char)payload[i]);
    msg = msg + String((char)payload[i]);               //get the message from the dashboar as a string
  }

  Serial.println();

  //get the frequency from the dashboard
  if(strcmp(topic, "FHz") == 0){
    fre = msg.toInt();
  }

  //Main buzzer function
  if(strcmp(topic, "Main-buzzer") == 0){
    if(msg[0]=='1'){
      tone(Buzz, fre);
    }else{
      noTone(Buzz);
    }
  }

   //servo motor function
  if(strcmp(topic, "Shade_ENTC") == 0){
    int pos = 0;
    pos = msg.toInt();                              //get the angle as a integer
    servo.write(pos);
  }
  

  //get the state of the main switch of scheduler
  if (strcmp(topic, "Sch") == 0){
    if(msg[0]=='1'){
      State_Main = '1';
    }else{
      State_Main = '0';
    }
  }


  if(State_Main=='1'){                     //only main switch of scheduler state == 1 --> scheduler functions can be applied         
  //get the remaining days
  if(strcmp(topic, "Day") == 0){
    re_day = msg.toInt();
  }

  //Scheduler aram 1 function
  char hour_1 = 0;
  char min_1 = 0;
  if(strcmp(topic, "A1_T") == 0){               //get the alarm 1 time
    hour_1 = (msg[0]+msg[1]);
    min_1 = (msg[3]+msg[4]);
  }

  if(strcmp(topic, "A_1") == 0){
    if (msg[0]=='1'){
      DateTime now = rtc.now();                      //get the current time
      now = now + TimeSpan(0, 5, 30, 0);
      delay(1000);
      DateTime hour = now.hour();                    //get the hour and minutes of current time
      DateTime minute = now.minute();
      if (hour == hour_1 & minute == min_1 & re_day >0){
        tone(Buzz, fre);
      }else{
        noTone(Buzz);
      }
    }
  }
 
  //Scheduler aram 2 function
  char hour_2 = 0;
  char min_2 = 0;
  if(strcmp(topic, "A2_T") == 0){
    hour_2 = (msg[0]+msg[1]);
    min_2 = (msg[3]+msg[4]);
  }

  if(strcmp(topic, "A_2") == 0){
    if (msg[0]=='1'){
      DateTime now = rtc.now();                      //get the current time
      now = now + TimeSpan(0, 5, 30, 0);
      DateTime hour = now.hour();                    //get the hour and minutes of current time
      DateTime minute = now.minute();
      if (hour == hour_2 & minute == min_2 & re_day >0){
        tone(Buzz, fre);
      }else{
        noTone(Buzz);
      }
    }
  }

  //Scheduler aram 3 function
  char hour_3 = 0;
  char min_3 = 0;
  if(strcmp(topic, "A3_T") == 0){
    hour_3 = (msg[0]+msg[1]);
    min_3 = (msg[3]+msg[4]);
  }

  if(strcmp(topic, "A_3") == 0){
    if (msg[0]=='1'){
      DateTime now = rtc.now();                      //get the current time
      now = now + TimeSpan(0, 5, 30, 0);
      DateTime hour = now.hour();                    //get the hour and minutes of current time
      DateTime minute = now.minute();
      if (hour == hour_3 & minute == min_3 & re_day >0){
        tone(Buzz, fre);
      }else{
        noTone(Buzz);
      }
    }
  }
}
}
 

