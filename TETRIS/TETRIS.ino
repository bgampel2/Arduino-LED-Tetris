/*
  Tetris Game
  Made by Bernie Gampel
  Made on 03/16/2026
*/

#include "LedControl.h"
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include "pitches.h"

// LedControl(DIN, CLK, CS, number of displays)
LedControl lc = LedControl(12, 10, 11, 4);

// LiquidCrystal(RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(7, 6, 5, 4, 3, 8);

const int SW_pin     = 2;  // joystick button
const int X_pin      = 0;  // joystick X axis (analog)
const int Y_pin      = 1;  // joystick Y axis (analog)
const int BUZZER_PIN = 13; // passive buzzer

const int boardWidth  = 8;  // 8 columns
const int boardHeight = 24; // 3 displays x 8 rows each

// 2D array representing locked cells on the board
// true = filled, false = empty
bool board[boardHeight][boardWidth] = {};

// All 7 tetriminos, each with 4 rotations
// Each rotation is 4 rows of 8 bits (left-aligned)
// A 1 bit means that cell is filled
const uint8_t TETRIMINOS[7][4][4] {
  // I
  {{0b00000000,0b11110000,0b00000000,0b00000000},
   {0b00100000,0b00100000,0b00100000,0b00100000},
   {0b00000000,0b11110000,0b00000000,0b00000000},
   {0b00100000,0b00100000,0b00100000,0b00100000}},
  // O
  {{0b01100000,0b01100000,0b00000000,0b00000000},
   {0b01100000,0b01100000,0b00000000,0b00000000},
   {0b01100000,0b01100000,0b00000000,0b00000000},
   {0b01100000,0b01100000,0b00000000,0b00000000}},
  // T
  {{0b01000000,0b11100000,0b00000000,0b00000000},
   {0b01000000,0b01100000,0b01000000,0b00000000},
   {0b11100000,0b01000000,0b00000000,0b00000000},
   {0b01000000,0b11000000,0b01000000,0b00000000}},
  // S
  {{0b01100000,0b11000000,0b00000000,0b00000000},
   {0b01000000,0b01100000,0b00100000,0b00000000},
   {0b01100000,0b11000000,0b00000000,0b00000000},
   {0b01000000,0b01100000,0b00100000,0b00000000}},
  // Z
  {{0b11000000,0b01100000,0b00000000,0b00000000},
   {0b00100000,0b01100000,0b01000000,0b00000000},
   {0b11000000,0b01100000,0b00000000,0b00000000},
   {0b00100000,0b01100000,0b01000000,0b00000000}},
  // J
  {{0b10000000,0b11100000,0b00000000,0b00000000},
   {0b01100000,0b01000000,0b01000000,0b00000000},
   {0b11100000,0b00100000,0b00000000,0b00000000},
   {0b01000000,0b01000000,0b11000000,0b00000000}},
  // L
  {{0b00100000,0b11100000,0b00000000,0b00000000},
   {0b01000000,0b01000000,0b01100000,0b00000000},
   {0b11100000,0b10000000,0b00000000,0b00000000},
   {0b11000000,0b01000000,0b01000000,0b00000000}},
};

// The currently falling piece
// x, y = position on board, type = which tetrimino (0-6), rot = rotation (0-3)
struct Piece { int x, y, type, rot; };
Piece cur;

// The next piece to spawn, shown on the top display
int nextType = 0;

// Player score, high score, and total lines cleared
int score     = 0;
int highScore = 0;
int lines     = 0;

// Timing for gravity, input debounce, and rotation debounce
unsigned long lastFall   = 0;
unsigned long lastInput  = 0;
unsigned long lastRotate = 0;
const int FALL_MS      = 600; // ms between each gravity tick
const int SOFT_DROP_MS = 50;  // ms between each soft drop tick
const int INPUT_MS     = 200; // ms debounce for left/right movement
const int ROTATE_MS    = 250; // ms debounce for rotation

// Pause state and timer for detecting joystick held up
bool paused = false;
unsigned long upHoldStart = 0;

