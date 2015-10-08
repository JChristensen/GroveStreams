/*----------------------------------------------------------------------*
 * GroveStreams Web Gateway                                             *
 * Data concentrator/web gateway node for a GroveStreams-based          *
 * wireless sensor network. Forwards data sent from sensor nodes via    *
 * an XBee ZB network to the GroveStreams web site.                     *
 *                                                                      *
 * GroveStreams limits PUTs to one every 10 seconds, but this is        *
 * averaged over a two-minute period. If multiple sensors send data     *
 * to the gateway asynchronously, it's possible for two or more         *
 * messages to arrive in a short interval. This sketch contains retry   *
 * code to address this situation by retrying a failed call to the      *
 * to GroveStreams.send() function. This works for the occasional       *
 * collision, but if more than a very few sensors are feeding the       *
 * gateway, some mechanism of synchronizing and spacing data            *
 * transmissions would be preferable. The retry mechanism also relies   *
 * somewhat on the XBee's ability to buffer messages, which is limited  *
 * by the internal memory available in the XBee, and will vary with     *
 * message size and frequency.                                          *
 *                                                                      *
 * v1.0  Developed with Arduino v1.0.6.                                 *
 * v1.1  Added retry mechanism.                                         *
 *                                                                      *
 * "GroveStreams Web Gateway" by Jack Christensen                       *
 * is licensed under CC BY-SA 4.0,                                      *
 * http://creativecommons.org/licenses/by-sa/4.0/                       *
 *                                                                      *
 * XBee Configuration                                                   *
 * Model no. XB24-Z7WIT-004 (XB24-ZB)                                   *
 * Firmware: ZigBee Coordinator API 21A7                                *
 * All parameters are factory default except:                           *
 *   ID PAN ID                     42                                   *
 *   NI Node ID                    Coord_00010000                       *
 *   BD Baud Rate                  115200 (7)                           *
 *   AP API Enable                 2                                    *
 * For networks with end devices that sleep up to 5 minutes, also set:  *
 *   SN Number of Sleep Periods    0x10                                 *
 *   SP Sleep Period               0x7D0                                *
 *----------------------------------------------------------------------*/

#include <Ethernet.h>
#include <SPI.h>
#include <Streaming.h>              //http://arduiniana.org/libraries/streaming/
#include <XBee.h>                   //http://github.com/andrewrapp/xbee-arduino
#include <GroveStreams.h>           //http://github.com/JChristensen/GroveStreams
#include <gsXBee.h>                 //http://github.com/JChristensen/gsXBee

//installation-specific variables that WILL need to be changed
PROGMEM const char gsApiKey[] = "Put *YOUR* GroveStreams API Key Here";
uint8_t macAddr[6] = { 0x02, 0, 0, 0, 0, 0x42 };   //Put YOUR MAC address here

//other global variables
const char* gsServer = "grovestreams.com";
const uint32_t RESET_DELAY(60);             //seconds before resetting the MCU for initialization failures
const uint32_t GS_INIT_TIMEOUT(10000);      //milliseconds to wait for GroveStreams response to initial message
const uint32_t DHCP_RENEW_INTERVAL(3600);   //how often to renew our IP address, in seconds
const uint32_t RETRY_INTERVAL(200);         //milliseconds between send retries
const uint8_t MAX_TRIES(10);                //maximum number of times to try send

//pin assignments
const uint8_t SD_CARD(4);                   //slave select signal for the SD card on the Ethernet shield
const uint8_t HB_LED(6);                    //heartbeat LED
const uint8_t WAIT_LED(7);                  //waiting for server response
const uint32_t HB_INTERVAL(1000);           //heartbeat LED interval, ms
const int32_t BAUD_RATE(115200);

