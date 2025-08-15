/**
 * MIT License
 *
 * @brief Abstract base class for button event handling.
 *
 * @file IButtonHandler.h
 * @author Little Man Builds
 * @date 2025-08-05
 * @copyright Copyright (c) 2025 Little Man Builds
 */

#pragma once

#include <stdint.h>

/**
 * @brief Enum for type of button press event.
 */
enum class ButtonPressType
{
    None,  ///< No event.
    Short, ///< Short press event.
    Long   ///< Long press event.
};

/**
 * @brief Configuration for button debounce and press duration timings.
 */
struct ButtonTimingConfig
{
    uint32_t debounce_ms;    ///< Minimum time to confirm a press/release.
    uint32_t short_press_ms; ///< Minimum time for a short press.
    uint32_t long_press_ms;  ///< Minimum time for a long press.

    constexpr ButtonTimingConfig(
        uint32_t debounce = 30,
        uint32_t short_press = 200,
        uint32_t long_press = 1000)
        : debounce_ms(debounce), short_press_ms(short_press), long_press_ms(long_press) {}
};

/**
 * @brief Abstract interface for button event handlers.
 */
class IButtonHandler
{
public:
    /**
     * @brief Virtual destructor for safe polymorphic deletion.
     */
    virtual ~IButtonHandler() noexcept = default;

    /**
     * @brief Scan and process button states (debounce and press timing).
     */
    virtual void update() noexcept = 0;

    /**
     * @brief Get debounced state of button.
     * @param buttonId Index of button.
     * @return True if button is pressed.
     */
    [[nodiscard]] virtual bool isPressed(uint8_t buttonId) const noexcept = 0;

    /**
     * @brief Get and consume press event for a button.
     * @param buttonId Index of button.
     * @return ButtonPressType Event type: Short, Long, or None.
     */
    virtual ButtonPressType getPressType(uint8_t buttonId) noexcept = 0;
};