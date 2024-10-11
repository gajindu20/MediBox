#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>

#define DHT_PIN 15
#define BUZZER 12
#define leftLDR 32 // Define the pin for the left LDR
#define rightLDR 33 // Define the pin for the right LDR

WiFiClient espClient; // Create a WiFi client object
PubSubClient mqttClient(espClient); // Create an MQTT client object

WiFiUDP ntpUDP; // Create a UDP object for NTP Client
NTPClient timeClient(ntpUDP); // Create an NTP client object

float MIN_ANGLE = 30; // Define default the minimum angle
float GAMMA = 0.75;  // Define the default gamma value
float intensity = 0.0;  // Initialize the light intensity
float D;  // Initialize the D value
char tempAr[6]; // Array to store temperature data
char lightAr[6]; // Array to store light intensity data
DHTesp dhtSensor;
Servo servo; // Create a servo object
const int servoPin = 18; // Define the pin for the servo motor


bool isScheduledON = false; // Initialize the scheduled ON state
unsigned long scheduledOnTime; // Initialize the scheduled ON time

void setup() {
  Serial.begin(115200);
  setupWifi();

  setupMqtt();

  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

  timeClient.begin(); // Start NTP client
  timeClient.setTimeOffset(5.5*3600); // Set time offset for NTP client
  pinMode(BUZZER, OUTPUT);
  pinMode(leftLDR, INPUT);
  pinMode(rightLDR, INPUT);
  pinMode(servoPin, OUTPUT); // Set servo pin as output
  digitalWrite(BUZZER, LOW); // Turn off buzzer if ON
  servo.attach(servoPin, 500, 2400); // Attach servo to pin

}

void loop() {
  if(!mqttClient.connected()){
    connectToBroker();                              // Connect to MQTT broker if not connected
  }
  mqttClient.loop();                                // Handle MQTT client loop
  updateTemperature();                              // Update temperature readings
  Serial.println(tempAr);
  mqttClient.publish("210213T-ADMIN-TEMP", tempAr); // Publish temperature to MQTT

  // Read analog values from LDRs
  int Left = analogRead(leftLDR);
  int Right = analogRead(rightLDR);
  //Serial.println(Left);
  //Serial.println(Right);

  // Scale analog values to a range of 0 to 1 for intensity calculation
  float leftValue = 1.0 - (map(Left, 32, 4063, 0, 1023) / 1023.0);
  float rightValue = 1.0 - (map(Right, 32, 4063, 0, 1023) / 1023.0);

  // Determine which side has higher light intensity
  bool isLeftHigher = leftValue > rightValue;

  // Publish light intensity and side to MQTT
  if (isLeftHigher) {
    String(leftValue, 2).toCharArray(lightAr, 6);
    mqttClient.publish("210213T-ADMIN-LIGHT", lightAr);
    mqttClient.publish("210213T-ADMIN-LIGHT-SIDE", "Left side");
    intensity = leftValue;
    D = 1.5; // Set D based on left side intensity
  } else {
    String(rightValue, 2).toCharArray(lightAr, 6);
    mqttClient.publish("210213T-ADMIN-LIGHT", lightAr);
    mqttClient.publish("210213T-ADMIN-LIGHT-SIDE", "Right side");
    intensity = rightValue;
    D = 0.5; // Set D based on right side intensity
  }
  checkSchedule(); // Check if there is a scheduled event
  delay(1000);     // Delay for stability
}

void setupWifi(){ 
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println("Wokwi-GUEST");
  WiFi.begin("Wokwi-GUEST","");  // Connect to WiFi network

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupMqtt(){
  mqttClient.setServer("test.mosquitto.org", 1883); // Set MQTT broker
  mqttClient.setCallback(recieveCallback);          // Set callback function
}

void connectToBroker(){
  while(!mqttClient.connected()){
    Serial.print("Attempting MQTT connection...");
    if(mqttClient.connect("ESP32-123456789")){
      Serial.println("connected");
      // Subscribe to MQTT topics
      mqttClient.subscribe("210213T-ADMIN-MAIN-ON-OFF");
      mqttClient.subscribe("210213T-ADMIN-SCH-ON");
      mqttClient.subscribe("210213T-ADMIN-SHADE-ANGLE");
      mqttClient.subscribe("210213T-ADMIN-SHADE-CONTROL");
      mqttClient.subscribe("210213T-ADMIN-LIGHT-INTENSITY");
    }else{
      Serial.print("failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void updateTemperature(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature,2).toCharArray(tempAr,6); // Convert temperature to char array
}

void buzzerOn(bool on){
  if (on){
    tone(BUZZER,256);
  }else{
    noTone(BUZZER);
  }
}

void recieveCallback(char* topic, byte* payload, unsigned int length){
  float angle;

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]");

  char payloadCharAr[length];
  for(int i = 0; i<length; i++){
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }

  Serial.println();
  
  if(strcmp(topic, "210213T-ADMIN-MAIN-ON-OFF")==0){
    buzzerOn(payloadCharAr[0]=='1');  // Control buzzer based on payload
  }else if(strcmp(topic, "210213T-ADMIN-SCH-ON")==0){
    if(payloadCharAr[0]=='N'){
      isScheduledON = false;
    }else{
      isScheduledON = true;
      scheduledOnTime = atol(payloadCharAr); // Set scheduled ON time
    }
  }
  if (strcmp(topic, "210213T-ADMIN-LIGHT-INTENSITY") == 0) {
    intensity = atof(payloadCharAr); // Update light intensity
    if (MIN_ANGLE != 0 && GAMMA!= 0) {
      angle = calculateMotorAngle(); // Calculate motor angle
      servo.write(angle);
    }
  }
  if (strcmp(topic, "210213T-ADMIN-SHADE-ANGLE") == 0) {
    MIN_ANGLE = atof(payloadCharAr); // Update minimum angle
    if (GAMMA != 0 && intensity != 0) {
      angle = calculateMotorAngle(); // Calculate motor angle
      servo.write(angle);
    }
  }
  if (strcmp(topic, "210213T-ADMIN-SHADE-CONTROL") == 0) {
    GAMMA = atof(payloadCharAr); // Update gamma value
    // Check if all parameters are available before calculating motor angle
    if (MIN_ANGLE != 0 && intensity != 0) {
      angle = calculateMotorAngle(); // Calculate motor angle
      servo.write(angle);
    }
  }
}

unsigned long getTime(){
  timeClient.update();
  return timeClient.getEpochTime(); // Get current time from NTP client
}

void checkSchedule(){
  if(isScheduledON){
    unsigned long currentTime = getTime();
    if(currentTime>scheduledOnTime){
      buzzerOn(true); // Turn on buzzer if scheduled time is reached
      isScheduledON = false;
      mqttClient.publish("210213T-ADMIN-MAIN-ON-OFF-ESP", "1");
      mqttClient.publish("210213T-ADMIN-SCH-ESP-ON", "0");
      Serial.println("Scheduled ON");
    }
  }
}

// Function to calculate motor angle
float calculateMotorAngle() {
  float theta = min((MIN_ANGLE* D) + (180 - MIN_ANGLE) * intensity * GAMMA, 180.0f);
  Serial.print("Motor angle: ");
  Serial.println(theta);
  return theta;
}