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
//void showFiledScene();
void otaScreen(String component);
void createSpriteBounce(String text, uint16_t rectColor, uint16_t textColor);
void runSpriteBounce();
bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
void identifyScreen();

void screenServer(void *pvParameters){
  //First time startup of the screen
  tft.init();
  tft.setRotation(1); //1 for default, set to 3 for test on equipment that is upside-down mounted
  tft.invertDisplay(true);
  tft.fillScreen(TFT_BLACK);
  bgColor = TFT_BLACK;

  byte previousScene = 0; //Stores the last scene we displayed
  unsigned long long frameTime = 0; //When we started drawing this frame, to control framerate
  bool screenHorizontal = true; //Used to determine if the scene should be rendered horizontally or vertically

  //Sprite-related startup
  screenW = tft.width();
  screenH = tft.height();
  sprite.createSprite(logoW, logoH);
  motdSprite.createSprite(screenW, motdHeight);
  timeSprite.createSprite(90, 25);

  //Start the JPEG decoder;
  //TJpgDec.setCallback(tftOutput);
  //TJpgDec.setJpgScale(1);

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
      //case 8: showFiledScene(); break;
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
      // Standard font 2 is approx 16px high
      int rectW = 60; 
      int rectH = 20;

      // 2. Draw the background rectangle (Top-Left Corner)
      // fillRect(x, y, width, height, color)
      //This essentially overwrites what we did before
      tft.fillRect(0, 0, rectW, rectH, bgColor);
      //For the filed screen, we should actually just re-draw the whole thing to cover this up.
      forceUpdateScreen = true;
    }

    //Do we need to rotate the screen?
    if(setRotation != 5){
      if(setRotation == 1 || setRotation == 3){
        //Screen is horizontal
        screenHorizontal = true;
      } else {
        screenHorizontal = false;
      }
      tft.setRotation(setRotation);
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
    tft.setFreeFont(&FreeSansBold24pt7b);
    tft.drawString("Starting...", tft.width() / 2, (tft.height() / 2) - 20);
    
    lastMessage = ""; // Reset tracker to force the sub-text to draw
  }

  // --- 2. CONDITIONAL UPDATE (Only if text changed) ---
  if (startupMessage != lastMessage) {
    int msgY = (tft.height() / 2) + 40;
    int msgHeight = 30; // Height of the FreeSans12pt7b area

    // Clear ONLY the specific area where the message lives
    // Using fillRect is much faster than clearing the whole screen
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
  //Screen used to convey a session in use (Unlocked or Always On)
  //Since this is used for a card reader attached to a single device, we can dedicate the whole screen to it.

  //If this is the first time we are going into the scene, we need to create the sprite for the future screensaver
  if(newScene){
    bgColor = TFT_GREEN;
    tft.fillScreen(TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_BLACK);
    tft.setFreeFont(&FreeSansBold18pt7b);
    tft.drawString("Equipment", tft.width() / 2, (tft.height() / 2) - 45);
    tft.drawString("Unlocked!", tft.width() / 2, (tft.height() / 2));
    int msgY = (tft.height() / 2) + 50;
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
    //Check if this is the first time we do this to wipe the screen
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
    tft.setFreeFont(&FreeSansBold18pt7b);
    tft.drawString("Access Denied!", tft.width() / 2, (tft.height() / 2) - 60);

    // 2. Setup Wrapped & Centered Reason
    // Using 12pt for better readability as requested
    tft.setFreeFont(&FreeSans12pt7b); 
    tft.setTextColor(TFT_WHITE);
    
    int padding = 15; 
    int maxWidth = tft.width() - (padding * 2);
    int startY = (tft.height() / 2); // Start near the middle

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
    // Get the next word
    if (nextSpace == -1) nextSpace = text.length();
    word = text.substring(startIndex, nextSpace);
    
    int wordWidth = tft.textWidth(word);

    // Check if word fits on current line
    if (cursorX + wordWidth > (x + maxWidth)) {
      cursorX = x;
      y += lineHeight;
      tft.setCursor(cursorX, y);
    }

    tft.print(word);
    tft.print(" "); // Add space after word
    cursorX += wordWidth + spaceWidth;

    startIndex = nextSpace + 1;
    nextSpace = text.indexOf(' ', startIndex);
  }
}

