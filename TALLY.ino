#define ESP8266WIFIMESH_DISABLE_COMPATIBILITY // Excludes redundant compatibility code. TODO: Should be used for new code until the compatibility code is removed with release 3.0.0 of the Arduino core.
 
#include <ESP8266WiFi.h>
#include <TypeConversionFunctions.h>
#include <assert.h>
#include <FloodingMesh.h>
#include "SSVQueueStackArray.h"
 
namespace TypeCast = MeshTypeConversionFunctions;
 
constexpr char exampleMeshName[] PROGMEM = "NODE_";
constexpr char exampleWiFiPassword[] PROGMEM = "ChangeThisWiFiPassword_TODO";

const char* ssid = "babel";
const char* password = "essasito";

bool WiFiMode = false;
WiFiClient client;
IPAddress server(81, 95, 192, 186);
 
uint8_t espnowEncryptedConnectionKey[16] = {0x33, 0x44, 0x33, 0x44, 0x33, 0x44, 0x33, 0x44, // This is the key for encrypting transmissions of encrypted connections.
                                            0x33, 0x44, 0x33, 0x44, 0x33, 0x44, 0x32, 0x11
                                           };
 
uint8_t espnowHashKey[16] = {0xEF, 0x44, 0x33, 0x0C, 0x33, 0x44, 0xFE, 0x44, // This is the secret key used for HMAC during encrypted connection requests.
                             0x33, 0x44, 0x33, 0xB0, 0x33, 0x44, 0x32, 0xAD
                            };
 
bool meshRequestHandler(String &message, FloodingMesh &meshInstance);

 
//` Create the mesh node object
FloodingMesh floodingMesh = FloodingMesh(meshRequestHandler, FPSTR(exampleWiFiPassword), espnowEncryptedConnectionKey, espnowHashKey, FPSTR(exampleMeshName), TypeCast::uint64ToString(ESP.getChipId()), true);
 
const int PROGRAM = 5;
const int PREVIEW = 6;
 
String  myMAC;
 
bool    master          = false;
bool    masterFound     = false;
bool    masterResponse  = false;
 
String  activeDevices[16][2];
int     activeDevicesNum = 0;
 
bool    haveToRespond = false;
SSVQueueStackArray <String> response;
 
const unsigned int MAX_MESSAGE_LENGTH = 102;

void broadcastRequest(String request, String adress, String data);
 
// Function to split the string
String split(String data, char separator, int index) {
  int found      = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex   = data.length() - 1;
 
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void SendActiveDevices() {
  String output = "DEVICE_LIST:";
  for (uint8_t i = 0; i < activeDevicesNum; i++) {
    output += activeDevices[i][0] + ";";
  }
  Serial.println(output);
}
 
// Callback when message recieved
bool requestHandler(String &message) {
 
  String op   = split(message, ':', 0);
  String macs = split(message, ':', 1);
  String data = split(message, ':', 2);
 
  int  macsLen = macs.length();
  bool myAdressIncluded = false;
 
  for (uint8_t i = 0; i < macsLen; i += 12) {
    if (macs.substring(i, i + 12) == String(myMAC)) {
      myAdressIncluded = true;
      break;
    }
  }
 
  // adress is envolved in adresses pool of recieved message
  if (myAdressIncluded) {
    if (op == "PGM") {
      digitalWrite(PROGRAM, HIGH);
    }
    else if (op == "PRV") {
      // digitalWrite(PREVIEW, HIGH);
      Serial.println(" PREVIEW ON");
    }
    // Master responded that he has my mac
    else if (op == "OK") {
      masterResponse = true;
 
      if (!masterFound) {
        masterFound = true;
        Serial.println(" MASTER FOUND");    
      }
    }
    // adress is not envolved in recieved message
  } else {
 
    // node is ready to be connected
    if (op == "R") {
 
      if (master) {
        bool    alreadyNoticed = false;
        uint8_t recievedMacIndex;
 
        for (uint8_t i = 0; i < activeDevicesNum; i++) {
          if (activeDevices[i][0] == macs) {
            activeDevices[i][1] = "1";
            recievedMacIndex = i;
            alreadyNoticed = true;
            break;
          }
        }
 
        // Recieved MAC index is not included in active Macs array
        if (!alreadyNoticed) {
          activeDevices[activeDevicesNum][0] = macs;
          activeDevices[activeDevicesNum][1] = "1";
          activeDevicesNum++;

          SendActiveDevices();

          response.push("OK:" + macs + ":x");
 
        } else {
          activeDevices[recievedMacIndex][1] = "1";
          response.push("OK:" + macs + ":x");
        }
 
      // No master node
      } else {
        if (!masterFound) {
          Serial.println(macs + " is ready");
        }
      }
    }
    if (op == "PGM") {
      digitalWrite(PROGRAM, LOW);
    }
    if (op == "PRV") {
      Serial.println(" PREVIEW OFF");
      // digitalWrite(PREVIEW, LOW);
    }
    if (master) {
      if (op == "LIST") {
          SendActiveDevices();
      }
    }
  }
  return true;
}

bool meshRequestHandler(String &message, FloodingMesh &meshInstance) {
  requestHandler(message);
  return true;
}
 
void setup() {
  Serial.begin(115200);
  while (!Serial) {;}

  uint8_t apMacArray[6] {0};
  myMAC = TypeCast::macToString(WiFi.softAPmacAddress(apMacArray));

  // LED setup
  pinMode(PROGRAM, OUTPUT);
  // pinMode(PREVIEW, OUTPUT);

  if (WiFiMode) {
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(1000);
    }

    Serial.println("Connected to the WiFi");

    if (client.connect(server, 8081)) {
      Serial.println("Connected to the server");
      client.print("X:" + myMAC + ":x");
    } else {
      Serial.println("Couldn't connect to server");
    }
  } else {
    WiFi.persistent(false);

    floodingMesh.begin();
    floodingMesh.activateAP(); // Required to receive messages

    Serial.println(" Setting up mesh node...");
    Serial.println(" My MAC adress: " + myMAC);

    floodingMeshDelay(2000); // Give some time for user to start the nodes
  }
}
 
