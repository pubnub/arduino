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
inline size_t strspn(const char* cs, const char* ct) {
    size_t n;
    for (n=0; *cs; cs++, n++) {
        const char* p;
        for (p=ct; *p && *p != *cs; ++p) {
            continue;
        }
        if (*p != '\0') {
            break;
        }
    }
    return n;
}

#include <ctype.h>
inline int strncasecmp(const char *s1, const char *s2, size_t n) {
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
    , json_enabled(false)
    {
        strcpy(timetoken, "0");
    }

    /* Customized functions that make reading stop as soon as we
     * have hit ',' outside of braces and string, which indicates
     * end of JSON body. */
    int read() {
        int c = PubNub_BASE_CLIENT::read();
        if (!json_enabled || c == -1) {
            return c;
        }
        
	this->_state_input(c, 0, 0);
	return c;
    }

    int read(uint8_t *buf, size_t size) {
	int len = PubNub_BASE_CLIENT::read(buf, size);
	if (!json_enabled || len <= 0) {
            return len;
        }
	for (int i = 0; i < len; i++) {
            this->_state_input(buf[i], &buf[i+1], len - i - 1);
            if (!available() && !connected()) {
                /* We have hit the end somewhere in this buffer.
                 * From user perspective, only characters up to
                 * index i are valid. */
                return i + 1;
            }
	}
	return len;
    }

    void stop() {
	if ((!available() && !connected()) || !json_enabled) {
            PubNub_BASE_CLIENT::stop();
            return;
	}
	/* We are still connected. Read the rest of the stream so that
	 * we catch the timetoken. */
	while (wait_for_data()) {
            char ch = read();
            this->_state_input(ch, 0, 0);
	}
	json_enabled = false;
    }

    /* Block until data is available. Returns false in case the
     * connection goes down or timeout expires. */
    bool wait_for_data(int timeout = 310) {
	unsigned long t_start = millis();
	while (connected() && (0 == available())) {
            if (millis() - t_start > (unsigned long) timeout * 1000) {
                DBGprintln("wait_for_data() timeout");
                return false;
            }
	}
	return available() > 0;
    }

    /* Enable the JSON state machine. */
    void start_body() {
	json_enabled = true;
	in_string = after_backslash = false;
	braces_depth = 0;
    }

    char const *server_timetoken() const { 
        return timetoken; 
    }

private:
    inline void _state_input(uint8_t ch, uint8_t *nextbuf, size_t nextsize);
    inline void _grab_timetoken(uint8_t *nextbuf, size_t nextsize);


    /* JSON state machine context */
    bool json_enabled:1;
    bool in_string:1;
    bool after_backslash:1;
    int braces_depth;
    
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
    bool begin(const char *publish_key, const char *subscribe_key, const char *origin = "pubsub.pubnub.com") {
        d_publish_key = publish_key;
        d_subscribe_key = subscribe_key;
        d_origin = origin;
        d_uuid = 0;
        d_auth = 0;
        d_last_http_status_code_class = http_scc_unknown;
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
     * Set the UUID identification of PubNub client. This is useful
     * e.g. for presence identification.
     *
     * Pass 0 to unset. The string is not copied over (just like
     * in begin()). See the PubNubSubscriber example for simple code
     * that generates a random UUID (although not 100% reliable). 
     */
    void set_uuid(const char *uuid) {
        d_uuid = uuid;
    }
    
    /**
     * Set the authorization key/token of PubNub client. This is useful
     * e.g. for access rights validation (PAM).
     *
     * Pass 0 to unset. The string is not copied over (just like
     * in begin()). */
    void set_auth(const char *auth) {
        d_auth = auth;
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
         if (PubNub.get_last_http_status_code_class() != PubNub::http_scc_success) {
             Serial.print("Got HTTP status code error from PubNub, class: ");
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
    inline PubNub_BASE_CLIENT *publish(const char *channel, const char *message, int timeout = 30);

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
    inline PubSubClient *subscribe(const char *channel, int timeout = 310);
    
    /**
     * History
     *
     * Receive list of the last N messages on the given channel.
     *
     * @param string channel required channel name.
     * @param int limit optional number of messages to retrieve.
     * @param string timeout optional timeout in seconds.
     * @return string Stream-ish object with reply message or 0 on error. */
    inline PubNub_BASE_CLIENT *history(const char *channel, int limit = 10, int timeout = 310);

    /** Returns the HTTP status code class of the last PubNub
        transaction. If the transaction failed without getting a
        (HTTP) response, it will be "unknown".
    */
    inline http_status_code_class get_last_http_status_code_class() const {
        return d_last_http_status_code_class;
    }

private:
    enum PubNub_BH {
        PubNub_BH_OK,
        PubNub_BH_ERROR,
        PubNub_BH_TIMEOUT,
    };

    inline enum PubNub_BH _request_bh(PubNub_BASE_CLIENT &client, unsigned long t_start, int timeout, char qparsep);
    
    const char *d_publish_key;
    const char *d_subscribe_key;
    const char *d_origin;
    const char *d_uuid;
    const char *d_auth;

    /// The HTTP status code class of the last PubNub transaction
    http_status_code_class d_last_http_status_code_class;
    
    PubNub_BASE_CLIENT publish_client, history_client;
    PubSubClient subscribe_client;
};


#if defined(__AVR)
#include <avr/pgmspace.h>
#else
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

inline void PubSubClient::_state_input(uint8_t ch, uint8_t *nextbuf, size_t nextsize)
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
                goto body_end;
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
                goto body_end;
            return;
        default:
            return;
        }
    }
    
    return;
body_end:
    /* End of data here. */
    this->_grab_timetoken(nextbuf, nextsize);
}


