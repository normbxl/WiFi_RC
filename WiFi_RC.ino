// Visual Micro is in vMicro>General>Tutorial Mode
// 
/*
    Name:       WiFi_RC.ino
    Created:	10.12.2018 16:23:00
    Author:     DOTTI-2009\norman
*/
#include <Servo.h>
#include <WiFiUdp.h>
#include <WiFiServer.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <ESP8266WiFiType.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFi.h>


#include <string.h>
#include "jsmn.h"

#define PIN_M1_PWM			12
#define PIN_M1_POLARITY		4
#define PIN_M2_PWM			13
#define PIN_M2_POLARITY		5


const char* SSID_BASE = "Nrrm-RC_";
const char* PWD = "thepasswd";


const IPAddress ipAddr(192, 168, 2, 1);
const IPAddress gateway(192, 168, 2, 2);
const IPAddress subnet(255, 255, 255, 0);
// Define User Types below here or use a .h file
//
typedef enum {
	NOT_USED=0,
	DIGITAL,
	SERVO,
	PWM,
	H_BRIDGE
} ConnectorUsage_t;


typedef struct {
	int pin;
	int value;
	ConnectorUsage_t type;
} Connector_t;



uint8_t macAddress[7] = { 0 };
char ssid[20];

WiFiUDP udp;
unsigned int udpPort = 4210;
char udpBuffer[255];

const char* udpReply = "ok";
const char* 

// Define Function Prototypes that use User Types below here or use a .h file
//
void onDisconnect();

// Define Functions below here or use other .ino or cpp files
//
int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start) {
		return strncmp(json + tok->start, s, tok->end - tok->start);
	}
	return -1;
}


void initGPIO() {
	// init GPIO
	pinMode(PIN_M1_PWM, OUTPUT);
	pinMode(PIN_M1_POLARITY, OUTPUT);
	pinMode(PIN_M2_PWM, OUTPUT);
	pinMode(PIN_M2_POLARITY, OUTPUT);
	digitalWrite(PIN_M1_PWM, 0);
	digitalWrite(PIN_M2_PWM, 0);
}

void initWiFi() {
	char macChars[10] = { 0 };
	WiFi.softAPmacAddress(macAddress);
	macAddress[7] = 0;
	memset(macChars, 0, sizeof(macChars));

	strcpy(ssid, SSID_BASE);
	sprintf(macChars, "%02X:%02X", macAddress[4], macAddress[5]);
	strcat(ssid, macChars);

	Serial.print("SSID: ");
	Serial.println(ssid);

	if (WiFi.softAPConfig(ipAddr, gateway, subnet) == false) {
		Serial.println("ERROR: failed to set IP address");
		return;
	}
	if (WiFi.softAP(ssid, PWD) == false) {
		Serial.println("ERROR enabling soft-AP");
		return;
	}

	if (udp.begin(udpPort)) {
		Serial.printf("Listening on port %d\r\n", udpPort);
	}
	else {
		Serial.println("Failed to create UDP port.");
	}

}

// The setup() function runs once each time the micro-controller starts
void setup()
{
	Serial.begin(9600);
	Serial.println("--WiFi-RC starting up--");
	
	initGPIO();
	initWiFi();

}

void handleUdp() {
	int size = udp.parsePacket();
	if (size > 0) {
		Serial.printf("Received %d bytes from %s, port %d\r\n", size, udp.remoteIP().toString().c_str(), udp.remotePort());
		int len = udp.read(udpBuffer, sizeof(udpBuffer)-1);
		if (len > 0) {
			udpBuffer[len] = 0;	// string terminiator
			Serial.printf("Data: %s\r\n", udpBuffer);
			
			udp.beginPacket(udp.remoteIP(), udp.remotePort());
			udp.write(udpReply);
			udp.endPacket();
		}
	}
	else {
		Serial.printf("UDPparse returned %i\r\n", size);
	}
}

// Add the main program code into the continuous loop() function
void loop()
{
	char buffer[20];
	size_t length;
	int speed;
	
	if (udp.available() > 0) {
		handleUdp();
	}
	if (Serial.available()) {
		memset(buffer, 0, sizeof(buffer));
		length=Serial.readBytesUntil('\r', buffer, sizeof(buffer));
		speed = atoi(buffer);
		Serial.printf("speed=%d\r\n", speed);
		if (speed > 0) {
			digitalWrite(PIN_M2_POLARITY, HIGH);
		}
		else {
			digitalWrite(PIN_M2_POLARITY, LOW);
			speed = -speed;
		}
		speed = speed > PWMRANGE ? PWMRANGE : speed;
		analogWrite(PIN_M2_PWM, speed);
	}

}
