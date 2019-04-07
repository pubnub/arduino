/* -*- c-file-style:"stroustrup"; indent-tabs-mode: nil -*- */
#ifndef PubNub_h
#define PubNub_h


#include <stdint.h>
#include <stdlib.h>
#include <string.h>


#if !defined(PubNub_BASE_CLIENT)
#define PubNub_BASE_CLIENT EthernetClient
#endif

#ifdef PUBNUB_DEBUG
#define DBGprint(x...) Serial.print(x)
#define DBGprintln(x...) Serial.println(x)
#else
#define DBGprint(x...)
#define DBGprintln(x...)
#endif


/* Under some board support libraries, like ESP8266,
   the (de-facto) standard library functions are missing.
   To use Pubnub library with those boards, you need to
   define the following preprocessor macro-constant.
   */
#if defined(PUBNUB_DEFINE_STRSPN_AND_STRNCASECMP)
inline size_t strspn(const char* cs, const char* ct)
{
    size_t n;
    for (n = 0; *cs; cs++, n++) {
        const char* p;
        for (p = ct; *p && *p != *cs; ++p) {
            continue;
        }
        if (*p == '\0') {
            break;
        }
    }
    return n;
}

#include <ctype.h>
inline int strncasecmp(const char* s1, const char* s2, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i) {
        const int diff = tolower(s1[i]) - tolower(s2[i]);
        if ((diff != 0) || (s1[i] == '\0')) {
            return diff;
        }
    }
    return 0;
}
#endif

/** This is a very thin Arduino #Client interface wrapper.
    It's reason d'^etre is the fact that some clients,
    namely the WiFiClient for ESP32, drops the available()
    count to 0 on connection close. So, if you don't read
    the incoming octets fast enough, you won't read them
    at all (if you observe the result of available()).

 */
class PubNonSubClient : public PubNub_BASE_CLIENT {
public:
    PubNonSubClient()
        : PubNub_BASE_CLIENT()
        , d_avail(0)
    {
    }

    int available()
    {
        if (0 == d_avail) {
            d_avail = PubNub_BASE_CLIENT::available();
        }
        return d_avail;
    }
    int read()
    {
        if (d_avail > 0) {
            --d_avail;
        }
        return PubNub_BASE_CLIENT::read();
    }
    int read(uint8_t* buf, size_t size)
    {
        int len = PubNub_BASE_CLIENT::read(buf, size);
        if (d_avail > len) {
            if (len > 0) {
                d_avail -= len;
            }
        }
        else {
            d_avail = 0;
        }
        return len;
    }

private:
    int d_avail;
};


/* This class is a thin #EthernetClient (in general, any class that
 * implements the Arduino #Client "interface") wrapper whose
 * goal is to automatically acquire time token information when
 * reading subscribe call response.
 *
 * The user application sees only the JSON body, not the timetoken.
 * As soon as the body ends, PubSubclient reads the rest of HTTP reply
 * itself and disconnects. The stored timetoken is used in the next call
 * to the PubNub::subscribe() method.
 */
class PubSubClient : public PubNub_BASE_CLIENT {
public:
    PubSubClient()
        : PubNub_BASE_CLIENT()
        , d_avail(0)
        , json_enabled(false)
    {
        strcpy(timetoken, "0");
    }

    int available()
    {
        if (0 == d_avail) {
            d_avail = PubNub_BASE_CLIENT::available();
        }
        return d_avail;
    }

    /* Customized functions that make reading stop as soon as we
     * have hit ',' outside of braces and string, which indicates
     * end of JSON body. */
    int read()
    {
        int c = PubNub_BASE_CLIENT::read();
        if (c != -1) {
            if (d_avail > 0) {
                --d_avail;
            }
        }
        if (!json_enabled || c == -1) {
            return c;
        }

        this->_state_input(c, 0, 0);
        return c;
    }

    int read(uint8_t* buf, size_t size)
    {
        int len = PubNub_BASE_CLIENT::read(buf, size);

        if (d_avail > len) {
            if (len > 0) {
                d_avail -= len;
            }
        }
        else {
            d_avail = 0;
        }
        if (!json_enabled || len <= 0) {
            return len;
        }
        for (int i = 0; i < len; i++) {
            this->_state_input(buf[i], buf + i + 1, len - i - 1);
        }
        return len;
    }

    void stop()
    {
        if ((!available() && !connected()) || !json_enabled) {
            PubNub_BASE_CLIENT::stop();
            return;
        }
        /* We are still connected. Read the rest of the stream so that
         * we catch the timetoken. */
        while (wait_for_data(10)) {
            read();
        }
        json_enabled = false;
    }

