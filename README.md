# PubNub Arduino Library

This library allows your sketches to communicate with the PubNub cloud
message passing system using any network hardware (chip/shield) that
has a class compatible with Arduino de facto standard `Client`. Your 
application can receive (subscribe to) and send (publish) messages.

## Copy-and-Paste-Ready Code!
See how easy it is to [Publish](examples/PubNubPublisher) 
and [Subscribe](examples/PubNubSubscriber)!

### Synopsis


    void setup() {
        /* For debugging, set to speed of your choice */
        Serial.begin(9600);

        /* If you use some other HW, you need to do some other
           initialization of it here... */
        Ethernet.begin(mac);

        /* Start the Pubnub library by giving it a publish and subscribe
           keys */
        PubNub.begin(pubkey, subkey);
    }

    void loop() {
        /* Maintain DHCP lease. For other HW, you may need to do
           something else here, or maybe nothing at all. */
        Ethernet.maintain();

        /* Publish message. You could use `auto` here... */
        PubNonSubClient *pclient = PubNub.publish(pubchannel, "\"message\"");
        if (!pclient) return;
        PublishCracker cheez;
        cheez.read_and_parse(pclient);
        /** You're mostly interested in `outcome()`, and,
            if it's "failed", then `description()`. 
        */
        Serial.print("Outcome: "); Serial.print(cheez.outcome());
        Serial.print(' '); Serial.println(cheez.to_str(cheez.outcome()));
        Serial.print("description: "); Serial.println(cheez.description());
        Serial.print("timestamp: "); Serial.println(cheez.timestamp());
        Serial.print("state: "); Serial.print(cheez.state());
        Serial.print(' '); Serial.println(cheez.to_str(cheez.state()));
        pclient->stop();

        /* Wait for news. */
        PubSubClient *sclient = PubNub.subscribe(subchannel);
        if (!sclient) return; // error
        String msg;
        SubscribeCracker ritz(sclient);
        while (!ritz.finished()) {
            ritz.get(msg);
            if (msg.length() > 0) {
                Serial.print("Received: "); Serial.println(msg);
            }
        }
        sclient->stop();

        delay(1000);
    }

## Upgrading from version 2

The `publish()` method now returns PubNub's "own" `Client` compatible
class (that is, pointer to an object of said class). It used to return
a pointer to the network client class used. In most cases, your sketch
will continue to work, only in some special cases (like ESP32), where
the implementation of the client class is not actually conforming to
the "contract" of the `Cient` interface, will you need to update to
make things work.

BTW, if you used `auto` instead of naming the class, there will be no
need to update.

Same goes for `history()`, it now returns PubNub's own Client.

But, `subscribe()` does not need any update, it still returns
`PubSubClient*`.

It is now recommended to use the higher level interface for parsing
the response returned from PubNub, which is introduced in version
3.0.0. It is much easier to use. You can still use the "low level"
interface, that is, the (only) one that was available in previous
versions. So, while no upgrade is required, you will probably benefit
from upgrading, in most cases.

## Library Reference

``bool PubNub.begin(char *publish_key, char *subscribe_key, char *origin)``

To start using PubNub, use PubNub.begin().  This should be called
after initializing your network hardware (like `Ethernet.begin()`).

Note that the string parameters are not copied; do not overwrite or free the
memory where you stored the keys! (If you are passing string literals, don't
worry about it.) Note that you should run only one of publish, subscribe and
history requests each at once.

The origin parameter is optional, defaulting to "pubsub.pubnub.com".

``PubNonSubClient *publish(char *channel, char *message, int timeout)``

Send a message (assumed to be well-formed JSON) to a given channel.

Returns `NULL` in case of error, otherwise a pointer to an instance of
`Client`-compatible class that you can use to read the response to the
publish command. If you don't care about the response, call
``client->stop()`` right away.

Since v2.1.0, if Pubnub responds with a HTTP status code indicating a
failure, this will not return `NULL`. Of course, `NULL` will still be
returned for other errors, mostly networking errors like DNS failure,
connection failure, etc.  If you care, you should check the HTTP
status code class, like:

    if (PubNub.get_last_http_status_code_class() != PubNub::http_scc_success) {
        Serial.print("Got HTTP status code error from PubNub, class: ");
        Serial.print((int)PubNub.get_last_http_status_code_class(), DEC);
    }

The timeout parameter is optional, with a sensible default. See also a
note about timeouts below.

