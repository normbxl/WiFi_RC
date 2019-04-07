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
#include <ESP8266mDNS.h>

#include <stdint.h>
#include <string.h>
#include "jsmn.h"

#define PIN_M1_PWM			12
#define PIN_M1_POLARITY		4
#define PIN_M2_PWM			13
#define PIN_M2_POLARITY		5
#define PIN_S1          14
#define PIN_S2          16



typedef enum BoardConnector_en {
	S1=0, S2, M1, M2, MAX_NUM_PORTS
} BoardConnector_t;

typedef enum PortType_en{
	NOT_USED = 0,
	DIGITAL,
	SERVO,
	ANALOG,
	NUM_PORT_TYPES, // IMPORTANT: H_BRIDGE must be the last entry!
	H_BRIDGE
} PortType_t;

typedef struct Port_s {
	PortType_t type;
	int value;
  uint8_t pinOut;
  uint8_t pinExtra;
} Port_t;

Port_t ports[MAX_NUM_PORTS] = {
	{ .type = DIGITAL, .value = 0, .pinOut=PIN_S1, .pinExtra=0 },	// S1
	{ .type = DIGITAL, .value = 0, .pinOut=PIN_S2, .pinExtra=0 },	// S2
	{ .type = H_BRIDGE,.value = 0, .pinOut=PIN_M1_PWM, .pinExtra=PIN_M1_POLARITY},	// M1
	{ .type = H_BRIDGE,.value = 0, .pinOut=PIN_M2_PWM, .pinExtra=PIN_M2_POLARITY}		// M2
};

const char* portCfgTokens[] = {
	"NOUSE",
	"DIGIT",
	"SERVO",
	"ANLOG",
  "-----",
  "MOTOR",
  "VOID-"
};
const char* BoardConnNames[] = {
  "S1", "S2", "M1", "M2"
};

const char* SSID_BASE = "Nrrm-RC_";
const char* PWD = "thepasswd";

const char* STA_SSID = "lovely-N";
const char* STA_PWD = "d0tlab1$$up1";

const IPAddress ipAddr(192, 168, 2, 1);
const IPAddress gateway(192, 168, 2, 1);
const IPAddress subnet(255, 255, 255, 0);

uint8_t macAddress[7] = { 0 };
char ssid[20];
WiFiUDP udp;

unsigned int udpPort = 4210;

char udpBuffer[255];


String replyStr = String();

jsmntok_t jTokens[23];
jsmn_parser jParser;

Servo servos[2];

// Define Function Prototypes that use User Types below here or use a .h file
//

void handleGet();

/**
 * For now the only safety-net on disconnet is to set value actuator value to zero.
 */
void onDisconnect() {
  uint8_t i;
  for (i=0; i<MAX_NUM_PORTS; i++) {
    ports[i].value=0;
  }
}

// Define Functions below here or use other .ino or cpp files
//
int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start) {
		return strncmp(json + tok->start, s, tok->end - tok->start);
	}
	return -1;
}

int jsonInt(const char *json, jsmntok_t *tok) {
	if (tok->type == JSMN_PRIMITIVE) {
		return (int)strtol(json + tok->start, NULL, 10);
	}
	return 0;
}

void jsonParsePort(const char *json, jsmntok_t *tok, BoardConnector_t portNumber, bool cfgMode) {
	uint8_t i;
	if (cfgMode) {
		for (i = 0; i < NUM_PORT_TYPES; i++) {
			if (jsoneq(json, tok, portCfgTokens[i]) == 0) {
				ports[portNumber].type = (PortType_t)i;
				return;
			}
		}
	}
	else {
		ports[portNumber].value = jsonInt(json, tok);
	}
}
void applyBoardConfigChange() {
  if (ports[S1].type==SERVO) {
    servos[S1].attach(PIN_S1);
  }
  else if (servos[S1].attached()) {
    servos[S1].detach();
  }
  if (ports[S2].type==SERVO) {
    servos[S2].attach(PIN_S2);
  }
  else if (servos[S2].attached()) {
    servos[S2].detach();
  }
}

void initGPIO() {
	// init GPIO
	pinMode(PIN_M1_PWM, OUTPUT);
	pinMode(PIN_M1_POLARITY, OUTPUT);
	pinMode(PIN_M2_PWM, OUTPUT);
	pinMode(PIN_M2_POLARITY, OUTPUT);
  pinMode(PIN_S1, OUTPUT);
  pinMode(PIN_S2, OUTPUT);
	digitalWrite(PIN_M1_PWM, 0);
	digitalWrite(PIN_M2_PWM, 0);
}

