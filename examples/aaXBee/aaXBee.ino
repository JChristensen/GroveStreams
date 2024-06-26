// Arduino GroveStreams Library
// https://github.com/JChristensen/GroveStreams
// Copyright (C) 2015-2024 by Jack Christensen and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html
//
// Example sketch:
// Double-A XBee Sensor Node for GroveStreams Wireless Sensor Network.
// A low-power Arduino-based wireless sensor node than runs on two AA cells.
// For complete information on the circuit design, including Eagle files,
// configuration options, programming requirements, etc. see the GitHub
// repository: http://github.com/JChristensen/aaXBee_HW
//
// This basic sketch sends a sequence number, the temperature from the
// MCP9808, battery and regulator voltages.
//
// Developed with Arduino 1.0.6, updated for 1.8.19.
// Set ATmega328P Fuses (L/H/E): 0x7F, 0xDE, 0x06.
// Uses an 8MHz crystal with the CKDIV8 fuse bit programmed, so the system
// clock is 1MHz after reset. This is to ensure the MCU is not overclocked
// at low voltages when the boost regulator is disabled. Clock is
// changed to 8MHz after the regulator is enabled.
//
// Set ATmega328P Fuses:
// avrdude -p m328p -U lfuse:w:0x7f:m -U hfuse:w:0xde:m -U efuse:w:0x06:m -v
//
// XBee Configuration.
// Model no. XB24-Z7WIT-004 (XB24-ZB)
// Firmware: ZigBee End Device API 29A7
// All parameters factory default except:
//   ID PAN ID       42 (or other non-zero value, all modules on a network must have the same PAN ID)
//   NI Node ID      aaXB1_10010001
//   BD Baud Rate    57600 [6]
//   AP API Enable   2
//   SM Sleep Mode   Pin Hibernate [1]
//   D5 DIO5 Config  4 (ASSOC LED off)
//   P0 PIO10 Config 4 (RSSI LED off)
//   PO Polling Rate 1 (poll the parent device faster so we can sleep sooner)
//
// Because this node sleeps for relatively long intervals, parent devices
// in the network need their child poll timeout set long enough to maintain
// the sleeping node in their child table. The following settings should be
// made for the coordinator and routers in the network to allow for end
// devices that sleep up to 5 minutes:
//   SN Number of Sleep Periods    0x10
//   SP Sleep Period               0x7D0

#include <avr/sleep.h>
#include <avr/wdt.h>
#include <DS3232RTC.h>      // https://github.com/JChristensen/DS3232RTC
#include <gsXBee.h>         // https://github.com/JChristensen/gsXBee
#include <MCP9808.h>        // https://github.com/JChristensen/MCP9808
#include <Streaming.h>      // https://github.com/janelia-arduino/Streaming
#include <TimeLib.h>        // https://github.com/PaulStoffregen/Time
#include <Wire.h>
#include <XBee.h>           // https://github.com/andrewrapp/xbee-arduino
#include "circuit.h"

//parameters
const uint32_t XBEE_TIMEOUT(10000);      //ms to wait for ack

//trap the MCUSR value after reset to determine the reset source
//and ensure the watchdog is reset. this code does not work with a bootloader.
uint8_t mcusr __attribute__ ((section (".noinit")));
void wdt_init() __attribute__((naked)) __attribute__((section(".init3")));

void wdt_init()
{
    mcusr = MCUSR;
    MCUSR = 0;        //must clear WDRF in MCUSR in order to clear WDE in WDTCSR
    wdt_reset();
    wdt_disable();
}

void setup()
{
    Circuit.begin( F(__FILE__) );
}