To avoid parsing the response, you should use `PublishCracker` "on"
the result of this member function.

``PubSubClient *subscribe(char *channel, int timeout)``

Listen for a message on a given channel. The function will block and
return when message(s) arrive(s) (or timeout expires). NULL is
returned in case of error.  The return type is `PubSubClient`, which
is `Client` compatible, but it also provides an extra convenience
method ``wait_for_data()`` that allows you to wait for more data with
sensible timeout.

Typically, you will run this function from `loop()` function to keep
listening for messages indefinitely.

As a reply, if all goes well, you will get a JSON array with messages,
e.g.:

```
    ["msg1",{msg2:"x"}]
```

and so on. Empty reply (`[]`) is also normal and your code must be
able to handle that - it means no messages were pulished during a
time(out) set on the PubNub side. Note that the reply specifically
does not include the time token present in the raw reply from PubNub;
it is filtered out by `PubSubClient`.

The timeout parameter is optional, with a sensible default. See also a
note about timeouts below.

To avoid parsing the response, you should use `SubscribeCracker` "on"
the result of this member function.


``PubNonSubClient *history(char *channel, int limit, int timeout)``

Receive list of the last messages published on the given channel.  The
limit argument is optional and defaults to 10. Keep in mind that
PubNub network has its own limit, which was 100 at the time of this
writing. Thus, even if you set `limit` to something higher than that
(say `1000`) you will not actually get that many messages.

The timeout parameter is optional, with sensible default. See also
a note about timeouts below.

### Message crackers

These are used to interpret/parse the response from Pubnub, so that
you don't have to. Their interface is much easier to use than the "low
level" (essentially `Client`) interface. This parsing is minimal and
non-validating, mostly to "tell elements of the response apart", thus
they are named "message crackers" (and not "parsers" or
"interpreters"), which might make for an interesting piece of
nostalgia to users familiar w/WinAPI.

Each API (publish, subscribe...) has its own class, because the format
of the PubNub response is different. But, for some groups of classes
(APIs), the user interface is essentially the same.

``PublishCracker``

Just declare an object, call `read_and_parse()` on it and then
use the "getters" to see the parts of the message:

* `outcome()`: to see if the publish succeeded or not. For logging,
  use `to_str()` to get a string "representation" of the outcome.
* `description()`: to get the description of the outcome as returned,
  in the response, by PubNub
* `timestamp()`: to get the string of the timestamp (token) of the
  moment the publish was executed, as returned by PubNub. In general,
  this is seldom interesting.
* `state()` to see if parsing is complete (`done`). For logging,
  use `to_str()` to get a string "representation" of the state.

If you want more control, you can read the response characters
yourself and use `handle()` to pass them to the parser/cracker,
(instead of using `read_and_parse()`).

``SubscribeCracker``

Declare an object passing the `PubSubClient` you got from
`subscribe()`.  Then, until parsing is `finished()`, call `get()` to
obtain the next message in the PubNub response. Keep in mind that one
subscribe can yield more than one message in the response, as more
than one message might have been published on the channel(s) you are
subscribing to between two calls to `subscribe()`.

If you want more control, you can read the characters of the response
yourself and use `handle()` to pass them to the parser/cracker,
(instead of using `get()`).

To read the timetoken that was returned in the PubNub response, use
`PubNub::server_timetoken()`, as the timetoken is filtered by
`PubSubClient`.

``HistoryCracker``

The usage is essentially the same as `SubscribeCracker`.


### Debug logging

To enable debugg logging to the Arduino console, add

    #define PUBNUB_DEBUG

before `#include <PubNub.h>`

## Installation

Since version 1.1.1, Pubnub SDK is part of the Arduino Library
Manager and you can use it directly from Arduino IDE (v 1.6.8
or newer).

