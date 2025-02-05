#ifdef LOGGER_ENABLED
#include "Logger.hpp"
#include <cstdarg>
#include <cstring>
#include <iostream>

#ifdef __arm__
#include "usbd_cdc_if.h"
#endif
#ifdef __x86_64__
#include "mock_hal.h"
#endif

#ifdef LOGGER_OUTPUT_UART
UART_HandleTypeDef *Logger::huart_ = nullptr;
#endif
#ifdef LOGGER_OUTPUT_STDERR
std::ostream *Logger::stream_ = &std::cerr; // Initialize with std::cerr
#endif
#ifdef LOGGER_OUTPUT_CYPHAL
// CyphalTransferID Logger::transfer_id_ = 0;
#endif

void Logger::log(int level, const char *format, va_list args) // Added va_list arg
{
    if (level >= LOG_LEVEL)
    {
        char buffer[256]; // Adjust buffer size as needed
        int len = vsnprintf(buffer, sizeof(buffer), format, args);

        if (len > 0)
        {
#ifdef LOGGER_OUTPUT_UART
            uart_transmit_log_message(buffer, strlen(buffer));
#endif

#ifdef LOGGER_OUTPUT_USB
            usb_cdc_transmit_log_message(buffer, strlen(buffer));
#endif

#ifdef LOGGER_OUTPUT_CYPHAL
            can_transmit_log_message(buffer, strlen(buffer), level);
#endif

#ifdef LOGGER_OUTPUT_STDERR
            stream_transmit_log_message(buffer);
#endif
        }
    }
}

#ifdef LOGGER_OUTPUT_UART
void Logger::uart_transmit_log_message(const char *str, size_t size)
{
    HAL_UART_Transmit(huart_, (uint8_t *)str, size, 1000);
}
#endif

#ifdef LOGGER_OUTPUT_USB
void Logger::usb_cdc_transmit_log_message(const char *str, size_t size)
{
    CDC_Transmit_FS((uint8_t *)str, size);
}
#endif

#ifdef LOGGER_OUTPUT_CYPHAL
void Logger::can_transmit_log_message(const char *str, size_t size, int level) {}
#endif

#ifdef LOGGER_OUTPUT_STDERR
void Logger::stream_transmit_log_message(const char *str)
{
    *stream_ << str << std::endl;
}
#endif

void log(int level, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    Logger::log(level, format, args);
    va_end(args);
}
#endif /* LOGGER_ENABLED */
