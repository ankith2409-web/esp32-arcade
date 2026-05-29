# 🎮 ESP32 Retro Arcade — Snake & Floppy Bird

> Two classic arcade games running on an ESP32 microcontroller with a joystick and LCD display. Built from scratch in C/C++ using the Arduino framework.

---

## 🔧 Hardware Requirements

| Component | Details |
|-----------|---------|
| Microcontroller | ESP32 (any variant — DevKit v1 recommended) |
| Display | ST7735 / ILI9341 TFT LCD (128×160 or 240×320) |
| Input | Analog joystick module (X/Y axes + push button) |
| Power | USB (5V via DevKit) or 3.7V LiPo with TP4056 |
| Extras | Breadboard, jumper wires |

> **Note:** Floppy Bird uses the joystick button (or Y-axis tap) to flap. Snake uses the joystick axes for direction.

---

## 🗂️ Project Structure

```
esp32-retro-arcade/
│
├── snake/
│   ├── snake.ino           # Main sketch
│   ├── game.h              # Game logic header
│   └── display.h           # Display helper functions
│
├── floppy_bird/
│   ├── floppy_bird.ino     # Main sketch
│   ├── bird.h              # Bird physics & state
│   └── pipes.h             # Pipe generation & collision
│
├── shared/
│   └── config.h            # Pin definitions & common config
│
└── README.md
```

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

> Pin assignments can be changed in `shared/config.h`.

---

## 🚀 Getting Started

### 1. Clone the repo

```bash
git clone https://github.com/YOUR_USERNAME/esp32-retro-arcade.git
cd esp32-retro-arcade
```

### 2. Install dependencies

Open Arduino IDE and install the following libraries via **Library Manager** (`Sketch → Include Library → Manage Libraries`):

- **Adafruit GFX Library**
- **Adafruit ST7735 and ST7789 Library** *(or ILI9341 depending on your screen)*
- **ESP32 Arduino Core** — install via Board Manager if not already done

### 3. Flash a game

- Open `snake/snake.ino` or `floppy_bird/floppy_bird.ino` in Arduino IDE
- Select your board: `Tools → Board → ESP32 Dev Module`
- Select the correct COM port
- Hit **Upload** ⬆️

---

## 🎮 Controls

### Snake
| Action | Input |
|--------|-------|
| Move Up | Joystick ↑ |
| Move Down | Joystick ↓ |
| Move Left | Joystick ← |
| Move Right | Joystick → |
| Pause / Restart | Joystick Button |

### Floppy Bird
| Action | Input |
|--------|-------|
| Flap | Joystick Button (press) |
| Start Game | Joystick Button |
| Restart after death | Joystick Button |


---

## 🧠 How It Works

### Snake
The game grid is mapped to the LCD resolution divided by `GRID_SIZE`. The snake body is stored as a queue of (x, y) positions. Each frame, the head advances in the current direction, and the tail is removed — unless food was just eaten, in which case the tail stays (grow). Collision detection checks the head against walls and the body array.

### Floppy Bird
The bird has a vertical velocity that increases each frame due to gravity. A button press applies an upward impulse (`FLAP_STRENGTH`). Pipes are generated off-screen at random heights and scroll left each frame. Collision is AABB (axis-aligned bounding box) between the bird sprite and each pipe rectangle.

---

## 📋 TODO / Roadmap

- [ ] High score saved to ESP32 NVS (non-volatile storage)
- [ ] Game selection menu on boot
- [ ] Buzzer / speaker for sound effects
- [ ] Difficulty levels for both games
- [ ] Bluetooth score sharing

---

## 🤝 Contributing

Pull requests are welcome! If you port this to a different display driver or add a new game, feel free to open a PR.

---

## 📄 License

MIT License — use it, hack it, build on it.

---
built by HB MRUDHAL ANKITH
