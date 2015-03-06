//TO DO:  Differentiate additional statuses ... current code should be good?
//        Count errors, reset MCU after n consecutive errors
//        Component name and ID part of send() call ... Looks like only comp ID needed. -- DONE

//GroveStreams Class
#include "GroveStreams.h"

//Constructor
GroveStreams::GroveStreams(const char* serverName, const __FlashStringHelper* apiKey, int ledPin)
{
    _serverName = serverName;
    _apiKey = apiKey;
    _ledPin = ledPin;
}

//Initialize GroveStreams
void GroveStreams::begin(void)
{
    int ret = dnsLookup(_serverName, serverIP);
    if (ret == 1) {
        Serial << millis() << F(" GroveStreams ") << serverIP << endl;
    }
    else {
        Serial << millis() << F(" GS DNS lookup fail, ret=") << ret << endl;
        mcuReset();
    }
    ipToText(_localIP, Ethernet.localIP());
    ipToText(_groveStreamsIP, serverIP);
}

enum gsState_t { GS_WAIT, GS_SEND, GS_RECV, GS_DISCONNECT } GS_STATE;

//GroveStreams state machine
ethernetStatus_t GroveStreams::run(void)
{
    ethernetStatus_t ret = NO_STATUS;
    const char httpOKText[] = "HTTP/1.1 200 OK";
    static char statusBuf[sizeof(httpOKText)];

    switch (GS_STATE)
    {
    case GS_WAIT:    //wait for next send
        break;

    case GS_SEND:
        if ( _xmit() == PUT_COMPLETE ) {
            _msLastPacket = millis();    //initialize receive timeout
            GS_STATE = GS_RECV;
            ret = PUT_COMPLETE;
        }
        else {
            GS_STATE = GS_WAIT;
            ret = CONNECT_FAILED;
        }
        break;

    case GS_RECV:
        {
        boolean haveStatus = false;
        boolean httpOK = false;

        if(client.connected()) {
            uint16_t nChar = client.available();
            if (nChar > 0) {
                _msLastPacket = millis();
                Serial << _msLastPacket << F(" received packet, len=") << nChar << endl;
                char* b = statusBuf;
                for (uint16_t i = 0; i < nChar; i++) {
                    char ch = client.read();
                    Serial << _BYTE(ch);
                    if ( !haveStatus && i < sizeof(statusBuf) ) {
                        if ( ch == '\r' || i == sizeof(statusBuf) - 1 ) {
                            haveStatus = true;
                            *b++ = 0;
                            if (strncmp(statusBuf, httpOKText, sizeof(httpOKText)) == 0) {
                                httpOK = true;
                                ret = HTTP_OK;
//                                Serial << endl << endl << millis() << F(" HTTP OK") << endl;
                            }
                            else {
                                ret = HTTP_OTHER;
                                Serial << endl << endl << millis() << F(" HTTP STATUS: ") << statusBuf << endl;
                            }
                        }
                        else {
                            *b++ = ch;
                        }
                    }
                }
            }
            //if too much time has elapsed since the last packet, time out and close the connection from this end
            else if (millis() - _msLastPacket >= RECEIVE_TIMEOUT) {
                ++timeout;
                _msLastPacket = millis();
                Serial << endl << _msLastPacket << F(" Recv timeout") << endl;
                client.stop();
                if (_ledPin >= 0) digitalWrite(_ledPin, LOW);
                GS_STATE = GS_DISCONNECT;
                ret = TIMEOUT;
            }
        }
        else {
            GS_STATE = GS_DISCONNECT;
            ret = DISCONNECTING;
        }
        break;
    }

    case GS_DISCONNECT:
        // close client end
        _msDisconnecting = millis();
        Serial << _msDisconnecting << F(" disconnecting") << endl;
        client.stop();
        if (_ledPin >= 0) digitalWrite(_ledPin, LOW);
        _msDisconnected = millis();
        respTime = _msLastPacket - _msPutComplete;
        discTime = _msDisconnected - _msDisconnecting;
        Serial << _msDisconnected << F(" disconnected") << endl;
        GS_STATE = GS_WAIT;
        ret = DISCONNECTED;
        break;
    }
    if (ret != NO_STATUS) lastStatus = ret;
    return ret;
}

