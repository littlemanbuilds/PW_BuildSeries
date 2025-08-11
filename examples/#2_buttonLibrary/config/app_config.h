/**
 * MIT License
 *
 * @brief Configuration file for all ESP32 parameters.
 *
 * @file app_config.h
 * @author Little Man Builds
 * @date 2025-08-01
 * @copyright Copyright (c) 2025 Little Man Builds
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Debugging macros.
 */
#define DEBUGGING true

#if DEBUGGING == true
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif

/**
 * @brief Application-wide settings.
 */
namespace cfg
{
    constexpr int LOOP_INTERVAL_MS = 10;
    constexpr int LOOP_INTERVAL_TEST_SHORT = 250;
    constexpr int LOOP_INTERVAL_TEST_LONG = 1000;
}

/**
 * @brief Button pin assignments/index.
 */
#define BUTTON_LIST(X) \
    X(TestButton, 7)

struct ButtonsPins
{
#define X(name, pin) static constexpr uint8_t name = pin;
    BUTTON_LIST(X)
#undef X
};

constexpr uint8_t BUTTON_PINS[] = {
#define X(name, pin) pin,
    BUTTON_LIST(X)
#undef X
};

constexpr size_t NUM_BUTTONS = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);

enum ButtonIndex : uint8_t
{
#define X(name, pin) name,
    BUTTON_LIST(X)
#undef X
        NUM_BUTTONS_ENUM
};