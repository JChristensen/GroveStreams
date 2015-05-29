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

//pin assignments
const uint8_t HB_LED(6);                 //heartbeat LED
const uint8_t WAIT_LED(7);               //waiting for XBee to ack transmitted data
const uint8_t TMP36(A2);                 //TMP36 temperature sensor

//other constants
const uint32_t XBEE_TIMEOUT(10000);      //ms to wait for ack
const uint32_t HB_INTERVAL(1000);        //heartbeat LED interval, ms
const int32_t BAUD_RATE(115200);

gsXBee xb;                               //the XBee

void setup(void)
{
    pinMode(HB_LED, OUTPUT);
    pinMode(WAIT_LED, OUTPUT);
    digitalWrite(HB_LED, HIGH);          //HB LED on during initialization
    Serial.begin(BAUD_RATE);
    Serial << F( "\n" __FILE__ " " __DATE__ " " __TIME__ "\n" );
    Serial.flush();
    xb.begin(BAUD_RATE);
    digitalWrite(HB_LED, LOW);
}

void loop(void)
{
    enum STATES_t        //state machine states
    {
        WAIT_SEND, SEND, WAIT_ACK
    };
    static STATES_t STATE = WAIT_SEND;

    static uint32_t msTX;                   //time data was sent via the XBee
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

