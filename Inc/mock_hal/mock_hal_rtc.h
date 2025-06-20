#ifndef MOCK_HAL_RTC_H
#define MOCK_HAL_RTC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

// Include core definitions
#include "mock_hal/mock_hal_core.h"

#define RTC_WEEKDAY_MONDAY ((uint8_t)0x01)
#define RTC_WEEKDAY_TUESDAY ((uint8_t)0x02)
#define RTC_WEEKDAY_WEDNESDAY ((uint8_t)0x03)
#define RTC_WEEKDAY_THURSDAY ((uint8_t)0x04)
#define RTC_WEEKDAY_FRIDAY ((uint8_t)0x05)
#define RTC_WEEKDAY_SATURDAY ((uint8_t)0x06)
#define RTC_WEEKDAY_SUNDAY ((uint8_t)0x07)

#define RTC_HOURFORMAT_24 ((uint32_t)0x00000000)
#define RTC_HOURFORMAT_12 ((uint32_t)0x00000040)

#define RTC_HOURFORMAT12_AM ((uint8_t)0x00)
#define RTC_HOURFORMAT12_PM ((uint8_t)0x40)

#define RTC_DAYLIGHTSAVING_SUB1H ((uint32_t)0x00020000)
#define RTC_DAYLIGHTSAVING_ADD1H ((uint32_t)0x00010000)
#define RTC_DAYLIGHTSAVING_NONE ((uint32_t)0x00000000)

#define RTC_STOREOPERATION_RESET        0x00000000U
#define RTC_STOREOPERATION_SET          0x00040000U

    // STM32 HAL RTC definitions (same as before)
    typedef struct
    {
        uint8_t Hours;           /*!< Specifies the RTC Time Hour. */
        uint8_t Minutes;         /*!< Specifies the RTC Time Minutes. */
        uint8_t Seconds;         /*!< Specifies the RTC Time Seconds. */
        uint8_t TimeFormat;      /*!< Specifies the RTC AM/PM Time. */
        uint32_t SubSeconds;     /*!< Specifies the RTC_SSR RTC Sub Second register content. */
        uint32_t SecondFraction; /*!< Specifies the range or granularity of Sub Second register content */
        uint32_t DayLightSaving; /*!< Specifies DayLight Save Operation. */
        uint32_t StoreOperation; /*!< Specifies RTC_StoreOperation value to be written in the BCK bit */
    } RTC_TimeTypeDef;

    typedef struct
    {
        uint8_t WeekDay; /*!< Specifies the RTC Date WeekDay. */
        uint8_t Month;   /*!< Specifies the RTC Date Month (in BCD format). */
        uint8_t Date;    /*!< Specifies the RTC Date. */
        uint8_t Year;    /*!< Specifies the RTC Date Year. */
    } RTC_DateTypeDef;

    typedef struct
    {
        // Add members that your RTC_HandleTypeDef would normally have.  For this mock, we don't need much.
    } RTC_HandleTypeDef;

    //------------------------------------------------------------------------------
    //  MOCK RTC STATE VARIABLES
    //------------------------------------------------------------------------------

    // Variables to store the mocked RTC time and date (same as before)
    extern RTC_TimeTypeDef mocked_rtc_time;
    extern RTC_DateTypeDef mocked_rtc_date;

    // Variable to store the mocked return status of HAL_RTC_SetTime and HAL_RTC_SetDate (same as before)
    extern HAL_StatusTypeDef mocked_rtc_set_status;
    extern HAL_StatusTypeDef mocked_rtc_get_status;

    // Variable to store the mocked synchro shift value
    extern uint32_t mocked_synchro_shift;

    // Variable to store the mocked HAL Status for HAL_RTCEx_SetSynchroShift
    extern HAL_StatusTypeDef mocked_rtc_ex_set_synchro_shift_status;

    //------------------------------------------------------------------------------
    //  MOCK RTC HELPER FUNCTION PROTOTYPES (Getters and Setters)
    //------------------------------------------------------------------------------

    // Time and Date Setters (for setting the mocked values) (same as before)
    void set_mocked_rtc_time(RTC_TimeTypeDef time);
    void set_mocked_rtc_date(RTC_DateTypeDef date);

    // Setters for mocked HAL status for Set functions
    void set_mocked_rtc_set_status(HAL_StatusTypeDef status);
    // Setters for mocked HAL status for Get functions
    void set_mocked_rtc_get_status(HAL_StatusTypeDef status);

    // Setter for mocked synchro shift
    void set_mocked_synchro_shift(uint32_t shift);

    // Setter for mocked HAL Status for synchro shift set
    void set_mocked_rtc_ex_set_synchro_shift_status(HAL_StatusTypeDef status);

    // Time and Date Getters (for reading the mocked values) (same as before)
    RTC_TimeTypeDef get_mocked_rtc_time(void);
    RTC_DateTypeDef get_mocked_rtc_date(void);

    // Getter for mocked synchro shift
    uint32_t get_mocked_synchro_shift(void);

    // Clear the values
    void clear_mocked_rtc();

    //------------------------------------------------------------------------------
    //  MOCK HAL FUNCTION PROTOTYPES
    //------------------------------------------------------------------------------

    HAL_StatusTypeDef HAL_RTC_Init(RTC_HandleTypeDef *hrtc);
    HAL_StatusTypeDef HAL_RTC_DeInit(RTC_HandleTypeDef *hrtc);
    void HAL_RTC_MspInit(RTC_HandleTypeDef *hrtc);
    void HAL_RTC_MspDeInit(RTC_HandleTypeDef *hrtc);
    HAL_StatusTypeDef HAL_RTC_SetTime(RTC_HandleTypeDef *hrtc, RTC_TimeTypeDef *sTime, uint32_t Format);
    HAL_StatusTypeDef HAL_RTC_GetTime(RTC_HandleTypeDef *hrtc, RTC_TimeTypeDef *sTime, uint32_t Format);
    HAL_StatusTypeDef HAL_RTC_SetDate(RTC_HandleTypeDef *hrtc, RTC_DateTypeDef *sDate, uint32_t Format);
    HAL_StatusTypeDef HAL_RTC_GetDate(RTC_HandleTypeDef *hrtc, RTC_DateTypeDef *sDate, uint32_t Format);

    // Add the HAL_RTCEx_SetSynchroShift
    HAL_StatusTypeDef HAL_RTCEx_SetSynchroShift(RTC_HandleTypeDef *hrtc, uint32_t Shift);

    uint8_t RTC_ByteToBcd2(uint8_t Value);
    uint8_t RTC_Bcd2ToByte(uint8_t Value);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_HAL_RTC_H */