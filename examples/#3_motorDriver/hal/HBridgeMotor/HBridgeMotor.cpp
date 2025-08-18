/**
 * MIT License
 *
 * @brief Dual-H-bridge motor driver implementation for ESP32.
 *
 * @file HBridgeMotor.cpp
 * @author Little Man Builds
 * @date 2025-06-11
 * @copyright Copyright (c) 2025 Little Man Builds
 */

#include "HBridgeMotor.h"

using MathFns::clamp;

// Destroy the HBridgeMotor object and any soft-brake timer resources.
HBridgeMotor::~HBridgeMotor() noexcept
{
    stopSoftBrake();
    if (soft_timer_)
    {
        (void)esp_timer_stop(soft_timer_); ///< Ignore if not running.
        (void)esp_timer_delete(soft_timer_);
        soft_timer_ = nullptr;
    }
}

// Initialize the motor driver with a given MCPWM hardware configuration.
void HBridgeMotor::setup(const MotorMCPWMConfig &cfg)
{
    MotorBehaviorConfig def{}; ///< Default: HiZ, 300Hz and dither 30.

    // If there's no EN pin, map default HiZ -> HiZ_Awake.
    if (cfg.en_pin < 0 && def.freewheel_mode == FreewheelMode::HiZ)
    {
        def.freewheel_mode = FreewheelMode::HiZ_Awake;
    }

    setup(cfg, def);
}

// Initialize the motor driver with a given MCPWM configuration and per-instance behavior profile.
void HBridgeMotor::setup(const MotorMCPWMConfig &cfg, const MotorBehaviorConfig &beh)
{
    lpwm_pin_ = cfg.lpwm_pin;
    rpwm_pin_ = cfg.rpwm_pin;
    en_pin_ = cfg.en_pin;
    mcpwm_unit_ = cfg.unit;
    mcpwm_timer_ = cfg.timer;
    mcpwm_sig_l_ = cfg.sig_l;
    mcpwm_sig_r_ = cfg.sig_r;

    // Apply behavior (per instance).
    beh_ = beh;
    soft_hz_ = beh_.soft_brake_hz;

    // Auto-detect if enable pin should be used.
    use_en_ = (en_pin_ >= 0);

    // Set up MCPWM outputs.
    ESP_ERROR_CHECK(mcpwm_gpio_init(mcpwm_unit_, mcpwm_sig_l_, lpwm_pin_));
    ESP_ERROR_CHECK(mcpwm_gpio_init(mcpwm_unit_, mcpwm_sig_r_, rpwm_pin_));

    // Configure MCPWM timer/channel.
    mcpwm_config_t pwm_config{};
    pwm_config.frequency = kPwmFreq_;
    pwm_config.cmpr_a = 0;
    pwm_config.cmpr_b = 0;
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;

    ESP_ERROR_CHECK(mcpwm_init(mcpwm_unit_, mcpwm_timer_, &pwm_config));

    // Duty type is static: set once.
    ESP_ERROR_CHECK(mcpwm_set_duty_type(mcpwm_unit_, mcpwm_timer_, MCPWM_OPR_A, MCPWM_DUTY_MODE_0));
    ESP_ERROR_CHECK(mcpwm_set_duty_type(mcpwm_unit_, mcpwm_timer_, MCPWM_OPR_B, MCPWM_DUTY_MODE_0));

    // If enable pin is used, configure it.
    if (use_en_)
    {
        pinMode(en_pin_, OUTPUT);
        setEnable(true); ///< Default enabled.
    }

    // Create soft-brake scheduler (one-shot timer). Not started yet.
    if (!soft_timer_)
    {
        esp_timer_create_args_t args{};
        args.callback = [](void *p)
        { static_cast<HBridgeMotor *>(p)->softBrakeISR(); }; ///< Interrupt Service Routine.
        args.arg = this;
        args.dispatch_method = ESP_TIMER_TASK;
        args.name = "soft_brake";

        ESP_ERROR_CHECK(esp_timer_create(&args, &soft_timer_));
    }
}

// Put motor driver in freewheel (coast) mode.
void HBridgeMotor::setFreewheel() noexcept
{
    stopSoftBrake();
    switch (beh_.freewheel_mode)
    {
    case FreewheelMode::HiZ: ///< True coast mode, motor slows down due to friction.
        setEnable(false);
        writeAB(0.0f, 0.0f);
        break;
    case FreewheelMode::HiZ_Awake: ///< Coast, but keeps the electronics powered for responsiveness.
        setEnable(true);
        writeAB(0.0f, 0.0f);
        break;
    case FreewheelMode::DitherBrake: ///< Light braking via brake/coast pulse.
        setSoftBrakePWM(beh_.dither_pwm);
        startSoftBrake();
        break;
    }
}

// Set hard-brake (full stop) on both outputs.
void HBridgeMotor::setHardBrake() noexcept
{
    stopSoftBrake();
    setEnable(true);
    writeAB(100.0f, 100.0f); ///< Set both channels to 100% duty.
}

// Set the soft-brake PWM value (clamped to 0–max).
void HBridgeMotor::setSoftBrakePWM(uint16_t pwm) noexcept
{
    soft_brake_pwm_ = static_cast<uint16_t>(clamp<int>(pwm, 0, kPwmMaxInput_));

    if (soft_active_)
    {
        recomputeSoftDurations(); ///< Next ISR tick will use the new durations.
    }
}

