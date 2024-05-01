// Arduino GroveStreams Library
// https://github.com/JChristensen/GroveStreams
// Copyright (C) 2015-2024 by Jack Christensen and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html

// Example sketch: Basic GroveStreams Client
// Sends data from an analog temperature sensor to GroveStreams,
// once per minute.
//
// Before running this sketch, create an account and an Organization
// on GroveStreams. From the start page, click the Organization name
// to go to Observation Studio. Next click on the yellow lock
// "API Keys" icon near the top right. This will open a dialog that
// allows keys to be displayed. Check the box, "Feed Put API Key
// (with auto-registration and request stream rights)", then click
// View Secret Key. Copy the key into the line below that starts
// with PROGMEM const char gsApiKey[].
//
// When the sketch is started, GroveStream will create a component
// called "analog" that has two data streams, "s", a sequence number,
// and "C", temperature in Celsius. After the sketch has run at least
// a minute, refresh the Observation Studio page to see these.
//
// v1.0  Developed with Arduino v1.0.6, updated for 1.8.19.
//
// Hardware:
//   Arduino Uno
//   Arduino Ethernet Shield
//   TMP36 Temperature sensor
//   Yellow LED (heartbeat)
//   Red LED (wait)

#include <Ethernet.h>
#include <SPI.h>
#include <Streaming.h>      // https://github.com/janelia-arduino/Streaming
#include <GroveStreams.h>   // https://github.com/JChristensen/GroveStreams

//installation-specific variables that WILL need to be changed
PROGMEM const char gsApiKey[] = "Put *YOUR* GroveStreams API key here";
uint8_t macAddr[6] = { 0, 2, 0, 0, 0, 0x42 };   //Put YOUR MAC address here
const char gsCompID[] = "analog";               //GroveStreams component ID

//other global variables
const char* gsServer = "grovestreams.com";
const uint32_t XMIT_INTERVAL(60000);            //data transmission interval
const uint32_t HB_INTERVAL(1000);               //heartbeat LED interval, ms
const uint32_t DHCP_RENEW_INTERVAL(3600000);    //renew our IP address hourly
const int32_t BAUD_RATE(115200);

//pin assignments
const uint8_t SD_CARD(4);    //slave select signal for the SD card on the Ethernet shield
const uint8_t HB_LED(6);     //heartbeat LED
const uint8_t WAIT_LED(7);   //waiting for server response
const uint8_t TMP36(A2);     //TMP36 temperature sensor (A0 and A1 are used for SD card on Ethernet shield)

//object instantiations
EthernetClient gsClient;
GroveStreams GS(gsClient, gsServer, (const __FlashStringHelper*)gsApiKey, WAIT_LED);

void setup()
{
    Serial.begin(BAUD_RATE);
    pinMode(SD_CARD, OUTPUT);
    digitalWrite(SD_CARD, HIGH);         //de-select the SD card
    pinMode(HB_LED, OUTPUT);
    digitalWrite(HB_LED, HIGH);          //HB LED on during initialization
    pinMode(WAIT_LED, OUTPUT);
    Serial << F( "\n" __FILE__ " " __DATE__ " " __TIME__ "\n" );
    delay(500);                          //allow some time for the ethernet chip to boot up

    //start Ethernet, display IP
    if ( !Ethernet.begin(macAddr) )      //DHCP
    {
        Serial << millis() << F(" DHCP fail, reset in 60 seconds...\n");
        Serial.flush();
        digitalWrite(WAIT_LED, HIGH);
        GS.mcuReset(60000);
    }
    Serial << millis() << F(" Ethernet started ") << Ethernet.localIP() << endl;
    GS.begin();                          //connect to GroveStreams
    wdt_enable(WDTO_8S);                 //guard against network hangs, etc.
}

void loop()
{
    wdt_reset();
    GS.run();                            //run the GroveStreams state machine

    static uint32_t msLastXmit;
    if ( millis() - msLastXmit >= XMIT_INTERVAL )    //send data
    {
        msLastXmit += XMIT_INTERVAL;
        static uint16_t seqNbr;
        char buf[40];
        int tC100 = readTMP36(TMP36);
        int intDegrees = tC100 / 100;
        int fracDegrees = abs(tC100 % 100);
        sprintf(buf, "&s=%u&C=%i.%i", ++seqNbr, intDegrees, fracDegrees );
        Serial << endl << millis() << F(" Sending ") << buf << endl;
        if ( GS.send(gsCompID, buf) == SEND_ACCEPTED )
        {
            Serial << millis() << F(" Send OK\n");
        }
        else
        {
            Serial << millis() << F(" Send FAIL\n");
        }
    }

    //renew DHCP address regularly
    static uint32_t lastMaintain;
    if ( millis() - lastMaintain >= DHCP_RENEW_INTERVAL )
    {
        lastMaintain += DHCP_RENEW_INTERVAL;
        Serial << millis() << F(" Ethernet.maintain ") << Ethernet.maintain() << endl;
    }

    //run the heartbeat LED
    static uint32_t hbLast;                 //last heartbeat
    static bool hbState;
    if ( millis() - hbLast >= HB_INTERVAL )
    {
        hbLast += HB_INTERVAL;
        digitalWrite(HB_LED, hbState = !hbState);
    }
}

//read TMP36 temperature sensor, return °C * 100
//TMP36 output is 10mV/C with a 500mV offset
long readTMP36(int muxChannel)
{
    long uV = (analogRead(muxChannel) * 5000000L + 512) / 1024;    //microvolts from the TMP36 sensor
    return (uV - 500000 + 50) / 100;
}