//object instantiations
GroveStreams GS(gsServer, (const __FlashStringHelper*)gsApiKey, WAIT_LED);
gsXBee XB;

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
    if ( !XB.begin(Serial) ) XB.mcuReset(RESET_DELAY * 1000UL);    //reset if XBee initialization fails

    //start Ethernet, display IP
    if ( !Ethernet.begin(macAddr) )                //DHCP
    {
        Serial << millis() << F(" DHCP fail\n");
        Serial.flush();
        digitalWrite(WAIT_LED, HIGH);
        GS.mcuReset(RESET_DELAY * 1000UL);
    }
    Serial << millis() << F(" Ethernet started ") << Ethernet.localIP() << endl;
    GS.begin();                                    //connect to GroveStreams

    //send an initial message to GroveStreams to verify communication
    enum STATES_t
    {
        GS_INIT_MSG, GS_INIT_WAIT, GS_INIT_COMPLETE
    };
    static STATES_t STATE = GS_INIT_MSG;

    while ( STATE != GS_INIT_COMPLETE )
    {
        ethernetStatus_t gsStatus = GS.run();          //run the GroveStreams state machine

        switch ( STATE )
        {
        uint32_t msSend;

        case GS_INIT_MSG:                              //send a message to GroveStreams to say we've reset
            STATE = GS_INIT_WAIT;
            msSend = millis();
            GS.send(XB.compID, "&msg=MCU%20reset");
            break;

        case GS_INIT_WAIT:
            if (gsStatus == HTTP_OK) {
                Serial << millis() << F(" GroveStreams init complete\n");
                STATE = GS_INIT_COMPLETE;
            }
            else if ( millis() - msSend >= GS_INIT_TIMEOUT ) {
                Serial << millis() << F(" GroveStreams init fail, resetting MCU\n");
                GS.mcuReset(RESET_DELAY * 1000UL);
            }
            break;
            
        case GS_INIT_COMPLETE:
            break;
        }
    }

    wdt_enable(WDTO_8S);                   //guard against network hangs, etc.
    digitalWrite(HB_LED, LOW);
}

void loop(void)
{
    enum STATES_t
    {
        WAIT_INPUT, SEND_RETRY
    };
    static STATES_t STATE = WAIT_INPUT;

    wdt_reset();
    ethernetStatus_t gsStatus = GS.run();          //run the GroveStreams state machine
    static uint32_t msSend;                        //time of last GS.send

    switch ( STATE )
    {
    case WAIT_INPUT:                                //wait for incoming data from the XBee
        if ( XB.read() == RX_DATA )
        {
            char rss[8];
            itoa(XB.rss, rss, 10);
            strcat(XB.payload, "&rss=");
            strcat(XB.payload, rss);
            msSend = millis();
            if ( GS.send(XB.sendingCompID, XB.payload) == SEND_ACCEPTED )
            {
                Serial << endl << millis() << F(" Send OK ") << XB.payload << endl;
            }
            else
            {
                STATE = SEND_RETRY;
                Serial << endl << millis() << F(" Send FAIL ") << XB.payload << endl;
            }
        }

        //housekeeping stuff while we're waiting
        {
            static uint32_t msLast;
            static uint32_t uptimeSeconds;
            static uint32_t lastMaintain;
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
            if ( uptimeSeconds - lastMaintain >= DHCP_RENEW_INTERVAL )
            {
                lastMaintain += DHCP_RENEW_INTERVAL;
                Serial << ms << F(" Ethernet.maintain ") << Ethernet.maintain() << endl;
            }
        }
        break;

    //initial call to GS.send() failed. retrying a few times should help if the reason was
    //that more than one message arrived nearly simultaneously, before the GroveStreams
    //site could respond to the first PUT.
    case SEND_RETRY:
        {
            static uint8_t retryCount;
            if ( millis() - msSend >= RETRY_INTERVAL )
            {
                msSend = millis();
                Serial << millis() << F(" Send retry\n");
                if ( GS.send(XB.sendingCompID, XB.payload) == SEND_ACCEPTED )
                {
                    STATE = WAIT_INPUT;
                    Serial << endl << millis() << F(" Post OK, ") << retryCount + 2 << F(" tries ") << XB.payload << endl;
                    retryCount = 0;
                }
                else if ( ++retryCount >= MAX_TRIES - 1 )
                {
                    STATE = WAIT_INPUT;
                    Serial << endl << millis() << F(" Post FAIL, ") << ++retryCount << F(" tries ") << XB.payload << endl;
                    retryCount = 0;
                }
            }
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

