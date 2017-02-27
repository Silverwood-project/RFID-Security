/* 
Pro Trinket with NFC reader, TFT touchscreen, xbee module and a reed switch
v0.1 Configured to read cards and identify me and a guest card
v0.2 Added the tft screen and experimented with interrupts
v0.3 Figured out how to use the interrupt and set up some functions
v0.4 Optical tuning on the TFT screen (nicer identification and alarm toggle handling)
v0.5 Adding the magnetic (reed) switch to interrupt instead of the pushbutton, setting up the timer to ignore the door opening and closing after the alarm is turned on
v0.6 Adding the xbee module
v0.7 Porting to the Pro Trinket, changing PIN for the RFID shield IRQ from 2 to 8
v0.8 Updating the loop to rule out false alarms due to power fluctuatuion near the magnetic sensor and the wiring (based on initial results of the test run)
v1.0 Removing the lines that were included for troubleshooting and updating the comments for publishing.
     Also modifying the NFC tag reading function taking the strings and tag numbers out and bringing them to the beginning of the code for easier usability.
*/

//IMPORTANT!!!
//**************************************************************************

//Enter the UID of your NFC tag(s) here. You should be able to find out the numbers by reading the tags first.
//With this hardware using the Adafruit PN532 shield, use the example sketch provided as part of the "Adafruit_PN532" library.
//You can add more than two, just remember to update the "identify()" function too.

String myTag = "XXXX"; //enter the UID here
int myChar = 16; //the number of characters in the UID of your tag
String myName = "Master"; //the name that will be displayed when you come home and turn the alarm off

String guestTag = "XXXX"; //guest UID (if you don't have one, leave it as it is)
int guestChar = 12; //the number of characters in the UID of the guest tag
String guestName = "guest"; //the name that will be displayed when the guest tag is used

//**************************************************************************


#include "Adafruit_ILI9341.h"
#include <Adafruit_PN532.h>
#include <SoftwareSerial.h>
SoftwareSerial mySerial = SoftwareSerial(5, 4); // TX and RX pins for the xbee

//TFT shield:
#define TFT_DC 9
#define TFT_CS 10
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

//NFC shield using I2C:
#define PN532_IRQ   (8)
#define PN532_RESET (A3)  // Not used but needs to be defined
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

//defining the variables:
  bool AlarmOn = false;
  bool AlarmTrigger = false;
  bool FirstTrigger = true;
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID of the NFC tag (7 bytes max)
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on the type of the NFC tag)
  String UID;  
  uint16_t TimeToExit = 10000; //the time delay to ignore the door opening and closing after turning the alarm on (in milliseconds)
  unsigned long AlarmOnTime = 0; // to store the time when the alarm is turned on, so that we can wait for TimeToExit
  unsigned long CurrentTime = 0; // to store the current time so that we can check for TimeToExit
  
/**************************************************************************/

void setup(void) {
  mySerial.begin(9600); //for the xbee
  nfc.begin();
  tft.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    //Didn't find PN53x board (seems to be a hardware error)
    while (1); // halt
  }

// Set the max number of retry attempts to read from a card
// This prevents us from waiting forever for a card, which is
// the default behaviour of the PN532.
  nfc.setPassiveActivationRetries(0x10);
    
  nfc.SAMConfig(); // configure the board to read NFC tags

  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(1);
  tft.setTextSize(6);
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_WHITE);
  tft.println("RESET"); //to let you know if there was a power outage

// to define the interrupt for the trigger (the 2 pins used for the reed switch)
  pinMode(3, INPUT_PULLUP);
  pinMode(6, OUTPUT);
  digitalWrite(6, LOW);
  attachInterrupt(1, Trigger, RISING); // either FALLING or RISING based on the type of sensor (trigger is the name of the function to be called by the interrupt)

} //End of setup
/**************************************************************************/


void Trigger() {
  if (FirstTrigger == true){
    FirstTrigger = false; //ignore the trigger going off when the system is plugged in or when the power is restored after a power outage
  }
  else if (AlarmOn == true) {
    CurrentTime = millis();
    if (CurrentTime <= (AlarmOnTime + TimeToExit)) {
      //alarm is ignored for 10 seconds giving you time to leave after the alarm is turned on
    }
    else {
      //alarm is going active now
      AlarmTrigger = true;
      mySerial.println("1"); //sending the 1st signal through the xbee module to start the countdown on the receiver side too
    }
  }
  else {
    //the alarm is off, so opening the door will be ignored
  }
}