void multiEquipmentAccess() {
  // --- Global Denial Check ---
  if (newScene) {
    bool allDenied = true;
    String firstReason = channelsData[0].deniedReason;
    bool sameReason = true;

    for (int i = 0; i < activeChannels; i++) {
      if (!channelsData[i].accessDenied) allDenied = false;
      if (channelsData[i].deniedReason != firstReason) sameReason = false;
    }

    if (allDenied && sameReason && activeChannels > 0 && firstReason != "") {
      currentScene = 3; 
      deniedReason = firstReason; 
      return; 
    }
  }

  // --- Static tracking for timers ---
  static String lastDisplayedTime[4] = {"", "", "", ""};

  // Only redraw the heavy UI elements if it's a new scene or data changed
  if (newScene || forceUpdateScreen) {
    forceUpdateScreen = false; 

    for (int i = 0; i < 4; i++) lastDisplayedTime[i] = "";

    // NEW: Auto-build the error string without touching the global MOTD
    errorMotd = ""; 
    for (int i = 0; i < activeChannels; i++) {
      if (channelsData[i].accessDenied && channelsData[i].deniedReason != "") {
        if (errorMotd != "") errorMotd += "   |   "; 
        errorMotd += channelsData[i].name + ": " + channelsData[i].deniedReason;
      }
    }

    // 1. Determine usable screen real estate (Checking both strings now)
    int usableHeight = screenH;
    if (motd != "" || errorMotd != "") {
      usableHeight -= motdHeight;
    }

    int rowHeight = usableHeight / activeChannels;

    // 2. Draw the channels
    for (int i = 0; i < activeChannels; i++) {
      int yOffset = i * rowHeight; 

      uint16_t rowBgColor = TFT_BLACK;
      uint16_t txtColor = TFT_WHITE;

      if (channelsData[i].accessDenied) {
        rowBgColor = TFT_RED;
        txtColor = TFT_WHITE;
      } else {
        // NEW: Both ALWAYS_ON and UNLOCKED are now Green
        if (channelsData[i].state == "ALWAYS_ON" || channelsData[i].state == "UNLOCKED") {
          rowBgColor = TFT_GREEN;
          txtColor = TFT_BLACK;
        } else if (channelsData[i].state == "IDLE") {
          rowBgColor = TFT_YELLOW;
          txtColor = TFT_BLACK;
        } else if (channelsData[i].state == "LOCKED_OUT") {
          rowBgColor = TFT_RED;
          txtColor = TFT_WHITE; 
        }
      }

      // Fill the row background
      tft.fillRect(0, yOffset, screenW, rowHeight, rowBgColor);

      // Draw separator lines 
      if (i > 0) {
        tft.drawFastHLine(0, yOffset, screenW, TFT_BLACK);
        tft.drawFastHLine(0, yOffset + 1, screenW, TFT_BLACK);
      }

      // --- Draw Equipment Name (Top Left) ---
      tft.setTextColor(txtColor);
      tft.setTextDatum(TL_DATUM); 
      tft.setFreeFont(&FreeSans12pt7b); 
      tft.drawString(channelsData[i].name, 15, yOffset + 5); 

      // --- Draw Subtext (Bottom Left) ---
      tft.setFreeFont(&FreeSans9pt7b); 
      String subText = "";
      
      if (channelsData[i].accessDenied) {
        // NEW: Updated subtext string
        subText = "Access Denied: See Below"; 
      } else {
        subText = "Status: " + channelsData[i].state; 
      }

      int subTextYOffset = yOffset + 30; 
      if (activeChannels < 4 && motd == "" && errorMotd == "") {
        subTextYOffset = yOffset + (rowHeight / 2); 
      }
      tft.drawString(subText, 15, subTextYOffset);
    }
  }

  // --- DYNAMIC TIMERS (Runs every frame) ---
  if (mode == "INSERT") {
    int usableHeight = screenH;
    if (motd != "" || errorMotd != "") usableHeight -= motdHeight;
    int rowHeight = usableHeight / activeChannels;

    for (int i = 0; i < activeChannels; i++) {
      if (channelsData[i].state == "UNLOCKED" || channelsData[i].state == "ALWAYS_ON") {
        
        // 1. Calculate Elapsed Time
        unsigned long long elapsedSeconds = 0;
        if (channelsData[i].activeStartTime > 0 && millis64() >= channelsData[i].activeStartTime) {
          elapsedSeconds = (millis64() - channelsData[i].activeStartTime) / 1000;
        }

        int hours = elapsedSeconds / 3600;
        int mins = (elapsedSeconds % 3600) / 60;
        int secs = elapsedSeconds % 60;

        char timeBuf[12];
        sprintf(timeBuf, "%02d:%02d:%02d", hours, mins, secs); 
        String timeStr = String(timeBuf);

        // 2. Smart Erase & Draw
        if (timeStr != lastDisplayedTime[i]) {
          lastDisplayedTime[i] = timeStr;

          int yOffset = i * rowHeight;
          // NEW: Background is always green now to match state colors
          uint16_t rowBgColor = TFT_GREEN; 

          int timerWidth = 110;
          int timerHeight = 25;
          int timerX = screenW - timerWidth - 15;
          int timerY = yOffset + 5; 

          tft.fillRect(timerX, timerY, timerWidth, timerHeight, rowBgColor);

          tft.setTextColor(TFT_BLACK);
          tft.setTextDatum(TR_DATUM); 
          tft.setFreeFont(&FreeSans12pt7b); 
          tft.drawString(timeStr, screenW - 15, yOffset + 5); 
        }
      }
    }
  }

  runMOTD();
}

