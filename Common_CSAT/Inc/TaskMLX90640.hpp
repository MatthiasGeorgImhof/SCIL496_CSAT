#ifndef INC_TASKMLX90640_HPP_
#define INC_TASKMLX90640_HPP_

#include <cstdint>
#include <memory>

#include "Logger.hpp"
#include "Task.hpp"
#include "MLX90640.hpp"
#include "PowerSwitch.hpp"
#include "RegistrationManager.hpp"

extern bool toHexAsciiWords(const uint8_t* data,
                     size_t dataSize,
                     char* out,
                     size_t outSize,
                     size_t wordsPerLine);


// ─────────────────────────────────────────────
// MLX90640 Task State Machine
// ─────────────────────────────────────────────
enum class MLXState : uint8_t
{
    Off = 0,
    PoweringOn,
    BootDelay,
    Initializing,

    WaitCompleteFrame,
    WaitForReadyA,
    ReadSubpageA,

    WaitForReadyB,
    ReadSubpageB,

    FrameComplete,

    ShuttingDown,
    PoweringOff,

    Idle
};

// ─────────────────────────────────────────────
// MLX90640 Acquisition Modes
// ─────────────────────────────────────────────
enum class MLXMode : uint8_t
{
    OneShot,
    Burst,
    Continuous
};

// ─────────────────────────────────────────────
// TaskMLX90640
// ─────────────────────────────────────────────
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

    void registerTask(RegistrationManager *manager, std::shared_ptr<Task> task)
    {
        manager->subscribe(PURE_HANDLER, task);
    }

    void unregisterTask(RegistrationManager *manager, std::shared_ptr<Task> task)
    {
        manager->unsubscribe(PURE_HANDLER, task);
    }

    MLXState getState() const { return state; }
    MLXMode getMode() const { return mode; }
    uint32_t getBurstRemaining() const { return burstRemaining; }

protected:
    void handleTaskImpl() override
    {
        switch (state)
        {
        case MLXState::Off:              stateOff(); break;
        case MLXState::PoweringOn:       statePoweringOn(); break;
        case MLXState::BootDelay:        stateBootDelay(); break;
        case MLXState::Initializing:     stateInitialize(); break;
        case MLXState::WaitCompleteFrame:stateWaitCompleteFrame(); break;
        case MLXState::WaitForReadyA:    stateWaitingForReadyA(); break;
        case MLXState::ReadSubpageA:     stateReadSubpageA(); break;
        case MLXState::WaitForReadyB:    stateWaitForReadyB(); break;
        case MLXState::ReadSubpageB:     stateReadSubpageB(); break;
        case MLXState::FrameComplete:    stateFrameComplete(); break;
        case MLXState::ShuttingDown:     stateShuttingDown(); break;
        case MLXState::PoweringOff:      statePoweringOff(); break;
        case MLXState::Idle:             stateIdle(); break;
        }
    }

