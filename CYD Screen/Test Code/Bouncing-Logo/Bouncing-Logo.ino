#include <TFT_eSPI.h> 
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft); // Create the sprite object

// Custom RIT Orange (RGB565 format)
#define RIT_ORANGE 0xFFE0
#define BG_COLOR TFT_BLACK

// Logo boundaries
const int logoW = 200;
const int logoH = 50;

int screenW, screenH;

// Position and speed vectors
int xPos = 10;
int yPos = 10;
int oldX = 10; // Track previous position for smart erasing
int oldY = 10;
int dx = 1; 
int dy = 1; 

void setup() {
  Serial.begin(115200);
  
  tft.init();
  tft.setRotation(1); 
  tft.fillScreen(BG_COLOR);
  
  screenW = tft.width();
  screenH = tft.height();

  // 1. Allocate memory for the Sprite
  sprite.createSprite(logoW, logoH);
  
  // 2. Draw the logo to the RAM Sprite ONCE
  sprite.fillSprite(BG_COLOR); // Fill with background color so the corners blend in
  
  // Draw the rounded rectangle: x, y, width, height, corner radius, color
  // A radius of 8 to 12 usually looks great for this size!
  sprite.fillRoundRect(0, 0, logoW, logoH, 8, RIT_ORANGE);
  sprite.setTextColor(TFT_BLACK, RIT_ORANGE);
  
  //Add the text in the center
  sprite.setTextDatum(MC_DATUM); 
  sprite.drawString("INSERT CARD", logoW / 2, logoH / 2, 4);
}

void loop() {
  // 1. Save old position before calculating the new one
  oldX = xPos;
  oldY = yPos;

  // 2. Update the coordinates
  xPos += dx;
  yPos += dy;

  // 3. Collision detection
  if (xPos <= 0 || xPos + logoW >= screenW) {
    dx = -dx;       
    xPos += dx;     
  }
  
  if (yPos <= 0 || yPos + logoH >= screenH) {
    dy = -dy;       
    yPos += dy;     
  }

  // 4. SMART ERASE: Only paint black over the trailing edges
  // Erase horizontal trail
  if (oldX < xPos) {
    tft.fillRect(oldX, oldY, xPos - oldX, logoH, BG_COLOR); // Moving right
  } else if (oldX > xPos) {
    tft.fillRect(xPos + logoW, oldY, oldX - xPos, logoH, BG_COLOR); // Moving left
  }
  
  // Erase vertical trail
  if (oldY < yPos) {
    tft.fillRect(oldX, oldY, logoW, yPos - oldY, BG_COLOR); // Moving down
  } else if (oldY > yPos) {
    tft.fillRect(oldX, yPos + logoH, logoW, oldY - yPos, BG_COLOR); // Moving up
  }

  // 5. Push the pre-rendered sprite to the new location instantly
  sprite.pushSprite(xPos, yPos);

  // 6. Delay (Slightly reduced since the loop executes much faster now)
  delay(20); 
}