// Flag to avoid redrawing the board when nothing has changed
bool needsRender = true;

// Returns true if the cell at (r, c) in a given piece type and rotation is filled
bool cellSet(int type, int rot, int r, int c) {
  return (TETRIMINOS[type][rot][r] >> (7 - c)) & 1;
}

// Writes the current score, high score, and lines cleared to the LCD
void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.print("Sc:");
  lcd.print(score);
  lcd.print(" Ln:");
  lcd.print(lines);
  lcd.print("     ");

  lcd.setCursor(0, 1);
  lcd.print("HI:");
  lcd.print(highScore);
  lcd.print("     ");
}

// Saves the current score to EEPROM if it beats the high score
void checkHighScore() {
  if (score > highScore) {
    highScore = score;
    EEPROM.put(0, highScore); // save to EEPROM address 0
  }
}

// Short double beep when a piece locks into place
void soundLock() {
  tone(BUZZER_PIN, NOTE_G4, 30);
  delay(40);
  tone(BUZZER_PIN, NOTE_E4, 30);
  delay(30);
  noTone(BUZZER_PIN);
}

// Ascending chime that gets more impressive the more lines cleared at once
void soundLineClear(int cleared) {
  if (cleared == 1) {
    tone(BUZZER_PIN, NOTE_C5, 60);
    delay(70);
    tone(BUZZER_PIN, NOTE_E5, 60);
    delay(70);
    tone(BUZZER_PIN, NOTE_G5, 80);
    delay(80);
    noTone(BUZZER_PIN);
  } else if (cleared == 2) {
    tone(BUZZER_PIN, NOTE_C5, 60);
    delay(70);
    tone(BUZZER_PIN, NOTE_E5, 60);
    delay(70);
    tone(BUZZER_PIN, NOTE_G5, 60);
    delay(70);
    tone(BUZZER_PIN, NOTE_C6, 100);
    delay(100);
    noTone(BUZZER_PIN);
  } else if (cleared == 3) {
    tone(BUZZER_PIN, NOTE_C5, 50);
    delay(60);
    tone(BUZZER_PIN, NOTE_E5, 50);
    delay(60);
    tone(BUZZER_PIN, NOTE_G5, 50);
    delay(60);
    tone(BUZZER_PIN, NOTE_C6, 50);
    delay(60);
    tone(BUZZER_PIN, NOTE_E6, 100);
    delay(100);
    noTone(BUZZER_PIN);
  } else if (cleared == 4) {
    // Tetris! big fanfare for clearing 4 lines at once
    tone(BUZZER_PIN, NOTE_C5, 50);
    delay(60);
    tone(BUZZER_PIN, NOTE_E5, 50);
    delay(60);
    tone(BUZZER_PIN, NOTE_G5, 50);
    delay(60);
    tone(BUZZER_PIN, NOTE_C6, 50);
    delay(60);
    tone(BUZZER_PIN, NOTE_E6, 50);
    delay(60);
    tone(BUZZER_PIN, NOTE_G6, 150);
    delay(150);
    noTone(BUZZER_PIN);
  }
}

// Descending dramatic melody when the board fills up
void soundGameOver() {
  tone(BUZZER_PIN, NOTE_B4, 150);
  delay(160);
  tone(BUZZER_PIN, NOTE_GS4, 150);
  delay(160);
  tone(BUZZER_PIN, NOTE_E4, 150);
  delay(160);
  tone(BUZZER_PIN, NOTE_CS4, 150);
  delay(160);
  tone(BUZZER_PIN, NOTE_B3, 200);
  delay(210);
  tone(BUZZER_PIN, NOTE_GS3, 200);
  delay(210);
  tone(BUZZER_PIN, NOTE_E3, 400);
  delay(410);
  noTone(BUZZER_PIN);
}

// Short tick each time the piece moves down during soft drop
void soundSoftDrop() {
  tone(BUZZER_PIN, NOTE_A3, 15);
  delay(15);
  noTone(BUZZER_PIN);
}

