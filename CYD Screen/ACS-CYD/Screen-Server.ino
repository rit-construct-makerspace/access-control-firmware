/*

Screen Server

This task handles actually displaying information on the screen, running in a constant 60FPS loop switching between pre-determined animations.

*/

#define TARGET_FRAMERATE 60 //frames per second

//Variables standardized to camelCase
unsigned long long sceneStartTime = 0; //Tracks how long we've been in this scene
bool newScene = true; //Set to true to indicate we are changing scenes
bool firstBounce = true; //Used for scenes that transition to screensaver mode after a bit.
bool networkChange = false;
uint16_t bgColor; //Used so we can cover up when we use no net corner indicator
unsigned long long faultFlashTime = 0;
bool faultFlash = false;
bool screenHorizontal = true; // MOVED TO GLOBAL: Used to determine if the scene should be rendered horizontally or vertically

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft); // Create the sprite object
TFT_eSprite motdSprite = TFT_eSprite(&tft); 
int motdHeight = 30; // Height of the ticker area
bool motdActiveLastFrame = false;

// --- Screen & Color Settings ---
int screenW = 320;     
int screenH = 240;     

// --- Logo Variables ---
float xPos = 20, yPos = 20;      // Current position
float oldX = 20, oldY = 20;      // Previous position for smart erase
float dx = 0.5;                  // Horizontal speed/direction
float dy = 0.5;                  // Vertical speed/direction
int logoW = 64;         // Width of your logo sprite
int logoH = 64;         // Height of your logo sprite

// --- MOTD Variables ---
int motdX = screenW;          // Start the text at the right edge
int scrollSpeed = 1;          // How many pixels to move per frame

// --- Identify Variables ===
bool identifyStep = false;
unsigned long long identifyTime = 0;

// --- Corner Clock Variables ---
TFT_eSprite timeSprite = TFT_eSprite(&tft);
unsigned long long lastCornerChangeTime = 0;
int currentCorner = 0; // 0: Top-Right, 1: Bottom-Right, 2: Bottom-Left
int oldClockX = -1;
int oldClockY = -1;

// Function prototypes for this file
void startingScreen();
void idleScreen();
void singleEquipmentApproved();
void singleEquipmentDenied();
void multiEquipmentAccess();
void welcomeScreen();
void lockedScreen();
void faultScreen();
void showFiledScene();
void otaScreen(String component);
void createSpriteBounce(String text, uint16_t rectColor, uint16_t textColor);
void runSpriteBounce();
bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
void identifyScreen();
void drawWrappedCenteredText(String text, int y, int maxWidth);
void printWrappedText(String text, int x, int y, int maxWidth);
void runCornerClock();
void runMOTD();

