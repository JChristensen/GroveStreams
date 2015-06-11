//The Circuit object represents the hardware pin assignments and has functions
//to control hardware features.
//
//Double-A XBee Sensor Node by Jack Christensen is licensed under
//CC BY-SA 4.0, http://creativecommons.org/licenses/by-sa/4.0/

const long BAUD_RATE(57600);
const uint32_t SLEEP_BEFORE_RESET(900);   //seconds to sleep before resetting the MCU if XBee initialization fails
gsXBee XB;                                //the XBee
MCP9808 mcp9808(0);                       //MCP9808 temperature sensor

//pin assignments
const struct pins_t
{
    uint8_t peripPower;             //direct port code must change if pin number changes
    uint8_t rtcInterrupt;
    uint8_t boostRegulator;         //direct port code must change if pin number changes
    uint8_t xbeeCTS;                //low = ok to send, high = don't send
    uint8_t xbeeSleepRQ;            //high to sleep, low to wake
    uint8_t builtinLED;
    uint8_t sensorPower;
} 
PIN = { 2, 3, 4, 5, 6, 8, 9 };

#define xbeeSleep HIGH
#define xbeeWake LOW
#define xbeeSend LOW
#define xbeeWait HIGH

//MCU system clock prescaler values, used to set CLKPS[3:0]
enum clockSpeed_t { CLOCK_8MHZ = 0, CLOCK_1MHZ = 3 };    

//function prototypes
void printDateTime(time_t t, bool newLine = true);

class circuit
{
private:

public:
    circuit();
    void begin(const __FlashStringHelper* fileName);
    void gotoSleep(bool enableRegulator = false);
    void systemClock(clockSpeed_t clkpr);
    void xbeeEnable(boolean enable);
    void peripPower(bool enable);
    int readVcc(void);
    int readBattery(void);

    int vBat;       //battery and regulator voltages
    int vReg;
};

circuit Circuit;

circuit::circuit()
{
}

void circuit::begin(const __FlashStringHelper* fileName)
{
    const uint8_t pinModes[] = {        //initial pin configuration
        INPUT,           //0   PD0  RXD
        INPUT,           //1   PD1  TXD
        OUTPUT,          //2   PD2  peripheral power (RTC, temp sensor)
        INPUT_PULLUP,    //3   PD3  RTC interrupt [INT1]
        OUTPUT,          //4   PD4  regulator control
        INPUT,           //5   PD5  XBee On/Sleep (CTS)
        OUTPUT,          //6   PD6  XBee sleep request pin
        INPUT_PULLUP,    //7   PD7  unused
        OUTPUT,          //8   PB0  built-in LED
        OUTPUT,          //9   PB1  sensor power
        INPUT_PULLUP,    //10  PB2  unused [SS]
        INPUT_PULLUP,    //11  PB3  unused [MOSI]
        INPUT_PULLUP,    //12  PB4  unused [MISO]
        INPUT_PULLUP,    //13  PB5  unused [SCK]
        INPUT_PULLUP,    //A0  PC0  unused
        INPUT_PULLUP,    //A1  PC1  unused
        INPUT_PULLUP,    //A2  PC2  unused
        INPUT_PULLUP,    //A3  PC3  unused
        INPUT,           //A4  PC4  [SDA] (external pullup)
        INPUT            //A5  PC5  [SCL] (external pullup)
    };

    for (uint8_t i=0; i<sizeof(pinModes); i++)    //configure pins
    {
        pinMode(i, pinModes[i]);
    }
    systemClock(CLOCK_8MHZ);
    peripPower(true);                             //peripheral power on
    mcp9808.begin(twiClock400kHz);
    Serial.begin(BAUD_RATE);
    Serial << endl << F("Double-A XBee Sensor Node\n");
    Serial << fileName << F(" " __DATE__ " " __TIME__ "\n");
    xbeeEnable(true);

    //rtc initialization
    time_t rtcTime = RTC.get();
    printDateTime(rtcTime);
    RTC.squareWave(SQWAVE_NONE);                //no square waves please
    RTC.writeRTC( RTC_STATUS, RTC.readRTC(RTC_STATUS) & ~( _BV(BB32KHZ) | _BV(EN32KHZ) ) );   //no 32kHz output either
    if ( RTC.oscStopped() )                     //ensure the oscillator is running
    {
        RTC.set(rtcTime); 
    }

    if ( !XB.begin(Serial) )
    {
        Serial << millis() << F(" XBee initialization failed\n");
        rtcTime = RTC.get();
        time_t alarmTime = rtcTime + SLEEP_BEFORE_RESET;
        //set RTC alarm to match on hours, minutes, seconds
        RTC.setAlarm(ALM1_MATCH_HOURS, second(alarmTime), minute(alarmTime), hour(alarmTime), 0);
        RTC.alarm(ALARM_1);                   //clear RTC interrupt flag
        RTC.alarmInterrupt(ALARM_1, true);    //enable alarm interrupts

        EICRA = _BV(ISC11);               //interrupt on falling edge
        EIFR = _BV(INTF1);                //clear the interrupt flag (setting ISCnn can cause an interrupt)
        EIMSK = _BV(INT1);                //enable interrupt
        Circuit.gotoSleep();
        XB.mcuReset();
    }
}