// Short jingle that plays alongside the startup animation
void soundStartup() {
  tone(BUZZER_PIN, NOTE_C4, 80);
  delay(90);
  tone(BUZZER_PIN, NOTE_E4, 80);
  delay(90);
  tone(BUZZER_PIN, NOTE_G4, 80);
  delay(90);
  tone(BUZZER_PIN, NOTE_C5, 150);
  delay(160);
  noTone(BUZZER_PIN);
}

// Two tone rising confirmation when pausing
void soundPause() {
  tone(BUZZER_PIN, NOTE_G4, 60);
  delay(70);
  tone(BUZZER_PIN, NOTE_C5, 60);
  delay(60);
  noTone(BUZZER_PIN);
}

// Two tone falling confirmation when unpausing
void soundUnpause() {
  tone(BUZZER_PIN, NOTE_C5, 60);
  delay(70);
  tone(BUZZER_PIN, NOTE_G4, 60);
  delay(60);
  noTone(BUZZER_PIN);
}

// Draws the next piece centered on the top display (device 3)
void renderPreview() {
  // clear the top display first
  for (int row = 0; row < 8; row++)
    lc.setRow(3, row, 0x00);

  // center the piece in the 8x8 display with a 2 cell offset
  int offsetX = 2;
  int offsetY = 2;

  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++)
      if (cellSet(nextType, 0, r, c)) {
        int row = offsetY + r;
        int col = offsetX + c;
        if (row >= 0 && row < 8 && col >= 0 && col < 8)
          lc.setLed(3, col, row, true);
      }
}

// Returns true if the given piece overlaps a wall, the floor, or a locked cell
bool collides(const Piece& p) {
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++)
      if (cellSet(p.type, p.rot, r, c)) {
        int nx = p.x + c;
        int ny = p.y + r;
        // check walls and floor
        if (nx < 0 || nx >= boardWidth || ny >= boardHeight) return true;
        // check locked cells (ny >= 0 skips cells still above the board)
        if (ny >= 0 && board[ny][nx]) return true;
      }
  return false;
}

// Tries to rotate the piece, nudging left or right if blocked (wall kick)
// Returns true if a valid rotation was found
bool tryRotate(Piece& p) {
  Piece t = p;
  t.rot = (t.rot + 1) % 4;

  // try offsets: 0, +1, -1, +2, -2
  int kicks[] = {0, 1, -1, 2, -2};
  for (int i = 0; i < 5; i++) {
    t.x = p.x + kicks[i];
    if (!collides(t)) {
      p = t;
      return true;
    }
  }
  return false; // no valid position found, rotation blocked
}

// Flashes all displays 3 times then clears them
void gameOverAnimation() {
  for (int i = 0; i < 3; i++) {
    for (int dev = 0; dev < 4; dev++)
      for (int row = 0; row < 8; row++)
        lc.setRow(dev, row, 0xFF);
    delay(200);
    for (int dev = 0; dev < 4; dev++)
      lc.clearDisplay(dev);
    delay(200);
  }
}

// Lights up all 256 LEDs in a random order then turns them off
// Uses Fisher-Yates shuffle to randomize the order
void startupAnimation() {
  soundStartup();

  // build a list of all pixel indices 0-255
  int pixels[256];
  for (int i = 0; i < 256; i++)
    pixels[i] = i;

  // shuffle the list
  for (int i = 255; i > 0; i--) {
    int j = random(i + 1);
    int temp = pixels[i];
    pixels[i] = pixels[j];
    pixels[j] = temp;
  }

  // light up each pixel in shuffled order
  // each pixel index maps to: device = index/64, row = (index%64)/8, col = index%8
  for (int i = 0; i < 256; i++) {
    int dev = pixels[i] / 64;
    int r   = (pixels[i] % 64) / 8;
    int c   = pixels[i] % 8;
    lc.setLed(dev, r, c, true);
    delay(10);
  }

  delay(500);

  // shuffle again for a different turn-off order
  for (int i = 255; i > 0; i--) {
    int j = random(i + 1);
    int temp = pixels[i];
    pixels[i] = pixels[j];
    pixels[j] = temp;
  }

  // turn off each pixel in shuffled order
  for (int i = 0; i < 256; i++) {
    int dev = pixels[i] / 64;
    int r   = (pixels[i] % 64) / 8;
    int c   = pixels[i] % 8;
    lc.setLed(dev, r, c, false);
    delay(10);
  }

  delay(300);
}

