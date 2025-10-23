#ifndef __TRANSPORT_HPP__
#define __TRANSPORT_HPP__

#ifdef __arm__
#include "stm32l4xx_hal.h"
#include "usb_device.h"
#elif __x86_64__
#include "mock_hal.h"
#endif

#include "transport_config.h"

#include <cstdint>
#include <cstddef>
#include <concepts>
#include <type_traits>

// Mode tags
struct register_mode_tag
{
};
struct stream_mode_tag
{
};

// Transport tags
struct i2c_tag
{
};
struct spi_tag
{
};
struct uart_tag
{
};


// ─────────────────────────────────────────────
// I2C Transport (Register Mode)
// ─────────────────────────────────────────────

#ifdef HAS_I2C_HANDLE_TYPEDEF

template <I2C_HandleTypeDef &HandleRef, uint16_t Address, uint32_t Timeout = 100>
struct I2C_Config
{
    using transport_tag = i2c_tag;
    using mode_tag = register_mode_tag;

    static I2C_HandleTypeDef &handle() { return HandleRef; }
    static constexpr uint16_t address = Address << 1;
    static constexpr uint32_t timeout = Timeout;

    static_assert(std::is_same_v<decltype(HandleRef), I2C_HandleTypeDef &>, "Handle must be I2C_HandleTypeDef&");
    static_assert(Address <= 0x7F, "I2C address must be 7-bit");
    static_assert(Timeout > 0 && Timeout < 10000, "Timeout must be a reasonable value");
};

template <typename Config>
    requires std::is_same_v<typename Config::transport_tag, i2c_tag>
class I2CTransport
{
public:
    using config_type = Config;

    bool write(const uint8_t *tx_buf, uint16_t tx_len) const
    {
        return HAL_I2C_Master_Transmit(&Config::handle(), Config::address, const_cast<uint8_t *>(tx_buf), tx_len, Config::timeout) == HAL_OK;
    }

    bool write_then_read(const uint8_t *tx_buf, uint16_t tx_len, uint8_t *rx_buf, uint16_t rx_len) const
    {
        return HAL_I2C_Master_Transmit(&Config::handle(), Config::address, const_cast<uint8_t *>(tx_buf), tx_len, Config::timeout) == HAL_OK &&
               HAL_I2C_Master_Receive(&Config::handle(), Config::address, rx_buf, rx_len, Config::timeout) == HAL_OK;
    }
};

#endif // HAS_I2C_HANDLE_TYPEDEF

// ─────────────────────────────────────────────
// SPI Transport (Register Mode)
// ─────────────────────────────────────────────

#ifdef HAS_SPI_HANDLE_TYPEDEF

template <SPI_HandleTypeDef &HandleRef, uint16_t Pin,
          std::size_t MaxTransferSize, uint32_t Timeout = 100>
struct SPI_Config
{
    SPI_Config() = delete;
    SPI_Config(GPIO_TypeDef *csport) : csPort(csport) {}
    using transport_tag = spi_tag;
    using mode_tag = register_mode_tag;

    static SPI_HandleTypeDef &handle() { return HandleRef; }
    static constexpr uint16_t csPin = Pin;
    static constexpr uint32_t timeout = Timeout;
    static constexpr std::size_t max_transfer_size = MaxTransferSize;

public:
    GPIO_TypeDef *csPort;

    static_assert(std::is_same_v<decltype(HandleRef), SPI_HandleTypeDef &>, "Handle must be SPI_HandleTypeDef&");
    static_assert(Timeout > 0 && Timeout < 10000, "Timeout must be a reasonable value");
    static_assert(MaxTransferSize > 0 && MaxTransferSize <= 1024, "MaxTransferSize must be reasonable");
};

template <typename Config>
    requires std::is_same_v<typename Config::transport_tag, spi_tag>
class SPITransport
{
public:
    using config_type = Config;

    explicit SPITransport(const Config &cfg) : config(cfg) { deselect(); }

    bool write(const uint8_t *tx_buf, uint16_t tx_len) const
    {
        select();
        bool ok = HAL_SPI_Transmit(&Config::handle(), const_cast<uint8_t *>(tx_buf), tx_len, Config::timeout) == HAL_OK;
        deselect();
        return ok;
    }