void welcomeScreen(){
  //This screen is used when the attached card reader is a welcome reader. This function handles both the welcome "idle" screen, and the welcome approved/denied screen.
}

void lockedScreen(){
  //This screen is used to convey that equipment is unavailable because it is locked.
  //It is used if all equipment attached is locked out.
  //If this is the first time we are going into the scene, we need to create the sprite for the future screensaver
  if(newScene){
    bgColor = TFT_DARKGREEN;
    tft.fillScreen(TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeSansBold18pt7b);
    tft.drawString("Equipment", tft.width() / 2, (tft.height() / 2) - 25);
    tft.drawString("Locked Out!", tft.width() / 2, (tft.height() / 2) + 20);
    createSpriteBounce("LOCKED", TFT_RED, TFT_WHITE);
  }

  if(sceneStartTime + 15000 >= millis64()){
    //The scene is new, show a full screen we already generated.

  } else{
    //The scene is old, show the screensaver mode
    //Check if this is the first time we do this to wipe the screen
    if(firstBounce){
      firstBounce = false;
      bgColor = TFT_BLACK;
      tft.fillScreen(TFT_BLACK);
    }

    runSpriteBounce();
  }
}

void faultScreen(){
  //This screen conveys that the equipment is in a fault state, and what the source of the fault is. 
  bgColor = TFT_BLACK;
  if(newScene){
    tft.fillScreen(TFT_BLACK);
    //One time, write the warnings under the main text
    int msgY = (tft.height() / 2) + 50;
    // Draw the new message
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSans12pt7b);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Notify Staff Immediatley!", tft.width() / 2, msgY);
    tft.drawString(faultMessage, tft.width() / 2, msgY + 30);
  }
  //Flash the word "FAULT!" in the center of the screen
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSansBold24pt7b);
  if(faultFlashTime <= millis64()){
    faultFlashTime = millis64() + 1000;
    if(faultFlash){
      tft.setTextColor(TFT_WHITE);
      tft.drawString("FAULT!", tft.width() / 2, (tft.height() / 2) - 45);
      faultFlash = false;
    } else{
      faultFlash = true;
      tft.setTextColor(TFT_RED);
      tft.drawString("FAULT!", tft.width() / 2, (tft.height() / 2) - 45);
    }
  }
}

