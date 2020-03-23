#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <MD_MAX72xx.h>
#include <NTPClient.h>
#include <PubSubClient.h>

// OUTPUTS =========================================================================================
struct Relays {
  const byte PR_Relay {32}; // Connects to power relay channel
  const byte RS_Relay {33}; // Connects to reset relay channel
  const byte BL_LED_Relay {25}; // Connects to Blue LED relay channel - not yet implemented
  const byte WT_LED_RELAY {26}; // Connects to White LED relay channel - not yet implemented
};
Relays RelaySet; // Create the relay set instance
const byte Led_BuiltIn {2}; // Enable Built-in LED for NODEMCU-32S

// INPUTS ===========================================================================================
struct Switches {
  const byte PWR_SW {36}; // Connects to Power Switch with input pulldown
  const byte RST_SW {39}; // Connects to Reset Switch with input pulldown
  const byte RST_State {34}; // If HIGH it means the PC is ON
};
Switches Asus; // Create the switches set instance

// WIFI PARAMETERS ===================================================================================
const char *ssid = "******"; // Your router's SSID
const char *password = "******; // Your router's WiFi password
void setup_wifi();
void DisplayIP();
String ipaddress; // Stores the ESP's IP address

// MQTT Broker - Connection ==========================================================================
const char* mqtt_server = "192.168.1.50";
WiFiClient espClient;
PubSubClient client(espClient);
#define Output_Topic "esp32/output_topic" 
#define User_Input "esp32/user_input" 
#define Pub_State "esp32/state_topic"

void reconnect();
void callback(char* topic, byte* message, unsigned int length);
unsigned long mqttReconnect;

// SET DOT LED MATRIX DISPLAY PARAMETERS =============================================================
#define HARDWARE_TYPE MD_MAX72XX::ICSTATION_HW  // @EB-setup define the type of hardware connected
#define CLK_PIN     18                          // @EB-setup CLK / SCK pin number  (EB setup: yellow)
#define DATA_PIN    23                          // @EB-setup DIN / MOSI pin number (EB setup: brown)
#define CS_PIN      5                           // @EB-setup LOAD / SS pin number  (EB setup: green)
#define MAX_DEVICES 4  
#define CHAR_SPACING  1 // pixels between characters
#define PRINTS(x)
// SPI hardware interface
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Define NTP Client to get time ===============================================================
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
String IP; // A string to store the ESP's IP address

// SYSTEM POWER VARIABLES
boolean SwitchState1 = false;
boolean SwitchState2 = false;
boolean PC_State = false;
boolean LastPCState = false; // Use it to print shutdown message when shutdown was done from the OS
boolean ConfirmCalled = false;
String answer;
unsigned long checkconfirm;

// Variables to check passed time without using delay
unsigned long PassedTime;

// SYSTEM POWER FUNCTIONS
void Off_Mode(); // This function is looped when the PC is off
void CheckSwitchState(); // It checks if the power and reset buttons have been pushed
void CheckPCState(); // Checks if the PC is on or off
void PowerAction(); // Controls the power relay 
void ResetAction(); // Controls the reset relay
void HaltAction(); // Controls the power relay according if halt command is called
void RemotePowerOn(); // It's called when power on or off is requested through node red
void RemoteReset(); // It's called when reset is requested through node red
void RemoteHalt(); // It's called when power halt is requested through node red
void Confirm(String s);
void IRAM_ATTR Power() { // Function to Power On or Off the system
  SwitchState1 = true;
}; 
void IRAM_ATTR Reset() { // Function to Restart the system
  SwitchState2 = true;
};
void DisplayShutdown(); // Displays a message if the PC was shutdown through the OS

// TIME VARIABLES
const String Days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"}; 
const String Months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "Novemer", "December"};
int secondsIndex; // Stores seconds
int minutesIndex; // Stores minutes
int hoursIndex; // Stores hours
int dayIndex; // Stores day of week
int monthDay; // Stores day of month
int monthIndex; // Stores month
int year; // Stores year

int lastminute; // 
int lasthour {25}; //
int lastday; //
int lastmonth; //

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStampFull;
String timeStamp;

unsigned long checktime;
// TIME FUNCTIONS
void Time(void);
int getMonth();
int getMonthDay();
int getYear();
void DisplayTime();
void ShowDate();
void DayStamp();

