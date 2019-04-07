/*
  PubNub sample client

  This sample client will use just the minimal-footprint raw PubNub
  interface where it is your responsibility to deal with the JSON encoding.

  It will just send a hello world message and retrieve one back, reporting
  its deeds on serial console.

  Circuit:
  * Ethernet shield attached to pins 10, 11, 12, 13
  * (Optional.) LED on pin 8 for reception indication.
  * (Optional.) LED on pin 9 for publish indication.

  This code is in the public domain.
  */

#include <SPI.h>
#include <Ethernet.h>
#include <PubNub.h>

// Some Ethernet shields have a MAC address printed on a sticker on the shield;
// fill in that address here, or choose your own at random:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

const int subLedPin = 8;
const int pubLedPin = 9;

char pubkey[]  = "demo";
char subkey[]  = "demo";
char channel[] = "hello_world";

void setup()
{
    pinMode(subLedPin, OUTPUT);
    pinMode(pubLedPin, OUTPUT);
    digitalWrite(subLedPin, LOW);
    digitalWrite(pubLedPin, LOW);

    Serial.begin(9600);
    Serial.println("Serial set up");

    while (!Ethernet.begin(mac)) {
        Serial.println("Ethernet setup error");
        delay(1000);
    }
    Serial.println("Ethernet set up");

    PubNub.begin(pubkey, subkey);
    Serial.println("PubNub set up");
}

void flash(int ledPin, int times)
{
    for (int i = 0; i < times; i++) {
        digitalWrite(ledPin, HIGH);
        delay(100);
        digitalWrite(ledPin, LOW);
        delay(100);
    }
}

void loop()
{
    Ethernet.maintain();

    Serial.println("publishing a message");
    auto client = PubNub.publish(channel, "\"\\\"Hello world!\\\" she said.\"");
    if (!client) {
        Serial.println("publishing error");
        delay(1000);
        return;
    }
    PublishCracker cheez;
    cheez.read_and_parse(client);
    /** You're mostly interested in `outcome()`, and, if it's
    "failed", then `description()`, but let's print all, for
    completeness.
    */
    Serial.print("Outcome: ");
    Serial.print(cheez.outcome());
    Serial.print(' ');
    Serial.println(cheez.to_str(cheez.outcome()));
    Serial.print("description: ");
    Serial.println(cheez.description());
    Serial.print("timestamp: ");
    Serial.println(cheez.timestamp());
    Serial.print("state: ");
    Serial.print(cheez.state());
    Serial.print(' ');
    Serial.println(cheez.to_str(cheez.state()));

    client->stop();
    Serial.println();
    flash(pubLedPin, 3);

    Serial.println("waiting for a message (subscribe)");
    PubSubClient* subclient = PubNub.subscribe(channel);
    if (!subclient) {
        Serial.println("subscription error");
        delay(1000);
        return;
    }
    String                  msg;
    SubscribeCracker ritz(subclient);
    while (!ritz.finished()) {
        ritz.get(msg);
        if (msg.length() > 0) {
            Serial.print("Received: ");
            Serial.println(msg);
        }
    }
    subclient->stop();
    flash(subLedPin, 3);

    Serial.println("retrieving message history");
    auto hisclient = PubNub.history(channel);
    if (!hisclient) {
        Serial.println("history error");
        delay(1000);
        return;
    }
    HistoryCracker tuc(hisclient);
    while (!tuc.finished()) {
        tuc.get(msg);
        if (msg.length() > 0) {
            Serial.print("Received: ");
            Serial.println(msg);
        }
    }
    hisclient->stop();
    flash(pubLedPin, 2);

    delay(5000);
}
