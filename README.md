# Arduino GroveStreams Library
http://github.com/JChristensen/GroveStreams  
README file  

## License
Arduino GroveStreams Library Copyright (C) 2015 Jack Christensen GNU GPL v3.0

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License v3.0 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/gpl.html>

## Description
A library to communicate with the [GroveStreams Data Analytics Platform](https://grovestreams.com/).

**Prerequisites**  
Mikal Hart's Streaming library  
http://arduiniana.org/libraries/streaming/

## Installation
To use the **GroveStreams** library:  
- Go to https://github.com/JChristensen/GroveStreams, download the code as a ZIP file and save it to a convenient location on your PC.
- Uncompress the downloaded file.  This will result in a folder containing all the files for the library, that has a name that includes the branch name, usually **GroveStreams-master**.
- Rename the folder to just **GroveStreams**.
- Copy the renamed folder to the Arduino **sketchbook/libraries** folder.

## Examples
The following example sketches are included with the **GroveStreams** library:
- **gsAnalog:** A standalone GroveStreams client using an Arduino Uno, Arduino Ethernet Shield, and an analog temperature sensor.
- **gsGateway:** A data concentrator/web gateway node for an XBee ZB wireless sensor network. Use with the **gsSensor** example sketch.
- **gsSensor:** A wireless sensor node for use with **gsGateway**. Forwards sensor data to the gateway node which relays it to GroveStreams.
- **aaXBee:** A low-power, battery-operated wireless sensor node for use with **gsGateway**. Forwards sensor data to the gateway node which relays it to GroveStreams. For complete information on the circuit design, including Eagle files, configuration options, programming requirements, etc. see [the GitHub repository](https://github.com/JChristensen/aaXBee_HW).

See my [blog post](http://adventuresinarduinoland.blogspot.com/2015/05/a-grovestreams-wireless-sensor-network.html) for step-by-step instructions to build a wireless sensor network using the **gsGateway** and **gsSensor** sketches.

## Constructor

### GroveStreams(const char\* serverName, const __FlashStringHelper\* apiKey, int ledPin)
##### Description
Instantiates a GroveStreams object.
##### Syntax
`GroveStreams GS(serverName, apiKey, ledPin);`
##### Parameters
**serverName** A zero-terminated char array containing the address of the GroveStreams server _(char\*)_.

**apiKey** A zero-terminated char array in flash program memory containing the API key for the GroveStreams organization _(const __FlashStringHelper\*)_.

**ledPin** An optional argument giving the pin number of an LED to illuminate while waiting for responses from the GroveStreams server _(int)_.

##### Example
```c++
const char* gsServer = "grovestreams.com";
PROGMEM const char gsApiKey[] = "12345678-1234-5678-ABCD-1234567890AB";
const uint8_t WAIT_LED(7);		//LED on pin 7
GroveStreams myGS(gsServer, (const __FlashStringHelper*)gsApiKey, WAIT_LED);
```

## Methods
### begin(void)
##### Description
Initializes the GroveStreams library.
##### Syntax
`myGS.begin();`
##### Parameters
None.
##### Returns
None.

### run(void)
##### Description
Runs the GroveStreams state machine. This function should be called from `loop()` every time it is invoked, and `loop()` should be made to run as quickly as possible.
##### Syntax
`myGS.run();`
##### Parameters
None.
##### Returns
Status from the GroveStreams state machine. See the `ethernetStatus_t` enumeration in the [GroveStreams.h file](https://github.com/JChristensen/GroveStreams/blob/master/GroveStreams.h) *(ethernetStatus_t)*.
##### Example
```c++
ethernetStatus_t gsStatus;
gsStatus = myGS.run();
if ( gsStatus == CONNECT_FAILED )
{
	Serial.println("Could not connect to GroveStreams");
}
else
{
	Serial.println("Connected to GroveStreams");
}
```
### send(const char\* compID, const char\* data)
##### Description
Sends data to GroveStreams for a specific component.
##### Syntax
`myGS.send(compID, data);`
##### Parameters
**compID:** A zero-terminated char array containing the GroveStreams component ID to receive the data _(char*)_.

**data:** A zero-terminated char array containing the data to be sent. Each datastream must be formatted as ***&id=val*** where ***id*** is the GroveStreams datastream ID, and ***val*** is the value to be sent _(char*)_.
##### Returns
SEND_ACCEPTED or SEND_BUSY, the latter indicating that the data was not sent because a transmission is already in progress. See the `ethernetStatus_t` enumeration in the [GroveStreams.h file](https://github.com/JChristensen/GroveStreams/blob/master/GroveStreams.h) *(ethernetStatus_t)*.
##### Example
```c++
char myCompID[] = "c01";
char myPayload[40];
int speed, pressure, power;
sprintf( myPayload, "&ds1=%i&ds2=%i&ds3=%i, speed, pressure, power );
if ( myGS.send(myCompID, myPayload) == SEND_ACCEPTED )
{
    Serial.println("Data sent");
}
else
{
    Serial.println("Could not send data");
}
```
### mcuReset(uint32_t dly)
##### Description
Resets the microcontroller after a given number of milliseconds. The minimum is 4 seconds (4000 ms). If a number less than 4000 is given, the delay will be approximately 4 seconds.
##### Syntax
`myGS.mcuReset(dly);`
##### Parameters
**dly:** Delay in milliseconds before reset *(unsigned long)*.

##### Returns
None.
##### Example
```c++
myGS.resetMCU(60000);		//reset after a minute
```
### ipToText(char* dest, IPAddress ip)
##### Description
Utility function to convert an IP address to a zero-terminated char array for printing, etc.
##### Syntax
`myGS.ipToText(dest, ip);`
##### Parameters
**dest:** Array to receive the printable IP address. Minimum length is 16 characters _(char*)_.

**IPAddress:** Input IP address _(IPAddress)_.
##### Returns
None.
##### Example
```c++
IPAddress likeHome(127, 0, 0, 1);
char noPlace[16];
myGS.ipToText( noPlace, likeHome );
```