    /* Block until data is available. Returns false in case the
     * connection goes down or timeout expires. */
    bool wait_for_data(int timeout = 310)
    {
        unsigned long t_start = millis();
        while ((0 == available()) && connected()) {
            if (millis() - t_start > (unsigned long)timeout * 1000) {
                DBGprintln("wait_for_data() timeout");
                return false;
            }
            delay(10);
        }
        return available() > 0;
    }

    /* Enable the JSON state machine. */
    void start_body()
    {
        json_enabled = true;
        in_string = after_backslash = false;
        braces_depth                = 0;
    }

    char const* server_timetoken() const { return timetoken; }

private:
    inline void _state_input(uint8_t ch, uint8_t* nextbuf, size_t nextsize);
    inline void _grab_timetoken(uint8_t* nextbuf, size_t nextsize);

    int d_avail;

    /* JSON state machine context */
    bool json_enabled : 1;
    bool in_string : 1;
    bool after_backslash : 1;
    int  braces_depth;

    /* Time token acquired during the last subscribe request. */
    char timetoken[22];
};


class PubNub {
public:
    /**
     * Init the Pubnub Client API
     *
     * This should be called after Ethernet.begin().
     * Note that the string parameters are not copied; do not
     * overwrite or free the memory where you stored the keys!
     * (If you are passing string literals, don't worry about it.)
     * Note that you should run only a single publish at once.
     *
     * @param string publish_key required key to send messages.
     * @param string subscribe_key required key to receive messages.
     * @param string origin optional setting for cloud origin.
     * @return boolean whether begin() was successful.
     */
    bool begin(const char* publish_key,
               const char* subscribe_key,
               const char* origin = "pubsub.pubnub.com")
    {
        d_publish_key                 = publish_key;
        d_subscribe_key               = subscribe_key;
        d_origin                      = origin;
        d_uuid                        = 0;
        d_auth                        = 0;
        d_last_http_status_code_class = http_scc_unknown;
        set_port(http_port);
        return true;
    }

    /**
     * HTTP status code class. It is defined by the first digit of the
     * HTTP status code.
     *
     * @see RFC 7231 Section 6
     */
    enum http_status_code_class {
        /** This is not defined in the RFC, we use it to indicate
            "none/unknown" */
        http_scc_unknown = 0,
        /** The request was received, continuing process */
        http_scc_informational = 1,
        /** The request was successfully received, understood, and
            accepted */
        http_scc_success = 2,
        /** Further action needs to be taken in order to complete the
            request */
        http_scc_redirection = 3,
        /** The request contains bad syntax or cannot be fulfilled */
        http_scc_client_error = 4,
        /** The server failed to fulfill an apparently valid
            request */
        http_scc_server_error = 5
    };

    /**
     * Possible TCP/IP ports to use when connecting to Pubnub.
     */
    enum port_for_connection {
        /** Connect via HTTP on its default port */
        http_port,
        /** Connect via TLS (formerly known as SSL) on its default port) */
        tls_port
    };

    /**
     * Set the UUID identification of PubNub client. This is useful
     * e.g. for presence identification.
     *
     * Pass 0 to unset. The string is not copied over (just like
     * in begin()). See the PubNubSubscriber example for simple code
     * that generates a random UUID (although not 100% reliable).
     */
    void set_uuid(const char* uuid) { d_uuid = uuid; }

    /**
     * Set the authorization key/token of PubNub client. This is useful
     * e.g. for access rights validation (PAM).
     *
     * Pass 0 to unset. The string is not copied over (just like
     * in begin()). */
    void set_auth(const char* auth) { d_auth = auth; }

    /**
     * Set the TCP/IP port to use when connecting to Pubnub.
     * Basically, only call this if your client supports SSL/TLS
     * as you will need to set the `tls_port`.
     */
    void set_port(port_for_connection port) {
        switch (port) {
        case http_port:
        default:
            d_port = 80;
            break;
        case tls_port:
            d_port = 443;
            break;
        }
    }

    /**
     * Publish/Send a message (assumed to be well-formed JSON) to a
     * given channel.
     *
     * Note that the reply can be obtained using code like:

         client = PubNub.publish("demo", "\"lala\"");
         if (!client) {
             Serial.println("Failed to publish, got no response from PubNub");
             return;
         }
         if (PubNub.get_last_http_status_code_class() !=
     PubNub::http_scc_success) { Serial.print("Got HTTP status code error from
     PubNub, class: ");
             Serial.print((int)PubNub.get_last_http_status_code_class(), DEC);
         }
         while (client->connected()) {
         // More sophisticated code will want to add timeout handling here
         while (client->connected() && !client->available()) ; // wait
             char c = client->read();
             Serial.print(c);
         }
         client->stop();

     * You will get content right away, the header has been already
     * skipped inside the function. If you do not care about
     * the reply, just call client->stop(); immediately.
     *
     * It returns an object that is typically EthernetClient (but it
     * can be a WiFiClient if you enabled the WiFi shield).
     *
     * @param string channel required channel name.
     * @param string message required message string in JSON format.
     * @param string timeout optional timeout in seconds.
     * @return string Stream-ish object with reply message or 0 on error.
     */
    inline PubNonSubClient* publish(const char* channel,
                                    const char* message,
                                    int         timeout = 30);