void screenServer(void *pvParameters){
  //First time startup of the screen
  tft.init();
  tft.setRotation(1); //1 for default, set to 3 for test on equipment that is upside-down mounted
  tft.invertDisplay(true);
  tft.fillScreen(TFT_BLACK);
  bgColor = TFT_BLACK;

  byte previousScene = 0; //Stores the last scene we displayed
  unsigned long long frameTime = 0; //When we started drawing this frame, to control framerate

  //Sprite-related startup
  screenW = tft.width();
  screenH = tft.height();
  sprite.createSprite(logoW, logoH);
  motdSprite.createSprite(screenW, motdHeight);
  timeSprite.createSprite(90, 25);

  //Start the JPEG decoder;
  TJpgDec.setCallback(tftOutput);
  TJpgDec.setJpgScale(1);

  while(1){

    //Log when we start the loop to try and keep a framerate
    frameTime = millis64() + (1000 / TARGET_FRAMERATE);

    //First, check if we are in a new display
    if(currentScene != previousScene){
      Serial.print(F("Changing to scene: ")); Serial.println(currentScene);
      previousScene = currentScene;
      tft.fillScreen(TFT_BLACK);
      bgColor = TFT_BLACK;
      sceneStartTime = millis64();
      //If any scene needed restarting/initialization, this is where we could handle it. 
      newScene = true;
      firstBounce = true;
    }

    //Let's check and execute on the scene
    switch (currentScene){
      case 0: startingScreen(); break;
      case 1: idleScreen(); break;
      case 2: singleEquipmentApproved(); break;
      case 3: singleEquipmentDenied(); break;
      case 4: multiEquipmentAccess(); break;
      case 5: welcomeScreen(); break;
      case 6: lockedScreen(); break;
      case 7: faultScreen(); break;
      case 8: showFiledScene(); break;
      case 9: otaScreen("Screen"); break;
      case 10: otaScreen("Core"); break;
      case 11: identifyScreen(); break;
    }

    //Lastly, run through some checks that we always do, no matter the scene;

    //Should we say there is no network?
    if(noNetwork){
      networkChange = noNetwork;
      // Standard font 2 is approx 16px high
      int rectW = 60; 
      int rectH = 20;

      // 2. Draw the background rectangle (Top-Left Corner)
      // fillRect(x, y, width, height, color)
      tft.fillRect(0, 0, rectW, rectH, TFT_WHITE);

      // 3. Configure text settings
      tft.setTextColor(TFT_BLUE); // Black text on white background
      tft.setTextSize(1);          // Adjust size as needed
      
      // 4. Position and print the text
      // drawString(string, x, y, font_number)
      // We offset x and y slightly to center the text in the box
      tft.drawString("NO NET", 30, 10, 2);
    } else if(networkChange){
      //The network just came back
      networkChange = false;
      int rectW = 60; 
      int rectH = 20;

      //This essentially overwrites what we did before
      tft.fillRect(0, 0, rectW, rectH, bgColor);
      //For the filed screen, we should actually just re-draw the whole thing to cover this up.
      forceUpdateScreen = true;
    }

    //Do we need to rotate the screen?
    if(setRotation != 5){
      if(setRotation == 1 || setRotation == 3){
        screenHorizontal = true;
      } else {
        // Rotation 0 and 2 are Portrait modes. 
        screenHorizontal = false; 
      }
      tft.setRotation(setRotation);
      
      // Update global screen dimensions on rotation
      screenW = tft.width();
      screenH = tft.height();
      
      // --- FIX: Reset sprite positions so they don't get trapped out of bounds ---
      xPos = 20;
      yPos = 20;
      // Also reset oldX and oldY so the smart erase doesn't try to wipe a massive area
      oldX = 20;
      oldY = 20; 
      
      setRotation = 5;
      tft.fillScreen(TFT_BLACK); //Reset the screen
    }

    //This is no longer a new scene;
    newScene = false;

    //That's it, wait until the next time we should loop!
    while(millis64() <= frameTime){
      delay(1);
    }
  }
}

void startingScreen() {
  static String lastMessage = ""; // Keeps track of the previous state

  // --- 1. FULL REDRAW (Only on Scene Change) ---
  if (newScene) {
    bgColor = TFT_BLACK;
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    
    if (screenHorizontal) {
        tft.setFreeFont(&FreeSansBold24pt7b);
        tft.drawString("Starting...", tft.width() / 2, (tft.height() / 2) - 20);
    } else {
        // Drop font size slightly so "Starting..." doesn't clip on narrower screen
        tft.setFreeFont(&FreeSansBold18pt7b); 
        tft.drawString("Starting...", tft.width() / 2, (tft.height() / 2) - 40); // Spread out vertically
    }
    
    lastMessage = ""; // Reset tracker to force the sub-text to draw
  }

  // --- 2. CONDITIONAL UPDATE (Only if text changed) ---
  if (startupMessage != lastMessage) {
    int msgY = screenHorizontal ? (tft.height() / 2) + 40 : (tft.height() / 2) + 60;
    int msgHeight = 30; // Height of the FreeSans12pt7b area

    // Clear ONLY the specific area where the message lives
    tft.fillRect(0, msgY - (msgHeight / 2), tft.width(), msgHeight, TFT_BLACK);

    // Draw the new message
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString(startupMessage, tft.width() / 2, msgY);

    // Update the tracker
    lastMessage = startupMessage;

    //Check if this scene was brought on by a restart
    if(restartFlag){
      delay(50);
      ESP.restart();
    }
  }
}

void idleScreen(){
  //Bouncing sprite that says "INSERT CARD" or "TAP CARD" on a yellow rectangle. Moves to prevent burn-in.
  if(newScene){
    //Since it is the first time dealing with this scene, we need to create the sprite.
    Serial.println(F("Setting Idle Screen Sprite."));
    String toDisp;
    if(mode == "INSERT"){
      toDisp = "INSERT CARD";
    } else{
      toDisp = "TAP CARD";
    }
    createSpriteBounce(toDisp, TFT_YELLOW, TFT_BLACK);
  } else{
    runSpriteBounce();
  }
}

