/**
 * MIT License
 *
 * @brief Abstract base class for drive motor control.
 *
 * @file IMotorDriver.h
 * @author Little Man Builds
 * @date 2025-08-11
 * @copyright Copyright (c) 2025 Little Man Builds
 */

#pragma once

#include <cstdint>

struct MotorMCPWMConfig; ///< Forward-declare to avoid needing to include app_config.h here.

/**
 * @brief How the driver behave when setFreewheel() is called.
 */
enum class FreewheelMode : uint8_t
{
    HiZ,        ///< Coast with output HiZ; dirver may sleep.
    HiZ_Awake,  ///< Coast with output HiZ; driver stays awake.
    DitherBrake ///< Pulsed brake/coast for light drag.
};

/**
 * @brief Per-instance behavior (tunable at setup).
 */
struct MotorBehaviorConfig
{
    FreewheelMode freewheel_mode = FreewheelMode::HiZ; ///< Default mode.
    int soft_brake_hz = 300;                           ///< Dither frequency used by soft-brake.
    uint16_t dither_pwm = 30;                          ///< Tiny PWM used for DitherBrake mode.

    constexpr MotorBehaviorConfig() = default;

    constexpr MotorBehaviorConfig(FreewheelMode mode, int hz, uint16_t dither)
        : freewheel_mode(mode), soft_brake_hz(hz), dither_pwm(dither) {}
};

/**
 * @brief Motor direction enum.
 */
enum class Dir : uint8_t
{
    CW, ///< Clockwise / Forward.
    CCW ///< Counterclockwise / Reverse.
};

/**
 * @brief Abstract base class for controlling drive motors.
 */
class IMotorDriver
{
public:
    /**
     * @brief Virtual destructor for safe polymorphic deletion.
     */
    virtual ~IMotorDriver() noexcept = default;

    /**
     * @brief Initialize the motor driver with a given MCPWM hardware configuration.
     * @param cfg Motor MCPWM configuration (pin assignments, MCPWM unit/timer/signal mapping).
     */
    virtual void setup(const MotorMCPWMConfig &cfg) = 0;

    /**
     * @brief Initialize the motor driver with a given MCPWM configuration and per-instance behavior profile.
     * @param cfg Motor MCPWM configuration (pin assignments, MCPWM unit/timer/signal mapping).
     * @param beh Behavior configuration defining freewheel mode, soft-brake frequency and duty,
     *            and whether to use the enable pin if present.
     */
    virtual void setup(const MotorMCPWMConfig &cfg, const MotorBehaviorConfig & /*beh*/)
    {
        setup(cfg);
    }

    /**
     * @brief Place driver in freewheel (coast) mode.
     */
    virtual void setFreewheel() noexcept = 0;

    /**
     * @brief Set hard-brake (full stop).
     */
    virtual void setHardBrake() noexcept = 0;

    /**
     * @brief Optionally set the soft-brake PWM value (default: no-op).
     * @param pwm PWM value for soft-brake (0..getMaxPwmInput()).
     */
    virtual void setSoftBrakePWM(uint16_t /*pwm*/) noexcept {}

    /**
     * @brief Set motor speed and direction.
     * @param speed Speed value (0..getMaxPwmInput()).
     * @param dir Motor direction (CW/CCW).
     */
    virtual void setSpeed(int speed, Dir dir) noexcept = 0;

    /**
     * @brief Get the maximum PWM input value accepted by driver.
     * @return Maximum valid input value for setSpeed(), as an integer.
     */
    [[nodiscard]] virtual int getMaxPwmInput() const noexcept { return 255; } ///< Default: 255 (8-bit).

    /**
     * @brief Utility: return the opposite direction.
     * @param dir Input direction.
     * @return Dir Opposite direction.
     */
    static Dir changeDir(Dir dir) noexcept { return dir == Dir::CW ? Dir::CCW : Dir::CW; }
};