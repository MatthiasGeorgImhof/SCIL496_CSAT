#ifndef INC_TASKMLX90640_HPP_
#define INC_TASKMLX90640_HPP_

#include <cstdint>
#include <memory>

#include "Logger.hpp"
#include "Task.hpp"
#include "MLX90640.hpp"
#include "PowerSwitch.hpp"
#include "RegistrationManager.hpp"

// ─────────────────────────────────────────────
// MLX90640 Task State Machine
// ─────────────────────────────────────────────
enum class MLXState : uint8_t
{
    Off = 0,      // Sensor unpowered, waiting for trigger
    PoweringOn,   // PowerSwitch.on() has been called
    BootDelay,    // Waiting for MLX90640 internal boot time
    Initializing, // wakeUp(), chess mode, refresh rate, etc.

    WaitCompleteFrame, // Wait a full refresh cycle (both subpages)
    WaitForReadyA,     // Waiting for NEW_DATA for subpage A
    ReadSubpageA,      // Reading subpage A

    WaitForReadyB, // Waiting for NEW_DATA for subpage B
    ReadSubpageB,  // Reading subpage B

    FrameComplete, // Both subpages acquired, frame ready

    ShuttingDown, // Putting MLX90640 into sleep mode
    PoweringOff,  // PowerSwitch.off() has been called

    Idle // Task finished, waiting for next trigger
};

// ─────────────────────────────────────────────
// MLX90640 Acquisition Modes
// ─────────────────────────────────────────────
enum class MLXMode : uint8_t
{
    OneShot,   // Acquire exactly one frame
    Burst,     // Acquire N frames
    Continuous // Acquire frames indefinitely (not implemented yet)
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
    TaskMLX90640(PowerSwitchT &pwr, CIRCUITS circuit,
                 MLXT &mlx, MLXMode mode, uint32_t burstCount,
                 uint32_t interval, uint32_t tick)
        : Task(interval, tick),
          power(pwr),
          circuit(circuit),
          sensor(mlx),
          t0(0),
          state(MLXState::Off),
          mode(mode),
          burstRemaining(burstCount),
          spA(-1),
          spB(-1)
    {
    }

    virtual ~TaskMLX90640() = default;

    // Required by Task base class; no Cyphal registration for this task (yet).
    void registerTask(RegistrationManager *, std::shared_ptr<Task>) override {}
    void unregisterTask(RegistrationManager *, std::shared_ptr<Task>) override {}

    MLXState getState() const { return state; }
    MLXMode getMode() const { return mode; }
    uint32_t getBurstRemaining() const { return burstRemaining; }

protected:
    void handleTaskImpl() override
    {
        switch (state)
        {
        case MLXState::Off:
            stateOff();
            break;

        case MLXState::PoweringOn:
            statePoweringOn();
            break;

        case MLXState::BootDelay:
            stateBootDelay();
            break;

        case MLXState::Initializing:
            stateInitialize();
            break;

        case MLXState::WaitCompleteFrame:
            stateWaitCompleteFrame();
            break;

        case MLXState::WaitForReadyA:
            stateWaitingForReadyA();
            break;

        case MLXState::ReadSubpageA:
            stateReadSubpageA();
            break;

        case MLXState::WaitForReadyB:
            stateWaitForReadyB();
            break;

        case MLXState::ReadSubpageB:
            stateReadSubpageB();
            break;

        case MLXState::FrameComplete:
            stateFrameComplete();
            break;

        case MLXState::ShuttingDown:
            stateShuttingDown();
            break;

        case MLXState::PoweringOff:
            statePoweringOff();
            break;

        case MLXState::Idle:
            stateIdle();
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

    void stateOff()
    {
        if (shouldStart())
        {
            if (!power.on(circuit))
            {
                log(LOG_LEVEL_ERROR, "TaskMLX90640: power.on() failed\r\n");
                state = MLXState::Idle;
                return;
            }
            t0 = HAL_GetTick();
            state = MLXState::PoweringOn;
        }
    }

    void statePoweringOn()
    {
        state = MLXState::BootDelay;
    }

    void stateBootDelay()
    {
        if (HAL_GetTick() - t0 >= TASK_BOOT_DELAY_MS)
        {
            state = MLXState::Initializing;
        }
    }

    void stateInitialize()
    {
        if (sensor.wakeUp(REFRESH_RATE))
        {
            state = MLXState::WaitCompleteFrame;
            t0 = HAL_GetTick();
        }
        else
        {
            log(LOG_LEVEL_ERROR, "TaskMLX90640: wakeUp() failed\r\n");
            state = MLXState::ShuttingDown;
        }
    }

    void stateWaitCompleteFrame()
    {
        if ((HAL_GetTick() - t0) >= sensor.getRefreshIntervalMs(REFRESH_RATE))
        {
            state = MLXState::ReadSubpageA;
        }
    }

    void stateWaitingForReadyA()
    {
        if (((HAL_GetTick() - t0) >= (sensor.getRefreshIntervalMs(REFRESH_RATE) * 8 / 10)) && sensor.isReady())
        {
            state = MLXState::ReadSubpageA;
        }
    }

    void stateReadSubpageA()
    {
        if (sensor.readSubpage(subA, spA))
        {
            state = MLXState::WaitForReadyB;
            t0 = HAL_GetTick();
        }
        else
        {
            log(LOG_LEVEL_ERROR, "TaskMLX90640: readSubpage A failed\r\n");
            state = MLXState::ShuttingDown;
        }
    }

    void stateWaitForReadyB()
    {
        if (((HAL_GetTick() - t0) >= (sensor.getRefreshIntervalMs(REFRESH_RATE) * 8 / 10)) && sensor.isReady())
        {
            state = MLXState::ReadSubpageB;
        }
    }

    void stateReadSubpageB()
    {
        if (sensor.readSubpage(subB, spB))
        {
            state = MLXState::FrameComplete;
            t0 = HAL_GetTick();
        }
        else
        {
            log(LOG_LEVEL_ERROR, "TaskMLX90640: readSubpage B failed\r\n");
            state = MLXState::ShuttingDown;
        }
    }

    void stateFrameComplete()
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
            {
                state = MLXState::ShuttingDown;
            }
            else
            {
                state = MLXState::WaitForReadyA; // Next frame
            }
        }
        else // Continuous
        {
            state = MLXState::WaitForReadyA;
        }
    }

    void stateShuttingDown()
    {
        sensor.sleep();
        state = MLXState::PoweringOff;
    }

    void statePoweringOff()
    {
        power.off(circuit);
        state = MLXState::Idle;
    }

    void stateIdle()
    {
        // Stay idle until triggered.
    }

private:
    PowerSwitchT &power;
    CIRCUITS circuit;
    MLXT &sensor;

    uint32_t t0;

    MLXState state;
    MLXMode mode;
    uint32_t burstRemaining;

    uint16_t subA[MLX90640_SUBPAGE_WORDS];
    uint16_t subB[MLX90640_SUBPAGE_WORDS];
    uint16_t frame[MLX90640_FRAME_WORDS];
    int spA;
    int spB;

    constexpr static MLX90640_RefreshRate REFRESH_RATE = MLX90640_RefreshRate::Hz4;
    constexpr static uint32_t TASK_BOOT_DELAY_MS = MLX90640_BOOT_TIME_MS;
};

#endif /* INC_TASKMLX90640_HPP_ */
