#ifndef INC_TASKMLX90640_HPP_
#define INC_TASKMLX90640_HPP_

#include <cstdint>
#include <memory>

#include "Task.hpp"
#include "MLX90640.hpp"
#include "PowerSwitch.hpp"
#include "RegistrationManager.hpp"

// ─────────────────────────────────────────────
// MLX90640 Task State Machine
// ─────────────────────────────────────────────
enum class MLXState : uint8_t
{
    Off = 0,        // Sensor unpowered, waiting for trigger
    PoweringOn,     // PowerSwitch.on() has been called
    BootDelay,      // Waiting for MLX90640 internal boot time
    Initializing,   // wakeUp(), chess mode, refresh rate, etc.

    WaitForReadyA,  // Waiting for NEW_DATA for subpage A
    ReadSubpageA,   // Reading subpage A

    WaitForReadyB,  // Waiting for NEW_DATA for subpage B
    ReadSubpageB,   // Reading subpage B

    FrameComplete,  // Both subpages acquired, frame ready

    ShuttingDown,   // Putting MLX90640 into sleep mode
    PoweringOff,    // PowerSwitch.off() has been called

    Idle            // Task finished, waiting for next trigger
};

// ─────────────────────────────────────────────
// MLX90640 Acquisition Modes
// ─────────────────────────────────────────────
enum class MLXMode : uint8_t
{
    OneShot,        // Acquire exactly one frame
    Burst,          // Acquire N frames
    Continuous      // Acquire frames indefinitely (not implemented yet)
};

// ─────────────────────────────────────────────
// TaskMLX90640
// ─────────────────────────────────────────────
//
// This task owns the lifecycle of the MLX90640:
//  - Controls power via PowerSwitch
//  - Sequences wake-up and acquisition via MLX90640 driver
//  - Runs as a cooperative state machine inside Task::handleTask()
//
// Publishing, triggers, and error/timeout handling can be layered on later.
//
template <typename PowerSwitchT, typename MLXT>
class TaskMLX90640 : public Task
{
public:
    TaskMLX90640(uint32_t interval, uint32_t tick,
                 PowerSwitchT& pwr, CIRCUITS circuit,
                 MLXT& mlx, uint32_t boot_delay_ms = 50, MLXMode mode = MLXMode::OneShot, uint32_t burstCount = 1)
        : Task(interval, tick),
          power(pwr),
          sensor(mlx),
          circuit(circuit),
          bootDelay(boot_delay_ms),
          t0(0),
          state(MLXState::Off),
          mode(mode),
          burstRemaining(burstCount),
          spA(-1),
          spB(-1)
    {}

    virtual ~TaskMLX90640() = default;

    // Required by Task base class; no Cyphal registration for this task (yet).
    void registerTask(RegistrationManager*, std::shared_ptr<Task>) override {}
    void unregisterTask(RegistrationManager*, std::shared_ptr<Task>) override {}

    MLXState getState() const { return state; }
    MLXMode getMode() const { return mode; }
    uint32_t getBurstRemaining() const { return burstRemaining; }

protected:
    void handleTaskImpl() override
    {
        switch (state)
        {
            case MLXState::Off:
                if (shouldStart())
                {
                    if (!power.on(circuit))
                    {
                        log(LOG_LEVEL_ERROR, "TaskMLX90640: power.on() failed\r\n");
                        state = MLXState::Idle;
                        break;
                    }
                    t0 = HAL_GetTick();
                    state = MLXState::PoweringOn;
                }
                break;

            case MLXState::PoweringOn:
                state = MLXState::BootDelay;
                break;

            case MLXState::BootDelay:
                if (HAL_GetTick() - t0 >= bootDelay)
                    state = MLXState::Initializing;
                break;

            case MLXState::Initializing:
                if (sensor.wakeUp())
                    state = MLXState::WaitForReadyA;
                else
                {
                    log(LOG_LEVEL_ERROR, "TaskMLX90640: wakeUp() failed\r\n");
                    state = MLXState::ShuttingDown;
                }
                break;

            case MLXState::WaitForReadyA:
                if (sensor.isReady())
                    state = MLXState::ReadSubpageA;
                break;

            case MLXState::ReadSubpageA:
                if (sensor.readSubpage(subA, spA))
                    state = MLXState::WaitForReadyB;
                else
                {
                    log(LOG_LEVEL_ERROR, "TaskMLX90640: readSubpage A failed\r\n");
                    state = MLXState::ShuttingDown;
                }
                break;

            case MLXState::WaitForReadyB:
                if (sensor.isReady())
                    state = MLXState::ReadSubpageB;
                break;

            case MLXState::ReadSubpageB:
                if (sensor.readSubpage(subB, spB))
                    state = MLXState::FrameComplete;
                else
                {
                    log(LOG_LEVEL_ERROR, "TaskMLX90640: readSubpage B failed\r\n");
                    state = MLXState::ShuttingDown;
                }
                break;

            case MLXState::FrameComplete:
            {
                if (spA != spB && spA >= 0 && spB >= 0)
                {
                    if (spA == 0)
                        sensor.createFrame(subA, subB, frame);
                    else
                        sensor.createFrame(subB, subA, frame);

                    publishFrame();
                }
                else
                {
                    log(LOG_LEVEL_WARNING,
                        "TaskMLX90640: invalid subpage pair spA=%d spB=%d\r\n",
                        spA, spB);
                }

                // ─────────────────────────────────────────────
                // Mode logic
                // ─────────────────────────────────────────────
                if (mode == MLXMode::OneShot)
                {
                    state = MLXState::ShuttingDown;
                }
                else if (mode == MLXMode::Burst)
                {
                    if (burstRemaining > 0)
                        burstRemaining--;

                    if (burstRemaining == 0)
                        state = MLXState::ShuttingDown;
                    else
                        state = MLXState::WaitForReadyA;  // Next frame
                }
                else // Continuous
                {
                    state = MLXState::WaitForReadyA;
                }
            }
            break;

            case MLXState::ShuttingDown:
                sensor.sleep();
                state = MLXState::PoweringOff;
                break;

            case MLXState::PoweringOff:
                power.off(circuit);
                state = MLXState::Idle;
                break;

            case MLXState::Idle:
                // Waiting for next trigger
                break;
        }
    }

private:
    // For now, always start when in Off or Idle.
    // Later, this will be replaced by Trigger logic.
    bool shouldStart()
    {
        if (state != MLXState::Off && state != MLXState::Idle)
            return false;

        if (mode == MLXMode::OneShot)
            return true;

        if (mode == MLXMode::Burst)
            return burstRemaining > 0;

        return true; // Continuous
    }

    // Placeholder: integrate with Cyphal or other publication mechanism later.
    void publishFrame()
    {
        // No-op for now.
    }

private:
    PowerSwitchT& power;
    MLXT& sensor;
    CIRCUITS circuit;

    uint32_t bootDelay;
    uint32_t t0;

    MLXState state;
    MLXMode mode;
    uint32_t burstRemaining;

    uint16_t subA[MLX90640_SUBPAGE_WORDS];
    uint16_t subB[MLX90640_SUBPAGE_WORDS];
    uint16_t frame[MLX90640_FRAME_WORDS];
    int spA;
    int spB;
};

#endif /* INC_TASKMLX90640_HPP_ */
