/*----------------------------------------------------------------------*
 * GroveStreams Web Gateway                                             *
 * Data concentrator/web gateway node for a GroveStreams-based          *
 * wireless sensor network. Forwards data sent from sensor nodes via    *
 * an XBee ZB network to the GroveStreams web site.                     *
 *                                                                      *
 * v1.0  Developed with Arduino v1.0.6.                                 *
 *                                                                      *
 * "GroveStreams Web Gateway" by Jack Christensen                       *
 * is licensed under CC BY-SA 4.0,                                      *
 * http://creativecommons.org/licenses/by-sa/4.0/                       *
 *                                                                      *
 * XBee Configuration                                                   *
 * Model no. XB24-Z7WIT-004 (XB24-ZB)                                   *
 * Firmware: ZigBee Coordinator API 21A7                                *
 * All parameters are factory default except:                           *
 *   ID PAN ID       42                                                 *
 *   NI Node ID      Coord_00010000                                     *
 *   BD Baud Rate    115200 (7)                                         *
 *   AP API Enable   2                                                  *
 *----------------------------------------------------------------------*/

#include <Ethernet.h>
#include <SPI.h>
#include <Streaming.h>              //http://arduiniana.org/libraries/streaming/
#include <XBee.h>                   //http://github.com/andrewrapp/xbee-arduino
#include <GroveStreams.h>           //http://github.com/JChristensen/GroveStreams
#include <gsXBee.h>                 //http://github.com/JChristensen/gsXBee

//installation-specific variables that WILL need to be changed
PROGMEM const char gsApiKey[] = "Put *YOUR* GroveStreams API Key Here";
uint8_t macAddr[6] = { 0, 2, 0, 0, 0, 0x42 };   //Put YOUR MAC address here

//other global variables
const uint32_t DHCP_RENEW_INTERVAL(3600);       //how often to renew our IP address, in seconds
const char* gsServer = "grovestreams.com";

//pin assignments
const uint8_t SD_CARD(4);               //slave select signal for the SD card on the Ethernet shield
const uint8_t HB_LED(6);                //heartbeat LED
const uint8_t WAIT_LED(7);              //waiting for server response
const uint32_t HB_INTERVAL(1000);       //heartbeat LED interval, ms
const int32_t BAUD_RATE(115200);

//object instantiations
GroveStreams GS(gsServer, (const __FlashStringHelper*)gsApiKey, WAIT_LED);
gsXBee xb;

void setup(void)
{
    pinMode(SD_CARD, OUTPUT);
    pinMode(HB_LED, OUTPUT);
    pinMode(WAIT_LED, OUTPUT);
    digitalWrite(SD_CARD, HIGH);         //de-select the SD card
    digitalWrite(HB_LED, HIGH);          //HB LED on during initialization
    Serial.begin(BAUD_RATE);
    Serial << F( "\n" __FILE__ " " __DATE__ " " __TIME__ "\n" );
    Serial.flush();
    xb.begin(BAUD_RATE);

    //start Ethernet, display IP
    if ( !Ethernet.begin(macAddr) )                //DHCP
    {
        Serial << millis() << F(" DHCP fail, reset in 60 seconds...\n");
        Serial.flush();
        digitalWrite(WAIT_LED, HIGH);
        GS.mcuReset(60000);
    }
    Serial << millis() << F(" Ethernet started ") << Ethernet.localIP() << endl;
    GS.begin();                                    //connect to GroveStreams
    digitalWrite(HB_LED, LOW);
}

void loop(void)
{
    enum STATES_t
    {
        GS_HELLO, GS_INIT, RUN
    };
    static STATES_t STATE = GS_HELLO;

    wdt_reset();

    if ( xb.read() == RX_DATA )                    //check for incoming data from the XBee
    {
        char rss[8];
        itoa(xb.rss, rss, 10);
        strcat(xb.payload, "&rss=");
        strcat(xb.payload, rss);
        Serial << endl << millis();
        if ( GS.send(xb.sendingCompID, xb.payload) == SEND_ACCEPTED )
        {
            Serial << F(" Post OK ");
        }
        else
        {
            Serial << F(" Post FAIL ");
        }
        Serial << xb.payload << endl;
    }

    ethernetStatus_t gsStatus = GS.run();          //run the GroveStreams state machine

    switch ( STATE )
    {
        uint32_t msSend;

    case GS_HELLO:                                 //send a message to GroveStreams to say we've reset
        STATE = GS_INIT;
        msSend = millis();
        GS.send(xb.compID, "&msg=MCU%20reset");
        break;

    case GS_INIT:
        if (gsStatus == HTTP_OK) {
            Serial << millis() << F(" GS init\n");
            wdt_enable(WDTO_8S);                   //guard against network hangs, etc.
            STATE = RUN;
        }
        else if ( millis() - msSend >= 10000 ) {
            Serial << millis() << F(" GroveStreams send fail, resetting MCU\n");
            GS.mcuReset(60000);
        }
        break;

    case RUN:
        static uint32_t msLast;
        static uint32_t uptimeSeconds;
        uint32_t ms = millis();

        //count uptime in seconds, print once per minute
        if ( ms - msLast >= 1000 )
        {
            msLast += 1000;
            if ( ++uptimeSeconds % 60 == 0 )
            {
                Serial << ms << F(" Approx uptime ") << uptimeSeconds/60 << F(" min.") << ' ' << endl;
            }
        }

        //renew DHCP address regularly
        static uint32_t lastMaintain;
        if ( uptimeSeconds - lastMaintain >= DHCP_RENEW_INTERVAL )
        {
            lastMaintain += DHCP_RENEW_INTERVAL;
            Serial << ms << F(" Ethernet.maintain ") << Ethernet.maintain() << endl;
        }

        break;
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