void loop()
{
    time_t rtcTime, alarmTime;

    enum STATES_t { INIT, SEND_DATA, WAIT_ACK, SET_ALARM };
    static STATES_t STATE = INIT;

    switch (STATE)
    {
    static uint32_t msTX;
    case INIT:
        {
            //calculate the first alarm
            rtcTime = RTC.get();
            time_t alarmTime = rtcTime - rtcTime % (XB.txInterval * 60) + XB.txOffset * 60 + XB.txSec - XB.txWarmup;
            if ( alarmTime <= rtcTime + 5 ) alarmTime += XB.txInterval * 60;
            //set RTC alarm to match on hours, minutes, seconds
            RTC.setAlarm(DS3232RTC::ALM1_MATCH_HOURS, second(alarmTime), minute(alarmTime), hour(alarmTime), 0);
            RTC.alarm(DS3232RTC::ALARM_1);                  // clear RTC interrupt flag
            RTC.alarmInterrupt(DS3232RTC::ALARM_1, true);   // enable alarm interrupts

            EICRA = _BV(ISC11);               //interrupt on falling edge
            EIFR = _BV(INTF1);                //clear the interrupt flag (setting ISCnn can cause an interrupt)
            EIMSK = _BV(INT1);                //enable interrupt
            STATE = SEND_DATA;
        }
        break;

    case SEND_DATA:
        digitalWrite(PIN.builtinLED, LOW);
        Circuit.gotoSleep();                 //shut the regulator down and sleep until it's time to send data
        {                                    //read sensors, send data
            static uint16_t seqNbr;
            char buf[80];
            rtcTime = RTC.get();
            digitalWrite(PIN.sensorPower, HIGH);    //power up the sensors
            alarmTime = rtcTime + XB.txWarmup;      //sleep the MCU during sensor conversion time
            RTC.setAlarm(DS3232RTC::ALM1_MATCH_HOURS, second(alarmTime), minute(alarmTime), hour(alarmTime), 0);
            RTC.alarm(DS3232RTC::ALARM_1);          //clear RTC interrupt flag
            Circuit.gotoSleep(true);                //sleep while the sensors do their thing, leave boost on
            mcp9808.read();                         //read the temperature sensor
            digitalWrite(PIN.sensorPower, LOW);     //power sensors down

            //build the payload
            sprintf(buf, "&seq=%u&tRaw=%i&vBat=%i&vReg=%i", ++seqNbr, mcp9808.tAmbient, Circuit.vBat, Circuit.vReg );

            //print date to serial monitor
            rtcTime = RTC.get();
            Serial << millis() << ' ';
            printDateTime(rtcTime);

            //Send the data
            Circuit.xbeeEnable(true);
            while ( XB.read() != NO_TRAFFIC );    //handle any incoming traffic
            digitalWrite(PIN.builtinLED, HIGH);
            XB.sendData(buf);
            msTX = millis();
            STATE = WAIT_ACK;
        }
        break;

    case WAIT_ACK:
        if ( millis() - msTX >= XBEE_TIMEOUT )
        {
            Serial << millis() << F(" XBee ACK timeout\n");
            STATE = SET_ALARM;
            break;
        }
        else
        {
            xbeeReadStatus_t xbStatus = XB.read();
            if ( xbStatus == TX_ACK || xbStatus == TX_FAIL )
            {
                STATE = SET_ALARM;
                break;
            }
        }
        break;

    case SET_ALARM:                               //calculate and set the next alarm
        while ( XB.read() != NO_TRAFFIC );        //get any other traffic
        Circuit.xbeeEnable(false);                //done with the XBee, put it to sleep
        {
            rtcTime = RTC.get();
            time_t alarmTime = rtcTime - rtcTime % (XB.txInterval * 60) + XB.txOffset * 60 + XB.txSec - XB.txWarmup;
            if ( alarmTime <= rtcTime + 5 ) alarmTime += XB.txInterval * 60;
            RTC.setAlarm(DS3232RTC::ALM1_MATCH_HOURS, second(alarmTime), minute(alarmTime), hour(alarmTime), 0);
            RTC.alarm(DS3232RTC::ALARM_1);                  // clear RTC interrupt flag
            RTC.alarmInterrupt(DS3232RTC::ALARM_1, true);   // enable alarm interrupts
        }
        STATE = SEND_DATA;
        break;
    }
}

//interrupt from the RTC alarm. don't need to do anything, it's just to wake the MCU.
ISR(INT1_vect) {}