    /**
     * Subscribe/Listen for a message on a given channel. The function
     * will block and return when a message arrives. Typically, you
     * will run this function from loop() function to keep listening
     * for messages indefinitely.
     *
     * As a reply, you will get a JSON array with messages, e.g.:
     * 	["msg1",{msg2:"x"}]
     * and so on. Empty reply [] is also normal and your code must be
     * able to handle that. Note that the reply specifically does not
     * include the time token present in the raw reply.
     *
     * @param string channel required channel name.
     * @param string timeout optional timeout in seconds.
     * @return string Stream-ish object with reply message or 0 on error.
     */
    inline PubSubClient* subscribe(const char* channel, int timeout = 310);

    /**
     * History
     *
     * Receive list of the last N messages on the given channel.
     *
     * @param string channel required channel name.
     * @param int limit optional number of messages to retrieve.
     * @param string timeout optional timeout in seconds.
     * @return string Stream-ish object with reply message or 0 on error. */
    inline PubNonSubClient* history(const char* channel,
                                    int         limit   = 10,
                                    int         timeout = 310);

    /** Returns the HTTP status code class of the last PubNub
        transaction. If the transaction failed without getting a
        (HTTP) response, it will be "unknown".
    */
    inline http_status_code_class get_last_http_status_code_class() const
    {
        return d_last_http_status_code_class;
    }

#if defined(PUBNUB_UNIT_TEST)
    inline PubNonSubClient& publishClient() { return publish_client; }
    inline PubNonSubClient& historyClient() { return history_client; };
    inline PubSubClient& subscribeClient() { return subscribe_client; }
#endif /* PUBNUB_UNIT_TEST */    

private:
    enum PubNub_BH {
        PubNub_BH_OK,
        PubNub_BH_ERROR,
        PubNub_BH_TIMEOUT,
    };

    inline enum PubNub_BH _request_bh(PubNub_BASE_CLIENT& client,
                                      unsigned long       t_start,
                                      int                 timeout,
                                      char                qparsep);

    const char* d_publish_key;
    const char* d_subscribe_key;
    const char* d_origin;
    const char* d_uuid;
    const char* d_auth;

    /// TCP/IP port to use.
    unsigned d_port;

    /// The HTTP status code class of the last PubNub transaction
    http_status_code_class d_last_http_status_code_class;

    PubNonSubClient publish_client, history_client;
    PubSubClient    subscribe_client;
};


#if defined(__AVR)
#include <avr/pgmspace.h>
#elif !defined(strncasecmp_P) || defined(PUBNUB_DEFINE_STRSPN_AND_STRNCASECMP)
#define strncasecmp_P(a, b, c) strncasecmp(a, b, c)
#endif


/* There are some special considerations when using the WiFi libary,
 * compared to the Ethernet library:
 *
 * (i) The client object may return stale data from previous connection,
 * so we should call .flush() after initiating a connection.
 *
 * (ii) It appears .stop() does not block on really terminating
 * the connection, do that manually.
 *
 * (iii) Data may still be available while connected() returns false
 * already; use available() test on a lot of places where we used
 * connected() before.
 */

inline void PubSubClient::_state_input(uint8_t ch, uint8_t* nextbuf, size_t nextsize)
{
    /* Process a single character on input, updating the JSON
     * state machine. If we reached the last character of input
     * (just before expected ","), we will eat the rest of the body,
     * update timetoken and close the connection. */
    if (in_string) {
        if (after_backslash) {
            /* Whatever this is... */
            after_backslash = false;
            return;
        }
        switch (ch) {
        case '"':
            in_string = false;
            if (braces_depth == 0)
                this->_grab_timetoken(nextbuf, nextsize);
            return;
        case '\\':
            after_backslash = true;
            return;
        default:
            return;
        }
    }
    else {
        switch (ch) {
        case '"':
            in_string = true;
            return;
        case '{':
        case '[':
            braces_depth++;
            return;
        case '}':
        case ']':
            braces_depth--;
            if (braces_depth == 0)
                this->_grab_timetoken(nextbuf, nextsize);
            return;
        default:
            return;
        }
    }

    return;
}


