/**
 * MIT License
 *
 * @brief Main program for ESP32-based project. Sets up all hardware, RTOS tasks, queues, and system initialization.
 *
 * @file main.cpp
 * @author Little Man Builds
 * @date 2025-08-01
 * @copyright Copyright (c) 2025 Little Man Builds
 */

#include <app_config.h>
#include <ButtonHandler/ButtonHandler.h>
#include <HBridgeMotor/HBridgeMotor.h>

/**
 * @brief Constants and type definitions.
 */
constexpr int LISTENER_STACK = 2048; ///< Memory allocated to the listener stack (in bytes).
constexpr int HANDLER_STACK = 4096;  ///< Memory allocated to the handler stack (in bytes).

constexpr UBaseType_t PRI_LISTENER = 1; ///< Task Priority 1.
constexpr UBaseType_t PRI_HANDLER = 2;  ///< Task Priority 2.

/**
 * @brief Task Context Structs (data passed to the RTOS tasks).
 */
struct ListenerContext
{
  IButtonHandler *buttons; ///< Pointer to button handler interface for scanning inputs.
};

struct HandlerContext
{
  IMotorDriver *motor; ///< Pointer to motor driver interface implementation.
};

/**
 * @brief Global RTOS handles and queues.
 */
TaskHandle_t listener_t = nullptr; ///< Listener logic task handle.
TaskHandle_t handler_t = nullptr;  ///< Handler logic task handle.

/**
 * @brief Function declarations.
 */
void listener(void *parameter);
void handler(void *parameter);

void setup()
{
  // Start serial debugging output.
  Serial.begin(115200); // Default baud rate of the ESP32.

  debugln("===== Startup =====");

  // Hardware objects, initialized as statics for RTOS safety.
  static ButtonHandler<NUM_BUTTONS> btnHandler(BUTTON_PINS, nullptr, ButtonTimingConfig{30, 200, 1000});
  static HBridgeMotor driveMtr;

  // Setup motor and behavior.
  MotorBehaviorConfig mtrBeh(FreewheelMode::HiZ, 300, 30);
  driveMtr.setup(DRIVE_MCPWM, mtrBeh);
  // driveMtr.setup(DRIVE_MCPWM); ///< Default setup.

  // RTOS Task Context Structs.
  static ListenerContext listenerCtx{&btnHandler};
  static HandlerContext handlerCtx{&driveMtr};

  // RTOS Task Creation.
  configASSERT(xTaskCreatePinnedToCore(listener,       ///< Task function (must be void listener(void *)).
                                       "listener",     ///< Task name (for debugging).
                                       LISTENER_STACK, ///< Stack size (in bytes).
                                       &listenerCtx,   ///< Parameter passed to the task.
                                       PRI_LISTENER,   ///< Task Priority.
                                       &listener_t,    ///< Pointer to the task handle.
                                       0) == pdPASS);  ///< Core to pin task to (core 0).
  delay(50);
  configASSERT(xTaskCreatePinnedToCore(handler, "handler", HANDLER_STACK, &handlerCtx, PRI_HANDLER, &handler_t, 0) == pdPASS);
  delay(50);

  debugln("All RTOS tasks started!");
}

/**
 * @brief Main Arduino loop.
 *
 * Not used because all code is handled via RTOS tasks. This function simply deletes itself.
 */
void loop()
{
  vTaskDelete(NULL); ///< Delete this task.
}

/**
 * @brief RTOS task for event listening.
 *
 * @param parameter Pointer to the RTOS queue structure (cast from void*).
 */
void listener(void *parameter)
{
  auto *ctx = static_cast<ListenerContext *>(parameter); ///< Recover typed task context from void* arg.
  auto *btnHandler = ctx->buttons;                       ///< Local alias for button handler.
  TickType_t lastWake = xTaskGetTickCount();

  for (;;)
  {
    /* btnHandler->update(); // Scan all button states.

    if (btnHandler->isPressed(ButtonIndex::TestButton))
    {
      debugln("ButtonTest is pressed!");
    }
    else
    {
      debugln("No input detected.");
    } */

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(cfg::LOOP_INTERVAL_TEST_SHORT));
  }
}

/**
 * @brief RTOS task for event handling.
 *
 * @param parameter Pointer to the RTOS queue structure (cast from void*).
 */
void handler(void *parameter)
{
  auto *ctx = static_cast<HandlerContext *>(parameter); ///< Recover typed task context from void* arg.
  auto *mtrDriver = ctx->motor;                         ///< Local alias for motor driver.

  TickType_t lastWake = xTaskGetTickCount();

  // Determine the PWM range.
  const uint16_t maxPWM = mtrDriver->getMaxPwmInput();
  const uint16_t minPWM = 100; ///< Minimum effective PWM starting point.

  for (;;)
  {
    debugln("Ramp up start...");
    for (uint16_t pwm = minPWM; pwm <= maxPWM; pwm += 10)
    {
      mtrDriver->setSpeed(pwm, Dir::CW);
      debug("Ramping up -> PWM: ");
      debugln(pwm);
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    debugln("Holding speed...");
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    debugln("Ramp down start...");
    for (int pwm = maxPWM; pwm >= minPWM; pwm -= 10)
    {
      mtrDriver->setSpeed(pwm, Dir::CW);
      debug("Ramping down -> PWM: ");
      debugln(pwm);
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    debugln("Coasting / Freewheel...");
    mtrDriver->setFreewheel();
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    // vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(cfg::LOOP_INTERVAL_TEST_LONG));
  }
}
