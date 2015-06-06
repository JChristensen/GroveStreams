//Various utility functions
//
//Double-A XBee Sensor Node by Jack Christensen is licensed under
//CC BY-SA 4.0, http://creativecommons.org/licenses/by-sa/4.0/

//print date and time to Serial
void printDateTime(time_t t, bool newLine)
{
    printDate(t);
    printTime(t);
    if (newLine) Serial << endl;
}

//print time to Serial
void printTime(time_t t)
{
    printI00(hour(t), ':');
    printI00(minute(t), ':');
    printI00(second(t), ' ');
}

//print date to Serial
void printDate(time_t t)
{
    Serial << year(t) << '-';
    printI00(month(t), '-');
    printI00(day(t), ' ');
}

//Print an integer in "00" format (with leading zero),
//followed by a delimiter character to Serial.
//Input value assumed to be between 0 and 99.
void printI00(int val, char delim)
{
    if (val < 10) Serial << '0';
    Serial << _DEC(val) << delim;
    return;
}

