# Tetris on Arduino

[![Demo Video](https://img.youtube.com/vi/_eHdkO_Wsmk/0.jpg)](https://youtube.com/shorts/_eHdkO_Wsmk)

A fully playable Tetris game running on an Arduino Uno with a 4x8x8 LED matrix display, analog joystick, LCD screen, and a buzzer for sound effects.

---

## Parts

- Arduino Uno
- 4x 8x8 LED matrix display with MAX7219 (daisy chained)
- Analog joystick module
- LCD1602 display (16 pin parallel)
- Passive buzzer
- 10k potentiometer (for LCD contrast)
- Jumper wires

---

## Wiring

### LED Matrix (MAX7219)
| LED Matrix | Arduino |
|---|---|
| DIN | D12 |
| CLK | D10 |
| CS | D11 |
| VCC | 5V |
| GND | GND |

The 4 matrices daisy chain together — only the first module connects to the Arduino, the rest chain through DOUT → DIN.

### Joystick
| Joystick | Arduino |
|---|---|
| VCC | 5V |
| GND | GND |
| VRx | A0 |
| VRy | A1 |
| SW | D2 |

### LCD1602
| LCD Pin | Arduino |
|---|---|
| VSS (1) | GND |
| VDD (2) | 5V |
| V0 (3) | Potentiometer middle pin |
| RS (4) | D7 |
| RW (5) | GND |
| E (6) | D6 |
| D4 (11) | D5 |
| D5 (12) | D4 |
| D6 (13) | D3 |
| D7 (14) | D8 |
| A (15) | 5V |
| K (16) | GND |

### Buzzer
| Buzzer | Arduino |
|---|---|
| + | D13 |
| - | GND |

---

## Features

- All 7 tetriminos with 4 rotations each
- Wall kicks so pieces can rotate near walls
- Soft drop by pushing the joystick down
- Next piece preview on the top display
- Score, lines cleared, and high score shown on the LCD
- High score saved to EEPROM so it persists after power off
- Line clear animation — completed rows flash before disappearing
- Sound effects for locking, line clears, soft drop, game over, pause, and startup
- Pause by holding the joystick up for 2 seconds
- Game over animation with automatic board reset
- Fisher-Yates shuffle startup animation

---

## Controls

| Input | Action |
|---|---|
| Joystick left / right | Move piece |
| Joystick down | Soft drop |
| Joystick up (hold 2s) | Pause / unpause |
| Button | Rotate piece |

---

## Libraries

Install these through the Arduino Library Manager before uploading:

- [LedControl](https://github.com/wayoda/LedControl)
- LiquidCrystal (built into Arduino IDE)
- EEPROM (built into Arduino IDE)
- pitches.h (included in the repo)

---

Made by Bernie Gampel
