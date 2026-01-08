#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include "mock_hal.h"

#include "TaskMLX90640.hpp"
#include "RegistrationManager.hpp"


void advance_time_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        HAL_IncTick();
    }
}

// -----------------------------------------------------------------------------
// Mock MLX90640 driver
// -----------------------------------------------------------------------------
struct MockMLX
{
    bool wakeUp_called = false;
    bool sleep_called = false;
    int isReady_calls = 0;
    int readSubpage_calls = 0;

    bool wakeUp(MLX90640_RefreshRate = MLX90640_RefreshRate::Hz4)
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
        return true;    // Always ready for tests
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

    uint32_t getRefreshIntervalMs(MLX90640_RefreshRate)
    {
        return 0;   // Keep timing trivial for unit tests
    }
};

// -----------------------------------------------------------------------------
// Mock PowerSwitch
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// TESTS
// -----------------------------------------------------------------------------

TEST_CASE("TaskMLX90640 basic state progression")
{
    HAL_SetTick(0);

    RegistrationManager mgr;
    auto pwr = std::make_shared<MockPower>();
    auto mlx = std::make_shared<MockMLX>();

    auto task = std::make_shared<TaskMLX90640<MockPower, MockMLX>>(
        *pwr,
        CIRCUITS::CIRCUIT_0,
        *mlx,
        MLXMode::OneShot,
        1,      // burst count
        0, 0    // interval, tick
    );

    mgr.add(task);

    for (int i = 0; i < 200; i++) {
        advance_time_ms(1);
        task->handleTask();
    }

    CHECK(pwr->on_called == true);
    CHECK(mlx->wakeUp_called == true);
    CHECK(mlx->readSubpage_calls == 2);
    CHECK(mlx->sleep_called == true);
    CHECK(pwr->off_called == true);
    CHECK(task->getState() == MLXState::Idle);
}

TEST_CASE("TaskMLX90640 OneShot mode produces exactly one frame")
{
    HAL_SetTick(0);

    RegistrationManager mgr;
    auto pwr = std::make_shared<MockPower>();
    auto mlx = std::make_shared<MockMLX>();

    auto task = std::make_shared<TaskMLX90640<MockPower, MockMLX>>(
        *pwr,
        CIRCUITS::CIRCUIT_0,
        *mlx,
        MLXMode::OneShot,
        1,      // burst count
        0, 0
    );

    mgr.add(task);

    for (int i = 0; i < 300; i++) {
        advance_time_ms(1);
        task->handleTask();
    }

    CHECK(mlx->readSubpage_calls == 2);   // exactly one frame
    CHECK(task->getState() == MLXState::Idle);
}

TEST_CASE("TaskMLX90640 Burst mode produces N frames")
{
    HAL_SetTick(0);

    RegistrationManager mgr;
    auto pwr = std::make_shared<MockPower>();
    auto mlx = std::make_shared<MockMLX>();

    const int N = 3;

    auto task = std::make_shared<TaskMLX90640<MockPower, MockMLX>>(
        *pwr,
        CIRCUITS::CIRCUIT_0,
        *mlx,
        MLXMode::Burst,
        N,      // burst count
        0, 0
    );

    mgr.add(task);

    for (int i = 0; i < 1000; i++) {
        advance_time_ms(1);
        task->handleTask();
    }

    CHECK(mlx->readSubpage_calls == 2 * N);
    CHECK(task->getState() == MLXState::Idle);
}