// Spawns the next piece at the top of the board
// If it immediately collides the board is full — game over
void spawnPiece() {
  cur = { 2, -1, nextType, 0 };
  nextType = random(7);
  renderPreview();
  if (collides(cur)) {
    checkHighScore(); // save before resetting score
    memset(board, 0, sizeof(board));
    score = 0;
    lines = 0;
    soundGameOver();
    gameOverAnimation();
    renderPreview();
    updateLCD();
  }
  needsRender = true;
}

// Scans for full rows, flashes them all simultaneously, then removes them
// Scoring: 1 line = 100, 2 = 300, 3 = 500, 4 = 800
void clearLines() {
  // find all full rows
  bool fullRows[boardHeight] = {};
  int cleared = 0;
  for (int row = 0; row < boardHeight; row++) {
    bool full = true;
    for (int c = 0; c < boardWidth; c++)
      if (!board[row][c]) { full = false; break; }
    if (full) {
      fullRows[row] = true;
      cleared++;
    }
  }

  if (cleared == 0) return;

  // flash all full rows together 3 times
  for (int flash = 0; flash < 3; flash++) {
    for (int row = 0; row < boardHeight; row++) {
      if (fullRows[row]) {
        int display  = 2 - (row / 8);
        int localRow = row % 8;
        lc.setColumn(display, localRow, 0x00);
      }
    }
    delay(60);
    for (int row = 0; row < boardHeight; row++) {
      if (fullRows[row]) {
        int display  = 2 - (row / 8);
        int localRow = row % 8;
        lc.setColumn(display, localRow, 0xFF);
      }
    }
    delay(60);
  }

  // turn off all full rows before shifting
  for (int row = 0; row < boardHeight; row++) {
    if (fullRows[row]) {
      int display  = 2 - (row / 8);
      int localRow = row % 8;
      lc.setColumn(display, localRow, 0x00);
    }
  }

  // rebuild the board by copying non-full rows from bottom up
  // then zero out the remaining top rows
  int writeRow = boardHeight - 1;
  for (int row = boardHeight - 1; row >= 0; row--) {
    if (!fullRows[row]) {
      memcpy(board[writeRow], board[row], boardWidth);
      writeRow--;
    }
  }
  for (int row = writeRow; row >= 0; row--)
    memset(board[row], 0, boardWidth);

  // update score based on how many lines were cleared at once
  lines += cleared;
  if      (cleared == 1) score += 100;
  else if (cleared == 2) score += 300;
  else if (cleared == 3) score += 500;
  else if (cleared == 4) score += 800;
  soundLineClear(cleared);
  updateLCD();
}

// Locks the current piece into the board, clears any full lines, spawns next piece
void lockPiece() {
  soundLock();
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++)
      if (cellSet(cur.type, cur.rot, r, c) && cur.y + r >= 0)
        board[cur.y + r][cur.x + c] = true;
  clearLines();
  spawnPiece();
  needsRender = true;
}

// Builds a framebuffer from the board and active piece
// Only sends rows to the display that have changed since last frame
void renderBoard() {
  static uint8_t prevFb[boardHeight] = {};
  uint8_t fb[boardHeight] = {};

  // draw locked cells into framebuffer
  for (int row = 0; row < boardHeight; row++)
    for (int col = 0; col < boardWidth; col++)
      if (board[row][col])
        fb[row] |= (1 << (7 - col));

  // draw active piece into framebuffer
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++)
      if (cellSet(cur.type, cur.rot, r, c)) {
        int ny = cur.y + r;
        int nx = cur.x + c;
        if (ny >= 0 && ny < boardHeight && nx >= 0 && nx < boardWidth)
          fb[ny] |= (1 << (7 - nx));
      }

  // only push rows that changed to reduce SPI traffic
  for (int row = 0; row < boardHeight; row++) {
    if (fb[row] != prevFb[row]) {
      int display  = 2 - (row / 8); // device 2=top, 1=middle, 0=bottom of play field
      int localRow = row % 8;
      lc.setColumn(display, localRow, fb[row]);
      prevFb[row] = fb[row];
    }
  }
}

