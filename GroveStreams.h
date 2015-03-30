//TO DO
//Count errors, meaning any of: SEND_BUSY, CONNECT_FAILED, TIMEOUT, HTTP_OTHER
//Reset count when HTTP_OK occurs.  WDT reset if three consecutive errors.
//Use conditional compilation to make posting stats optional.
//Use WDT in main loop? (8 sec) -- DONE
//
//Pullups on unused pins
//struct for data to be posted
//test sprintf performance vs. DIY -- DONE sprintf faster but increases code space by ~1000 bytes (so what)

//GroveStreams Class
#ifndef _GROVESTREAMS_H
#define _GROVESTREAMS_H

#include <avr/wdt.h>
#include <Arduino.h>
#include <Ethernet.h>               //http://arduino.cc/en/Reference/Ethernet
#include <NTP.h>
#include <MemoryFree.h>             //http://playground.arduino.cc/Code/AvailableMemory

enum ethernetStatus_t { NO_STATUS, SEND_ACCEPTED, PUT_COMPLETE, DISCONNECTING, DISCONNECTED, HTTP_OK, SEND_BUSY, CONNECT_FAILED, TIMEOUT, HTTP_OTHER };

const int serverPort = 80;
extern EthernetClient client;
const unsigned long RECEIVE_TIMEOUT = 8000;    //ms to wait for response from server

class GroveStreams
{
    public:
        GroveStreams(const char* serverName, const __FlashStringHelper* apiKey, int ledPin = -1);
        void begin(void);
        ethernetStatus_t send(const char* compID, const char* data);
        ethernetStatus_t run(void);
        void ipToText(char* dest, IPAddress ip);

        IPAddress serverIP;
        ethernetStatus_t lastStatus;

        //data to be posted
        unsigned int seq;                   //post sequence number
        unsigned long connTime;             //time to connect to server in milliseconds
        unsigned long respTime;             //response time in milliseconds
        unsigned long discTime;             //time to disconnect from server in milliseconds
        unsigned int success;               //number of successful posts
        unsigned int fail;                  //number of Ethernet connection failures
        unsigned int timeout;               //number of Ethernet timeouts
        unsigned int freeMem;               //bytes of free SRAM

    private:
        ethernetStatus_t _xmit(void);
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

const uint16_t PKTSIZE = 300;
class ethernetPacket
{
    public:
        ethernetPacket(void);
        void putChar(const char* c);
        void putChar(const __FlashStringHelper *f);
        void flush(void);
        
    private:
        char _buf[PKTSIZE];     //the packet
        uint16_t _nchar;        //number of characters in the packet
        char* _next;            //pointer to the next available position in the packet
};

#endif