//request data to be sent. returns 0 if accepted.
//returns -1 if e.g. transmission already in progress, waiting response, etc.
ethernetStatus_t GroveStreams::send(const char* compID, const char* data)
{
    if (GS_STATE == GS_WAIT) {
        _compID = compID;
        _data = data;
        GS_STATE = GS_SEND;
        lastStatus = SEND_ACCEPTED;
    }
    else {
        lastStatus = SEND_BUSY;
    }
    return lastStatus;
}

//Send data to GroveStreams
ethernetStatus_t GroveStreams::_xmit(void)
{
    ethernetPacket packet;
    
    _msConnect = millis();
    Serial << _msConnect << F(" connecting") << F(" WDTCSR=0x") << _HEX(WDTCSR) << endl;
    if (_ledPin >= 0) digitalWrite(_ledPin, HIGH);
    if ( client.connect(serverIP, serverPort) ) {
        _msConnected = millis();
        Serial << _msConnected << F(" connected") << endl;
        freeMem = freeMemory();
//        client << F("PUT /api/feed?&api_key=") << _apiKey << F("&compId=") << _compID;
//        client << _data << F(" HTTP/1.1\nHost: ") << serverIP << F("\nConnection: close\nX-Forwarded-For: ");
//        client << Ethernet.localIP() << F("\nContent-Type: application/json\n\n");
        packet.putChar( F("PUT /api/feed?&api_key=") );
        packet.putChar(_apiKey);
        packet.putChar( F("&compId=") );
        packet.putChar(_compID);
        packet.putChar(_data);
        packet.putChar( F(" HTTP/1.1\nHost: ") );
        packet.putChar(_groveStreamsIP);
        packet.putChar( F("\nConnection: close\nX-Forwarded-For: ") );
        packet.putChar(_localIP);
        packet.putChar( F("\nContent-Type: application/json\n\n") );
        packet.flush();
        _msPutComplete = millis();
        Serial << _msPutComplete << F(" PUT complete ") << strlen(_data) << endl;
        connTime = _msConnected - _msConnect;
        lastStatus = PUT_COMPLETE;
    }
    else {
        _msConnected = millis();
        connTime = _msConnected - _msConnect;
        Serial << _msConnected << F(" connect failed") << endl;
        if (_ledPin >= 0) digitalWrite(_ledPin, LOW);
        lastStatus = CONNECT_FAILED;
    }
    return lastStatus;
}

//convert an IPAddress to text
void GroveStreams::ipToText(char* dest, IPAddress ip)
{
    sprintf(dest, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

ethernetPacket::ethernetPacket(void)
{
    _nchar = 0;
    _next = _buf;
}

void ethernetPacket::putChar(const char* c)
{
    if (_nchar > 0) {       //if buffer is not empty, back up one character to overlay the previous zero terminator
        --_nchar;
        --_next;
    }        
    while ( *_next++ = *c++ ) {             //copy the next character
        if (++_nchar >= PKTSIZE - 1) {      //if only one byte left
            *_next++ = 0;                   //put in the terminator
            ++_nchar;                       //and count it
            flush();                        //send the buffer
        }
    }
    ++_nchar;                               //count the terminator
}

void ethernetPacket::putChar(const __FlashStringHelper *f)
{
    const char PROGMEM *c = (const char PROGMEM *)f;

    if (_nchar > 0) {       //if buffer is not empty, back up one character to overlay the previous zero terminator
        --_nchar;
        --_next;
    }        
    while ( *_next++ = pgm_read_byte(c++) ) {   //copy the next character
        if (++_nchar >= PKTSIZE - 1) {      //if only one byte left
            *_next++ = 0;                   //put in the terminator
            ++_nchar;                       //and count it
            flush();                        //send the buffer
        }
    }
    ++_nchar;                               //count the terminator
}

void ethernetPacket::flush(void)
{
    if (_nchar > 0) {
        client << _buf;
//        Serial << millis() << F(" Ethernet packet len: ") << _nchar - 1 << endl;
//        Serial << _buf << endl;
        _nchar = 0;
        _next = _buf;
    }
}