// DISPLAY FUNCTIONS
void DisplayInfo(String s);
void TranscodeScroll(String c, unsigned int x);
void scrollText(char *p); // Function for text scrolling on DOT LED DISPLAY
void printText(uint8_t modStart, uint8_t modEnd, char *pMsg); // Function for displaying Text
void printString(String s);
void delayAndClear(unsigned int x);
void ClearInfo();
unsigned long clearTime; 

void setup() {
  pinMode(RelaySet.PR_Relay, OUTPUT);
  pinMode(RelaySet.RS_Relay, OUTPUT);
  pinMode(RelaySet.BL_LED_Relay, OUTPUT);
  pinMode(RelaySet.WT_LED_RELAY, OUTPUT);
  pinMode(Led_BuiltIn, OUTPUT);
  digitalWrite(RelaySet.PR_Relay, HIGH);
  digitalWrite(RelaySet.RS_Relay, HIGH);
  pinMode(Asus.PWR_SW, INPUT);
  pinMode(Asus.RST_SW, INPUT);
  pinMode(Asus.RST_State, INPUT);
  // Attach the two Interrupts
  attachInterrupt(digitalPinToInterrupt(Asus.PWR_SW), Power, HIGH);
  attachInterrupt(digitalPinToInterrupt(Asus.RST_SW), Reset, HIGH);
  // Start the DOT LED DISPLAY
  mx.begin();
  TranscodeScroll("MAXIMUS-III", 250);
  mx.clear();
  setup_wifi(); // Set up WIFI
  client.setServer(mqtt_server, 1883); // Set up MQTT broker
  client.setCallback(callback);
  timeClient.begin(); // Set up time server
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT -1 = -3600
  timeClient.setTimeOffset(7200); 
  PassedTime = millis();
  checktime = millis();
  mqttReconnect = millis();
  mx.clear();
}

void loop() {
  mx.clear();
  while (!PC_State && !SwitchState1) {
    Off_Mode();
  }
  CheckSwitchState(); 
  reconnect();
  client.loop();
  CheckPCState();
}