void singleEquipmentApproved(){
  //If this is the first time we are going into the scene, we need to create the sprite for the future screensaver
  if(newScene){
    bgColor = TFT_GREEN;
    tft.fillScreen(TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_BLACK);
    
    if (screenHorizontal) {
        tft.setFreeFont(&FreeSansBold18pt7b);
        tft.drawString("Equipment", tft.width() / 2, (tft.height() / 2) - 45);
        tft.drawString("Unlocked!", tft.width() / 2, (tft.height() / 2));
    } else {
        // Drop to 12pt or keep 18pt if it fits, but spread vertically
        tft.setFreeFont(&FreeSansBold18pt7b);
        tft.drawString("Equipment", tft.width() / 2, (tft.height() / 2) - 60);
        tft.drawString("Unlocked!", tft.width() / 2, (tft.height() / 2) - 15);
    }
    
    int msgY = screenHorizontal ? (tft.height() / 2) + 50 : (tft.height() / 2) + 60;
    
    // Draw the new message
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSans12pt7b);
    tft.setTextColor(TFT_BLACK);
    String unlockReason;
    String toDisp;
    if(acsState == "UNLOCKED"){
      toDisp = "UNLOCKED";
      unlockReason = "Access Granted";
    } else{
      toDisp = "ALWAYS ON";
      unlockReason = "Staff Override";
    }
    tft.drawString(unlockReason, tft.width() / 2, msgY);
    createSpriteBounce(toDisp, TFT_GREEN, TFT_BLACK);

  }

  if(sceneStartTime + 5000 >= millis64()){
    //The scene is new, show a full screen we already generated.
  } else{
    //The scene is old, show the screensaver mode
    if(firstBounce){
      firstBounce = false;
      bgColor = TFT_BLACK;
      tft.fillScreen(TFT_BLACK);
    }
    runSpriteBounce();
  }
}

void singleEquipmentDenied() {
  if (newScene) {
    tft.fillScreen(TFT_RED);
    bgColor = TFT_RED;

    // 1. Draw the Main Header (Centered)
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    
    if (screenHorizontal) {
        tft.setFreeFont(&FreeSansBold18pt7b);
        tft.drawString("Access Denied!", tft.width() / 2, (tft.height() / 2) - 60);
    } else {
        // 18pt "Access Denied!" might clip on 240px. 12pt is safer for narrow portrait.
        tft.setFreeFont(&FreeSansBold12pt7b);
        tft.drawString("Access Denied!", tft.width() / 2, (tft.height() / 2) - 80);
    }

    // 2. Setup Wrapped & Centered Reason
    tft.setFreeFont(&FreeSans12pt7b); 
    tft.setTextColor(TFT_WHITE);
    
    int padding = 15; 
    int maxWidth = tft.width() - (padding * 2);
    // Push the text down slightly further in portrait
    int startY = screenHorizontal ? (tft.height() / 2) : (tft.height() / 2) - 20; 

    // Call the centering helper
    drawWrappedCenteredText(deniedReason, startY, maxWidth);
  }
}

// Helper function to ensure text stays within your custom padding
void printWrappedText(String text, int x, int y, int maxWidth) {
  tft.setCursor(x, y);
  String word = "";
  int cursorX = x;
  int spaceWidth = tft.textWidth(" ");
  int lineHeight = tft.fontHeight();

  int startIndex = 0;
  int nextSpace = text.indexOf(' ');

  while (startIndex < text.length()) {
    if (nextSpace == -1) nextSpace = text.length();
    word = text.substring(startIndex, nextSpace);
    
    int wordWidth = tft.textWidth(word);

    if (cursorX + wordWidth > (x + maxWidth)) {
      cursorX = x;
      y += lineHeight;
      tft.setCursor(cursorX, y);
    }

    tft.print(word);
    tft.print(" "); 
    cursorX += wordWidth + spaceWidth;

    startIndex = nextSpace + 1;
    nextSpace = text.indexOf(' ', startIndex);
  }
}

void multiEquipmentAccess() {
 //This is a work in progress. See Test Code -> Multi-Channel for more info. 
}

void welcomeScreen(){
  //This screen is used when the attached card reader is a welcome reader. 
}

