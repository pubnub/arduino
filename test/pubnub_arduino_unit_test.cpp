/* -*- c-file-style:"stroustrup"; indent-tabs-mode: nil -*- */
#include <ArduinoUnitTests.h>
#include "../test_stubs/Ethernet.h"
#define PUBNUB_UNIT_TEST
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

unittest(PubNub_subscribe)    
{
    String msg;
    PubNub PubNubObject;
    String request("GET /subscribe/airliner/flight/0/0?pnsdk=PubNub-Arduino/1.0 HTTP/1.1\r\n"
                   "Host: pubsub.pubnub.com\r\n"
                   "User-Agent: PubNub-Arduino/1.0\r\n"
                   "Connection: close\r\n"
                   "\r\n");
    String response("HTTP/1.1 200 OK\r\n"
                    "Date: Mon, 01 Apr 2019 18:07:10 GMT\r\n"
                    "Content-Type: text/javascript; charset=\"UTF-8\"\r\n"
                    "Content-Length: 24\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "[[],\"15541420302549923\"]");
    unsigned long delay = 1;
    /* Preparing input data and receiving delay conditions for transaction under test */
    PubNubObject.subscribeClient().mGodmodeDataIn = &response;
    PubNubObject.subscribeClient().mGodmodeMicrosDelay = &delay;
    assertEqual(true, PubNubObject.begin("jet", "airliner"));
    assertEqual(PubNub::http_scc_unknown, PubNubObject.get_last_http_status_code_class());

    auto subclient = PubNubObject.subscribe("flight");
    assertEqual(PubNub::http_scc_success, PubNubObject.get_last_http_status_code_class());
    assertEqual(request, subclient->getOuttaHere());
    
    SubscribeCracker ritz(subclient);

    assertFalse(ritz.finished());
    assertEqual(0, ritz.get(msg));
    assertEqual(0, msg.length());
    assertTrue(ritz.finished());
    assertEqual("15541420302549923", subclient->server_timetoken()); 

    request = String("GET /subscribe/airliner/flight/0/15541420302549923"
                     "?uuid=xyzxyzxy-xxxx-4444-9999-xxxxxxxxxxxx"
                     "&auth=caribean"
                     "&pnsdk=PubNub-Arduino/1.0 HTTP/1.1\r\n"
                     "Host: pubsub.pubnub.com\r\n"
                     "User-Agent: PubNub-Arduino/1.0\r\n"
                     "Connection: close\r\n"
                     "\r\n");
    response = String("HTTP/1.1 200 OK\r\n"
                      "Date: Mon, 01 Apr 2019 18:07:10 GMT\r\n"
                      "Content-Type: text/javascript; charset=\"UTF-8\"\r\n"
                      "Content-Length: 88\r\n"
                      "Connection: close\r\n"
                       "\r\n"
                      "[[{\"latitud\":17.05,\"longitud\":61.50,\"country\":\"Antigua & Barbuda\"}],\"15541618056552715\"]");
    PubNubObject.set_uuid("xyzxyzxy-xxxx-4444-9999-xxxxxxxxxxxx");
    PubNubObject.set_auth("caribean");
    PubNubObject.subscribe("flight");
    assertEqual(PubNub::http_scc_success, PubNubObject.get_last_http_status_code_class());
    assertEqual(request, subclient->getOuttaHere());

    ritz = SubscribeCracker(subclient);

    assertFalse(ritz.finished());
    assertEqual(0, ritz.get(msg));
    assertEqual("{\"latitud\":17.05,\"longitud\":61.50,\"country\":\"Antigua & Barbuda\"}",
                msg.c_str());
    assertFalse(ritz.finished());
    assertEqual(0, ritz.get(msg));
    assertEqual(0, msg.length());
    assertTrue(ritz.finished());
    assertEqual("15541618056552715", subclient->server_timetoken()); 

    subclient->stop();
}

unittest(PubNub_publish)    
{
    PubNub PubNubObject;
    String request("GET /publish/jet/airliner/0/flight/0/package%21"
                   "?pnsdk=PubNub-Arduino/1.0 HTTP/1.1\r\n"
                   "Host: pubsub.pubnub.com\r\n"
                   "User-Agent: PubNub-Arduino/1.0\r\n"
                   "Connection: close\r\n"
                   "\r\n");
    String response("HTTP/1.1 200 OK\r\n"
                    "Date: Tue, 02 Apr 2019 02:30:30 GMT\r\n"
                    "Content-Type: text/javascript; charset=\"UTF-8\"\r\n"
                    "Content-Length: 30\r\n"
                    "Connection: close\r\n"
                     "\r\n"
                    "[1,\"Sent\",\"15541724007473323\"]");
    unsigned long delay = 1;
    /* Preparing input data and receiving delay conditions for transaction under test */
    PubNubObject.publishClient().mGodmodeDataIn = &response;
    PubNubObject.publishClient().mGodmodeMicrosDelay = &delay;
    assertEqual(true, PubNubObject.begin("jet", "airliner"));
    assertEqual(PubNub::http_scc_unknown, PubNubObject.get_last_http_status_code_class());

    auto client = PubNubObject.publish("flight","package!");
    assertEqual(PubNub::http_scc_success, PubNubObject.get_last_http_status_code_class());
    assertEqual(request, client->getOuttaHere());

    PublishCracker cheez;

    assertEqual(cheez.sent, cheez.read_and_parse(client));
    assertEqual(cheez.sent, cheez.outcome());
    assertEqual("Sent", cheez.description());
    assertEqual("15541724007473323", cheez.timestamp()); 

    request = String("GET /publish/jet/airliner/0/flight/0/round%20trip"
                     "?auth=atlantic"
                     "&pnsdk=PubNub-Arduino/1.0 HTTP/1.1\r\n"
                     "Host: pubsub.pubnub.com\r\n"
                     "User-Agent: PubNub-Arduino/1.0\r\n"
                     "Connection: close\r\n"
                     "\r\n");
    response = String("HTTP/1.1 400 INVALID\r\n"
                      "Date: Tue, 02 Apr 2019 02:30:31 GMT\r\n"
                      "Content-Type: text/javascript; charset=\"UTF-8\"\r\n"
                      "Content-Length: 60\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "[0,\"Account quota exceeded (2/1000000)\",\"15541733686301100\"]");
    PubNubObject.set_uuid("abcdefgh-xxxx-3333-8888-oooooooooooo");
    PubNubObject.set_auth("atlantic");
    PubNubObject.publish("flight", "round trip");
    assertEqual(PubNub::http_scc_client_error, PubNubObject.get_last_http_status_code_class());
    assertEqual(request, client->getOuttaHere());

    cheez = PublishCracker();
    
    assertEqual(cheez.failed, cheez.read_and_parse(client));
    assertEqual(cheez.failed, cheez.outcome());
    assertEqual("Account quota exceeded (2/1000000)", cheez.description());
    assertEqual("15541733686301100", cheez.timestamp()); 

    client->stop();
}

unittest(PubNub_history)    
{
    String msg;
    PubNub PubNubObject;
    String request("GET /history/date/retro/0/10"
                   "?pnsdk=PubNub-Arduino/1.0 HTTP/1.1\r\n"
                   "Host: pubsub.pubnub.com\r\n"
                   "User-Agent: PubNub-Arduino/1.0\r\n"
                   "Connection: close\r\n"
                   "\r\n");
    String response("HTTP/1.1 200 OK\r\n"
                    "Date: Tue, 02 Apr 2019 02:30:31 GMT\r\n"
                    "Content-Type: text/javascript; charset=\"UTF-8\"\r\n"
                    "Content-Length: 74\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "[{\"rocket\":\"Saturn V\",\"mission\":\"Apolo 11\"},\"The Eagle has landed\",\"1969\"]");
    unsigned long delay = 1;
    /* Preparing input data and receiving delay conditions for transaction under test */
    PubNubObject.historyClient().mGodmodeDataIn = &response;
    PubNubObject.historyClient().mGodmodeMicrosDelay = &delay;
    assertEqual(true, PubNubObject.begin("book", "date"));
    assertEqual(PubNub::http_scc_unknown, PubNubObject.get_last_http_status_code_class());

    auto client = PubNubObject.history("retro");
    assertEqual(PubNub::http_scc_success, PubNubObject.get_last_http_status_code_class());
    assertEqual(request, client->getOuttaHere());

    HistoryCracker smoki(client);

    assertFalse(smoki.finished());
    assertEqual(0, smoki.get(msg));
    assertEqual("{\"rocket\":\"Saturn V\",\"mission\":\"Apolo 11\"}", msg.c_str());
    assertFalse(smoki.finished());
    assertEqual(0, smoki.get(msg));
    assertEqual("\"The Eagle has landed\"", msg.c_str());
    assertFalse(smoki.finished());
    assertEqual(0, smoki.get(msg));
    assertEqual("\"1969\"", msg.c_str());
    assertFalse(smoki.finished());
    assertEqual(0, smoki.get(msg));
    assertEqual(0, msg.length());
    assertTrue(smoki.finished());

    request = String("GET /history/date/retro/0/2"
                     "?pnsdk=PubNub-Arduino/1.0 HTTP/1.1\r\n"
                     "Host: pubsub.pubnub.com\r\n"
                     "User-Agent: PubNub-Arduino/1.0\r\n"
                     "Connection: close\r\n"
                     "\r\n");
    response = String("HTTP/1.1 200 OK\r\n"
                      "Date: Tue, 02 Apr 2019 02:30:31 GMT\r\n"
                      "Content-Type: text/javascript; charset=\"UTF-8\"\r\n"
                      "Content-Length: 40\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "[\"radio\",{\"materials\":\"semiconductors\"}]");
    PubNubObject.set_uuid("bright-smile-5555-7777-oooooooooooo");
    PubNubObject.set_auth("palm-trees");
    PubNubObject.history("retro", 2);
    assertEqual(PubNub::http_scc_success, PubNubObject.get_last_http_status_code_class());
    assertEqual(request, client->getOuttaHere());

    smoki = HistoryCracker(client);
    
    assertFalse(smoki.finished());
    assertEqual(0, smoki.get(msg));
    assertEqual("\"radio\"", msg.c_str());
    assertFalse(smoki.finished());
    assertEqual(0, smoki.get(msg));
    assertEqual("{\"materials\":\"semiconductors\"}", msg.c_str());
    assertFalse(smoki.finished());
    assertEqual(0, smoki.get(msg));
    assertEqual(0, msg.length());
    assertTrue(smoki.finished());

    client->stop();
}


unittest_main()