void callback(char* topic, byte* message, unsigned int length) {
  String messageTemp;
  for (int i = 0; i < length; i++) {
    //Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  // Calls a function according to the message
  if (String(topic) == User_Input) {
    if(messageTemp == "on"){
      Confirm("on");
    }
    if (messageTemp == "reset") {
      Confirm("reset");
    }
    if (messageTemp == "halt") {
      Confirm("halt");
    }
    if (messageTemp == "date") {
      ShowDate();
    }
    if (messageTemp == "ip") {
      DisplayIP();
    }
    if (messageTemp == "clear") {
    //  ClearInfo();
    //  mx.clear();
    }
    
    else {
      //digitalWrite(2, LOW);
    }
  }
}

// SYSTEM POWER FUNCTIONS
void Off_Mode() {
  DisplayShutdown();
  CheckPCState();
  client.loop();
  DisplayTime();
  reconnect();
}

void CheckPCState() {
  if (PassedTime <= millis() - 1000) {
    if(digitalRead(Asus.RST_State)) {
      PC_State = true;
      client.publish(Pub_State, "true");
      if (!ConfirmCalled) DisplayInfo("PC is on");
    }
    else {
      PC_State = false;
      client.publish(Pub_State, "false");
      if (!ConfirmCalled) DisplayInfo("PC is off");
    }
    if (PC_State && !LastPCState) {
      LastPCState = true;
      //DisplayPowerOn();
    }
    PassedTime = millis();
  }
}

void Confirm(String s) {
  ConfirmCalled = !ConfirmCalled;
  if (ConfirmCalled) {
    if ((s == "reset" || s == "halt") && !PC_State) DisplayInfo("System is off!");
    else {
      checkconfirm = millis();
      answer = s;
      DisplayInfo("Are you sure?");
    }
  }
  if (!ConfirmCalled && (checkconfirm >= millis() - 5000)) {
    if (s == answer) {
      if (answer == "on") RemotePowerOn();
      if (answer == "reset") RemoteReset();
      if (answer == "halt") RemoteHalt();
    }
  }
}

void RemotePowerOn() {
  mx.clear();
  if (!PC_State) {
    DisplayInfo("Power ON");
    TranscodeScroll("Remote Power ON", 0);
  }
  else {
    DisplayInfo("Shut Down");
    TranscodeScroll("Remote Shut Down", 0);
  }
  PowerAction();
}

void RemoteReset() {
  mx.clear();
  DisplayInfo("RESET");
  TranscodeScroll("Remote Reset", 0);
  ResetAction();
}

void RemoteHalt() {
  mx.clear();
  DisplayInfo("HALT");
  TranscodeScroll("Remote Halt", 0);
  HaltAction();
}

void PowerAction() {
  mx.clear();
  digitalWrite(RelaySet.PR_Relay, LOW);
  delay(1000);
  digitalWrite(RelaySet.PR_Relay, HIGH);
  if (!PC_State) TranscodeScroll("Power ON", 1000);
  else TranscodeScroll("Power OFF", 1000);
  SwitchState1 = false;
}

void ResetAction() {
  mx.clear();
  digitalWrite(RelaySet.RS_Relay, LOW);
  delay(1000);
  digitalWrite(RelaySet.RS_Relay, HIGH);
  TranscodeScroll("System RESET", 1000);
  SwitchState2 = false;
}

void HaltAction() {
  digitalWrite(RelaySet.PR_Relay, LOW);
  delay(5200);
  digitalWrite(RelaySet.PR_Relay, HIGH);
  mx.clear();
  TranscodeScroll("System HALT", 1000);
}

void CheckSwitchState() {
  if (SwitchState1) PowerAction();
  if (SwitchState2) ResetAction();
}

void DisplayShutdown() {
  if (LastPCState) {
    LastPCState = false;
    TranscodeScroll("System was Shut Down", 250);
  }
}

// TIME FUNCTIONS
void DisplayTime() {
  if (checktime <= millis() + 5000) {
    Time();
    printString(timeStamp);
  }
}

void ShowDate() {
  mx.clear();
  boolean Shown = false;
  boolean scrollonce = false;
  Time();
  DayStamp();
  mx.clear();
  DisplayInfo(dayStamp);
  TranscodeScroll("Today is: " + dayStamp, 500);
  unsigned long current_time = millis();
  while(!Shown) {
    Time();
    DisplayInfo(timeStampFull);
    if (!scrollonce) {
      mx.clear();  
      printString(timeStamp);
      scrollonce = true;
    }
    if (current_time <= millis() - 4000) {
      Shown = true;
    }
  }
  ClearInfo();
  mx.clear();
}

void DisplayIP() {
  mx.clear();
  DisplayInfo("ESP32's IP is: " + ipaddress);
  TranscodeScroll("ESP32's IP is: " + ipaddress, 500);
  mx.clear();  
  DisplayInfo("MQTT broker's IP is: " + (String)mqtt_server);
  TranscodeScroll("MQTT broker's IP is: " + (String)mqtt_server, 500);
  ClearInfo();
  mx.clear();
}

void DayStamp() {
  if (monthDay == 1) dayStamp = (String) (Days[dayIndex] + " the " + String(monthDay) + "st of " + Months[monthIndex-1] + " " + year);
  else if (monthDay == 2) dayStamp = (String) (Days[dayIndex] + " the " + String(monthDay) + "nd of " + Months[monthIndex-1] + " " + year);
  else if (monthDay == 3) dayStamp = (String) (Days[dayIndex] + " the " + String(monthDay) + "rd of " + Months[monthIndex-1] + " " + year);
  else dayStamp = (String) (Days[dayIndex] + " the " + String(monthDay) + "th of " + Months[monthIndex-1] + " " + year);
}

void Time(void) {
  if (checktime <= millis() - 500) {
    while(!timeClient.update()) {
      timeClient.forceUpdate();
    }
    // The formattedDate comes with the following format:
    // 2018-05-28T16:00:13Z
    // We need to extract date and time
    formattedDate = timeClient.getFormattedDate();
    int splitT = formattedDate.indexOf("T");
    //dayStamp = formattedDate.substring(0, splitT);
    timeStampFull = formattedDate.substring(splitT+1, formattedDate.length()-1);
    timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-4);
    //secondsIndex = timeClient.getSeconds();
    minutesIndex = timeClient.getMinutes();
    if (lastminute != minutesIndex) {
      hoursIndex = timeClient.getHours();
      lastminute = minutesIndex;
      if (lasthour != hoursIndex) {
        dayIndex = timeClient.getDay();
        monthDay = getMonthDay();
        lasthour = hoursIndex;
        if (lastday != monthDay) {
          monthIndex = getMonth();
          lastday = monthDay;
          if (lastmonth != monthIndex) {
            year = getYear();
            lastmonth = monthIndex;
          }
        }
      }
    }
  }
}

int getMonthDay() {
  int monthday;
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti;
  ti = localtime (&rawtime);
  monthday = (ti->tm_mday) < 10 ? 0 + (ti->tm_mday) : (ti->tm_mday);
  return monthday;
}

