/**
 * MIT License
 *
 * @brief Math utility functions for common math(s) operations.
 *
 * @file MathFns.h
 * @author Little Man Builds
 * @date 2025-06-09
 * @copyright Copyright (c) 2025 Little Man Builds
 */

#pragma once

#include <app_config.h>
#include <cmath>

/**
 * @brief MathFns provides static utility methods for common math(s) operations.
 */
namespace MathFns
{
    /**
     * @brief Clamp a value between a lower and upper bound.
     *
     * @tparam T Numeric or comparable type.
     * @param value Input value to clamp.
     * @param low Minimum allowable value.
     * @param high Maximum allowable value.
     * @return constexpr T The clamped value.
     */
    template <typename T>
    constexpr T clamp(T value, T low, T high)
    {
        return (value < low)    ? low
               : (value > high) ? high
                                : value;
    }
}