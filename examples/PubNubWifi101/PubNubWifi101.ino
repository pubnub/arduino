
/*
  PubNub over WiFi Example using WiFi Shield 101 

  This sample client will publish raw (string) PubNub messages from
  boards like Arduino Uno and Zero with WiFi Shield 101
  
  See https://www.arduino.cc/en/Reference/WiFi101 for more info 
  
  created Dec 17, 2015
  by Tomomi Imura
*/
  
#include <SPI.h>

#include <WiFi101.h>
#define PubNub_BASE_CLIENT WiFiClient
#include <PubNub.h>

static char ssid[] = "your-wifi-network";   // your network SSID (name)
static char pass[] = "your-wifi-password";  // your network password
int status = WL_IDLE_STATUS;                // the Wifi radio's status

const static char pubkey[] = "demo";         // your publish key 
const static char subkey[] = "demo";         // your subscribe key
const static char channel[] = "hello_world"; // channel to use


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Serial set up");

  // attempt to connect using WPA2 encryption:
  Serial.println("Attempting to connect to WPA network...");
  status = WiFi.begin(ssid, pass);

  // if you're not connected, stop here:
  if ( status != WL_CONNECTED) {
    Serial.println("Couldn't get a wifi connection");
    while (true);
  } else {
    Serial.print("WiFi connecting to SSID: ");
    Serial.println(ssid);

    PubNub.begin(pubkey, subkey);
    Serial.println("PubNub set up");
  }
}

void loop() {

  /* Publish */
  
  WiFiClient *client;
  
  char msg[] = "\"Yo!\"";

  client = PubNub.publish(channel, msg);

  if (!client) {
    Serial.println("publishing error");
    delay(1000);
    return;
  }
  if (PubNub.get_last_http_status_code_class() != PubNub::http_scc_success) {
    Serial.print("Got HTTP status code error from PubNub, class: ");
    Serial.print(PubNub.get_last_http_status_code_class(), DEC);
  }
  while (client->available()) {
    Serial.write(client->read());
  }
  client->stop();
  Serial.println("---");
}


