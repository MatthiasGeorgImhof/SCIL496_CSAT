#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <cstring>
#include <memory>

#include "TaskMLX90640.hpp"
#include "RegistrationManager.hpp"

// ─────────────────────────────────────────────
// Mock MLX90640 driver
// ─────────────────────────────────────────────
struct MockMLX
{
    bool wakeUp_called = false;
    bool sleep_called = false;
    int isReady_calls = 0;
    int readSubpage_calls = 0;

    bool wakeUp()
    {
        wakeUp_called = true;
        return true;
    }

    bool sleep()
    {
        sleep_called = true;
        return true;
    }

    bool isReady()
    {
        isReady_calls++;
        return true;        // Always ready for now
    }

    bool readSubpage(uint16_t* buf, int& sp)
    {
        readSubpage_calls++;
        sp = (readSubpage_calls == 1 ? 0 : 1);
        buf[0] = 0xABCD;
        return true;
    }

    void createFrame(const uint16_t* a, const uint16_t* b, uint16_t* out)
    {
        out[0] = a[0];
        out[1] = b[0];
    }
};

// ─────────────────────────────────────────────
// Mock PowerSwitch
// ─────────────────────────────────────────────
struct MockPower
{
    bool on_called = false;
    bool off_called = false;

    bool on(CIRCUITS)
    {
        on_called = true;
        return true;
    }

    bool off(CIRCUITS)
    {
        off_called = true;
        return true;
    }
};

// ─────────────────────────────────────────────
// TESTS
// ─────────────────────────────────────────────

TEST_CASE("TaskMLX90640 basic state progression")
{
    RegistrationManager mgr;

    auto pwr = std::make_shared<MockPower>();
    auto mlx = std::make_shared<MockMLX>();

    // Pass boot_delay_ms = 0 to avoid depending on HAL_GetTick() advancing
    auto task = std::make_shared<TaskMLX90640<MockPower, MockMLX>>(
        0,                  // interval
        0,                  // tick
        *pwr, CIRCUITS::CIRCUIT_0,
        *mlx,
        0                   // boot_delay_ms
    );

    // Register task (no-op currently, but matches real usage)
    mgr.add(task);

    // Simulate scheduler calling handleTask repeatedly
    for (int i = 0; i < 20; i++)
        task->handleTask();

    CHECK(pwr->on_called == true);
    CHECK(mlx->wakeUp_called == true);
    CHECK(mlx->sleep_called == true);
    CHECK(pwr->off_called == true);

    CHECK(task->getState() == MLXState::Idle);
}

TEST_CASE("TaskMLX90640 OneShot mode produces exactly one frame")
{
    RegistrationManager mgr;
    auto pwr = std::make_shared<MockPower>();
    auto mlx = std::make_shared<MockMLX>();

    auto task = std::make_shared<TaskMLX90640<MockPower, MockMLX>>(
        0, 0, *pwr, CIRCUITS::CIRCUIT_0, *mlx, 
        0,                   // boot delay
        MLXMode::OneShot,    // mode
        1                    // burst count
    );

    mgr.add(task);

    for (int i = 0; i < 200; i++)
        task->handleTask();

    CHECK(mlx->readSubpage_calls == 2);   // exactly one frame
    CHECK(task->getState() == MLXState::Idle);
}

TEST_CASE("TaskMLX90640 Burst mode produces N frames")
{
    RegistrationManager mgr;
    auto pwr = std::make_shared<MockPower>();
    auto mlx = std::make_shared<MockMLX>();

    const int N = 3;

    auto task = std::make_shared<TaskMLX90640<MockPower, MockMLX>>(
        0, 0, *pwr, CIRCUITS::CIRCUIT_0, *mlx, 
        0,                   // boot delay
        MLXMode::Burst,      // mode
        N                    // burst count
    );

    mgr.add(task);

    for (int i = 0; i < 500; i++)
        task->handleTask();

    CHECK(mlx->readSubpage_calls == 2 * N);   // N frames = 2*N subpages
    CHECK(task->getState() == MLXState::Idle);
}
