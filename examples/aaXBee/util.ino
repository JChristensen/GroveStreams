// Example sketch
// Arduino GroveStreams Library
// https://github.com/JChristensen/GroveStreams
// Copyright (C) 2015-2024 by Jack Christensen and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html
// Example sketch: Double-A XBee Sensor Node for GroveStreams.

// Miscellaneous utility functions.

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