But, sometimes Arduino online repository for its Library manager takes
time to update to new releases of Pubnub SDK, so, you might want to
install it manually. To do so, download a release from
[Arduino SDK on Github](https://github.com/pubnub/arduino/) and move
the contents to your Arduino libraries directory (on Linux, default
would be: `~/sketchbook/libraries/PubNub/`) and restart your Arduino
IDE.  Try out the examples!

Keep in mind that if you both install the library via Arduino Library
Manager _and_ manually, and the versions mismatch, Arduino IDE will
issue warnings like:

    Invalid version found: x.y.z

Where `x.y.z` would be the version ID ofthe manually installed library.
This is just a warning, the build and upload process is not impacted in
any way by this.


## Supported Hardware

In general, the most widely available Arduino boards and shields are
supported and tested. Any Arduino board that has networking hardware
that supports an `Client` compatible class should work. In most cases,
they are actually derived from `Client`, but there are some subtle
differences in the base `Client` as implemented in various libraries.

The Arduino ecosystem features a multitude of platforms that
have significant differences regarding their hardware capabilities.
Keeping up with all of them is next to impossible.

If you find some Arduino board/shield that does provide an `Client`
compatbile class and it doesn't work with Pubnub library, let us
know. In general, this means that it is not _really_ compatible. Such
was the case with ESP32 library.

Also, if you have some Arduino board/shield that doesn't provide an
`Client` compatible class and you want to use Pubnub with it, please
let us know.

### Arduino Ethernet Shield

For this to work, all you need to do is to include the Ethernet
Shield Arduino library and start your sketch with:

    #include <EthernetClient>
    #include <PubNub.h>

As `EthernetClient` is the default `Pubnub_BASE_CLIENT`.

Of course, you also need to initialize the shield and do any
maintenance (like DHCP lease).


### WiFi (Shield) Support

Whether you are using the older WiFi shield or the
[Arduino WiFi Shield 101](https://www.arduino.cc/en/Main/ArduinoWiFiShield101),
you will be using the `WiFiClient` class. Keep in mind that the
WiFi101 library is used with other shields/boards (Arduino MKR1000,
Adafruit Feather M0 WINC1500...) and that `WiFiClient` is the name
of the client class for most Wifi hardware even if it uses another
library.

So, for any WiFi101 compatible hardware, you would:

    #include <WiFi101.h>
    #define PubNub_BASE_CLIENT WiFiClient
    #include <PubNub.h>

For hadware that doesn't use WiFi101 library, but provides a
`WiFiClient` class, like ESP8266, you would:

    #include <ESP8266WiFi.h>
    #define PubNub_BASE_CLIENT WiFiClient
    #include <PubNub.h>

Of course, please keep in mind that you need to initialize your WiFi
hardware, connect to a WiFi network and possibly do some maintenance,
which is hardware specific. But, Pubnub SDK has nothing to do with
that, it expects a working network.  We provide examples for some HW.

### ESP8266

In previous section we already showed how to use ESP8266, but in some
(older) versions of ESP8266 support for Arduino, some of the
(de-facto) standard library functions that we use are missing. To use
our own implementation of them, `#define` a macro constant before you
include `PubNub.h`, like this:

    #define PUBNUB_DEFINE_STRSPN_AND_STRNCASECMP
    #include <PubNub.h>

## Notes

* If you `#include <PubNub.h>`, it will define the global `PubNub`
  object in your code. Thus, you can't `#include <Pubnub.h>` in two or
  more different files in you project, but only in one file. In all
  other source files (if you have them) `#include <PubNubDefs.h>`,
  which doesn't define the global `PubNub` object. This shouldn't be
  much of an inconvenience, as most Arduino projects have only one
  file - the sketch itself.

* We don't provide any SSL/TLS support, because of modest resource of
  most Arduino compatible boards. But, some shields/boards have SSL
  ("Secure") clients and you may succeed in using them instead of the
  non-secure clients (`WiFiClientSecure` instead of `WiFiClient`).
  But don't forget to `PubNub.set_port(PubNub.tls_port)`.

* We re-resolve the origin server IP address before each request.
  This means some slow-down for intensive communication, but we rather
  expect light traffic and very long-running sketches (days, months),
  where refreshing the IP address is quite desirable.

* We let the users read replies at their leisure instead of
  returning an already preloaded string so that (a) they can do that
  in loop() code while taking care of other things as well (b) we don't
  waste precious RAM by pre-allocating buffers that are never needed.

* The optional timeout parameter allows you to specify a timeout
  period after which the subscribe call shall be cancelled. Note
  that this timeout is applied only for reading response, not for
  connecting or sending data; use retransmission parameters of
  the network library to tune this. As a rule of thumb, timeout
  smaller than 30 seconds may still block longer with flaky
  network.

* In general, there may be many issues with different shields and
  Arduino-compatible boards. A common issue is a firmware bug. Please
  look to the available info on your shield and board for
  troubleshooting.
