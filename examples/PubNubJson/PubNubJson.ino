/*
  PubNub sample JSON-parsing client

  This sample client will properly parse JSON-encoded PubNub subscription
  replies using the ArduinoJson library. It will send a simple message, then
  properly parsing and inspecting a subscription message received back.

  Circuit:
  * Ethernet shield attached to pins 10, 11, 12, 13
  * (Optional) Analog sensors attached to analog pin.
  * (Optional) LEDs to be dimmed attached to PWM pins 8 and 9.

  Note that due to use of the ArduinoJSON library, this sketch is more memory
  sensitive than the others. In order to be able to use it on the boards
  based on ATMega328, some special care is needed. Memory saving tips:

  (i) Remove myI, dnsI from the sketch unless you need them.

  (ii) In the file hardware/arduino/cores/arduino/HardwareSerial.cpp
  which should be part of your Arduino installation, decrease SERIAL_BUFFER_SIZE
  from 64 to 16 or even smaller value (depends on your Serial usage).

  (iii) Look for other memory-saving tips in Arduino docs and various Arduino
  info sites/forums.

  This code is in the public domain.
  */

#include <SPI.h>
#include <Ethernet.h>
#include <PubNub.h>

#include <ArduinoJson.h>

using namespace ArduinoJson;


// Some Ethernet shields have a MAC address printed on a sticker on the shield;
// fill in that address here, or choose your own at random:
const static byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// Memory saving tip: remove myI and dnsI from your sketch if you
// are content to rely on DHCP autoconfiguration.
IPAddress myI(10, 42, 0, 2);
IPAddress dnsI(8, 8, 8, 8);

const static char pubkey[]  = "demo";
const static char subkey[]  = "demo";
const static char channel[] = "hello_world";

void setup()
{
    Serial.begin(9600);
    Serial.println("Serial set up");

    Ethernet.begin((byte*)mac, myI, dnsI);
    Serial.println("Ethernet set up");

    PubNub.begin(pubkey, subkey);
    Serial.println("PubNub set up");
}

JsonObject& createMessage(JsonBuffer& jb)
{
    JsonObject& msg = jb.createObject();

    JsonObject& sender      = jb.createObject();
    sender["name"]          = "Arduino";
    sender["mac_last_byte"] = mac[5];
    msg["sender"]           = sender;

    JsonArray& analog = jb.createArray();
    for (int i = 0; i < 6; i++) {
        analog.add(analogRead(i));
    }
    msg["analog"] = analog;
}

/* Process message like: { "pwm": { "8": 0, "9": 128 } } */
void processPwmInfo(JsonObject& item)
{
    JsonObject& pwm = item["pwm"];
    if (!pwm.success()) {
        Serial.println("no pwm data");
        return;
    }

    const static int pins[] = { 8, 9 };
    const static int pins_n = sizeof pins / sizeof pins[0];
    for (int i = 0; i < pins_n; ++i) {
        char pinstr[3];
        snprintf(pinstr, sizeof(pinstr), "%d", pins[i]);

        if (!pwm[pinstr].is<int>())
            continue; /* Integer Value not provided, ok. */

        Serial.print(" setting pin ");
        Serial.print(pins[i], DEC);
        int val = pwm[pinstr].as<int>();
        Serial.print(" to value ");
        Serial.println(val, DEC);
        analogWrite(pins[i], val);
    }
}

void dumpMessage(Stream& s, JsonArray& response)
{
    size_t i = 0;
    for (JsonObject& obj : response) {
        s.print("Msg #");
        s.println(i++, DEC);

        processPwmInfo(obj);

        /* Below, we parse and dump messages from fellow Arduinos. */
        JsonObject& sender = obj["sender"];
        if (!sender.success()) {
            s.println("sender not acquired");
            delay(1000);
            return;
        }

        s.print(" mac_last_byte: ");
        if (!sender.containsKey("mac_last_byte")) {
            s.println("mac_last_byte not acquired");
            delay(1000);
            return;
        }
        s.print(sender["mac_last_byte"].as<int>(), DEC);

        s.print(" A2: ");
        JsonArray& analog = sender["analog"];
        if (!analog.success()) {
            s.println("analog not acquired");
            delay(1000);
            return;
        }
        if (analog.size() < 3) {
            s.println("analog[2] not acquired");
            delay(1000);
            return;
        }
        s.print(analog[2].as<int>(), DEC);

        s.println();
    }
}

void loop()
{
    Ethernet.maintain();

    /** Additional data for copying strings */
    int const slack = 50;
    /** Max expected mesasges in a subscribe response */
    int const maxmsg = 2;
    /** Arduino JSON element capacity calculation */
    int const json_elem_size = 2 * JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(6);

    int const                  capacity = maxmsg * json_elem_size + slack;
    StaticJsonBuffer<capacity> pubjb;

    /* Publish */
    {
        Serial.print("publishing a message: ");
        JsonObject& msg = createMessage(pubjb);
        char        output[128];
        msg.printTo(output);

        Serial.println(output);
        auto client = PubNub.publish(channel, output);
        if (!client) {
            Serial.println("publishing error");
            delay(1000);
            return;
        }
        /* If you wish, you could parse the response here. */
        client->stop();
    }
    /* Subscribe and load reply */
    {
        Serial.println("waiting for a message (subscribe)");
        PubSubClient* subclient = PubNub.subscribe(channel);
        if (!subclient) {
            Serial.println("subscription error");
            delay(1000);
            return;
        }

        /* This is less code, but, requires "guessing" how
           many messages will there be in a response, otherwise
           the ArduinoJson buffer will not be large enough.
        */
        JsonArray& subjson = pubjb.parseArray(*subclient);
        if (!subjson.success()) {
            Serial.println("parse error");
            delay(1000);
            return;
        }
        dumpMessage(Serial, subjson);

        /** With some more code, use SubscribeCracker and get one
            message at a time (into a `String`) and parse each with
            ArduinoJson, where the capacity could be precisely
            `json_elem_size` (assuming you do get the message you are
            expecting).

            Of course, if you are comfortable with using
            DynamicJsonBuffer, then all this discussion about the
            static buffer is pointless.
        */
        subclient->stop();
    }
    delay(10000);
}
