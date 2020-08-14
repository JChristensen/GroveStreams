// Arduino GroveStreams Library
//
// "Arduino GroveStreams Library" by Jack Christensen
// is licensed under CC BY-SA 4.0,
// http://creativecommons.org/licenses/by-sa/4.0/

#ifndef _GROVESTREAMS_H
#define _GROVESTREAMS_H

#include <Arduino.h>
#include <avr/wdt.h>
#include <Dns.h>
#include <Ethernet.h>
#include <Streaming.h>              //http://arduiniana.org/libraries/streaming/

enum ethernetStatus_t
{
    NO_STATUS, SEND_ACCEPTED, PUT_COMPLETE, DISCONNECTING, DISCONNECTED, HTTP_OK,
    SEND_BUSY, CONNECT_FAILED, TIMEOUT, HTTP_OTHER
};

const uint8_t MAX_ERROR(5);             //reset mcu after this many consecutive errors
const uint32_t RECEIVE_TIMEOUT(8000);   //ms to wait for response from server
const int serverPort(80);               //http port

class GroveStreams
{
public:
    GroveStreams(const char* serverName, const __FlashStringHelper* apiKey, int ledPin = -1);
    void begin();
    ethernetStatus_t send(const char* compID, const char* data);
    ethernetStatus_t run();
    void mcuReset(uint32_t dly = 0 );
    void ipToText(char* dest, IPAddress ip);

    IPAddress serverIP;
    ethernetStatus_t lastStatus;

    //web posting stats
    uint16_t httpOK;                    //number of HTTP OK responses received
    uint8_t nError;                     //error count since last httpOK (SEND_BUSY, CONNECT_FAILED, TIMEOUT, HTTP_OTHER)
    uint16_t sendSeq;                   //number of sends requested
    uint16_t sendBusy;                  //number of sends rejected
    uint16_t connFail;                  //number of connection failures
    uint16_t recvTimeout;               //number of timeouts waiting for server response
    uint16_t httpOther;                 //number of non-OK HTTP responses received (i.e. not HTTP status 200)
    uint32_t connTime;                  //time to connect to server in milliseconds
    uint32_t respTime;                  //response time in milliseconds
    uint32_t discTime;                  //time to disconnect from server in milliseconds

private:
    ethernetStatus_t _xmit();
    int dnsLookup(const char* hostname, IPAddress& addr);

    char _localIP[16];
    char _groveStreamsIP[16];
    const char* _serverName;
    const __FlashStringHelper* _apiKey;
    const char* _compID;                //component ID
    const char* _data;
    unsigned long _msConnect;
    unsigned long _msConnected;
    unsigned long _msPutComplete;
    unsigned long _msLastPacket;
    unsigned long _msDisconnecting;
    unsigned long _msDisconnected;
    int _ledPin;
};

const uint16_t PKTSIZE(300);
class ethernetPacket
{
public:
    ethernetPacket();
    void putChar(const char* c);
    void putChar(const __FlashStringHelper *f);
    void flush();

private:
    char _buf[PKTSIZE];     //the packet
    uint16_t _nchar;        //number of characters in the packet
    char* _next;            //pointer to the next available position in the packet
};

#endif