void initWiFi() {
	char macChars[10] = { 0 };
  long unsigned int ts;
  bool notConnected=true;
  // Try to connect to WiFi network
  Serial.print("Trying to connect to ");
  Serial.println(STA_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(STA_SSID, STA_PWD);
  ts = millis();
  // Wait for connection
  while (notConnected && millis() - ts < 15000) {
    delay(1000);
    Serial.print(".");
    notConnected = (WiFi.status() != WL_CONNECTED);
  }
  if (!notConnected) {
    Serial.print("\nConnected to AP, got IP ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("\nGoing into Soft-AP mode");
    
	  WiFi.softAPmacAddress(macAddress);
	  WiFi.mode(WIFI_AP);
    
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
  }
	if (udp.begin(udpPort)) {
		Serial.printf("Listening on port %d\r\n", udpPort);
	}
	else {
		Serial.println("Failed to create UDP port.");
	}
  if (!MDNS.begin("esp8266")) {
    Serial.println("Error setting up MDNS responder!");
  }
  else {
    // Add service to MDNS-SD
    MDNS.addService("wifi-rc", "udp", 4210);
  }

}

// The setup() function runs once each time the micro-controller starts
void setup()
{
	Serial.begin(115200);
	Serial.println("--WiFi-RC starting up--");
	
	initGPIO();
	initWiFi();
  applyBoardConfigChange();
	jsmn_init(&jParser);
  replyStr.reserve(200);
  Serial.println("Ready.");
}

void handleGet() {
  // todo: apply mean to adc value
  int adc=analogRead(A0);
  int i;
  
  replyStr ="{ ";

  for (i=0; i<MAX_NUM_PORTS; i++) {
    // Serial.print(i);
    replyStr += "\""+String(BoardConnNames[i])+"\": {";
    replyStr += "\"type\":\"";
    //replyStr += k;
    replyStr += portCfgTokens[ports[i].type];
    replyStr += "\", \"value\": ";
    replyStr += ports[i].value;
    replyStr += " },";
  }
  replyStr += "\"adc\": ";
  replyStr += adc;
  replyStr += " }";
}


void handleJsonPayload(int len) {
	int i, res;
	bool getValue = false;
	bool configMode = false;
	res = jsmn_parse(&jParser, udpBuffer, len, jTokens, sizeof(jTokens) / sizeof(jTokens[0]));
	if (res < 0) {
		//Serial.println("Failed to parse JSON");
	}
	else {
		if (res < 1 || jTokens[0].type != JSMN_OBJECT) {
			//Serial.println("JSON: Root object expected");
			return;
		}
		// Danger: Assuming the first token is the cmd!
		for (i = 1; i < res; i++) {
			if (jsoneq(udpBuffer, &jTokens[i], "cmd") == 0) {
				if (jsoneq(udpBuffer, &jTokens[i + 1], "get") == 0) {
					getValue = true;
          break;
				}
			}
			else if (jsoneq(udpBuffer, &jTokens[i], "type") == 0) {
				if (jsoneq(udpBuffer, &jTokens[i + 1], "config") == 0) {
					configMode = true;
				}
			}
			else if (jsoneq(udpBuffer, &jTokens[i], "s1") == 0) {
				jsonParsePort(udpBuffer, &jTokens[i + 1], S1, configMode);
			}
			else if (jsoneq(udpBuffer, &jTokens[i], "s2") == 0) {
				jsonParsePort(udpBuffer, &jTokens[i + 1], S2, configMode);
			}
			else if (!configMode) {
				// Motor-ports can't be configured
				if (jsoneq(udpBuffer, &jTokens[i], "m1") == 0) {
					ports[M1].value = jsonInt(udpBuffer, &jTokens[i + 1]);
				}
				else if (jsoneq(udpBuffer, &jTokens[i], "m2") == 0) {
					ports[M2].value = jsonInt(udpBuffer, &jTokens[i + 1]);
				}
			}
		}
    if (configMode) {
      applyBoardConfigChange();
    }
    if (getValue) {
      handleGet();
    }
    else {
      replyStr = "ok";
    }
	}
}

void handleUdp() {
	int size = udp.parsePacket();
	if (size > 0) {
		Serial.printf("Received %d bytes from %s, port %d\r\n", size, udp.remoteIP().toString().c_str(), udp.remotePort());
		int len = udp.read(udpBuffer, sizeof(udpBuffer)-1);
		if (len > 0) {
			handleJsonPayload(len);
      
      //memset(udpReply, 0, sizeof(udpReply));
      //replyStr.getBytes((unsigned char*)udpReply, sizeof(udpReply));
      Serial.println(replyStr);
      
			udp.beginPacket(udp.remoteIP(), udp.remotePort());
			size_t written=udp.write(replyStr.c_str(), replyStr.length());
			udp.endPacket();
      
      Serial.printf("reply length: %d, written %d\n", replyStr.length(), written);
		}
	}
}

void setActuators() {
  uint8_t i;
  int value;
  for (i=0; i<MAX_NUM_PORTS; i++) {
    value = ports[i].value;
    switch (ports[i].type) {
      
      case H_BRIDGE:
        if (i>=M1) {
          if (value > 0) {
              digitalWrite(ports[i].pinExtra, HIGH);
            }
            else {
              digitalWrite(ports[i].pinExtra, LOW);
              value = -value;
            }
            value = map(value, -127, 127, 0, PWMRANGE);
            value = value > PWMRANGE ? PWMRANGE : value;
            analogWrite(ports[i].pinOut, value);
        }
        break;
      case SERVO:
        if (i <= S2) {
          value = map(value, -127, 127, 0, 180);
          servos[i].write(value);
        }
        break;
      case DIGITAL:
        digitalWrite(ports[i].pinOut, ports[i].value > 0 ? HIGH : LOW);
        break;
      case ANALOG:
        value = map(value, 0, 127, 0, PWMRANGE);
        value = value > PWMRANGE ? PWMRANGE : value;
        analogWrite(ports[i].pinOut, value);
        break;
      default:
        break;
    }
  }
}


// Add the main program code into the continuous loop() function
void loop()
{
	
	handleUdp();
  setActuators();

}