// Set motor speed and direction. Uses soft-brake if speed = 0.
void HBridgeMotor::setSpeed(int speed, Dir dir) noexcept
{
    const uint16_t clamped = static_cast<uint16_t>(clamp<int>(speed, 0, kPwmMaxInput_));

    // If speed == 0, don't drive the motor.
    if (clamped == 0)
    {
        startSoftBrake();
        return;
    }

    // Non-zero speed => normal drive (ensure soft-brake scheduler is off).
    stopSoftBrake();
    setEnable(true);

    const float duty = static_cast<float>(clamped) * percent_per_count_;
    if (dir == Dir::CW)
    {
        writeAB(duty, 0.0f);
    }
    else
    {
        writeAB(0.0f, duty);
    }
}

// ISR for the soft brake timer — toggles between brake and coast phases.
void HBridgeMotor::softBrakeISR() noexcept
{
    // Flip phase, apply it, and schedule the next one-shot interval.
    soft_phase_ = (soft_phase_ == BrakePhase::Coast) ? BrakePhase::Brake : BrakePhase::Coast;
    applyPhase(soft_phase_);
    scheduleNextPhase();
}

// Apply the specified brake phase output pattern.
void HBridgeMotor::applyPhase(BrakePhase p) noexcept
{
    switch (p)
    {
    case BrakePhase::Brake:
        setEnable(true);
        writeAB(100.0f, 100.0f); ///< Hard short = dynamic braking.
        break;
    case BrakePhase::Coast:
        // Keep EN asserted during soft-brake dither to avoid flapping EN at soft_hz_.
        writeAB(0.0f, 0.0f);
        break;
    }
}

// Schedule the next phase in the soft-brake cycle (one-shot timer).
void HBridgeMotor::scheduleNextPhase() noexcept
{
    if (!soft_active_)
        return;

    // Pick duration for the current phase.
    const int64_t dur_us = (soft_phase_ == BrakePhase::Brake) ? soft_us_brake_ : soft_us_coast_;

    // Guardrail to avoid ultra-short slots (audible/overhead artifacts).
    const int64_t use_us = (dur_us < kMinPhaseUs) ? kMinPhaseUs : dur_us;

    // Arm next one-shot; safe to call from ESP_TIMER_TASK context.
    ESP_ERROR_CHECK(esp_timer_start_once(soft_timer_, use_us));
}

// Start the soft-brake cycle with the current soft_brake_pwm_ setting.
void HBridgeMotor::startSoftBrake() noexcept
{
    // Compute level and durations first.
    recomputeSoftDurations();

    // Pure coast or pure hard-brake (no timer needed).
    if (soft_level_ <= 0.001f)
    {
        stopSoftBrake();
        setFreewheel(); // Freewheel mode (HiZ, HiZ_Awake, DitherBrake).
        return;
    }
    if (soft_level_ >= 0.999f)
    {
        stopSoftBrake();
        setEnable(true);
        writeAB(100.0f, 100.0f);
        return;
    }

    // Compute exact brake/coast durations for one cycle at soft_hz_.
    const double period_us = kMicrosPerSec / static_cast<double>(soft_hz_);
    soft_us_brake_ = static_cast<int64_t>(period_us * soft_level_);
    soft_us_coast_ = static_cast<int64_t>(period_us) - soft_us_brake_;

    if (!soft_active_)
    {
        soft_phase_ = BrakePhase::Coast; // start in coast; ISR will flip to brake.
        soft_active_ = true;
        applyPhase(soft_phase_);
        scheduleNextPhase();
    }
}

// Stop the soft-brake cycle and any scheduled timer.
void HBridgeMotor::stopSoftBrake() noexcept
{
    if (soft_active_)
    {
        // Stop any scheduled one-shot; ignore error if not running.
        (void)esp_timer_stop(soft_timer_);
        soft_active_ = false;
    }
}

// Control the motor driver's enable pin, if configured.
void HBridgeMotor::setEnable(bool on) noexcept
{
    if (use_en_ && en_state_ != on)
    {
        digitalWrite(en_pin_, on ? HIGH : LOW);
        en_state_ = on;
    }
}

// Set the duty cycle for both A and B outputs.
void HBridgeMotor::writeAB(float a_percent, float b_percent) noexcept
{
    mcpwm_set_duty(mcpwm_unit_, mcpwm_timer_, MCPWM_OPR_A, a_percent);
    mcpwm_set_duty(mcpwm_unit_, mcpwm_timer_, MCPWM_OPR_B, b_percent);
}

// Recalculate soft-brake phase durations.
void HBridgeMotor::recomputeSoftDurations() noexcept
{
    soft_level_ = clamp<float>(
        static_cast<float>(soft_brake_pwm_) / static_cast<float>(kPwmMaxInput_), 0.0f, 1.0f);

    const double period_us = kMicrosPerSec / static_cast<double>(soft_hz_);
    soft_us_brake_ = static_cast<int64_t>(period_us * soft_level_);
    soft_us_coast_ = static_cast<int64_t>(period_us) - soft_us_brake_;

    // Enforce minimum segment length.
    if (soft_us_brake_ < kMinPhaseUs && soft_us_brake_ > 0)
        soft_us_brake_ = kMinPhaseUs;
    if (soft_us_coast_ < kMinPhaseUs && soft_us_coast_ > 0)
        soft_us_coast_ = kMinPhaseUs;
}