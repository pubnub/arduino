#ifndef stub_ethernet_h_
#define stub_ethernet_h_

#include "Client.h"

#define WL_CONNECTED 3

class EthernetClient : public Client {
public:
	EthernetClient() { }
/* Functions and class fields commented out are not currently used by 'pubnub' arduino
   unit tests but, they are the original EthernetClient class members and there is a
   possibility that some of them might become stubs in the future.
 */
//	EthernetClient(uint8_t s) : sockindex(s), _timeout(1000) { }

	uint8_t status()
    {
        return WL_CONNECTED;
    }
//	virtual int connect(IPAddress ip, uint16_t port);
	virtual int connect(const char *host, uint16_t port)
    {
        return +1;
    }
//	virtual int availableForWrite(void);
	using Stream::available;
	using Stream::read;
	using Stream::peek;
	using Client::write;
    using Client::flush;
	virtual int read(uint8_t *buf, size_t size)
    {
        return (int)readBytes(buf, size);
    }
	virtual void stop()
    {
    }
	virtual uint8_t connected()
    {
        return +1;
    }
//	virtual operator bool() { return sockindex < MAX_SOCK_NUM; }
//	virtual bool operator==(const bool value) { return bool() == value; }
//	virtual bool operator!=(const bool value) { return bool() != value; }
//	virtual bool operator==(const EthernetClient&);
//	virtual bool operator!=(const EthernetClient& rhs) { return !this->operator==(rhs); }
//	uint8_t getSocketNumber() const { return sockindex; }
//	virtual uint16_t localPort();
//	virtual IPAddress remoteIP();
//	virtual uint16_t remotePort();
//	virtual void setConnectionTimeout(uint16_t timeout) { _timeout = timeout; }

//	friend class EthernetServer;
private:
//	uint8_t sockindex; // MAX_SOCK_NUM means client not in use
//	uint16_t _timeout;
};


#endif /* stub_ethernet_h_ */