void lockedScreen(){
  if(newScene){
    bgColor = TFT_DARKGREEN; // Wait, original code sets bgColor = TFT_DARKGREEN but fills with TFT_RED? Left unchanged from your code.
    tft.fillScreen(TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeSansBold18pt7b);
    
    if (screenHorizontal) {
        tft.drawString("Equipment", tft.width() / 2, (tft.height() / 2) - 25);
        tft.drawString("Locked Out!", tft.width() / 2, (tft.height() / 2) + 20);
    } else {
        // Spread the text out vertically to use the extra height
        tft.drawString("Equipment", tft.width() / 2, (tft.height() / 2) - 40);
        tft.drawString("Locked Out!", tft.width() / 2, (tft.height() / 2) + 40);
    }
    
    createSpriteBounce("LOCKED", TFT_RED, TFT_WHITE);
  }

  if(sceneStartTime + 15000 >= millis64()){
    //The scene is new, show a full screen we already generated.
  } else{
    if(firstBounce){
      firstBounce = false;
      bgColor = TFT_BLACK;
      tft.fillScreen(TFT_BLACK);
    }
    runSpriteBounce();
  }
}

void faultScreen(){
  bgColor = TFT_BLACK;
  if(newScene){
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSans12pt7b);
    tft.setTextColor(TFT_WHITE);
    
    if (screenHorizontal) {
        int msgY = (tft.height() / 2) + 50;
        tft.drawString("Notify Staff Immediatley!", tft.width() / 2, msgY);
        tft.drawString(faultMessage, tft.width() / 2, msgY + 30);
    } else {
        // "Notify Staff Immediately!" is very wide, it likely needs wrapping in portrait.
        // We can utilize your awesome wrap helper here!
        int msgY = (tft.height() / 2) + 40;
        int maxWidth = tft.width() - 20; 
        drawWrappedCenteredText("Notify Staff Immediately!", msgY, maxWidth);
        drawWrappedCenteredText(faultMessage, msgY + 45, maxWidth);
    }
  }
  
  //Flash the word "FAULT!" in the center of the screen
  tft.setTextDatum(MC_DATUM);
  if (screenHorizontal) {
      tft.setFreeFont(&FreeSansBold24pt7b);
  } else {
      tft.setFreeFont(&FreeSansBold18pt7b); // Drop font size to prevent edge clipping
  }
  
  if(faultFlashTime <= millis64()){
    faultFlashTime = millis64() + 1000;
    
    int faultY = screenHorizontal ? (tft.height() / 2) - 45 : (tft.height() / 2) - 60;
    
    if(faultFlash){
      tft.setTextColor(TFT_WHITE);
      tft.drawString("FAULT!", tft.width() / 2, faultY);
      faultFlash = false;
    } else{
      faultFlash = true;
      tft.setTextColor(TFT_RED);
      tft.drawString("FAULT!", tft.width() / 2, faultY);
    }
  }
}

void showFiledScene() {
  if(newScene){
    tft.fillScreen(TFT_BLACK);
    bgColor = TFT_BLACK;
    lastFiledScene = "";
  }
  
  if(filedScene != lastFiledScene || forceUpdateScreen){
    forceUpdateScreen = false;
    lastFiledScene = filedScene;
    
    String filePath = "/scenes/" + filedScene; 
    
    tft.setFreeFont(FSS9); 
    tft.setTextDatum(MC_DATUM); 
    
    if (SD_MMC.cardType() == CARD_NONE) {
      tft.fillScreen(TFT_RED);
      tft.setTextColor(TFT_WHITE); 
      tft.drawString("ERROR: No SD Card", tft.width() / 2, tft.height() / 2);
      return; 
    }

    if (!SD_MMC.exists(filePath)) {
      tft.fillScreen(TFT_BLUE);
      tft.setTextColor(TFT_WHITE);
      tft.drawString("ERROR: File Not Found", tft.width() / 2, (tft.height() / 2) - 15);
      tft.drawString(filePath, tft.width() / 2, (tft.height() / 2) + 15);
      return; 
    }

    // You may need to generate portrait versions of the scenes on the SD card 
    // depending on the dimensions of the images saved.
    TJpgDec.drawFsJpg(0, 0, filePath, SD_MMC); 
  }
}