inline void PubSubClient::_grab_timetoken(uint8_t* nextbuf, size_t nextsize)
{
    char                new_timetoken[22] = { '\0' };
    size_t              new_timetoken_len = 0;
    unsigned long       t_start           = millis();
    const unsigned long timeout           = /*3*/ 10000UL;

    enum NTTState {
        await_comma,
        await_quote,
        read_timetoken,
        done
    } state = await_comma;

    /* Expected followup now is:
     * 	,"13511688131075270"]
     */
    while (state != done) {
        uint8_t ch;

        if (millis() - t_start > timeout) {
            DBGprintln("Timeout while reading timetoken");
            return;
        }
        if (nextsize > 0) {
            ch = *nextbuf++;
            --nextsize;
        }
        else {
            if (0 == available()) {
                if (!connected()) {
                    DBGprintln("Lost connection while reading timetoken");
                    return;
                }
                delay(10);
            }
            // Some clients (actually, quite a few) implement
            // character read via array read, like:
            //
            //     int read() { uint8_t c; read(&c, 1); return c;}
            ///
            // and have both reads virtual. So, if we call such a
            // `read()`, it will call the virtual "array read", which
            // is actually _our_ array read, which will then call
            // _state_input(), in a sort of unwanted recursion.
            //
            // @todo: Of course, this does _not_ solve such problems
            // in general, as some other client might do the opposite,
            // implement array read via character read! Thus, this
            // whole idea of inheriting Client classes is wrong (as
            // inheritance often is) and needs to be refactored.
            int len = PubNub_BASE_CLIENT::read(&ch, 1);
            if (len != 1) {
                continue;
            }
            if (d_avail > 0) {
                --d_avail;
            }
        }

        switch (state) {
        case await_comma:
            if (',' == ch) {
                state = await_quote;
            }
            break;
        case await_quote:
            if ('"' == ch) {
                state = read_timetoken;
            }
            break;
        case read_timetoken:
            if (ch == '"') {
                state = done;
                break;
            }
            new_timetoken[new_timetoken_len++] = ch;
            if (new_timetoken_len >= sizeof(new_timetoken) - 1) {
                /* TODO: handle this as a kind of error */
                state = done;
                break;
            }
            break;
        default:
            break;
        }
    }

    if (new_timetoken_len > 0) {
        memcpy(timetoken, new_timetoken, new_timetoken_len);
    }
    timetoken[new_timetoken_len] = 0;
}

inline bool await_disconnect(PubNub_BASE_CLIENT& client, unsigned long timeout) {
    unsigned long    t_start = millis();
    while (client.connected()) {
        if (millis() - t_start > timeout * 1000UL) {
            return false;
        }
        delay(10);
    }
    return true;
}


inline PubNonSubClient* PubNub::publish(const char* channel,
                                        const char* message,
                                        int         timeout)
{
    PubNonSubClient& client = publish_client;
    int              have_param = 0;
    unsigned long    t_start = millis();

    /* connect() timeout is about 30s, much lower than our usual
     * timeout is. */
    int rslt = client.connect(d_origin, d_port);
    if (rslt != 1) {
        DBGprint("Connection error ");
        DBGprintln(rslt);
        client.stop();
        return 0;
    }

    d_last_http_status_code_class = http_scc_unknown;
    client.flush();
    client.print("GET /publish/");
    client.print(d_publish_key);
    client.print("/");
    client.print(d_subscribe_key);
    client.print("/0/");
    client.print(channel);
    client.print("/0/");

    /* Inject message, URI-escaping it in the process.
     * We are careful to save RAM by not using any copies
     * of the string or explicit buffers. */
    const char* pmessage = message;
    while (pmessage[0]) {
        /* RFC 3986 Unreserved characters plus few
         * safe reserved ones. */
        size_t okspan = strspn(
            pmessage,
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~"
            ",=:;@[]");
        if (okspan > 0) {
            client.write((const uint8_t*)pmessage, okspan);
            pmessage += okspan;
        }
        if (pmessage[0]) {
            /* %-encode a non-ok character. */
            char enc[3] = { '%' };
            enc[1]      = "0123456789ABCDEF"[pmessage[0] / 16];
            enc[2]      = "0123456789ABCDEF"[pmessage[0] % 16];
            client.write((const uint8_t*)enc, 3);
            pmessage++;
        }
    }

    if (d_auth) {
        client.print(have_param ? '&' : '?');
        client.print("auth=");
        client.print(d_auth);
        have_param = 1;
    }

    enum PubNub::PubNub_BH ret =
        this->_request_bh(client, t_start, timeout, have_param ? '&' : '?');
    switch (ret) {
    case PubNub_BH_OK:
        return &client;
    case PubNub_BH_ERROR:
        DBGprintln("publish() BH_ERROR");
        client.stop();
        if (!await_disconnect(client, 10)) {
            DBGprintln("publish() BH_ERROR: disconnect timeout");
        }
        return 0;
    case PubNub_BH_TIMEOUT:
        DBGprintln("publish() BH_TIMEOUT");
        client.stop();
        if (!await_disconnect(client, 10)) {
            DBGprintln("publish() BH_TIMEOUT: disconnect timeout");
        }
        return 0;
    }
    return 0;
}


