/**
 * MIT License
 *
 * @brief Generic multi-button handler with debounce and press duration detection.
 *
 * @file ButtonHandler.h
 * @author Little Man Builds
 * @date 2025-08-05
 * @copyright Copyright (c) 2025 Little Man Builds
 */

#pragma once

#include <app_config.h>
#include <IButtonHandler.h>
#include <functional>
#include <stdint.h>

/**
 * @brief Generic multi-button handler (adaptable to any digital input source).
 *
 * Uses a template parameter N for compile-time button count.
 */
template <size_t N> ///< Sets the number of buttons at compile time.
class ButtonHandler : public IButtonHandler
{
    static_assert(N > 0, "Button<N>: N must be greater than 0."); ///< Ensures there is at least 1 button.

public:
    /**
     * @brief Type alias for the function used to read button state. Returns true if pressed.
     */
    using ReadFunc = std::function<bool(uint8_t)>; ///< Defines a callable type to read the state of a button.

    /**
     * @brief Construct a Button handler.
     * @param buttonPins Array of button pin numbers (length N).
     * @param readFunc Function to read button state: takes pin index, returns true if pressed.
     *                 Defaults to native digitalRead with LOW=pressed (internal pullup).
     * @param timing Debounce and press duration configuration.
     */
    ButtonHandler(const uint8_t buttonPins[N], ReadFunc readFunc = nullptr, ButtonTimingConfig timing = {})
        : read_func_{readFunc}, timing_{timing}
    {
        for (size_t i = 0; i < N; ++i)
        {
            pins_[i] = buttonPins[i];
            pinMode(pins_[i], INPUT_PULLUP);
            last_state_[i] = false;
            last_state_change_[i] = 0;
            press_start_[i] = 0;
            event_[i] = ButtonPressType::None;
        }
        // Default read function for native ESP32 pins (LOW = pressed).
        if (!read_func_)
        {
            read_func_ = [](uint8_t pin)
            { return digitalRead(pin) == LOW; };
        }
    }

    /**
     * @brief Scan and process button states (debounce and press timing).
     */
    void update() noexcept override
    {
        uint32_t now = millis();

        for (size_t i = 0; i < N; ++i) ///< Loop through each button.
        {
            bool pressed = read_func_(pins_[i]); // Check to see if a button is pressed.

            // Debounce logic: only accept state change if stable for debunce_ms.
            if (pressed != last_state_[i])
            {
                // Check debounce timing.
                if (now - last_state_change_[i] >= timing_.debounce_ms)
                {
                    last_state_change_[i] = now; // Update last state change time.
                    last_state_[i] = pressed;

                    if (pressed)
                    {
                        press_start_[i] = now; ///< New press detected, mark start time.
                    }
                    else
                    {
                        // Button released, check duration.
                        uint32_t duration = now - press_start_[i];
                        if (duration >= timing_.long_press_ms)
                            event_[i] = ButtonPressType::Long;
                        else if (duration >= timing_.short_press_ms)
                            event_[i] = ButtonPressType::Short;
                        else
                            event_[i] = ButtonPressType::None;
                        press_start_[i] = 0;
                    }
                }
            }
            // If button is held, keep press_start_[i] (do not reset).
        }
    }

    /**
     * @brief Get debounced state of button.
     * @param buttonId Index of button.
     * @return True if button is pressed.
     */
    bool isPressed(uint8_t buttonId) const noexcept override
    {
        if (buttonId >= N)
            return false;
        return last_state_[buttonId];
    }

    /**
     * @brief Get and consume the press event type for a button.
     * @param buttonId Index of button.
     * @return ButtonPressType Event type: Short, Long, or None.
     */
    ButtonPressType getPressType(uint8_t buttonId) noexcept override
    {
        if (buttonId >= N)
            return ButtonPressType::None;
        ButtonPressType e = event_[buttonId];
        event_[buttonId] = ButtonPressType::None; // Consume event.
        return e;
    }

private:
    uint8_t pins_[N];               ///< Pin numbers for each button.
    bool last_state_[N];            ///< Last debounced state for each button.
    uint32_t last_state_change_[N]; ///< Timestamp (ms) of the last stable state change.
    uint32_t press_start_[N];       ///< Timestamp (ms) when press started.
    ButtonPressType event_[N];      ///< Pending event (short/long) per button.
    ReadFunc read_func_;            ///< Function to read the button state.
    ButtonTimingConfig timing_;     ///< Debounce and press duration configuration.
};