void createSpriteBounce(String text, uint16_t rectColor, uint16_t textColor) {
  sprite.deleteSprite();
  xPos = 20, yPos = 20; 

  tft.setFreeFont(&FreeSansBold12pt7b); 
  
  int tWidth = tft.textWidth(text);
  int tHeight = tft.fontHeight();

  logoW = tWidth + 24; 
  logoH = tHeight + 20;

  if (sprite.createSprite(logoW, logoH) == nullptr) return;

  sprite.fillSprite(TFT_BLACK);
  sprite.fillRoundRect(0, 0, logoW, logoH, 8, rectColor);
  
  sprite.setFreeFont(&FreeSansBold12pt7b);
  sprite.setTextColor(textColor);
  sprite.setTextDatum(MC_DATUM); 
  
  sprite.drawString(text, logoW / 2, (logoH / 2) + 2); 
}

void runSpriteBounce() {
  int currentMotdHeight = (motd == "") ? 0 : motdHeight;
  int effectiveFloor = screenH - currentMotdHeight;

  oldX = (int)floor(xPos);
  oldY = (int)floor(yPos);

  xPos += dx;
  yPos += dy;

  if (xPos <= 0 || xPos + logoW >= screenW) {
    dx = -dx;
    xPos += dx; 
  }
  if (yPos <= 0 || yPos + logoH >= effectiveFloor) {
    dy = -dy;
    yPos += dy;
    if (yPos + logoH > effectiveFloor) yPos = (float)(effectiveFloor - logoH);
  }

  int ix = (int)floor(xPos);
  int iy = (int)floor(yPos);

  if (ix != oldX) {
    if (ix > oldX) tft.fillRect(oldX, oldY, ix - oldX, logoH, TFT_BLACK);
    else tft.fillRect(ix + logoW, oldY, oldX - ix, logoH, TFT_BLACK);
  }
  
  if (iy != oldY) {
    if (iy > oldY) tft.fillRect(oldX, oldY, logoW, iy - oldY, TFT_BLACK);
    else tft.fillRect(oldX, iy + logoH, logoW, oldY - iy, TFT_BLACK);
  }

  sprite.pushSprite(ix, iy);

  runMOTD();
  runCornerClock();
}

void otaScreen(String component) {
  static int lastProgress = -1; 

  int barWidth = tft.width() - 40; 
  int barHeight = 30;
  int barX = 20;
  // Push the bar down further in portrait mode to give the text breathing room
  int barY = screenHorizontal ? (tft.height() / 2) + 10 : (tft.height() / 2) + 30; 

  if (newScene) {
    bgColor = TFT_BLACK;
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeSansBold18pt7b);
    
    String deviceUpdate = component + " Updating..."; 
    
    int titleY = screenHorizontal ? (tft.height() / 2) - 40 : (tft.height() / 2) - 60;
    tft.drawString(deviceUpdate, tft.width() / 2, titleY);

    tft.drawRect(barX, barY, barWidth, barHeight, TFT_WHITE);
    
    lastProgress = 0; 
  }

  int currentProgress = otaProgressPercent;

  if (currentProgress > lastProgress && currentProgress <= 100) {
    
    int innerBarMax = barWidth - 2;
    
    int currentPixelWidth = (currentProgress * innerBarMax) / 100;
    int lastPixelWidth = (lastProgress * innerBarMax) / 100;
    
    int chunkX = barX + 1 + lastPixelWidth; 
    int chunkY = barY + 1;                  
    int chunkWidth = currentPixelWidth - lastPixelWidth;
    int chunkHeight = barHeight - 2;        
    
    if (chunkWidth > 0) {
       tft.fillRect(chunkX, chunkY, chunkWidth, chunkHeight, TFT_GREEN);
    }

    tft.fillRect(0, barY + barHeight + 5, tft.width(), 40, TFT_BLACK);
    
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextColor(TFT_WHITE);
    String pctText = String(currentProgress) + "%";
    tft.drawString(pctText, tft.width() / 2, barY + barHeight + 25);

    lastProgress = currentProgress;

    if(otaProgressPercent == 100){
      delay(250);
      ESP.restart();
    }
  }
}

bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1; 
}

void identifyScreen(){
  bool updateIdentify = false;
  
  if(identifyTime <= millis64()){
    identifyTime = millis64() + 1000;
    identifyStep =! identifyStep;
    updateIdentify = true;
  }
  
  if(updateIdentify){
    updateIdentify = false;
    if(identifyStep){
      bgColor = TFT_YELLOW;
      tft.setTextColor(TFT_BLUE);
    } else{
      bgColor = TFT_BLUE;
      tft.setTextColor(TFT_YELLOW);
    }
    tft.fillScreen(bgColor);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold18pt7b);
    
    int textY = screenHorizontal ? (tft.height() / 2) - 20 : (tft.height() / 2) - 30;
    tft.drawString("IDENTIFY", tft.width() / 2, textY);
  }
}