inline PubSubClient* PubNub::subscribe(const char* channel, int timeout)
{
    PubSubClient& client = subscribe_client;
    int           have_param = 0;
    unsigned long t_start = millis();

    /* connect() timeout is about 30s, much lower than our usual
     * timeout is. */
    if (!client.connect(d_origin, d_port)) {
        DBGprintln("Connection error");
        client.stop();
        return 0;
    }

    d_last_http_status_code_class = http_scc_unknown;
    client.flush();
    client.print("GET /subscribe/");
    client.print(d_subscribe_key);
    client.print("/");
    client.print(channel);
    client.print("/0/");
    client.print(client.server_timetoken());
    if (d_uuid) {
        client.print("?uuid=");
        client.print(d_uuid);
        have_param = 1;
    }
    if (d_auth) {
        client.print(have_param ? '&' : '?');
        client.print("auth=");
        client.print(d_auth);
        have_param = 1;
    }

    enum PubNub::PubNub_BH ret =
        this->_request_bh(client, t_start, timeout, have_param ? '&' : '?');
    switch (ret) {
    case PubNub_BH_OK:
        /* Success and reached body. We need to eat '[' first,
         * as our API contract is to return only the "message body"
         * part of reply from subscribe. */
        if (!client.wait_for_data()) {
            DBGprintln("No data received!");
            client.stop();
            if (!await_disconnect(client, 10)) {
                DBGprintln("subscribe() no data received: disconnect timeout");
            }
            return 0;
        }
        if (client.read() != '[') {
            DBGprintln("Unexpected body in subscribe response");
            client.stop();
            if (!await_disconnect(client, 10)) {
                DBGprintln("subscribe() unexpected body: disconnect timeout");
            }
            return 0;
        }
        /* Now return handle to the client for further perusal.
         * PubSubClient class will make sure that the client does
         * not see the time token but we stop right after the
         * message body. */
        client.start_body();
        return &client;

    case PubNub_BH_ERROR:
        DBGprintln("subscribe() BH_ERROR");
        client.stop();
        if (!await_disconnect(client, 10)) {
            DBGprintln("subscribe() BH_ERROR: disconnect timeout");
        }
        return 0;

    case PubNub_BH_TIMEOUT:
        DBGprintln("subscribe() BH_TIMEOUT");
        client.stop();
        DBGprintln("subscribe() BH_TIMEOUT stopped");
        if (await_disconnect(client, 10)) {
            DBGprintln("subscribe() BH_TIMEOUT: disconnected");
        }
        else {
            DBGprintln("subscribe() BH_TIMEOUT: disconnect timeout");
        }
        return 0;
    }
    return 0;
}


inline PubNonSubClient* PubNub::history(const char* channel, int limit, int timeout)
{
    PubNonSubClient& client = history_client;
    unsigned long    t_start = millis();

    if (!client.connect(d_origin, d_port)) {
        DBGprintln("Connection error");
        client.stop();
        return 0;
    }

    d_last_http_status_code_class = http_scc_unknown;
    client.flush();
    client.print("GET /history/");
    client.print(d_subscribe_key);
    client.print("/");
    client.print(channel);
    client.print("/0/");
    client.print(limit, DEC);

    enum PubNub::PubNub_BH ret = this->_request_bh(client, t_start, timeout, '?');
    switch (ret) {
    case PubNub_BH_OK:
        return &client;
    case PubNub_BH_ERROR:
        DBGprintln("history() BH_ERROR");
        client.stop();
        if (!await_disconnect(client, 10)) {
            DBGprintln("history() BH_ERROR: disconnect timeout");
        }
        return 0;
    case PubNub_BH_TIMEOUT:
        DBGprintln("history() BH_TIMEOUT");
        client.stop();
        if (!await_disconnect(client, 10)) {
            DBGprintln("history() BH_TIMEOUT: disconnect timeout");
        }
        return 0;
    }
    return 0;
}

/** A helper that "cracks" the messages from an array of them.
    It is, essentially, a simple, non-validating parser of
    a JSON array, yielding individual elements of said array.
*/
class MessageCracker {
public:
    enum State {
        bracket_open,
        ground_zero,
        in_message,
        in_quotes,
        malformed,
        done
    };

    MessageCracker()
        : d_state(bracket_open)
    {
    }