void broadcastRequest(String request) {
  if (WiFiMode) {
    client.print(request);
  } else {
    floodingMesh.broadcast(request);
  }
}

// Timers
int32_t timeOfLastProclamation = -3000;
int32_t timeOfLastPresence     = -6000;
int32_t timeOfLastMaster       = -6000;
int     incomingByte           = 0;
 
void loop() {
  floodingMeshDelay(1);

  if (WiFiMode) {
    // ping that this device is ready to be connected to master node
    if (millis() - timeOfLastProclamation > 3000) {
      if (!master) {
        broadcastRequest("CON:X:X");   
      }
      timeOfLastProclamation = millis();
      floodingMeshDelay(20);
    }
    while(client.available()){
      String reply = client.readStringUntil('\r');
      // Serial.println(reply);
      requestHandler(reply);
    }
  } else {
      // ping that this device is ready to be connected to master node
      if (millis() - timeOfLastProclamation > 3000) {
        if (!master) {
          broadcastRequest("R:" + String(myMAC) + ":x");   
        }
        timeOfLastProclamation = millis();
        floodingMeshDelay(20);
      }
    
      // check if master responded
      if (millis() - timeOfLastMaster > 6000) {
        if (!masterResponse && masterFound) {
          masterFound = false;
          digitalWrite(PROGRAM, LOW);
          // digitalWrite(PREVIEW, LOW);
    
          Serial.println(" PREVIEW OFF");
    
          Serial.println();
          Serial.println(" MASTER LOST");
          Serial.println();
    
        } else {
          masterResponse = false;  
        }
        timeOfLastMaster = millis();
      }
    
    
      // if this device is master every 6 seconds update
      // currently connected devices if they pinged thier presence
      // in this time interval
      if (master && millis() - timeOfLastPresence > 6000) {
    
        String newActiveDevices[16][2];
        int    newActiveDevicesNum = 0;
    
        // loop throught array of devices that pinged thier presence
        // fill new active devices array with thier mac adresses
        for (uint8_t i = 0; i < activeDevicesNum; i++) {
          if (activeDevices[i][1] == "1") {
            newActiveDevices[newActiveDevicesNum][0] = activeDevices[i][0];
            newActiveDevices[newActiveDevicesNum][1] = "0";
            newActiveDevicesNum++;
          } 
        }
    
        // copy new values
        bool sendNewList = false;
        if(newActiveDevicesNum != activeDevicesNum) {
          sendNewList = true;
        }
        
        for (uint8_t i = 0; i < newActiveDevicesNum; i++) {
          if (activeDevices[i][0] != newActiveDevices[i][0]) {
            sendNewList = true;
          }
          activeDevices[i][0] = newActiveDevices[i][0];
          activeDevices[i][1] = newActiveDevices[i][1];
        }
    
        activeDevicesNum = newActiveDevicesNum;
        timeOfLastPresence = millis();

        if (sendNewList) {
          SendActiveDevices();
        }
      }
    
      // respond on the recieved request
      if (!response.isEmpty()) {
        broadcastRequest(response.pop());
        floodingMeshDelay(20);
      }
    // Check to see if anything is available in the serial receive buffer
    while (Serial.available() > 0) {
  
      //Create a place to hold the incoming message from serial input
      static char message[MAX_MESSAGE_LENGTH];
      static unsigned int message_pos = 0;
  
      // Read the next available byte in the serial receive buffer
      char inByte = Serial.read();
  
      // Message coming in (check not terminating character) and guard for over message size
      if ( inByte != '\n' && (message_pos < MAX_MESSAGE_LENGTH - 1) ) {
        message[message_pos] = inByte;
        message_pos++;
  
      } else {
  
        // if enter have been pressed set this device to master
        if (!master && !masterFound) {
          master = true;
        }
  
        // Add null character to string
        message[message_pos] = '\0';
  
        // Broadcast message
        broadcastRequest(message);
  
        // Print request on the screen
        Serial.println("REQUEST_SENT: " + String(message));
  
        // Reset for the next message
        message_pos = 0;
      }
    }
  }
}
