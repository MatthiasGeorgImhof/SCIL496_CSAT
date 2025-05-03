//#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "mock_hal.h"
#include <string.h>
#include <stdio.h>


TEST_CASE("HAL_Delay")
{
    set_current_tick(0);
    HAL_Delay(100);
    CHECK(HAL_GetTick() == 100);
    HAL_Delay(200);
    CHECK(HAL_GetTick() == 300);
}

TEST_CASE("HAL_GetTick")
{
    set_current_tick(10);
    CHECK(HAL_GetTick() == 10);
    set_current_tick(20);
    CHECK(HAL_GetTick() == 20);
}
