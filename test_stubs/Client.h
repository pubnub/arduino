#ifndef stub_client_h
#define stub_client_h

#include "Stream.h"

class Client : public Stream {

public:
    Client() {}
    
    using Print::write;
    
    virtual size_t write(uint8_t aChar)
    {
        mGodmodeDataOut.append(String((char)aChar));
        return 1;
    }

    String getOuttaHere() {
        String ret(mGodmodeDataOut);
        mGodmodeDataOut.clear();
        return ret;
    }

    virtual void flush()
    {
        mGodmodeDataOut.clear();
    }
private:
    String mGodmodeDataOut;
};

#endif /* stub_client_h */
