/**
 * MIT License
 *
 * @brief Dual-H-bridge motor driver implementation for ESP32.
 *
 * @file HBridgeMotor.h
 * @author Little Man Builds
 * @date 2025-08-11
 * @copyright Copyright (c) 2025 Little Man Builds
 */

#pragma once

#include <cstdint>
#include <app_config.h>
#include <IMotorDriver.h>
#include <MathFns/MathFns.h>
#include <esp_timer.h>

/**
 * @brief Dual-H-bridge motor driver class.
 *
 * Provides direction, speed, and soft-brake control via dual PWM.
 */
class HBridgeMotor : public IMotorDriver
{
public:
    /**
     * @brief Construct a new HBridgeMotor instance.
     */
    HBridgeMotor() = default;

    /**
     * @brief Destroy the HBridgeMotor object and any soft-brake timer resources.
     */
    ~HBridgeMotor() noexcept override;

    /**
     * @brief Initialize the motor driver with a given MCPWM hardware configuration.
     * @param cfg Motor MCPWM configuration (pin assignments, MCPWM unit/timer/signal mapping).
     */
    void setup(const MotorMCPWMConfig &cfg) override;

    /**
     * @brief Initialize the motor driver with a given MCPWM configuration and per-instance behavior profile.
     * @param cfg Motor MCPWM configuration (pin assignments, MCPWM unit/timer/signal mapping).
     * @param beh Behavior configuration defining freewheel mode, soft-brake frequency and duty,
     *            and whether to use the enable pin if present.
     */
    void setup(const MotorMCPWMConfig &cfg, const MotorBehaviorConfig &beh) override;

    /**
     * @brief Put motor in freewheel (coast) mode.
     */
    void setFreewheel() noexcept override;

    /**
     * @brief Set hard-brake (full stop).
     */
    void setHardBrake() noexcept override;

    /**
     * @brief Set the soft-brake PWM value (default: 50).
     * @param pwm PWM value for soft-brake (0..getMaxPwmInput()).
     */
    void setSoftBrakePWM(uint16_t pwm) noexcept override;

    /**
     * @brief Set motor speed and direction. Uses soft-brake if speed = 0.
     * @param speed Speed value (0..getMaxPwmInput()).
     * @param dir Motor direction (CW/CCW).
     */
    void setSpeed(int speed, Dir dir) noexcept override;

    /**
     * @brief Get the maximum PWM input value accepted by driver.
     * @return Maximum valid input value for setSpeed(), as an integer.
     */
    [[nodiscard]] int getMaxPwmInput() const noexcept override { return kPwmMaxInput_; }

private:
    static constexpr int kPwmFreq_ = 20000;                   ///< PWM frequency (Hz).
    static constexpr int kBitRes_ = 10;                       ///< PWM resolution (bits).
    static constexpr int kPwmMaxInput_ = (1 << kBitRes_) - 1; ///< Max input value (e.g., 1023 for 10-bit).
    static constexpr int kDefaultBrake_ = 50;                 ///< Default soft-brake PWM.
    int lpwm_pin_{-1};                                        ///< Left PWM pin number.
    int rpwm_pin_{-1};                                        ///< Right PWM pin number.
    int en_pin_{-1};                                          ///< Enable pin number (-1 if unused).
    bool use_en_{false};                                      ///< If true, enable pin will be managed.
    bool en_state_{false};                                    ///< Enable state (HIGH = enabled).
    float percent_per_count_ = 100.0f / kPwmMaxInput_;        ///< Duty percent per input count.

    uint16_t soft_brake_pwm_{kDefaultBrake_};  ///< Requested soft-brake level (0..kPwmMaxInput_).
    mcpwm_unit_t mcpwm_unit_{MCPWM_UNIT_0};    ///< MCPWM unit.
    mcpwm_timer_t mcpwm_timer_{MCPWM_TIMER_0}; ///< MCPWM timer.
    mcpwm_io_signals_t mcpwm_sig_l_{MCPWM0A};  ///< MCPWM output for left signal.
    mcpwm_io_signals_t mcpwm_sig_r_{MCPWM0B};  ///< MCPWM output for right signal.
    MotorBehaviorConfig beh_{};                ///< Per instance behavior configuration.

    /**
     * @brief Brake phase state machine for soft braking.
     */
    enum class BrakePhase : uint8_t
    {
        Coast, ///< Outputs set to 0% duty, motor freewheels.
        Brake  ///< Outputs shorted for dynamic braking.
    };

    static constexpr double kMicrosPerSec = 1e6; ///< Microseconds in one second.
    static constexpr int64_t kMinPhaseUs = 1500; ///< Minimum phase length to avoid audible noise and timer overhead issues.
    esp_timer_handle_t soft_timer_{nullptr};     ///< Handle for ESP-IDF one-shot timer used in soft brake.
    bool soft_active_{false};                    ///< Checks if the one-shot timer is running.
    BrakePhase soft_phase_{BrakePhase::Coast};   ///< Current phase of the soft-brake cycle.
    float soft_level_{0.0f};                     ///< 0..1 derived from soft_brake_pwm_.
    int soft_hz_{300};                           ///< Perceptual dither frequency (200–500 recommended).
    int64_t soft_us_brake_{0};                   ///< µs for BRAKE phase in one cycle.
    int64_t soft_us_coast_{0};                   ///< µs for COAST phase in one cycle.

    /**
     * @brief ISR for the soft brake timer — toggles between brake and coast phases.
     */
    void softBrakeISR() noexcept;

    /**
     * @brief Apply the specified brake phase output pattern.
     * @param p Brake phase to apply (coast or brake).
     */
    void applyPhase(BrakePhase p) noexcept;

    /**
     * @brief Schedule the next phase in the soft-brake cycle (one-shot timer).
     */
    void scheduleNextPhase() noexcept;

    /**
     * @brief Start the soft-brake cycle with the current soft_brake_pwm_ setting.
     */
    void startSoftBrake() noexcept;

    /**
     * @brief Stop the soft-brake cycle and any scheduled timer.
     */
    void stopSoftBrake() noexcept;

    /**
     * @brief Control the motor driver's enable pin, if configured.
     * @param on Set to 'true' to enable driver, or 'false' to disable.
     */
    void setEnable(bool on) noexcept;

    /**
     * @brief Set the duty cycle for both A and B outputs.
     * @param a_percent Duty cycle percentage for output A (0.0-100.0).
     * @param b_percent Duty cycle percentage for output B (0.0-100.0).
     */
    void writeAB(float a_percent, float b_percent) noexcept;

    /**
     * @brief Recalculate soft-brake phase durations.
     */
    void recomputeSoftDurations() noexcept;

    /**
     * @brief Prevent copying/assignment.
     */
    HBridgeMotor(const HBridgeMotor &) = delete;            ///< Non-copyable: owns/controls hardware resources.
    HBridgeMotor &operator=(const HBridgeMotor &) = delete; ///< Non-assignable for the same reason.
};