//enter power down mode, optionally leave boost regulator enabled
void circuit::gotoSleep(bool enableRegulator)
{
    if (!enableRegulator) vReg = readVcc();     //read regulator voltage before shutdown
    xbeeEnable(false);
    Serial << millis() << F(" MCU sleep\n");
    Serial.flush();
    Serial.end();
    digitalWrite(PIN.builtinLED, LOW);     //LED off
    pinMode(SCL, INPUT);                   //tri-state the i2c bus   
    pinMode(SDA, INPUT);

    if (!enableRegulator) {
        digitalWrite(PIN.sensorPower, LOW);   //sensor power off
        peripPower(false);                    //peripheral power off
        systemClock(CLOCK_1MHZ);
    }

    byte adcsra = ADCSRA;          //save the ADC Control and Status Register A
    ADCSRA = 0;                    //disable ADC
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli();
    sleep_enable();
    //disable brown-out detection while sleeping (20-25µA)
#if __AVR_LIBC_VERSION__ >= 10607
    sleep_bod_disable();
#else
    uint8_t mcucr1 = MCUCR | _BV(BODS) | _BV(BODSE);
    uint8_t mcucr2 = mcucr1 & ~_BV(BODSE);
    MCUCR = mcucr1;
    MCUCR = mcucr2;
#endif
    sei();                         //ensure interrupts enabled so we can wake up again
    sleep_cpu();                   //go to sleep
    sleep_disable();               //wake up here
    ADCSRA = adcsra;               //restore ADCSRA    

    if (!enableRegulator) systemClock(CLOCK_8MHZ);
    Serial.begin(BAUD_RATE);
    peripPower(true);              //peripheral power on (rtc)
    delay(5);                      //a little ramp-up time
    Serial << endl << millis() << F(" MCU wake\n");
}

//enables the boost regulator to provide higher voltage and increases the system clock frequency,
//or decreases the system clock frequency and disables the regulator to run on direct battery voltage.
void circuit::systemClock(clockSpeed_t clkpr)
{
    if (clkpr == CLOCK_8MHZ) {               //prepare to increase clock to 8MHz
        ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1);    //adjust ADC prescaler for faster clock
        PORTD |= _BV(PORTD4);                //boost on
        delay(1);                            //actually 8ms because the clock is 1MHz at this point
    }

    cli();                                   //clock adjustment happens here
    CLKPR = _BV(CLKPCE);                     //set the clock prescaler change enable bit
    CLKPR = (uint8_t) clkpr;
    sei();

    if (clkpr == CLOCK_1MHZ) {               //clock has been reduced to 1MHz
        ADCSRA = _BV(ADEN) | _BV(ADPS1) | _BV(ADPS0);    //adjust ADC prescaler for slower clock
        PORTD &= ~_BV(PORTD4);               //boost off
        delay(1);                            //actually 8ms because the clock is 1MHz at this point
        vBat = readVcc();                    //read battery voltage
    }
}

void circuit::xbeeEnable(boolean enable)
{
    static bool xbeeAwake;      //flag to avoid waking the XBee if already awake, or sleeping it if already sleeping
    
    if (enable && !xbeeAwake)
    {
        digitalWrite(PIN.xbeeSleepRQ, xbeeWake);           //ask the XBee to wake up
        while ( digitalRead(PIN.xbeeCTS) == xbeeWait );    //wait for the XBee to wake up
        xbeeAwake = true;
        Serial << millis() << F(" XBee wake\n");
    }
    else if ( xbeeAwake )                                  //don't bother if it's already sleeping
    {
        digitalWrite(PIN.xbeeSleepRQ, xbeeSleep);          //ask the XBee to go to sleep
        while ( digitalRead(PIN.xbeeCTS) == xbeeSend );    //wait for the XBee to sleep
        xbeeAwake = false;
        Serial << millis() << F(" XBee sleep\n");
    }
}

//turn peripheral (rtc) power on or off,
//using direct port manipulation for fastest transition.
void circuit::peripPower(bool enable)
{
    if (enable) {                   //turn power on
        PORTD |= _BV(PORTD2);       //input pullup is transition state
        DDRD |= _BV(DDD2);          //output high
    }
    else {                          //turn power off
        DDRD &= ~_BV(DDD2);         //input pullup is transition state
        PORTD &= ~_BV(PORTD2);      //turn off pullup for tri-state/hi-z
    }
}

//read 1.1V reference against AVcc
//from http://code.google.com/p/tinkerit/wiki/SecretVoltmeter
int circuit::readVcc(void)
{
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
    delay(2);                                 //Vref settling time
    ADCSRA |= _BV(ADSC);                      //start conversion
    loop_until_bit_is_clear(ADCSRA, ADSC);    //wait for it to complete
    return 1126400L / ADC;                    //calculate AVcc in mV (1.1 * 1000 * 1024)
}

//read battery voltage using ADC6, ADC7 and voltage divider.
//resistors R4 and R5 form the voltage divider.
//NOTE: When switching from the DEFAULT to the INTERNAL 1.1V ADC reference, it can take
//5-10ms for Aref to stabilize because it is held up by a 100nF capacitor on the board.
int circuit::readBattery(void)
{
    const int R4 = 47500;    //ohms
    const int R5 = 10000;    //ohms
    long adc6, adc7;

    analogReference(INTERNAL);
    adc6 = analogRead(6);
    adc7 = analogRead(7);
    return ((adc7 - adc6) * (R4 + R5) / R5 + adc6) * 1100 / 1024;
}