/*
void showFiledScene() {
  // This is a function to display a screen that has been saved to the onboard SD card as a JPEG. 
  if(newScene){
    // Clear any references to old screens
    tft.fillScreen(TFT_BLACK);
    bgColor = TFT_BLACK;
    lastFiledScene = "";
  }
  
  if(filedScene != lastFiledScene || forceUpdateScreen){
    // There is a different filed screen to present.
    forceUpdateScreen = false;
    lastFiledScene = filedScene;
    
    String filePath = "/scenes/" + filedScene; 
    
    // Set our font and text alignment upfront so all error messages use it
    tft.setFreeFont(FSS9); // Free Sans 9pt (Requires Free_Fonts.h included)
    tft.setTextDatum(MC_DATUM); // Middle-Center alignment
    
    // 1. Check if the SD Card is mounted
    if (SD_MMC.cardType() == CARD_NONE) {
      tft.fillScreen(TFT_RED);
      tft.setTextColor(TFT_WHITE); // BG color is ignored with Free Fonts anyway
      tft.drawString("ERROR: No SD Card", tft.width() / 2, tft.height() / 2);
      return; 
    }

    // 2. Check if the file exists
    if (!SD_MMC.exists(filePath)) {
      tft.fillScreen(TFT_BLUE);
      tft.setTextColor(TFT_WHITE);
      tft.drawString("ERROR: File Not Found", tft.width() / 2, (tft.height() / 2) - 15);
      tft.drawString(filePath, tft.width() / 2, (tft.height() / 2) + 15);
      return; 
    }

    // 3. Draw the image directly
    TJpgDec.drawFsJpg(0, 0, filePath, SD_MMC); 
  }
}
*/

void createSpriteBounce(String text, uint16_t rectColor, uint16_t textColor) {
  sprite.deleteSprite();
  xPos = 20, yPos = 20; //Reset the positions of the sprite

  // 1. Set the font for measurement
  tft.setFreeFont(&FreeSansBold12pt7b); 
  
  // 2. Measure
  int tWidth = tft.textWidth(text);
  int tHeight = tft.fontHeight();

  // GFX fonts need slightly more vertical padding
  logoW = tWidth + 24; 
  logoH = tHeight + 20;

  if (sprite.createSprite(logoW, logoH) == nullptr) return;

  sprite.fillSprite(TFT_BLACK);
  sprite.fillRoundRect(0, 0, logoW, logoH, 8, rectColor);
  
  // 3. Set the font for the sprite
  sprite.setFreeFont(&FreeSansBold12pt7b);
  sprite.setTextColor(textColor);
  sprite.setTextDatum(MC_DATUM); 
  
  // Draw - Note: setTextSize(1) is default for GFX fonts
  sprite.drawString(text, logoW / 2, (logoH / 2) + 2); // +2 helps center GFX fonts better
}

void runSpriteBounce() {
  // 1. Setup boundaries
  int currentMotdHeight = (motd == "") ? 0 : motdHeight;
  int effectiveFloor = screenH - currentMotdHeight;

  // 2. Capture old pixel position BEFORE updating
  oldX = (int)floor(xPos);
  oldY = (int)floor(yPos);

  // 3. Increment the floating point position
  xPos += dx;
  yPos += dy;

  // 4. Collision Detection
  if (xPos <= 0 || xPos + logoW >= screenW) {
    dx = -dx;
    xPos += dx; 
  }
  if (yPos <= 0 || yPos + logoH >= effectiveFloor) {
    dy = -dy;
    yPos += dy;
    if (yPos + logoH > effectiveFloor) yPos = (float)(effectiveFloor - logoH);
  }

  // 5. Get NEW pixel position
  int ix = (int)floor(xPos);
  int iy = (int)floor(yPos);

  // 6. SMART ERASE
  // We only erase if the integer (pixel) position has actually changed.
  // If ix == oldX, the sprite is still on the same pixel, so no trail was left!
  if (ix != oldX) {
    if (ix > oldX) tft.fillRect(oldX, oldY, ix - oldX, logoH, TFT_BLACK);
    else tft.fillRect(ix + logoW, oldY, oldX - ix, logoH, TFT_BLACK);
  }
  
  if (iy != oldY) {
    if (iy > oldY) tft.fillRect(oldX, oldY, logoW, iy - oldY, TFT_BLACK);
    else tft.fillRect(oldX, iy + logoH, logoW, oldY - iy, TFT_BLACK);
  }

  // 7. ALWAYS push the sprite
  // Pushing every frame (even if on same pixel) ensures no flicker/blackouts
  sprite.pushSprite(ix, iy);

  // 8. SMART MOTD TICKER
  runMOTD();

  // 9. NEW: Run the corner clock overlay
  runCornerClock();

}