    bool write_then_read(const uint8_t *tx_buf, uint16_t tx_len, uint8_t *rx_buf, uint16_t rx_len) const
    {
        select();
        bool ok = HAL_SPI_Transmit(&Config::handle(), const_cast<uint8_t *>(tx_buf), tx_len, Config::timeout) == HAL_OK;
        if (ok)
        {
            ok = HAL_SPI_TransmitReceive(&Config::handle(), rx_buf, rx_buf, rx_len, Config::timeout) == HAL_OK;
        }
        deselect();
        return ok;
    }

private:
    Config config;

    void select() const { HAL_GPIO_WritePin(config.csPort, Config::csPin, GPIO_PIN_RESET); }
    void deselect() const { HAL_GPIO_WritePin(config.csPort, Config::csPin, GPIO_PIN_SET); }
};

#endif // HAS_SPI_HANDLE_TYPEDEF
// ─────────────────────────────────────────────
// UART Transport (Stream Mode)
// ─────────────────────────────────────────────

#ifdef HAS_UART_HANDLE_TYPEDEF

template <UART_HandleTypeDef &HandleRef, uint32_t Timeout = 100>
struct UART_Config
{
    using transport_tag = uart_tag;
    using mode_tag = stream_mode_tag;

    static UART_HandleTypeDef &handle() { return HandleRef; }
    static constexpr uint32_t timeout = Timeout;

    static_assert(std::is_same_v<decltype(HandleRef), UART_HandleTypeDef &>, "Handle must be UART_HandleTypeDef&");
    static_assert(Timeout > 0 && Timeout < 10000, "Timeout must be a reasonable value");
};

template <typename Config>
    requires std::is_same_v<typename Config::transport_tag, uart_tag>
class UARTTransport
{
public:
    using config_type = Config;

    bool send(const uint8_t *buf, uint16_t len) const
    {
        return HAL_UART_Transmit(&Config::handle(), const_cast<uint8_t *>(buf), len, Config::timeout) == HAL_OK;
    }

    bool receive(uint8_t *buf, uint16_t len) const
    {
        return HAL_UART_Receive(&Config::handle(), buf, len, Config::timeout) == HAL_OK;
    }
};

#endif // HAS_UART_HANDLE_TYPEDEF

// ─────────────────────────────────────────────
// Transport Concepts
// ─────────────────────────────────────────────

template <typename T>
concept RegisterModeTransport = std::is_same_v<typename T::config_type::mode_tag, register_mode_tag>;

template <typename T>
concept StreamModeTransport = std::is_same_v<typename T::config_type::mode_tag, stream_mode_tag>;

template <typename T>
concept RegisterWriteTransport = requires(T t, uint8_t *buf, uint16_t len) {
    { t.write(buf, len) } -> std::same_as<bool>;
};

template <typename T>
concept RegisterReadTransport = requires(T t, uint8_t *tx, uint16_t tx_len, uint8_t *rx, uint16_t rx_len) {
    { t.write_then_read(tx, tx_len, rx, rx_len) } -> std::same_as<bool>;
};

template <typename T>
concept StreamTransport = requires(T t, uint8_t *buf, uint16_t len) {
    { t.send(buf, len) } -> std::same_as<bool>;
    { t.receive(buf, len) } -> std::same_as<bool>;
};

template <typename T>
concept TransportProtocol =
    RegisterWriteTransport<T> || RegisterReadTransport<T> || StreamTransport<T>;

// ─────────────────────────────────────────────
// Transport Kind Traits
// ─────────────────────────────────────────────

enum class TransportKind
{
    I2C,
    SPI,
    UART
};

template <typename T>
struct TransportTraits;

#ifdef HAS_I2C_HANDLE_TYPEDEF
template <typename Config>
struct TransportTraits<I2CTransport<Config>>
{
    static constexpr TransportKind kind = TransportKind::I2C;
};
#endif // HAS_I2C_HANDLE_TYPEDEF

#ifdef HAS_SPI_HANDLE_TYPEDEF
template <typename Config>
struct TransportTraits<SPITransport<Config>>
{
    static constexpr TransportKind kind = TransportKind::SPI;
};
#endif // HAS_SPI_HANDLE_TYPEDEF

#ifdef HAS_UART_HANDLE_TYPEDEF
template <typename Config>
struct TransportTraits<UARTTransport<Config>>
{
    static constexpr TransportKind kind = TransportKind::UART;
};
#endif // HAS_UART_HANDLE_TYPEDEF

#endif // __TRANSPORT_HPP__