void runCornerClock() {
  static String lastTimeStr = "";

  unsigned long long elapsedSeconds = (millis64() - sceneStartTime) / 1000;
  int hours = elapsedSeconds / 3600;
  int mins = (elapsedSeconds % 3600) / 60;
  int secs = elapsedSeconds % 60;

  char timeBuf[12]; 
  sprintf(timeBuf, "%02d:%02d:%02d", hours, mins, secs);
  String currentTimeStr = String(timeBuf);

  if (millis64() - lastCornerChangeTime > 60000) {
    lastCornerChangeTime = millis64();
    currentCorner = (currentCorner + 1) % 3;
  }

  bool isMotdActive = (motd != "");
  int effectiveBottom = screenH - (isMotdActive ? motdHeight : 0);
  int clockW = 90; 
  int clockH = 25; 
  int clockX = 0;
  int clockY = 0;

  switch (currentCorner) {
    case 0: // Top-Right
      clockX = screenW - clockW;
      clockY = 0;
      break;
    case 1: // Bottom-Right
      clockX = screenW - clockW;
      clockY = effectiveBottom - clockH;
      break;
    case 2: // Bottom-Left
      clockX = 0;
      clockY = effectiveBottom - clockH;
      break;
  }

  bool positionChanged = ((oldClockX != clockX || oldClockY != clockY) && oldClockX != -1);
  bool timeChanged = (currentTimeStr != lastTimeStr);

  if (positionChanged) {
     if (!newScene) tft.fillRect(oldClockX, oldClockY, clockW, clockH, bgColor);
  } else if (timeChanged) {
     if (!newScene) tft.fillRect(clockX, clockY, clockW, clockH, bgColor);
  }
  
  oldClockX = clockX;
  oldClockY = clockY;
  lastTimeStr = currentTimeStr;

  timeSprite.fillSprite(bgColor); 
  timeSprite.setFreeFont(&FreeSans9pt7b);
  timeSprite.setTextColor(TFT_LIGHTGREY); 
  timeSprite.setTextDatum(MC_DATUM);
  timeSprite.drawString(currentTimeStr, clockW / 2, clockH / 2);
  
  timeSprite.pushSprite(clockX, clockY, bgColor);
}

void drawWrappedCenteredText(String text, int y, int maxWidth) {
  int lineHeight = tft.fontHeight();
  int spaceWidth = tft.textWidth(" ");
  int screenWidth = tft.width();
  
  String currentLine = "";
  String word = "";
  int startIndex = 0;
  int nextSpace = text.indexOf(' ');

  tft.setTextDatum(TC_DATUM);

  while (startIndex < text.length()) {
    if (nextSpace == -1) nextSpace = text.length();
    word = text.substring(startIndex, nextSpace);

    String testLine = currentLine.length() == 0 ? word : currentLine + " " + word;
    
    if (tft.textWidth(testLine) <= maxWidth) {
      currentLine = testLine;
    } else {
      tft.drawString(currentLine, screenWidth / 2, y);
      y += lineHeight;
      currentLine = word; 
    }

    startIndex = nextSpace + 1;
    nextSpace = text.indexOf(' ', startIndex);
    
    if (startIndex >= text.length()) {
      tft.drawString(currentLine, screenWidth / 2, y);
    }
  }
}

void runMOTD() {
  if (motd != "") {
    motdActiveLastFrame = true; 
    
    motdSprite.fillSprite(TFT_BLACK);
    motdSprite.setFreeFont(&FreeSans9pt7b);
    motdSprite.setTextColor(TFT_WHITE);
    motdSprite.setTextDatum(ML_DATUM);
    
    int textWidth = tft.textWidth(motd);
    motdX -= scrollSpeed;
    if (motdX < -textWidth) motdX = screenW;

    motdSprite.drawString(motd, motdX, motdHeight / 2);
    motdSprite.pushSprite(0, screenH - motdHeight);
    
  } else {
    if (motdActiveLastFrame) {
      tft.fillRect(0, screenH - motdHeight, screenW, motdHeight, TFT_BLACK);
      motdActiveLastFrame = false; 
    }
  }
}