    void handle(char c, String& msg)
    {
        switch (d_state) {
        case bracket_open:
            if ('[' == c) {
                d_state = ground_zero;
                msg.remove(0);
            }
            break;
        case ground_zero:
            switch (c) {
            case '{':
            case '[':
                d_bracket_level = 1;
                d_state         = in_message;
                msg.concat(c);
                break;
            case '"':
                d_bracket_level = 0;
                d_state         = in_quotes;
                d_backslash     = false;
                msg.concat(c);
                break;
            case ',':
                d_bracket_level = 0;
                d_state         = in_message;
                break;
            case ']':
                d_state = done;
                break;
            default:
                d_bracket_level = 0;
                d_state         = in_message;
                msg.concat(c);
                break;
            }
            break;
        case in_quotes:
            switch (c) {
            case '"':
                if (!d_backslash) {
                    d_state = (0 == d_bracket_level) ? ground_zero : in_message;
                }
                else {
                    d_backslash = false;
                }
                msg.concat(c);
                break;
            case '\\':
                d_backslash = true;
                msg.concat(c);
                break;
            default:
                msg.concat(c);
                break;
            }
            break;
        case in_message:
            switch (c) {
            case '{':
            case '[':
                ++d_bracket_level;
                msg.concat(c);
                break;
            case '"':
                d_state     = in_quotes;
                d_backslash = false;
                msg.concat(c);
                break;
            case '}':
            case ']':
                if (0 == d_bracket_level) {
                    d_state = done;
                }
                else {
                    if (--d_bracket_level == 0) {
                        d_state = ground_zero;
                    }
                    msg.concat(c);
                }
                break;
            default:
                msg.concat(c);
                break;
            }
            break;
        case malformed:
        case done:
        default:
            break;
        }
    }

    State state() const { return d_state; }

    bool msg_complete(String& msg) const
    {
        return (msg.length() > 0) && (ground_zero == state());
    }

private:
    /** Parser/cracker state */
    State d_state;
    /** Current bracket level - starts at 0 */
    size_t d_bracket_level;
    /** Are we inside quotes (string) */
    bool d_in_quotes;
    /** Was the last character a backslash? Valid only inside quotes */
    bool d_backslash;
};

/** This assumes that the received message is valid JSON.  If it is
    not, nothing will crash or burn, but, it might parse in an
    unexpected way.
 */
class SubscribeCracker {
public:
    enum State { cracking, bracket_close, malformed, done };

    SubscribeCracker(PubSubClient* psc)
        : d_psc(psc)
        , d_state(cracking)
    {
    }

    /** Low-level interface, handles one incoming/response character
        at a time. To see if a message has been "cracked out" of the
        response, use `message_complete()`.
    */
    void handle(char c, String& msg)
    {
        switch (d_state) {
        case cracking:
            d_crack.handle(c, msg);
            if (d_crack.state() == d_crack.done) {
                d_state = bracket_close;
            }
            break;
        case bracket_close:
            switch (c) {
            case ']':
                d_state = done;
                break;
            default:
                d_state = malformed;
                break;
            }
            break;
        case malformed:
        case done:
        default:
            break;
        }
    }

    /** Returns whether the parsing of the whole response is
     * complete/finished */
    bool finished() const
    {
        switch (d_state) {
        case malformed:
        case done:
            return true;
        default:
            return false;
        }
    }

    /** Returns whether the `msg` has been "cracked out" of the
        response (you can use it, it is complete).
    */
    bool message_complete(String& msg) const
    {
        return d_crack.msg_complete(msg);
    }

    /** Get's the next message, reading from the client interface.
     */
    int get(String& msg)
    {
        msg.remove(0);
        while (!finished() && !message_complete(msg)) {
            if (!d_psc->wait_for_data()) {
                break;
            }
            // @todo For yet unexplained reasons, things stop working if we
            // (for performance reasons) try an "array read" here . We get a
            // "connection lost while getting timetoken". Should
            // investigate.
            handle(d_psc->read(), msg);
        }
        if ((done == state()) || (d_crack.state() == d_crack.ground_zero)) {
            return 0;
        }
        else {
            return -1;
        }
    }

    /** Current parsing state. In general, you don't need it, but, it
        could be useful for debugging. */
    State state() const { return d_state; }

private:
    /** Client to read incoming response from */
    PubSubClient* d_psc;
    /** Current cracker/parser state */
    State d_state;
    /** The message array cracker */
    MessageCracker d_crack;
};


/** This is _very_ similar to SubcribeCracker and has the same
    user-interface.
*/
class HistoryCracker {
public:
    HistoryCracker(PubNonSubClient* pnsc)
        : d_pnsc(pnsc)
    {
    }

    bool finished() const { return d_crack.state() == d_crack.done; }