// Reads joystick and button, applies movement or rotation to the current piece
// Returns true if the piece actually moved
bool handleInput() {
  if (millis() - lastInput < INPUT_MS) return false;

  int jx   = analogRead(X_pin);
  int jy   = analogRead(Y_pin);
  bool btn = (digitalRead(SW_pin) == LOW);

  Piece t = cur;

  if (jx < 300) {
    // joystick right = move piece right
    t.x++;
    if (!collides(t)) {
      cur = t;
      lastInput = millis();
      return true;
    }
    lastInput = millis();
    return false;
  }
  else if (jx > 700) {
    // joystick left = move piece left
    t.x--;
    if (!collides(t)) {
      cur = t;
      lastInput = millis();
      return true;
    }
    lastInput = millis();
    return false;
  }
  else if (btn) {
    // button press = rotate with wall kick
    if (millis() - lastRotate < ROTATE_MS) return false;
    if (tryRotate(cur)) {
      lastInput  = millis();
      lastRotate = millis();
      return true;
    }
    lastInput = millis();
    return false;
  }

  return false;
}

void setup() {
  // initialise all 4 LED matrix displays
  for (int dev = 0; dev < 4; dev++) {
    lc.shutdown(dev, false); // wake from power-save mode
    lc.setIntensity(dev, 2); // brightness 0-15
    lc.clearDisplay(dev);
  }

  // initialise LCD, load saved high score, and show starting values
  lcd.begin(16, 2);
  EEPROM.get(0, highScore);
  if (highScore < 0) highScore = 0; // guard against uninitialized EEPROM
  updateLCD();

  // initialise buzzer pin
  pinMode(BUZZER_PIN, OUTPUT);

  // play startup animation and jingle before game begins
  startupAnimation();

  pinMode(SW_pin, INPUT_PULLUP); // button reads HIGH at rest, LOW when pressed
  randomSeed(analogRead(A2));    // A2 unconnected = floating = good random seed
  nextType = random(7);
  spawnPiece();
}

void loop() {
  int jy = analogRead(Y_pin);

  if (!paused) {
    // detect joystick held up for 2 seconds to pause
    if (jy < 300) {
      if (upHoldStart == 0) upHoldStart = millis();
      else if (millis() - upHoldStart > 2000) {
        paused = true;
        upHoldStart = 0;
        soundPause();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("PAUSED");
      }
    } else {
      upHoldStart = 0;
    }

    // handle left, right, rotate
    if (handleInput()) needsRender = true;

    // soft drop — joystick down moves piece faster
    if (jy > 700 && millis() - lastFall > SOFT_DROP_MS) {
      Piece t = cur;
      t.y++;
      if (collides(t)) {
        lockPiece();
      } else {
        cur = t;
        soundSoftDrop();
        needsRender = true;
      }
      lastFall = millis();
    }
    // normal gravity tick
    else if (jy <= 700 && millis() - lastFall > FALL_MS) {
      Piece t = cur;
      t.y++;
      if (collides(t)) lockPiece();
      else             cur = t;
      lastFall    = millis();
      needsRender = true;
    }

    // only redraw when something changed
    if (needsRender) {
      renderBoard();
      needsRender = false;
    }

  } else {
    // paused — detect joystick held up for 2 seconds to unpause
    if (jy < 300) {
      if (upHoldStart == 0) upHoldStart = millis();
      else if (millis() - upHoldStart > 2000) {
        paused = false;
        upHoldStart = 0;
        lastFall = millis(); // reset gravity timer so piece doesn't instantly drop
        soundUnpause();
        updateLCD();         // restore score and lines on LCD
      }
    } else {
      upHoldStart = 0;
    }
  }
}