private:
    bool shouldStart()
    {
        if (state != MLXState::Off && state != MLXState::Idle)
            return false;

        if (mode == MLXMode::OneShot)
            return true;

        if (mode == MLXMode::Burst)
            return burstRemaining > 0;

        return true;
    }

    void publishFrame()
	{
//    				char buffer[10*MLX90640_SUBPAGE_SIZE];
//    				toHexAsciiWords(reinterpret_cast<uint8_t*>(frame), sizeof(frame), buffer, sizeof(buffer), 32);
//    				CDC_Transmit_FS((uint8_t*) buffer, strlen(buffer));
	}

    // ─────────────────────────────────────────────
    // STATE IMPLEMENTATIONS
    // ─────────────────────────────────────────────

    void stateOff()
    {
        if (!shouldStart())
            return;

        if (!power.on(circuit))
        {
            state = MLXState::Idle;
            log(LOG_LEVEL_ERROR, "TaskMLX90640::stateOff power.on() failed\r\n");
            return;
        }

        t0 = HAL_GetTick();
        state = MLXState::PoweringOn;
        log(LOG_LEVEL_DEBUG, "TaskMLX90640::stateOff: Off -> PoweringOn\r\n");
    }

    void statePoweringOn()
    {
        state = MLXState::BootDelay;
        log(LOG_LEVEL_DEBUG, "TaskMLX90640::statePoweringOn: PoweringOn -> BootDelay\r\n");
    }

    void stateBootDelay()
    {
        if (HAL_GetTick() - t0 >= TASK_BOOT_DELAY_MS)
        {
            state = MLXState::Initializing;
            log(LOG_LEVEL_DEBUG, "TaskMLX90640::stateBootDelay: BootDelay -> Initializing\r\n");
        }
    }

    void stateInitialize()
    {
        if (sensor.wakeUp(REFRESH_RATE))
        {
            state = MLXState::WaitCompleteFrame;
            t0 = HAL_GetTick();
            log(LOG_LEVEL_DEBUG, "TaskMLX90640::stateInitialize: Initializing -> WaitCompleteFrame\r\n");
        }
        else
        {
            state = MLXState::ShuttingDown;
            log(LOG_LEVEL_ERROR, "TaskMLX90640::stateInitialize wakeUp() failed\r\n");
        }
    }

    void stateWaitCompleteFrame()
    {
        if ((HAL_GetTick() - t0) >= sensor.getRefreshIntervalMs(REFRESH_RATE))
        {
            state = MLXState::ReadSubpageA;
            log(LOG_LEVEL_DEBUG, "TaskMLX90640::stateWaitCompleteFrame: WaitCompleteFrame -> ReadSubpageA\r\n");
        }
    }

    void stateWaitingForReadyA()
    {
        if (((HAL_GetTick() - t0) >= (sensor.getRefreshIntervalMs(REFRESH_RATE) * 8 / 10))
            && sensor.isReady())
        {
            state = MLXState::ReadSubpageA;
            log(LOG_LEVEL_DEBUG, "TaskMLX90640::stateWaitingForReadyA: WaitForReadyA -> ReadSubpageA\r\n");
        }
    }

    void stateReadSubpageA()
    {
        if (sensor.readSubpage(subA, spA))
        {
            state = MLXState::WaitForReadyB;
            t0 = HAL_GetTick();
            log(LOG_LEVEL_DEBUG, "TaskMLX90640::stateReadSubpageA: ReadSubpageA -> WaitForReadyB\r\n");
        }
        else
        {
            state = MLXState::ShuttingDown;
            log(LOG_LEVEL_ERROR, "TaskMLX90640::stateReadSubpageA failed\r\n");
        }
    }

    // ─────────────────────────────────────────────
    // FIXED VERSION: STATUS‑BASED SUBPAGE‑FLIP CHECK
    // ─────────────────────────────────────────────
    void stateWaitForReadyB()
    {
        const auto elapsed = HAL_GetTick() - t0;
        const auto min_wait = sensor.getRefreshIntervalMs(REFRESH_RATE) * 8 / 10;

        if (elapsed < min_wait)
            return;

        uint16_t status = 0;
        if (!sensor.readStatus(status))
            return;

        bool ready = (status & 0x0008u) != 0u;
        int sub = static_cast<int>(status & 0x0001u);

        if (ready && sub != spA)
        {
            state = MLXState::ReadSubpageB;
            log(LOG_LEVEL_DEBUG,
                "TaskMLX90640::stateWaitForReadyB: WaitForReadyB -> ReadSubpageB (sub=%d, spA=%d)\r\n",
                sub, spA);
        }
    }

    void stateReadSubpageB()
    {
        if (sensor.readSubpage(subB, spB))
        {
            state = MLXState::FrameComplete;
            t0 = HAL_GetTick();
            log(LOG_LEVEL_DEBUG, "TaskMLX90640::stateReadSubpageB: ReadSubpageB -> FrameComplete\r\n");
        }
        else
        {
            state = MLXState::ShuttingDown;
            log(LOG_LEVEL_ERROR, "TaskMLX90640::stateReadSubpageB failed\r\n");
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
                "TaskMLX90640::stateFrameComplete invalid subpage pair spA=%d spB=%d\r\n",
                spA, spB);
        }

        if (mode == MLXMode::OneShot)
        {
            state = MLXState::ShuttingDown;
        }
        else if (mode == MLXMode::Burst)
        {
            if (burstRemaining > 0)
                burstRemaining--;

            state = (burstRemaining == 0)
                        ? MLXState::ShuttingDown
                        : MLXState::WaitForReadyA;
        }
        else
        {
            state = MLXState::WaitForReadyA;
        }
    }

    void stateShuttingDown()
    {
        sensor.sleep();
        state = MLXState::PoweringOff;
        log(LOG_LEVEL_DEBUG, "TaskMLX90640::stateShuttingDown: ShuttingDown -> PoweringOff\r\n");
    }

    void statePoweringOff()
    {
        power.off(circuit);
        state = MLXState::Idle;
        log(LOG_LEVEL_DEBUG, "TaskMLX90640::statePoweringOff: PoweringOff -> Idle\r\n");
    }

    void stateIdle()
    {
        log(LOG_LEVEL_DEBUG, "TaskMLX90640::stateIdle\r\n");
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
