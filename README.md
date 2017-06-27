# PubNub Arduino Library

This library allows your sketches to communicate with the PubNub cloud
message passing system using an Ethernet shield or any other network
hardware (chip/shield) that has a class compatible with Arduino de
facto standard `EthernetClient`. Your application can receive and send
messages.

## Copy-and-Paste-Ready Code!
See how easy it is to [Publish](examples/PubNubPublisher) and [Subscribe](examples/PubNubSubscriber)!

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

		/* Publish message. To write portable code, change `EthernetClient`
		   to `Pubnub_BASE_CLIENT`. */
		EthernetClient *pclient = PubNub.publish(pubchannel, "\"message\"");
		if (pclient)
			pclient->stop();

		/* Wait for news. */
		PubSubClient *sclient = PubNub.subscribe(subchannel);
		if (!sclient) return; // error
		char buffer[64]; size_t buflen = 0;
		while (sclient->wait_for_data()) {
			buffer[buflen++] = sclient->read();
		}
		buffer[buflen] = 0;
		sclient->stop();

		/* Print received message. You will want to look at it from
		 * your code instead. */
		Serial.println(buffer);
		delay(10000);
	}

## Library Reference

``bool PubNub.begin(char *publish_key, char *subscribe_key, char *origin)``

To start using PubNub, use PubNub.begin().  This should be called
after initializing your network hardware (like `Ethernet.begin()`).

Note that the string parameters are not copied; do not overwrite or free the
memory where you stored the keys! (If you are passing string literals, don't
worry about it.) Note that you should run only one of publish, subscribe and
history requests each at once.

The origin parameter is optional, defaulting to "pubsub.pubnub.com".

``Pubnub_BASE_CLIENT *publish(char *channel, char *message, int timeout)``

Send a message (assumed to be well-formed JSON) to a given channel.

Returns `NULL` in case of error, instead a pointer to an instance of
`Pubnub_BASE_CLIENT` class that you can use to read the reply to the
publish command. If you don't care about it, call ``client->stop()``
right away. The default `Pubnub_BASE_CLIENT` is `EthernetClient`,
and the second most used one is `WiFiClient`, but, _any_ compatible
client class will do.

Since v2.1.0, if Pubnub responds with a HTTP status code indicating a
failure, this will not return `NULL`. Of course, `NULL` will still be
returned for other errors, like, DNS failure, network failure, etc.
If you care, you should check the HTTP status code class, like:

    if (PubNub.get_last_http_status_code_class() != PubNub::http_scc_success) {
        Serial.print("Got HTTP status code error from PubNub, class: ");
        Serial.print((int)PubNub.get_last_http_status_code_class(), DEC);
	}

The timeout parameter is optional, with a sensible default. See also a
note about timeouts below.

``PubSubClient *subscribe(char *channel)``

Listen for a message on a given channel. The function will block and
return when a message arrives. NULL is returned in case of error.  The
return type is `PubSubClient`, which you can work with it exactly like
with `EthernetClient`, but it also provides an extra convenience
method ``wait_for_data()`` that allows you to wait for more data with
sensible timeout.

Typically, you will run this function from `loop()` function to keep
listening for messages indefinitely.

As a reply, if all goes well, you will get a JSON array with messages,
e.g.:

```
	["msg1",{msg2:"x"}]
```

and so on. Empty reply [] is also normal and your code must be
able to handle that. Note that the reply specifically does not
include the time token present in the raw reply from PubNub;
no need to worry about that.

The timeout parameter is optional, with a sensible default. See also a
note about timeouts below.

``Pubnub_BASE_CLIENT *history(char *channel, int limit, int timeout)``

Receive list of the last messages published on the given channel.
The limit argument is optional and defaults to 10.

The timeout parameter is optional, defaulting to 305. See also
a note about timeouts below.

This is not an officially supported API, it is there mostly for
convinience for people that used it

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
that supports an `EthernetClient` compatible class should work.

The Arduino ecosystem features a multitude of platforms that
have significant differences regarding their hardware capabilities.
Keeping up with all of them is next to impossible.

If you find some Arduino board/shield that does provide an
`EthernetClient` compatbile class and it doesn't work with Pubnub
library, let us know.

Also, if you have some Arduino board/shield that doesn't provide an
`EthernetClient` compatible class and you want to use Pubnub with it,
please let us know.

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

In previous section we already showed what to do to use ESP8266, but
in most versions of ESP8266 support for Arduino, some of the (de-facto)
standard library functions that we use are missing. To use our own
implementation of them, `#define` a macro constant before you include 
`Pubnub.h`, like this:

    #define PUBNUB_DEFINE_STRSPN_AND_STRNCASECMP
    #include <Pubnub.h>

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
  non-secure clients (say `WiFiClientSecure` instead of
  `WiFiClient`). How to do that depends on the specific hardware and
  library.

* We re-resolve the origin server IP address before each request.
  This means some slow-down for intensive communication, but we rather
  expect light traffic and very long-running sketches (days, months),
  where refreshing the IP address is quite desirable.

* We let the users read replies at their leisure instead of
  returning an already preloaded string so that (a) they can do that
  in loop() code while taking care of other things as well (b) we don't
  waste precious RAM by pre-allocating buffers that are never needed.

* If you are having problems connecting, maybe you have hit
  a bug in Debian's version of Arduino pertaining the DNS code. Try using
  an IP address as origin and/or upgrading your Arduino package.

* The optional timeout parameter allows you to specify a timeout
  period after which the subscribe call shall be retried. Note
  that this timeout is applied only for reading response, not for
  connecting or sending data; use retransmission parameters of
  the network library to tune this. As a rule of thumb, timeout
  smaller than 30 seconds may still block longer with flaky
  network. 

* The vendor firmware for the WiFi shield has dubious TCP implementation;
  for example, TCP ports of outgoing connections are always chosen from the
  same sequence, so if you reset your Arduino, some of the new connections
  may interfere with an outstanding TCP connection that has not been closed
  before the reset; i.e. you will typically see a single failed request
  somewhere down the road after a reset.

* In general, there may be many issues with different shields and
  Arduino-compatible boards. A common issue is the firmware
  version. Please look to the available info on your shield and board
  for troubleshooting.