    int get(String& msg)
    {
        msg.remove(0);
        int retry = 5;
        while (!finished() && !d_crack.msg_complete(msg)) {
            // Could try array read here, but, it's tricky, would
            // need to keep the state of "read but unhandled characters".
            if (d_pnsc->available()) {
                d_crack.handle(d_pnsc->read(), msg);
            }
            else {
                if (--retry <= 0) {
                    break;
                }
                delay(10);
            }
        }
        if ((d_crack.done == d_crack.state())
            || (d_crack.state() == d_crack.ground_zero)) {
            return 0;
        }
        else {
            return -1;
        }
    }


private:
    /** Client to read incoming response from */
    PubNonSubClient* d_pnsc;
    /** The message array cracker */
    MessageCracker d_crack;
};

/** Used for (minimal) parsing of the response to publish.
    It assumes a valid response.
 */
class PublishCracker {
public:
    /** State of the parser. In general, user is concerned
        whether its `done` or not.
    */
    enum State {
        bracket_open,
        result,
        comma_description,
        quote_description,
        description_chars,
        comma_timestamp,
        quote_timestamp,
        timestamp_chars,
        bracket_close,
        done
    };
    /** The outcome of the publish - success, fail
        or "unknown" (IOW: did not finish parsing)
    */
    enum Outcome { sent, failed, unknown };

    /** You _don't_ provide the client here, the interface doesn't
        require it, so it would just take up space in our object.
    */
    PublishCracker()
        : d_state(bracket_open)
        , d_outcome(unknown)
    {
        d_description[0] = '\0';
        d_timestamp[0]   = '\0';
    }

    /** Low level interface - handles one character at a time.  Check
        `state() == done` to know when parsing is complete.
     */
    void handle(char c)
    {
        switch (d_state) {
        case bracket_open:
            if ('[' == c) {
                d_state = result;
            }
            break;
        case result:
            if ('1' == c) {
                d_outcome = sent;
                d_state   = comma_description;
            }
            else if ('0' == c) {
                d_outcome = failed;
                d_state   = comma_description;
            }
            else if (isdigit(c)) {
                DBGprint("Unexpected publish result: ");
                DBGprintln(c);
                d_outcome = failed;
                d_state   = comma_description;
            }
            break;
        case comma_description:
            if (',' == c) {
                d_state = quote_description;
            }
            break;
        case quote_description:
            if ('"' == c) {
                d_state  = description_chars;
                d_desc_i = 0;
            }
            break;
        case description_chars:
            /* Currently, no description uses backslash */
            if ('"' == c) {
                d_state                 = comma_timestamp;
                d_description[d_desc_i] = '\0';
            }
            else {
                if (d_desc_i < MAX_DESCRIPTION) {
                    d_description[d_desc_i++] = c;
                    d_description[d_desc_i]   = '\0';
                }
            }
            break;
        case comma_timestamp:
            if (',' == c) {
                d_state = quote_timestamp;
            }
            break;
        case quote_timestamp:
            if ('"' == c) {
                d_state = timestamp_chars;
                d_ts_i  = 0;
            }
            break;
        case timestamp_chars:
            if ('"' == c) {
                d_state             = bracket_close;
                d_timestamp[d_ts_i] = '\0';
            }
            else {
                if (d_ts_i < MAX_DESCRIPTION) {
                    d_timestamp[d_ts_i++] = c;
                    d_timestamp[d_ts_i]   = '\0';
                }
            }
            break;
        case bracket_close:
            if (']' == c) {
                d_state = done;
            }
            break;
        case done:
        default:
            break;
        }
    }

    /** Simple wrapper for `handle(c)` if you read
        an array of characters.
    */
    void handle(uint8_t const* s, size_t n)
    {
        for (size_t i = 0; i < n; ++i) {
            handle(s[i]);
        }
    }

    /** The "high level" interface, just call this and it will
        read and parse the response.
    */
    Outcome read_and_parse(PubNonSubClient* pnsc)
    {
        uint8_t minibuf[MAX_DESCRIPTION];
        int     retry = 5;
        while (state() != done) {
            int len = pnsc->read(minibuf, MAX_DESCRIPTION);
            if (len > 0) {
                handle(minibuf, len);
            }
            else {
                if (--retry <= 0) {
                    break;
                }
                delay(10);
            }
        }
        return outcome();
    }

    State   state() const { return d_state; }
    Outcome outcome() const { return d_outcome; }

    char const* description() const { return d_description; }