int getMonth() {
  int month;
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti;
  ti = localtime (&rawtime);
  month = (ti->tm_mon + 1) < 10 ? 0 + (ti->tm_mon + 1) : (ti->tm_mon + 1);
  return month;
}

int getYear() {
  int year;
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti;
  ti = localtime (&rawtime);
  year = ti->tm_year + 1900;
  return year;
}

// CONNECTION FUNCTIONS
void setup_wifi() { // OK
  // We start by connecting to a WiFi network
  TranscodeScroll("Connecting to ", 0);
  TranscodeScroll(ssid, 0);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    TranscodeScroll(".", 500);
  }
  ipaddress = WiFi.localIP().toString();
  mx.clear();
  TranscodeScroll("WiFi connected", 500);
}

void reconnect() { // try connection every 30 seconds
  if (!client.connected()) {
    if(mqttReconnect <= millis() - 30000) {
      digitalWrite(Led_BuiltIn, LOW);
      mx.clear();
      TranscodeScroll("Attempting MQTT connection", 250);
      // Attempt to connect
      if (client.connect("ESP32_Client")) {
        digitalWrite(Led_BuiltIn, HIGH);
        mx.clear();
        TranscodeScroll("Connected", 250);
        // Subscribe
        client.subscribe("esp32/user_input");
      } else {
        mx.clear();
        TranscodeScroll("Connection failed", 500);
        TranscodeScroll(" - Please check your MQTT broker", 2000);
      }
      mqttReconnect = millis();
    }
  }
}

// DISPLAY FUNCTIONS
void ClearInfo() {
  DisplayInfo("");
}

void DisplayInfo(String s) {
  int n = s.length(); 
  char char_array[n + 1]; 
  strcpy(char_array, s.c_str());
  client.publish(Output_Topic, char_array);
}

void TranscodeScroll(String c, unsigned int x) {
  int n = c.length(); 
  char char_array[n + 1]; 
  strcpy(char_array, c.c_str());
  scrollText(char_array);
  delay(x);
}

void delayAndClear(unsigned int x) {
  delay(x);
  mx.clear();
}

void scrollText(char *p) {
  const int DELAYTIME {50}; // In millisecs
  uint8_t charWidth;
  uint8_t cBuf[8];  // Should be ok for all built-in fonts
  PRINTS("\nScrolling text");
  while (*p != '\0') {
    charWidth = mx.getChar(*p++, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);
    for (uint8_t i=0; i<=charWidth; i++) {// allow space between characters
      mx.transform(MD_MAX72XX::TSL);
      if (i < charWidth)
        mx.setColumn(0, cBuf[i]);
      delay(DELAYTIME);
    }
  }
}

void printString(String s) {
  int n = s.length(); 
  char char_array[n + 1]; 
  strcpy(char_array, s.c_str());
  printText(0, MAX_DEVICES-1, char_array);
}

void printText(uint8_t modStart, uint8_t modEnd, char *pMsg) {
// Print the text string to the LED matrix modules specified.
// Message area is padded with blank columns after printing.
  uint8_t   state = 0;
  uint8_t   curLen;
  uint16_t  showLen;
  uint8_t   cBuf[8];
  int16_t   col = ((modEnd + 1) * COL_SIZE) - 1;
  mx.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  do {   // finite state machine to print the characters in the space available
    switch(state) {
      case 0: // Load the next character from the font table
        // if we reached end of message, reset the message pointer
        if (*pMsg == '\0') {
          showLen = col - (modEnd * COL_SIZE);  // padding characters
          state = 2;
          break;
        }
        // retrieve the next character form the font file
        showLen = mx.getChar(*pMsg++, sizeof(cBuf)/sizeof(cBuf[0]), cBuf);
        curLen = 0;
        state++;
        // !! deliberately fall through to next state to start displaying
      case 1: // display the next part of the character
        mx.setColumn(col--, cBuf[curLen++]);
        // done with font character, now display the space between chars
        if (curLen == showLen) {
          showLen = CHAR_SPACING;
          state = 2;
        }
        break;
      case 2: // initialize state for displaying empty columns
        curLen = 0;
        state++; // fall through
      case 3:	// display inter-character spacing or end of message padding (blank columns)
        mx.setColumn(col--, 0);
        curLen++;
        if (curLen == showLen)
          state = 0;
        break;
      default:
        col = -1;   // this definitely ends the do loop
    }
  } while (col >= (modStart * COL_SIZE));
  mx.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}