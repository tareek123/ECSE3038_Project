
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino.h>
#include<ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
//#include "env.h"
#define ONE_WIRE_BUS 4
 

const char* ssid = "DESKTOP-HVF4QBB 9884";
const char* password = "5007x-K5";
const char* serverName = "https://Tariq-iot-api.com";

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
const int Motor = 23;
const int PIR_motion_PIN = 15;  // PIR sensor output pin
const int LED_PIN = 22;  // LED pin
int warm_up;

float temp=0;
bool motion;
void postrequest();

void setup() {
  Serial.begin(9600); //Begin serial communication
  // Set the LED pin as output
  pinMode(PIR_motion_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(Motor, OUTPUT);

  Serial.println("Ready Serial Monitor Version"); //Print a message
  sensors.begin();
    // Connect to Wi-Fi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  sensors.begin();
}

void loop() {
  // Turn on the Motor
  digitalWrite(Motor, HIGH);
  delay(10000); // Wait for 1 second

  // Turn off the motor
  digitalWrite(Motor, LOW);
  delay(1000); // Wait for 1 second


   // Send the command to get temperatures
  sensors.requestTemperatures();  
  Serial.print("Temperature is: ");
  temp = sensors.getTempCByIndex(0);
  Serial.println(temp); // Why "byIndex"? You can have more than one IC on the same bus. 0 refers to the first IC on the wire
  //Update value every 1 sec.
  delay(1000);

//pir
 
  motion = digitalRead(PIR_motion_PIN);

   postrequest();
}

void postrequest(){
   HTTPClient http;
    http.begin("https://nileiotproject.onrender.com/api/state");
    http.addHeader("Content-Type", "application/json");

     StaticJsonDocument<1024> doc;
     String httpRequestData;

     doc["temperature"]= temp;
     doc["presence"]= motion;

     serializeJson(doc, httpRequestData);

    int httpResponseCode = http.POST(httpRequestData);

    String http_response;

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }

    http.end();

}