void otaScreen(String component) {
  // Static variable to track the last drawn percentage. 
  // It retains its value between function calls.
  static int lastProgress = -1; 

  // Define the geometry of the progress bar
  int barWidth = tft.width() - 40; // 20px padding on left and right
  int barHeight = 30;
  int barX = 20;
  int barY = (tft.height() / 2) + 10; // Positioned below the text

  // --- ONE-TIME SCENE SETUP ---
  if (newScene) {
    bgColor = TFT_BLACK;
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeSansBold18pt7b);
    
    // Added a space before "Updating..."
    String deviceUpdate = component + " Updating..."; 
    tft.drawString(deviceUpdate, tft.width() / 2, (tft.height() / 2) - 40);

    // Draw the outline of the progress bar
    tft.drawRect(barX, barY, barWidth, barHeight, TFT_WHITE);
    
    // Reset our tracker for a new scene
    lastProgress = 0; 
  }

  // --- 60FPS LOOP LOGIC ---
  
  // Grab the volatile variable safely
  int currentProgress = otaProgressPercent;

  // Only update the screen if the progress has actually increased.
  // This prevents the screen from flickering wildly at 60fps.
  if (currentProgress > lastProgress && currentProgress <= 100) {
    
    // Calculate the inner max width (subtracting 2px total to account for the left/right outline)
    int innerBarMax = barWidth - 2;
    
    // Calculate how many pixels the current and previous percentages represent
    int currentPixelWidth = (currentProgress * innerBarMax) / 100;
    int lastPixelWidth = (lastProgress * innerBarMax) / 100;
    
    // Draw only the NEW difference (the new chunk of progress)
    int chunkX = barX + 1 + lastPixelWidth; // +1 to stay inside the left outline
    int chunkY = barY + 1;                  // +1 to stay inside the top outline
    int chunkWidth = currentPixelWidth - lastPixelWidth;
    int chunkHeight = barHeight - 2;        // -2 to stay inside top/bottom outlines
    
    // Fill the new chunk
    if (chunkWidth > 0) {
       tft.fillRect(chunkX, chunkY, chunkWidth, chunkHeight, TFT_GREEN);
    }

    // --- DRAW PERCENTAGE TEXT ---
    // Clear just the small area where the percentage text goes so we don't clear the whole screen
    tft.fillRect(0, barY + barHeight + 5, tft.width(), 40, TFT_BLACK);
    
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextColor(TFT_WHITE);
    String pctText = String(currentProgress) + "%";
    tft.drawString(pctText, tft.width() / 2, barY + barHeight + 25);

    // Update the tracker so we don't redraw this frame again
    lastProgress = currentProgress;

    //If the progress makes it to 100%, wait a moment and then restart the device.
    if(otaProgressPercent == 100){
      delay(250);
      ESP.restart();
    }

  }
}

/*
//Callback for JPEG decoding
bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  // Stop further decoding if the image is running off the bottom of the screen
  if (y >= tft.height()) return 0;
  
  // Push the pixel block to the screen
  tft.pushImage(x, y, w, h, bitmap);
  
  // Return 1 to let the decoder know it should keep going
  return 1; 
}
*/

void identifyScreen(){
  //Flash a screen to identify the device;
  //First, should we step forwards in the pattern?
  
  bool updateIdentify = false;
  
  if(identifyTime <= millis64()){
    identifyTime = millis64() + 1000;
    identifyStep =! identifyStep;
    updateIdentify = true;
  }
  
  if(updateIdentify){
    updateIdentify = false;
    if(identifyStep){
      //Blue text on yellow background
      bgColor = TFT_YELLOW;
      tft.setTextColor(TFT_BLUE);
    } else{
      //The opposite
      bgColor = TFT_BLUE;
      tft.setTextColor(TFT_YELLOW);
    }
    tft.fillScreen(bgColor);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold18pt7b);
    tft.drawString("IDENTIFY", tft.width() / 2, (tft.height() / 2) - 20);
  }
}