inline void PubSubClient::_grab_timetoken(uint8_t *nextbuf, size_t nextsize)
{
    char new_timetoken[22] = { '\0' };
    size_t new_timetoken_len = 0;
    unsigned long t_start = millis();
    const unsigned long timeout = 310000UL;
    
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
                continue;
            }
            PubNub_BASE_CLIENT::read(&ch, sizeof ch);
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


inline PubNub_BASE_CLIENT *PubNub::publish(const char *channel, const char *message, int timeout)
{
    PubNub_BASE_CLIENT &client = publish_client;
    unsigned long t_start;
    int have_param = 0;
    
retry:
    t_start = millis();
    /* connect() timeout is about 30s, much lower than our usual
     * timeout is. */
    int rslt = client.connect(d_origin, 80);
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
    const char *pmessage = message;
    while (pmessage[0]) {
        /* RFC 3986 Unreserved characters plus few
         * safe reserved ones. */
        size_t okspan = strspn(pmessage, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~" ",=:;@[]");
        if (okspan > 0) {
            client.write((const uint8_t *) pmessage, okspan);
            pmessage += okspan;
        }
        if (pmessage[0]) {
            /* %-encode a non-ok character. */
            char enc[3] = {'%'};
            enc[1] = "0123456789ABCDEF"[pmessage[0] / 16];
            enc[2] = "0123456789ABCDEF"[pmessage[0] % 16];
            client.write((const uint8_t *) enc, 3);
            pmessage++;
        }
    }
    
    if (d_auth) {
        client.print(have_param ? '&' : '?');
        client.print("auth=");
        client.print(d_auth);
        have_param = 1;
    }
    
    enum PubNub::PubNub_BH ret = this->_request_bh(client, t_start, timeout, have_param ? '&' : '?');
    switch (ret) {
    case PubNub_BH_OK:
        return &client;
    case PubNub_BH_ERROR:
        client.stop();
        while (client.connected()) ;
        return 0;
    case PubNub_BH_TIMEOUT:
        client.stop();
        while (client.connected()) ;
        goto retry;
    }
}


inline PubSubClient *PubNub::subscribe(const char *channel, int timeout)
{
    PubSubClient &client = subscribe_client;
    unsigned long t_start;
    int have_param = 0;
    
retry:
    t_start = millis();
    /* connect() timeout is about 30s, much lower than our usual
     * timeout is. */
    if (!client.connect(d_origin, 80)) {
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
    
    enum PubNub::PubNub_BH ret = this->_request_bh(client, t_start, timeout, have_param ? '&' : '?');
    switch (ret) {
    case PubNub_BH_OK:
        /* Success and reached body. We need to eat '[' first,
         * as our API contract is to return only the "message body"
         * part of reply from subscribe. */
        if (!client.wait_for_data() || client.read() != '[') {
            DBGprintln("Unexpected body in subscribe response");
            client.stop();
            while (client.connected()) ;
            return 0;
        }
        /* Now return handle to the client for further perusal.
         * PubSubClient class will make sure that the client does
         * not see the time token but we stop right after the
         * message body. */
        client.start_body();
        return &client;
        
    case PubNub_BH_ERROR:
        client.stop();
        while (client.connected()) ;
        return 0;
        
    case PubNub_BH_TIMEOUT:
        client.stop();
        while (client.connected()) ;
        goto retry;
    }
}


inline PubNub_BASE_CLIENT *PubNub::history(const char *channel, int limit, int timeout)
{
    PubNub_BASE_CLIENT &client = history_client;
    unsigned long t_start;
    
retry:
    t_start = millis();
    if (!client.connect(d_origin, 80)) {
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
        client.stop();
        while (client.connected()) ;
        return 0;
    case PubNub_BH_TIMEOUT:
        client.stop();
        while (client.connected()) ;
        goto retry;
    }
}


inline enum PubNub::PubNub_BH PubNub::_request_bh(PubNub_BASE_CLIENT &client, unsigned long t_start, int timeout, char qparsep)
{
    /* Finish the first line of the request. */
    client.print(qparsep);
    client.print("pnsdk=PubNub-Arduino/1.0 HTTP/1.1\r\n");
    /* Finish HTTP request. */
    client.print("Host: ");
    client.print(d_origin);
    client.print("\r\nUser-Agent: PubNub-Arduino/1.0\r\nConnection: close\r\n\r\n");
    
#define WAIT() do {                   \
	while (!client.available()) {              \
            /* wait, just check for timeout */                          \
            if (millis() - t_start > (unsigned long) timeout * 1000) {  \
                DBGprintln("Timeout in bottom half");                   \
                return PubNub_BH_TIMEOUT;                               \
            }                                                           \
            if (!client.connected()) {                                  \
                /* Oops, connection interrupted. */                     \
                DBGprintln("Connection reset in bottom half");          \
                return PubNub_BH_ERROR;                                 \
            }                                                           \
	}                                                               \
    } while (0)
    
    /* Read first line with HTTP code. */
    /* "HTTP/1.x " */
    do {
        WAIT();
    } while (client.read() != ' ');
    /* Now, first digit of HTTP code. */
    WAIT();
    char c = client.read();
    d_last_http_status_code_class = static_cast<http_status_code_class>(c - '0');

    /* Now, we enter in a state machine that shall guide us through
     * the remaining headers to the beginning of the body. */
    enum {
        RS_SKIPLINE, /* Skip the rest of this line. */
        RS_LOADLINE, /* Try loading the line in a buffer. */
    } request_state = RS_SKIPLINE; /* Skip the rest of status line first. */
    bool chunked = false;
    
    while (client.connected() || client.available()) {
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
            char line[sizeof(chunked_str)]; /* Not NUL-terminated! */
            int linelen = 0;
            char ch = 0;
            do {
                WAIT();
                ch = client.read();
                line[linelen++] = ch;
                if (linelen == strlen_P(chunked_str)
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
            else if (linelen == 2 && line[0] == '\r') {
                /* Empty line. This means headers end. */
                break;
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