    char const* timestamp() const { return d_timestamp; }
    char const* to_str(Outcome e)
    {
        switch (e) {
        case sent:
            return "Sent";
        case failed:
            return "Failed";
        case unknown:
            return "Unknown";
        default:
            return "!?!";
        }
    }
    char const* to_str(State e)
    {
        switch (e) {
        case bracket_open:
            return "Bracket open";
        case result:
            return "Result";
        case comma_description:
            return "Comma before description";
        case quote_description:
            return "Quote before description";
        case description_chars:
            return "Characters of description";
        case comma_timestamp:
            return "Comma before timestamp";
        case quote_timestamp:
            return "Quote before timestamp";
        case timestamp_chars:
            return "Characters/digits of timestamp";
        case bracket_close:
            return "Bracket close";
        case done:
            return "Done.";
        }
        return "!?!";
    }
    enum { MAX_DESCRIPTION = 50, MAX_TIMESTAMP = 20 };

private:
    /** Current parser/cracker state */
    State d_state;
    /** Outcome of the publish as reported by PubNub */
    Outcome d_outcome;
    /** Statically allocated string of the description of
        the publish outcome as reported by PubNub */
    char d_description[MAX_DESCRIPTION + 1];
    /** Current position inside `d_description`, will
        write next char there */
    size_t d_desc_i;
    /** Statically allocated string of the timestamp/token of
        the publish  as reported by PubNub */
    char d_timestamp[MAX_TIMESTAMP + 1];
    /** Current position inside `d_timestamp`, will
        write next char there */
    size_t d_ts_i;
};


inline enum PubNub::PubNub_BH PubNub::_request_bh(PubNub_BASE_CLIENT& client,
                                                  unsigned long       t_start,
                                                  int                 timeout,
                                                  char                qparsep)
{
    /* Finish the first line of the request. */
    client.print(qparsep);
    client.print("pnsdk=PubNub-Arduino/1.0 HTTP/1.1\r\n");
    /* Finish HTTP request. */
    client.print("Host: ");
    client.print(d_origin);
    client.print(
        "\r\nUser-Agent: PubNub-Arduino/1.0\r\nConnection: close\r\n\r\n");

#define WAIT()                                                                 \
    do {                                                                       \
        while (0 == client.available()) {                                      \
            /* wait, just check for timeout */                                 \
            if (millis() - t_start > (unsigned long)timeout * 1000) {          \
                DBGprintln("Timeout in bottom half");                          \
                return PubNub_BH_TIMEOUT;                                      \
            }                                                                  \
            if (!client.connected()) {                                         \
                /* Oops, connection interrupted. */                            \
                DBGprintln("Connection reset in bottom half");                 \
                return PubNub_BH_ERROR;                                        \
            }                                                                  \
            delay(10);                                                         \
        }                                                                      \
    } while (0)

    /* Read first line with HTTP code. */
    /* "HTTP/1.x " */
    do {
        WAIT();
    } while (client.read() != ' ');

    /* Now, first digit of HTTP code. */
    WAIT();

    char c                        = client.read();
    d_last_http_status_code_class = static_cast<http_status_code_class>(c - '0');

    /* Now, we enter in a state machine that shall guide us through
     * the remaining headers to the beginning of the body. */
    enum {
        RS_SKIPLINE,               /* Skip the rest of this line. */
        RS_LOADLINE,               /* Try loading the line in a buffer. */
    } request_state = RS_SKIPLINE; /* Skip the rest of status line first. */
    bool chunked    = false;

    while (client.available()) {
        /* Let's hope there is no stray LF without CR. */
        if (request_state == RS_SKIPLINE) {
            do {
                WAIT();
            } while (client.read() != '\n');
            request_state = RS_LOADLINE;
        }
        else { /* request_state == RS_LOADLINE */
            /* line[] must be enough to hold
             * Transfer-Encoding: chunked (or \r\n) */
            const static char chunked_str[] = "Transfer-Encoding: chunked\r\n";

            char     line[sizeof(chunked_str)]; /* Not NUL-terminated! */
            unsigned linelen = 0;
            char     ch      = 0;
            do {
                WAIT();
                ch              = client.read();
                line[linelen++] = ch;
                if (linelen == (sizeof chunked_str - 1)
                    && !strncasecmp_P(line, chunked_str, linelen)) {
                    /* Chunked encoding header. */
                    chunked = true;
                    break;
                }
            } while (ch != '\n' && linelen < sizeof(line));
            if (ch != '\n') {
                /* We are not at the end of the line yet.
                 * Skip the rest of the line. */
                request_state = RS_SKIPLINE;
            }
            else {

                if (linelen == 2 && line[0] == '\r') {
                    /* Empty line. This means headers end. */
                    break;
                }
            }
        }
    }

    if (chunked) {
        /* There is one extra line due to Transfer-encoding: chunked.
         * Our minimalistic support means that we hope for just
         * a single chunk, just skip the first line after header. */
        do {
            WAIT();
        } while (client.read() != '\n');
    }

    /* Body begins now. */
    return PubNub_BH_OK;
}


#endif
