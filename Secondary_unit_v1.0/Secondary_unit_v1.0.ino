/*********************************************************************
Pro trinket 5V with an xbee module and a piezo buzzer to make noise (IoT board to be added later)
v0.1 Receiving signals via the xbee module
v0.2 Adding the piezo buzzer to make the alarm sound
v0.3 Updating the alarm to only go for about 60 seconds and fixing the code to ignore an incoming signal during the countdown that was resetting it
v1.0 Removing the lines that were included for troubleshooting and updating the comments for publishing
*********************************************************************/

#include <SoftwareSerial.h>
SoftwareSerial mySerial = SoftwareSerial(4, 3); //pins used for the xbee
char character; // to read the content of the incoming signal through xbee
bool AlarmON = false;

uint16_t TimeToWait = 13000; //the time (in milliseconds) between the signal received and the alarm going off
unsigned long FirstTime = 0; // to store the time when the 1st signal is received, so that we can wait for TimeToWait
unsigned long CurrentTime = 0; // to store the current time so that we can check for TimeToWait
bool FirstSignal = true; //to ignore incoming signals during the countdown

void setup() {
  mySerial.begin(9600); //for the xbee
  analogWrite(6, 0); //the pin used for the piezo buzzer
}

void loop(void) {
  if (AlarmON == true) {
    if (FirstSignal == true) {
      FirstTime = millis();
      FirstSignal = false;
    }
    CurrentTime = millis();
    if (CurrentTime <= (FirstTime + TimeToWait)) {
      //do nothing
    }
    else {
      alarm();
    }
  }
  ReadXbee(); //check if there is incoming signal
}

void alarm() {
  while (AlarmON == true) {
    for (int x = 0; x < 120; x++) { // make some noise(!!!) and keep checking the xbee for about 60 seconds (120 times 500 milliseconds)
    analogWrite(6, 255);
    delay(250);
    analogWrite(6, 122);
    delay(250);
    ReadXbee();
    if (AlarmON == false) {
      break;
      }
    }
  // if the alarm is on for more than 60 seconds, turn it off and reset
  AlarmON = false;
  analogWrite(6, 0);
  FirstSignal = true;
  }
}

void ReadXbee () {
  if (mySerial.available()) {
    character = ((char)mySerial.read());
    if (character == '1') {
      AlarmON = true;
    }
    else if (character == '0') {
      AlarmON = false;
      analogWrite(6, 0);
      FirstSignal = true;
    }
  }
}
