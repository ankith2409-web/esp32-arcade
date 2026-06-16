# 🎮 ESP32 Retro Arcade Collection

> A collection of classic arcade games running on an ESP32 microcontroller with a joystick and LCD display. Built in C/C++ using the Arduino framework.

---

## 🕹️ Games Included

- **Asteroid Dodge** (`asteroid_dodge.ino`): Survive an asteroid field by dodging space rocks!
- **Breakout** (`breakout.ino`): Classic brick-breaking action.
- **Flappy Bird** (`flappy.c` / `project.c`): Navigate your bird through obstacles.
- **Maze Runner** (`maze_runner.ino`): Find your way through a tricky maze.
- **Pong** (`pong.ino`): Classic arcade tennis, paddle against the AI.
- **Snake** (`snake.ino`): The classic snake arcade game, utilizing the analog joystick.
- **Tetris** (`tetris.ino`): The timeless block-stacking puzzle.
- **Whack-A-Mole** (`whack_a_mole.ino`): Test your reflexes by hitting targets as they appear.

---

## 🔧 Hardware Requirements

| Component | Details |
|-----------|---------|
| Microcontroller | ESP32 (any variant — DevKit v1 recommended) |
| Display | ST7735 / ILI9341 TFT LCD (128×160 or 240×320) |
| Input | Analog joystick module (X/Y axes + push button) |
| Power | USB (5V via DevKit) or 3.7V LiPo with TP4056 |
| Extras | Breadboard, jumper wires |

---

## ⚡ Wiring

### Joystick → ESP32

| Joystick Pin | ESP32 Pin |
|-------------|-----------|
| VCC | 3.3V |
| GND | GND |
| VRx | GPIO34 (ADC) |
| VRy | GPIO35 (ADC) |
| SW (Button) | GPIO32 |

### TFT LCD → ESP32 (SPI)

| LCD Pin | ESP32 Pin |
|---------|-----------|
| VCC | 3.3V |
| GND | GND |
| CS | GPIO5 |
| RESET | GPIO4 |
| DC/RS | GPIO2 |
| SDA/MOSI | GPIO23 |
| SCK | GPIO18 |
| LED/BL | 3.3V or GPIO (for brightness control) |

> Note: Pin assignments may vary slightly depending on the specific game sketch. Check the top of each `.ino` or `.c` file for accurate definitions.

---

## 🚀 Getting Started

### 1. Clone the repo

```bash
git clone https://github.com/ankith2409-web/esp32-arcade.git
cd esp32-arcade
```

### 2. Install dependencies

Open Arduino IDE and install the following libraries via **Library Manager** (`Sketch → Include Library → Manage Libraries`):

- **Adafruit GFX Library**
- **Adafruit ST7735 and ST7789 Library** *(or ILI9341 depending on your screen)*
- **ESP32 Arduino Core** — install via Board Manager if not already done

### 3. Flash a game

- Open any game file (e.g., `asteroid_dodge.ino`) in the Arduino IDE.
- Select your board: `Tools → Board → ESP32 Dev Module`
- Select the correct COM port
- Hit **Upload** ⬆️

---

## 📋 TODO / Roadmap

- [ ] High score saved to ESP32 NVS (non-volatile storage)
- [ ] Game selection menu on boot (combine all into one project!)
- [ ] Buzzer / speaker for sound effects
- [ ] Bluetooth score sharing

---

## 🤝 Contributing

Pull requests are welcome! If you port this to a different display driver or add a new game, feel free to open a PR.

---

## 📄 License

MIT License — use it, hack it, build on it.

---
built by HB MRUDHAL ANKITH
