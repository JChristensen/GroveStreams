/*----------------------------------------------------------------------*
 * GroveStreams Sensor Node                                             *
 * Sensor node for a GroveStreams-based wireless sensor network.        *
 * Transmits sensor data via an XBee ZB network to the web              *
 * gateway node.                                                        *
 *                                                                      *
 * v1.0  Developed with Arduino v1.0.6.                                 *
 *                                                                      *
 * "GroveStreams Sensor Node" by Jack Christensen                       *
*  is licensed under CC BY-SA 4.0,                                      *
 * http://creativecommons.org/licenses/by-sa/4.0/                       *
 *                                                                      *
 * XBee Configuration                                                   *
 * Model no. XB24-Z7WIT-004 (XB24-ZB)                                   *
 * Firmware: ZigBee Router API 23A7                                     *
 * All parameters are factory default except:                           *
 *   ID PAN ID       42                                                 *
 *   NI Node ID      TMP36a_00010000                                    *
 *   BD Baud Rate    115200 (7)                                         *
 *   AP API Enable   2                                                  *
 *----------------------------------------------------------------------*/

#include <Streaming.h>              //http://arduiniana.org/libraries/streaming/
#include <XBee.h>                   //http://github.com/andrewrapp/xbee-arduino
#include <gsXBee.h>                 //http://github.com/JChristensen/gsXBee

//the following macro forces the XBee to disassociate and reassociate after a
//reset. comment this line out if this is not desired.
//forcing disassociation will lengthen startup time a bit.
#define FORCE_DISASSOCIATE

//pin assignments
const uint8_t HB_LED(6);                 //heartbeat LED
const uint8_t WAIT_LED(7);               //waiting for XBee to ack transmitted data
const uint8_t TMP36(A0);                 //TMP36 temperature sensor

//other constants
const uint32_t XBEE_TIMEOUT(10000);      //ms to wait for ack
const uint32_t HB_INTERVAL(1000);        //heartbeat LED interval, ms
const uint32_t ASSOC_TIMEOUT(60000);     //milliseconds to wait for XBee to associate
const uint32_t RESET_DELAY(60000);       //milliseconds to wait before resetting MCU
const int32_t BAUD_RATE(115200);

gsXBee xb;                               //the XBee

void setup(void)
{
    xb.begin(BAUD_RATE);                 //also does Serial.begin()
    pinMode(HB_LED, OUTPUT);
    digitalWrite(HB_LED, HIGH);          //HB LED on during initialization
    pinMode(WAIT_LED, OUTPUT);
    Serial << F( "\n" __FILE__ " " __DATE__ " " __TIME__ "\n" );
    delay(500);                          //give the XBee time to initialize

    //initialization state machine establishes communication with the XBee and
    //ensures that it is associated.
    enum INIT_STATES_t        //state machine states
    {
        GET_NI, CHECK_ASSOC, WAIT_DISASSOC, WAIT_ASSOC, INIT_COMPLETE, MCU_RESET
    };
    INIT_STATES_t INIT_STATE = GET_NI;

    while ( INIT_STATE != INIT_COMPLETE )
    {
        uint32_t stateTimer;

        switch (INIT_STATE)
        {
        case GET_NI:
            while ( xb.read() != NO_TRAFFIC );      //handle any incoming traffic
            {
                uint8_t cmd[] = "NI";           //ask for the node ID
                xb.sendCommand(cmd);
                if ( xb.waitFor(NI_CMD_RESPONSE, 1000) == READ_TIMEOUT )
                {
                    INIT_STATE = MCU_RESET;
                    Serial << millis() << F(" The XBee did not respond\n");
                }
                else
                {
                    INIT_STATE = CHECK_ASSOC;
                }
            }
            break;

        case CHECK_ASSOC:
            {
                uint8_t cmd[] = "AI";           //ask for association indicator
                xb.sendCommand(cmd);
            }
            if ( xb.waitFor(AI_CMD_RESPONSE, 1000) == READ_TIMEOUT )
            {
                INIT_STATE = MCU_RESET;
                Serial << millis() << F(" XBee AI fail\n");
            }
            else if ( xb.assocStatus == 0 )    //zero means associated
            {
                uint8_t cmd[] = "DA";          //force disassociation
                xb.sendCommand(cmd);
                stateTimer = millis();
                INIT_STATE = WAIT_DISASSOC;
            }
            else
            {
                stateTimer = millis();
                INIT_STATE = WAIT_ASSOC;        //already disassociated, just wait for associate
            }
            break;

        case WAIT_DISASSOC:                     //wait for the XBee to disassociate
            xb.read();
            if (xb.assocStatus != 0) {          //zero means associated
                INIT_STATE = WAIT_ASSOC;
                stateTimer = millis();
            }
            else if (millis() - stateTimer >= ASSOC_TIMEOUT) {
                INIT_STATE = MCU_RESET;
                Serial << millis() << F(" XBee DA timeout\n");
            }
            break;

        case WAIT_ASSOC:                        //wait for the XBee to associate
            xb.read();
            if ( xb.assocStatus == 0 ) {        //zero means associated
                INIT_STATE = INIT_COMPLETE;
                digitalWrite(HB_LED, LOW);
                xb.disassocReset = true;        //any further disassociations are unexpected
                stateTimer = millis();
            }
            else if (millis() - stateTimer >= ASSOC_TIMEOUT) {
                INIT_STATE = MCU_RESET;
                Serial << millis() << F(" XBee associate fail\n");
            }
            break;

        case MCU_RESET:    //wait a minute, then reset the MCU
            Serial.flush();
            digitalWrite(WAIT_LED, HIGH);
            xb.mcuReset(RESET_DELAY);
            break;
        }
    }
}

void loop(void)
{
    enum STATES_t        //state machine states
    {
        WAIT_SEND, SEND, WAIT_ACK
    };
    static STATES_t STATE = WAIT_SEND;

    static uint32_t msTX;                   //time data sent via the XBee
    xbeeReadStatus_t xbStatus = xb.read();  //check for incoming XBee traffic

    switch (STATE)
    {
    case WAIT_SEND:
        static uint32_t lastXmit;
        static uint32_t msTxInterval = xb.txInterval * 60UL * 1000UL;
        if (millis() - lastXmit >= msTxInterval)     //time to send data?
        {
            lastXmit += msTxInterval;
            STATE = SEND;
        }
        break;

    case SEND:
        {
            static uint16_t seqNbr;
            char buf[32];
            int tC100 = readTMP36(TMP36);
            int intDegrees = tC100 / 100;
            int fracDegrees = abs(tC100 % 100);
            sprintf(buf, "&s=%u&C=%i.%i", ++seqNbr, intDegrees, fracDegrees );
            digitalWrite(WAIT_LED, HIGH);
            msTX = millis();
            xb.sendData(buf);
            STATE = WAIT_ACK;
        }
        break;

    case WAIT_ACK:
        if ( millis() - msTX >= XBEE_TIMEOUT )
        {
            digitalWrite(WAIT_LED, LOW);
            STATE = WAIT_SEND;
            Serial << millis() << F(" XBee ack timeout\n");
        }
        else if ( xbStatus == TX_ACK || xbStatus == TX_FAIL )
        {
            digitalWrite(WAIT_LED, LOW);
            STATE = WAIT_SEND;
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

//read TMP36 temperature sensor, return °C * 100
//TMP36 output is 10mV/C with a 500mV offset
long readTMP36(int muxChannel)
{
    long uV = (analogRead(muxChannel) * 5000000L + 512) / 1024;    //microvolts from the TMP36 sensor
    return (uV - 500000 + 50) / 100;
}