void loop(void) {
  if (AlarmTrigger == true) {
    //If the sensor thinks the door is open, do another reading of it with half a second delay.
    //This is to rule out false alarms due to power fluctuation near the magnetic sensor and the wiring.
    delay(500);
    byte var = digitalRead(3); //to check the state of the reed switch again with half a second delay
    if (var == 0) { //false alarm
      AlarmTrigger = false;
      mySerial.println("0"); //sending the 2nd signal through the xbee module to turn the alarm off
    }
    else { //countdown before the alarm goes off
      tft.fillScreen(ILI9341_RED);
      tft.setCursor(20, 20);
      tft.setTextColor(ILI9341_BLACK);
      tft.println("ALARM IN");
      tft.setTextSize(10);
      for (int x = 10; x > 0; x--) { //10 seconds on the clock
        tft.setCursor(150, 100);
        tft.setTextColor(ILI9341_BLACK);
        tft.println(x);
        identify(); //check for an NFC tag to abort the countdown
        if (AlarmTrigger == false) {
          //alarm deactivated
          break;
        }
        delay(1000);
        tft.setCursor(150, 100);
        tft.setTextColor(ILI9341_RED);
        tft.println(x);
      }
      if (AlarmTrigger == true) {
        AlarmTriggered();
      }   
    }
  }
  identify(); //check for an NFC tag to turn the alarm on
} // end of the loop
/**************************************************************************/


void AlarmTriggered() {
  //the alarm is going off with loud noise and the screen is blinking "ALARM" in red and black
  tft.setTextSize(6);
  tft.setCursor(20, 20);
  tft.setTextColor(ILI9341_RED);
  tft.println("ALARM IN");
  tft.setTextSize(10);
  while (AlarmTrigger == true) {
    tft.setCursor(10, 100);
    tft.setTextColor(ILI9341_RED);
    tft.println("ALARM");
    identify(); //check for an NFC tag to turn the alarm off
    tft.setCursor(10, 100);
    tft.setTextColor(ILI9341_BLACK);
    tft.println("ALARM");
  }
  tft.setTextSize(6);
}

void identify() {
  // Wait for an NFC tag.  When one is found 'uid' will be populated with
  // the UID, and uidLength will indicate the size of the UID (normally 7 bytes)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  if (success) {  
    UID.concat(uid[0]); // concatenate the 7 bytes into 1 string
    UID.concat(uid[1]);
    UID.concat(uid[2]);
    UID.concat(uid[3]);
    UID.concat(uid[4]);
    UID.concat(uid[5]);
    UID.concat(uid[6]);
  
    if (UID.substring(0,myChar) == myTag) { //your NFC tag recognized
      tft.fillScreen(ILI9341_BLACK);
      tft.setTextSize(6);
      tft.setCursor(0, 0);
      tft.setTextColor(ILI9341_WHITE);
      AlarmOn = !AlarmOn;
      AlarmTrigger = false;
      if (AlarmOn == true) { //the alarm is being turned on now
        tft.println("Alarm on");
        AlarmOnTime = millis();
      }
      else { //the alarm is being turned off now
        mySerial.println("0"); //sending the 2nd signal through the xbee module to turn the alarm off
        tft.println("Welcome");
        tft.println();
        tft.println("home");
        tft.println();
        tft.println(myName);
      }
      delay(2000);
      tft.fillScreen(ILI9341_BLACK);
    } //end of your NFC tag recognized

    else if (UID.substring(0,guestChar) == guestTag){ //guest NFC tag recognized
      tft.fillScreen(ILI9341_BLACK);
      tft.setTextSize(6);
      tft.setCursor(0, 0);
      tft.setTextColor(ILI9341_WHITE);
      AlarmOn = !AlarmOn;
      AlarmTrigger = false;
      if (AlarmOn == true) { //the alarm is being turned on now
        tft.println("Alarm on");
        AlarmOnTime = millis();
      }
      else {
        mySerial.println("0"); //sending the 2nd signal through the xbee module to turn the alarm off
        tft.println("Welcome");
        tft.println();
        tft.println(guestName);
      }
      delay(2000);
      tft.fillScreen(ILI9341_BLACK);
    } // end of guest NFC tag recognized
    else {
      // in case a card is read but not recognized, just ignore it
    }
    UID=""; //empty the variable
  } // end of successful read
} // end of the identify() function for reading the NFC tag

