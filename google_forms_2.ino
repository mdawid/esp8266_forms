/*
    HTTP over TLS (HTTPS) example sketch

    This example demonstrates how to use
    WiFiClientSecure class to access HTTPS API.
    We fetch and display the status of
    esp8266/Arduino project continuous integration
    build.

    Limitations:
      only RSA certificates
      no support of Perfect Forward Secrecy (PFS)
      TLSv1.2 is supported since version 2.4.0-rc1

    Created by Ivan Grokhotkov, 2015.
    This example is in public domain.
*/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <CStringBuilder.h>

#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <DoubleResetDetect.h>
#include "config.h"

// maximum number of seconds between resets that
// counts as a double reset 
#define DRD_TIMEOUT 2.0

// address to the block in the RTC user memory
// change it if it collides with another usage 
// of the address block
#define DRD_ADDRESS 0x00

#define DEEP_SLEEP_DELAY_S 30*60
#define WIFI_CONNECTION_TIMEOUT_S 30
#define DATA_READ_TIMEOUT_S 5
#define ONE_WIRE_BUS 5

DoubleResetDetect drd(DRD_TIMEOUT, DRD_ADDRESS);

String ssid = "Shusaku";
String password = "P4czuszk4";

const char* host = "docs.google.com";
const int httpsPort = 443;

// Use web browser to view and copy
// SHA1 fingerprint of the certificate
const char* fingerprint = "‎c8 47 46 6b 7b bb d0 d4 a3 1c e9 7a 40 74 9e cc ba fa 5e c0";

const char* formId = FORM_TOKEN;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

ADC_MODE(ADC_VCC); 

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT); 
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  
  if (drd.detect()) {
    Serial.println("** Double reset boot **");
    //WiFiManager
    WiFiManager wifiManager;

    //reset settings - for testing
    //wifiManager.resetSettings();

    //sets timeout until configuration portal gets turned off
    //useful to make it all retry or go to sleep
    //in seconds
    wifiManager.setTimeout(120);
    wifiManager.setConnectTimeout(30);
    wifiManager.setBreakAfterConfig(true);

    //it starts an access point with the specified name
    //and goes into a blocking loop awaiting configuration

    //WITHOUT THIS THE AP DOES NOT SEEM TO WORK PROPERLY WITH SDK 1.5 , update to at least 1.5.1
    //WiFi.mode(WIFI_STA);
    if (!wifiManager.startConfigPortal("TermometrAP")) {
      Serial.println("Failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    } else {
      Serial.print("***SSID: ");
      Serial.println( WiFi.SSID() );
      Serial.print("***Password: ");
      Serial.println( WiFi.psk() );
    }
  }

  //WiFi.disconnect(true);

  ssid = WiFi.SSID();
  password = WiFi.psk();
  
  connect();

  sensors.begin();
}

void connect() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println( "Already connected" );
    return;
  }

  Serial.println();
  Serial.print("Connecting to: " );
  Serial.println( ssid );

  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  uint32_t wifiTo = millis() + WIFI_CONNECTION_TIMEOUT_S * 1000; 
  do {
    if (WiFi.status() == WL_CONNECTED)
      break;
    delay(500);
    Serial.print(".");
  } while(millis() < wifiTo);

  if (WiFi.status() != WL_CONNECTED) {
    sweetDreams();
    return;
  }
  
  Serial.println("WiFi connected");
  Serial.print("IP address: " );
  Serial.println( WiFi.localIP() );
}

int send( float voltage, float temp, int rssi ) {
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  Serial.print("Connecting to: " );
  Serial.println(host);
  
  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection failed!");
    return -1;
  }

  if (client.verify(fingerprint, host)) {
    Serial.println("Certificate matches");
  } else {
    Serial.println("Certificate doesn't match");
  }

  char buff[512];
  CStringBuilder sb(buff,sizeof(buff));
  sb.printf( "/forms/d/e/%s/formResponse", formId );
  sb.print( "?ifq" );

  //sb.printf( "&entry.1528009232=%d", rssi );
  //sb.printf( "&entry.335980684=%.1f", voltage );
  //sb.printf( "&entry.268672932=%.1f", temp );

  sb.printf( "&entry.337882382=%d", rssi );
  sb.printf( "&entry.1666767716=%.1f", voltage );
  sb.printf( "&entry.578953062=%.1f", temp );
  
  sb.print( "&submit=Submit" );
  Serial.print( "Requesting URL: " );
  Serial.println( buff );
  client.print(String("GET ") + buff + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");
  client.flush();
  
  Serial.println("Request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    if (line == "\r") {
      Serial.println("Headers received");
      break;
    }
  }

/* To jest odczyt ogromnego body, ktore obecnie olewamy
  uint32_t to = millis() + DATA_READ_TIMEOUT_S * 1000; 
  do {
    char tmp[128]; 
    memset(tmp, 0, 128); 
    int rlen = client.read((uint8_t*)tmp, sizeof(tmp) - 1); 
    yield(); 
    if (rlen < 0) break;
    Serial.print(tmp); 
  } while (millis() < to); 
*/
  Serial.println("Closing connection");
  client.stop();
}

void sweetDreams() {
  // Wygasa przed snem
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  // Time in microseconds
  Serial.print( "Sleeping for: " );
  Serial.print( DEEP_SLEEP_DELAY_S );
  Serial.println( " seconds" );
  Serial.flush();  
  ESP.deepSleep( DEEP_SLEEP_DELAY_S * 1000000 ); 
}

void loop() {
  // Jesli wysyla, to LED wlaczony
  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level

  int rssi = WiFi.RSSI();
  float vcc = ((float)ESP.getVcc())/1024;
  
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");
  float temperature = sensors.getTempCByIndex(0);

  Serial.print( "VCC: " );
  Serial.println( vcc, 2 );
  Serial.print( "RSSI: " );
  Serial.println( rssi );
  Serial.print( "Temperature: " );
  Serial.println( temperature, 1 );
  
  send( vcc, temperature, rssi );
  
  sweetDreams();
}
