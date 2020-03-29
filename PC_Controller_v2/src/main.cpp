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
const char *password = "******"; // Your router's WiFi password
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
void PowerOnAction(); // Log power on actions
void PowerOffAction(); // Log power off actions
void Logger(boolean z); // Logs time and date
void DisplayShutdown(); // Displays a message if the PC was shutdown through the OS
void DisplayHistory();

// TIME VARIABLES
const String Days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"}; 
const String Months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "Novemer", "December"};
uint8_t secondsIndex; // Stores seconds
uint8_t minutesIndex; // Stores minutes
uint8_t hoursIndex; // Stores hours
uint8_t dayIndex; // Stores day of week
uint8_t monthDay; // Stores day of month
uint8_t monthIndex; // Stores month
uint16_t year; // Stores year

struct LogData {
  uint16_t year;
  uint8_t month;
  uint8_t dayofmonth;
  uint8_t dayofweek;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
};

LogData Boot;
LogData P_Off;
boolean OffData = false;

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStampFull;
String timeStamp;

// TIME FUNCTIONS
void Time(void);
int getMonth();
int getMonthDay();
int getYear();
void DisplayTime();
void ShowDate();
String DayStamp(uint8_t day, uint8_t m_day, uint8_t month);

// DISPLAY FUNCTIONS
void DisplayInfo(String s);
void TranscodeScroll(String c, unsigned int x); // Scrolls a string on the display and holds it for x millisecs
void scrollText(char *p); // Function for text scrolling on DOT LED DISPLAY
void printText(uint8_t modStart, uint8_t modEnd, char *pMsg); // Function for displaying Text
void printString(String s); // Prints a short string on the display without scrolling
void delayAndClear(unsigned int x); 
void ClearInfo();

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
  reconnect();
  client.setCallback(callback);
  timeClient.begin(); // Set up time server
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT -1 = -3600
  timeClient.setTimeOffset(7200); 
  Time();
  mx.clear();
}

void loop() {
  //mx.clear();
  CheckPCState();
  setup_wifi();
  reconnect();
  client.loop();
  while (!PC_State && !SwitchState1) {
    Off_Mode();
  }
  CheckSwitchState();
  PowerOnAction();
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
    if (messageTemp == "timer_true") {

    }
    if (messageTemp == "timer") {
      
    }
    if (messageTemp == "date") {
      ShowDate();
    }
    if (messageTemp == "ip") {
      DisplayIP();
    }
    if (messageTemp == "history") {
      DisplayHistory();
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
  PowerOffAction();
  setup_wifi();
  CheckPCState();
  client.loop();
  DisplayTime();
  reconnect();
}

void CheckPCState() {
  static unsigned long PassedTime {0};
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

void PowerOnAction() {
  static boolean Greeting = true;
  static unsigned long uptime {0};
  if (!PC_State && !Greeting) {
    Greeting = true;
    uptime = millis();
  }
  if (PC_State && Greeting) {
    uptime = millis();
    if (hoursIndex > 4 && hoursIndex <= 11) TranscodeScroll("Good morning!", 500);
    else TranscodeScroll("Welcome back!", 500);
    Logger(true);
    mx.clear();
    Greeting = false;
  }
  static uint32_t	prevTime {0};
  if (millis()-prevTime >= 2000) {
    printString("U: " + String((int)(millis()-uptime)/60000) + "'");
    prevTime = millis();
  }
}

void PowerOffAction() {
  if (LastPCState) {
    LastPCState = false;
    Logger(false);
    OffData = true;
    DisplayShutdown();
  }
}

void Logger(boolean z) {
  Time();
  if (z) {
    Boot.hours = hoursIndex;
    Boot.minutes = minutesIndex;
    Boot.seconds = secondsIndex;
    Boot.dayofweek = dayIndex;
    Boot.dayofmonth = monthDay;
    Boot.month = monthIndex;
  }
  else if (!z) {
    P_Off.hours = hoursIndex;
    P_Off.minutes = minutesIndex;
    P_Off.seconds = secondsIndex;
    P_Off.dayofweek = dayIndex;
    P_Off.dayofmonth = monthDay;
    P_Off.month = monthIndex;
  }
}

void DisplayHistory() {
  mx.clear();
  String Stamp = DayStamp(Boot.dayofweek, Boot.dayofmonth, Boot.month);
  DisplayInfo("Maximus was powered-on on: " + Stamp + ", at: " + (String)Boot.hours + ":" + (String)Boot.minutes + ":" + (String)Boot.seconds);
  TranscodeScroll("Maximus was powered-on on: " + Stamp + ", at: " + (String)Boot.hours + ":" + (String)Boot.minutes + ":" + (String)Boot.seconds, 500);
  mx.clear();
  if (OffData) {
    Stamp = DayStamp(P_Off.dayofweek, P_Off.dayofmonth, P_Off.month);
    DisplayInfo("Maximus was powered-off on: " + Stamp + ", at: " + (String)P_Off.hours + ":" + (String)P_Off.minutes + ":" + (String)P_Off.seconds);
    TranscodeScroll("Maximus was powered-off on: " + Stamp + ", at: " + (String)P_Off.hours + ":" + (String)P_Off.minutes + ":" + (String)P_Off.seconds, 500);
    ClearInfo();
    mx.clear();
  }
}

void DisplayShutdown() {
  TranscodeScroll("System was Shut Down", 250);
  mx.clear();
  if (hoursIndex >= 21 || hoursIndex <= 3) TranscodeScroll("Goodnight!", 500);
  else TranscodeScroll("Goodbye!", 500);
}

// TIME FUNCTIONS
void DisplayTime() {
  static unsigned long checktime {0};
  if (checktime <= millis() + 5000) {
    Time();
    printString(timeStamp);
    checktime = millis();
  }
}

void ShowDate() {
  mx.clear();
  boolean Shown = false;
  boolean scrollonce = false;
  Time();
  mx.clear();
  dayStamp = DayStamp(dayIndex, monthDay, monthIndex);
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

String DayStamp(uint8_t day, uint8_t m_day, uint8_t month) {
  String Stamp;
  if (m_day == 1) Stamp = (String) (Days[day] + " the " + String(m_day) + "st of " + Months[month-1] + " " + year);
  else if (m_day == 2) Stamp = (String) (Days[day] + " the " + String(m_day) + "nd of " + Months[month-1] + " " + year);
  else if (m_day == 3) Stamp = (String) (Days[day] + " the " + String(m_day) + "rd of " + Months[month-1] + " " + year);
  else Stamp = (String) (Days[day] + " the " + String(m_day) + "th of " + Months[month-1] + " " + year);
  return Stamp;
}

void Time(void) {
  static unsigned long checktime {0}; 
  if (checktime <= millis() - 500) {
    static uint8_t lastminute {60}; // 
    static uint8_t lasthour {25}; //
    static uint8_t lastday; //
    static uint8_t lastmonth; //
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
    secondsIndex = timeClient.getSeconds();
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
    checktime = millis();
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
if (WiFi.status() != WL_CONNECTED) {
    // We start by connecting to a WiFi network
    TranscodeScroll("Connecting to ", 0);
    TranscodeScroll(ssid, 0);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      TranscodeScroll(".", 500);
      CheckSwitchState();
    }
    ipaddress = WiFi.localIP().toString();
    mx.clear();
    TranscodeScroll("WiFi connected", 500);
  }
}

void reconnect() { // try connection every 30 seconds
  static unsigned long mqttReconnect {0};
  if (!client.connected()) {
    if(mqttReconnect <= millis() - 60000) {
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