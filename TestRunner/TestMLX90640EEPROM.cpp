#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "MLX90640EEPROM.h"

// -----------------------------------------------------------------------------
// TEST CASE
// -----------------------------------------------------------------------------

// TEST_CASE("MLX90640 EEPROM checksum validation")
// {
//     constexpr size_t WORD_COUNT = sizeof(MLX90640_EEPROM) / sizeof(uint16_t);

//     CHECK(WORD_COUNT == 832);

//     bool valid = mlx90640_validate_eeprom(MLX90640_EEPROM, WORD_COUNT);

//     CHECK(valid == true);

//     // Optional: also check the exact checksum value
//     constexpr size_t CHECKSUM_INDEX = 0x32;
//     uint16_t stored   = MLX90640_EEPROM[CHECKSUM_INDEX];
//     uint16_t computed = mlx90640_compute_checksum(MLX90640_EEPROM, WORD_COUNT);

//     CHECK(stored == computed);
// }

TEST_CASE("MLX90640 EEPROM defined registers")
{
    printf("0x240C: 0x%04X\n", MLX90640_EEPROM[0x0C]);
    printf("0x240D: 0x%04X\n", MLX90640_EEPROM[0x0D]);
    printf("0x240E: 0x%04X\n", MLX90640_EEPROM[0x0E]);
    printf("0x240F: 0x%04X\n", MLX90640_EEPROM[0x0F]);
    printf("0x2410: 0x%04X\n", MLX90640_EEPROM[0x10]);
}