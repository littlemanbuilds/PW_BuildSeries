# HBridgeMotor Library (ESP32)

A small, RTOS-friendly motor driver layer for ESP32 that abstracts H-bridge control (e.g. **BTS7960/IBT-2**, **DRV8871**). Supports **per-instance behavior** (freewheel strategy, soft-brake dither), clean initialization, and task-based usage.


## Features

- **Unified interface:** `IMotorDriver` with `setSpeed`, `setFreewheel`, `setHardBrake`, `setSoftBrakePWM`.
- **Per-instance behavior config:** choose how *freewheel* behaves (Hi-Z, Hi-Z while awake, or light dither braking).
- **Soft-brake dithering:** smoothly modulates braking when `speed == 0`.
- **ESP-IDF MCPWM** control path using GPIO matrix (ESP32/ESP32-S3).
- **RTOS friendly:** pass instances into tasks via small context structs—no globals required.

## Supported drivers (tested)

- **BTS7960 / IBT-2** (dual half-bridge module, typical pins LPWM/RPWM + L_EN/R_EN).
- **DRV8871** (simple dual-input H-bridge; no dedicated EN pin).  
  > Works with the same `HBridgeMotor` — just set `en_pin = -1`.

## Key Types

### `MotorMCPWMConfig`
```cpp
struct MotorMCPWMConfig {
  uint8_t           lpwm_pin;  // A output
  uint8_t           rpwm_pin;  // B output
  int8_t            en_pin;    // -1 if unused
  mcpwm_unit_t      unit;
  mcpwm_timer_t     timer;
  mcpwm_io_signals_t sig_l;    // MCPWM A signal
  mcpwm_io_signals_t sig_r;    // MCPWM B signal
};
```

**Example (BTS7960/IBT-2):**
```cpp
// app_config.h
const MotorMCPWMConfig DRIVE_MCPWM = {
  37, 38, 39,        // lpwm, rpwm, en (tie L_EN & R_EN to this GPIO)
  MCPWM_UNIT_0,
  MCPWM_TIMER_0,
  MCPWM0A, MCPWM0B
};
```

### `FreewheelMode`
```cpp
enum class FreewheelMode : uint8_t {
  HiZ,        // A=0,B=0; EN low if available (true coast; may sleep on some drivers)
  HiZ_Awake,  // A=0,B=0; EN kept high (stay awake, zero duty)
  DitherBrake // low-duty soft-brake pulses to add slight drag / keep driver awake
};
```

## How it maps

### 1. `HiZ` (true coast, EN low if available)
**Code meaning:**
- Both PWM inputs are driven low (`A = 0`, `B = 0`).
- If an enable pin exists (`en_pin >= 0`), it’s driven **low** to fully disable the H-bridge output stage.
- This allows the motor to spin freely with minimal drag.

**When to use:**
- Best for **maximum coasting distance** and **lowest power draw**.
- **BTS7960:** Also powers down the driver if EN is wired.
- **DRV8871:** Just floats the motor; still awake.

---

### 2. `HiZ_Awake` (stay awake, zero duty)
**Code meaning:**
- Both PWM inputs are driven low (`A = 0`, `B = 0`).
- If enable exists, it’s kept **high** so the driver doesn’t power down.
- Motor still spins freely, but driver logic stays “awake” for faster response to the next command.

**When to use:**
- Use when you want to **coast** but respond instantly to throttle commands.
- **BTS7960:** Keeps logic awake.
- **DRV8871:** Same as `HiZ`, but intention is clearer in code.

---

### 3. `DitherBrake` (light braking pulses)
**Code meaning:**
- Injects a **very low duty-cycle PWM** (set via `dither_pwm`) at a set frequency (`soft_brake_hz`) when “freewheeling”.
- Creates a tiny braking torque to prevent overspinning or to keep the driver from entering deep sleep.

**When to use:**
- Use to **keep driver awake** and/or add very light drag.
- **BTS7960:** Prevents overspinning without harsh stops.
- **DRV8871:** Keeps IC from sleeping, avoiding wake-up delay.

### `MotorBehaviorConfig`
```cpp
struct MotorBehaviorConfig {
  FreewheelMode freewheel_mode = FreewheelMode::HiZ;
  int           soft_brake_hz  = 300; // dither frequency
  uint16_t      dither_pwm     = 30;  // tiny duty (0..getMaxPwmInput())

  constexpr MotorBehaviorConfig() = default;
  constexpr MotorBehaviorConfig(FreewheelMode mode, int hz, uint16_t dither)
    : freewheel_mode(mode), soft_brake_hz(hz), dither_pwm(dither) {}
};
```

**Why these defaults?**  
`300 Hz` is a good middle ground (low audible impact, smooth torque). `dither_pwm=30` (~3% of 10-bit 1023) is “just enough” to keep drivers awake / add slight drag without wasting power.

## API Cheatsheet

```cpp
// Setup (must be called before use)
void setup(const MotorMCPWMConfig& cfg);
void setup(const MotorMCPWMConfig& cfg, const MotorBehaviorConfig& beh);

// Runtime control
void setSpeed(int speed, Dir dir);       // 0 engages soft-brake logic
void setFreewheel() noexcept;            // applies FreewheelMode behavior
void setHardBrake() noexcept;            // dynamic brake (A=B=100%)
void setSoftBrakePWM(uint16_t pwm);      // 0..getMaxPwmInput(); used when speed==0

// Info
int getMaxPwmInput() const;              // e.g., 1023 for 10-bit path
```

**Gotchas**
- If `setSpeed(0, ...)` is called and `setSoftBrakePWM(...)` is *very low*, the driver will fall back to your configured `FreewheelMode` (pure coast vs dither).
- During soft-brake dither, EN is held **on** to avoid flapping the enable pin.
- `en_pin` presence is **auto-detected** from `MotorMCPWMConfig::en_pin >= 0`.

## Safety & Wiring Notes

- **Common ground** between ESP32 and H-bridge power supply.
- Keep motor leads short/twisted to reduce EMI.  
- BTS7960: tie **L_EN** and **R_EN** together to the configured `en_pin`.  
- DRV8871: set `en_pin = -1` in `MotorMCPWMConfig`.

## License

MIT — see file headers.
