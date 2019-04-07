/* -*- c-file-style:"stroustrup"; indent-tabs-mode: nil -*- */
#include <ArduinoUnitTests.h>
#include "../test_stubs/Ethernet.h"
#if defined(__CYGWIN__)
#define PUBNUB_DEFINE_STRSPN_AND_STRNCASECMP
#endif
#include "../PubNubDefs.h"


unittest_setup()
{
}

unittest_teardown()
{
}

unittest(SubscribeCracker_cracks_valid_initial_response)
{
    String msg;
    /* PubNub.subscribe() API removes initial body bracket in order to prepare it for cracking */
    String body("[],\"15540679465349520\"]");
    PubSubClient subclient;
    /* subscribe response body waiting to be read and parsed */
    subclient.mGodmodeDataIn = &body;
    /* delay between received bytes in microseconds */ 
    unsigned long delay = 0;
    subclient.mGodmodeMicrosDelay = &delay;

    subclient.start_body();
    SubscribeCracker ritz(&subclient);

    assertFalse(ritz.finished());
    assertEqual(0, ritz.get(msg));
    assertEqual(0, msg.length());
    assertTrue(ritz.finished());

    assertEqual("15540679465349520", subclient.server_timetoken()); 
    subclient.stop();
}

unittest(SubscribeCracker_cracks_valid_response)
{
    String msg;
    /* PubNub.subscribe() API removes initial body bracket in order to prepare it for cracking */
    String body("[\"Hello_world\",{\"sender\":{\"name\":\"Arduino\",\"mac_last_byte\":237},\"analog\":[4095,0,255]}],\"15540677660037393\"]");
    PubSubClient subclient;
    /* subscribe response body waiting to be read and parsed */
    subclient.mGodmodeDataIn = &body;
    /* delay between received bytes in microseconds */ 
    unsigned long delay = 3;
    subclient.mGodmodeMicrosDelay = &delay;

    subclient.start_body();
    SubscribeCracker ritz(&subclient);
    
    assertFalse(ritz.finished());
    assertEqual(ritz.get(msg), 0);
    assertEqual("\"Hello_world\"", msg.c_str());
    assertFalse(ritz.finished());
    assertEqual(0, ritz.get(msg));
    assertEqual(
        "{\"sender\":{\"name\":\"Arduino\",\"mac_last_byte\":237},\"analog\":[4095,0,255]}",
        msg.c_str());
    assertFalse(ritz.finished());
    assertEqual(0, ritz.get(msg));
    assertEqual(0, msg.length());
    assertTrue(ritz.finished());

    assertEqual("15540677660037393", subclient.server_timetoken()); 
    subclient.stop();
}

unittest(PublishCracker_cracks_valid_response)
{
    /* when our API cracks publish response body we do start with its initial bracket */
    String body("[1,\"Sent\",\"15541191365593405\"]");
    PubNonSubClient client;
    /* subscribe response body waiting to be read and parsed */
    client.mGodmodeDataIn = &body;
    /* delay between received bytes in microseconds */ 
    unsigned long delay = 2;
    client.mGodmodeMicrosDelay = &delay;

    PublishCracker cheez;
    
    assertEqual(cheez.sent, cheez.read_and_parse(&client));
    assertEqual(cheez.sent, cheez.outcome());
    assertEqual("Sent", cheez.description());

    assertEqual("15541191365593405", cheez.timestamp()); 
    client.stop();
}

unittest(PublishCracker_cracks_valid_response_with_error_from_server)
{
    /* when our API cracks publish response body we do start with its initial bracket */
    String body("[0,\"Account quota exceeded (2/1000000)\",\"15541219160927237\"]");
    PubNonSubClient client;
    /* subscribe response body waiting to be read and parsed */
    client.mGodmodeDataIn = &body;
    /* delay between received bytes in microseconds */ 
    unsigned long delay = 1;
    client.mGodmodeMicrosDelay = &delay;

    PublishCracker cheez;
    
    assertEqual(cheez.failed, cheez.read_and_parse(&client));
    assertEqual(cheez.failed, cheez.outcome());
    assertEqual("Account quota exceeded (2/1000000)", cheez.description());

    assertEqual("15541219160927237", cheez.timestamp()); 
    client.stop();
}


unittest_main()