void runCornerClock() {
  // Static variable to track when the text changes
  static String lastTimeStr = "";

  // 1. Calculate elapsed time in seconds using sceneStartTime
  unsigned long long elapsedSeconds = (millis64() - sceneStartTime) / 1000;
  int hours = elapsedSeconds / 3600;
  int mins = (elapsedSeconds % 3600) / 60;
  int secs = elapsedSeconds % 60;

  // 2. Format the string strictly as HH:MM:SS
  char timeBuf[12]; 
  sprintf(timeBuf, "%02d:%02d:%02d", hours, mins, secs);
  String currentTimeStr = String(timeBuf);

  // 3. Rotate corners every 60 seconds
  if (millis64() - lastCornerChangeTime > 60000) {
    lastCornerChangeTime = millis64();
    currentCorner = (currentCorner + 1) % 3;
  }

  // 4. Determine coordinates (using 90px width for HH:MM:SS)
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

  // 5. Smart Erase & Transparency Prep
  bool positionChanged = ((oldClockX != clockX || oldClockY != clockY) && oldClockX != -1);
  bool timeChanged = (currentTimeStr != lastTimeStr);

  if (positionChanged) {
     // Wipe the OLD corner clean
     if (!newScene) tft.fillRect(oldClockX, oldClockY, clockW, clockH, bgColor);
  } else if (timeChanged) {
     // Because we are drawing transparently, we MUST wipe the background 
     // every second, otherwise the new numbers will draw over the old ones!
     if (!newScene) tft.fillRect(clockX, clockY, clockW, clockH, bgColor);
  }
  
  oldClockX = clockX;
  oldClockY = clockY;
  lastTimeStr = currentTimeStr;

  // 6. Draw the sprite in memory
  timeSprite.fillSprite(bgColor); 
  timeSprite.setFreeFont(&FreeSans9pt7b);
  timeSprite.setTextColor(TFT_LIGHTGREY); 
  timeSprite.setTextDatum(MC_DATUM);
  timeSprite.drawString(currentTimeStr, clockW / 2, clockH / 2);
  
  // 7. PUSH TRANSPARENTLY
  // Passing 'bgColor' as the 3rd argument tells the TFT to ignore those pixels!
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

  // We change datum to Top-Center to make the drawString math easier
  tft.setTextDatum(TC_DATUM);

  while (startIndex < text.length()) {
    if (nextSpace == -1) nextSpace = text.length();
    word = text.substring(startIndex, nextSpace);

    // Check if the word fits on the current line
    String testLine = currentLine.length() == 0 ? word : currentLine + " " + word;
    
    if (tft.textWidth(testLine) <= maxWidth) {
      currentLine = testLine;
    } else {
      // Current line is full, draw it centered
      tft.drawString(currentLine, screenWidth / 2, y);
      y += lineHeight;
      currentLine = word; // Start new line with the word that didn't fit
    }

    startIndex = nextSpace + 1;
    nextSpace = text.indexOf(' ', startIndex);
    
    // Draw the very last line
    if (startIndex >= text.length()) {
      tft.drawString(currentLine, screenWidth / 2, y);
    }
  }
}

void runMOTD() {
  // Combine the global motd with any active errors locally for display
  String activeText = motd;
  if (errorMotd != "") {
    if (activeText != "") activeText += "   |   ";
    activeText += errorMotd;
  }

  // Draw if there is ANY text to show
  if (activeText != "") {
    motdActiveLastFrame = true; 
    
    motdSprite.fillSprite(TFT_BLACK);
    motdSprite.setFreeFont(&FreeSans9pt7b);
    motdSprite.setTextColor(TFT_WHITE);
    motdSprite.setTextDatum(ML_DATUM);
    
    // Measure the combined text width
    int textWidth = motdSprite.textWidth(activeText);
    
    motdX -= scrollSpeed;
    if (motdX < -textWidth) motdX = screenW;

    motdSprite.drawString(activeText, motdX, motdHeight / 2);
    motdSprite.pushSprite(0, screenH - motdHeight);
    
  } else {
    // If MOTD is empty, check if we need to clean up a "ghost" from a previous frame
    if (motdActiveLastFrame) {
      tft.fillRect(0, screenH - motdHeight, screenW, motdHeight, TFT_BLACK);
      motdActiveLastFrame = false; 
    }
  }
}