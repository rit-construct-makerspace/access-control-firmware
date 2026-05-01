#include <TFT_eSPI.h>
#include "Free_Fonts.h" 

// Global Variables standardized to camelCase
byte currentScene = 4; //Stores what scene we should be showing.
bool noNetwork = false; //If set to true, shows a no network warning over any other scene.
String acsState = "UNKNOWN"; // Renamed from State to avoid ambiguity
String mode = "INSERT";
byte setRotation = 5;
bool isWelcoming = 0;
byte channels = 4; //How many access controller channels we expect to display info on
String startupMessage = "Waiting for Core..."; //Used to indicate state during startup
String motd = "This is a test MOTD to see how things work."; 
String errorMotd = ""; // Holds local equipment error strings
String faultMessage = "";
String filedScene = "";
String lastFiledScene = "";
bool forceUpdateScreen = false;

volatile int otaProgressPercent = 0;

bool hasSpeaker = false;
bool sceneStorage = false;
String sdCardType;
String sdCardCapacity;

byte volume = 12; // 0 to 21

bool isDenied = 0; //1 if we are denying an access attempt
String deniedReason;
bool identify = false;
bool restartFlag = false;
String url = "make.rit.edu";

// Define the structure for our equipment
struct Equipment {
  String name;
  String state;
  bool accessDenied;
  String deniedReason;
  unsigned long long activeStartTime; // NEW: Records millis64() when unlocked
};

// Array to hold the max possible channels (4)
Equipment channelsData[4];

// The variable determining how many channels to show (2 to 4)
uint8_t activeChannels = 4; 

void setup() {
  Serial.begin(115200);

  // --- Load Dummy Data for Testing ---
  // Notice channel 1 is technically IDLE, but access is denied.
  channelsData[0] = {"CNC Router", "UNLOCKED", false, "",millis64()};
  channelsData[1] = {"3D Printer 1", "IDLE", true, "Your account is archived (Are you an alumni?). Talk to staff for more info.",0};
  channelsData[2] = {"Laser Cutter", "IDLE", false, "",0};
  channelsData[3] = {"Lathe", "LOCKED_OUT", false, "",0}; 

  // Draw the UI
  xTaskCreate(screenServer, "ScreenServer", 50000, NULL, 4, NULL);
}

void loop() {
  // Update logic goes here
}

uint64_t millis64(){
  return esp_timer_get_time() / 1000;
}