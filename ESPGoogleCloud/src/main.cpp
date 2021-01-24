#include <Arduino.h>

#if defined(ESP8266)
#define __ESP8266_MQTT__
#endif

#ifdef __ESP8266_MQTT__
#include <CloudIoTCore.h>
#include "esp8266_mqtt.h"
#include <ArduinoJson.h>
#endif
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ctime>
#include <string>
#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

#define PIN_BUTTON 16 //D0

int STATE = 0;
#define RFID_Read 0
#define BUTTON_Read 1
#define TEMP_Read 2
#define CLOUD_Send 3

#define SS_PIN D4 //D4
#define RST_PIN D3 //D3

int buttonState = 0;

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

//JSON Objects
String ID;
byte ActualUID[4];
char aux[32]="";

String timestamp;
time_t now;
struct tm y2k = {0};
double seconds;
String secs;
double temperature;


static unsigned long lastMillis = 0;

void array_to_string(byte array[], unsigned int len, char buffer[])
{
    for (unsigned int i = 0; i < len; i++)
    {
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;
        buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    }
    buffer[len*2] = '\0';
}

void func_RFID(){
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
	if ( ! mfrc522.PICC_IsNewCardPresent()) {
		return;
	}
	// Select one of the cards
	if ( ! mfrc522.PICC_ReadCardSerial()) {
		return;
	}
	Serial.print("Card UID:");
	for (byte i = 0; i < mfrc522.uid.size; i++) {
			Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
			Serial.print(mfrc522.uid.uidByte[i], HEX); 
			ActualUID[i]=mfrc522.uid.uidByte[i];
	} 
	Serial.println();
	array_to_string(ActualUID, 4, aux);
	ID = aux;
	Serial.println("String ID: " + ID);

	// Terminamos la lectura de la tarjeta  actual
	mfrc522.PICC_HaltA();   
  STATE=BUTTON_Read;
  Serial.println("State: Button");

}

void func_Button(){
  buttonState = digitalRead (PIN_BUTTON);
  if (buttonState == HIGH){
    Serial.println("Button Pushed");
    digitalWrite(LED_BUILTIN, LOW);
    delay(600);
    STATE = TEMP_Read;
    Serial.println("State: Temp read");
  }else{
    digitalWrite(LED_BUILTIN, HIGH);
  }

}

void func_Temp(){
  temperature = mlx.readObjectTempC();
  Serial.print("Temp: "); Serial.print(temperature); Serial.println("");

  now = time(0);
  timestamp = ctime(&now);
  timestamp.trim();
  Serial.println("Timestamp: " + timestamp);
  seconds = difftime(now, mktime(&y2k));
  secs = String(seconds);
  Serial.println(secs + "seconds since January 1, 2000 in the current timezone");
  //Add timestamp
  STATE = CLOUD_Send;
  Serial.println("State: Cloud");
}

void func_CLOUD(){
  // TODO: Replace with your code here
  if (millis() - lastMillis > 6000)
  {
    Serial.println("Publishing");
    lastMillis = millis();
    //Serial.println("{\"ID\": " + ID + ", \"temperature\": " + temperature + ", \"timestamp\": " + timestamp + "}");
    publishTelemetry("{\"ID\": \"" + ID + "\", \"temperature\": \"" + temperature + "\", \"timestamp\": \"" + timestamp + "\", \"seconds\": \""+ secs + "\"}");

    STATE = RFID_Read;
    Serial.println("State: RFID");
  }

}



void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  while (!Serial);		// Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  
  pinMode(LED_BUILTIN, OUTPUT);

  //Button Setup
  pinMode(PIN_BUTTON, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println("Boton configurado");
  //Button 

  //Temp sensor setup
   mlx.begin(); 


  //RFID Conf
	SPI.begin();			// Init SPI bus
	mfrc522.PCD_Init();		// Init MFRC522
	delay(4);                       // Necessary to get a correct Firmware Version reading with the next command
	mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details

  Serial.println("Setting up Cloud");
  setupCloudIoT(); // Creates globals for MQTT
  Serial.println("Cloud setup Done");

  Serial.println("State: RFID");
  STATE = RFID_Read;

  y2k.tm_hour = 0;   y2k.tm_min = 0; y2k.tm_sec = 0;
  y2k.tm_year = 100; y2k.tm_mon = 0; y2k.tm_mday = 1;
}



void loop() {
  
  //Always connected to Cloud
    if (!mqtt->loop())
  {
    Serial.println("Failure on MQTT Loop");
    //se atasca aquí
    mqtt->mqttConnect();
    //nunca llega aquí
  }

  delay(10); // <- fixes some issues with WiFi stability

  //Cloud Stability End


  // put your main code here, to run repeatedly:
  switch (STATE)
  {
  case  RFID_Read:
    func_RFID();
    break;
  
  case BUTTON_Read:
    func_Button();
    break;
  
  case TEMP_Read:
    func_Temp();
    break;
  
  case CLOUD_Send:
    func_CLOUD();
    break;
  
  default:
    Serial.println("Something Went Wrong");
    STATE = RFID_Read;
    break;
  }
}

