#ifndef _CAMERA_SWITCH_HPP_
#define _CAMERA_SWITCH_HPP_

#include "I2CSwitch.hpp"
#include "Transport.hpp"
#include <array>

// --- FIX: define ResetPinShim BEFORE using it in the base class ---
struct ResetPinShim
{
    GPIO_TypeDef* port;
    uint16_t pin;

    ResetPinShim(GPIO_TypeDef* p, uint16_t n) : port(p), pin(n) {}

    void high() const { HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET); }
    void low()  const { HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET); }
};

template <typename Transport>
    requires StreamAccessTransport<Transport>
class CameraSwitch : public I2CSwitch<Transport, ResetPinShim>
{
public:
    CameraSwitch() = delete;

    CameraSwitch(const Transport& transport,
                 GPIO_TypeDef* resetPort, uint16_t resetPin,
                 GPIO_TypeDef* channelPort,
                 std::array<uint16_t, 4> channelPins)
        : I2CSwitch<Transport, ResetPinShim>(transport, ResetPinShim{resetPort, resetPin}),
          channel_port_(channelPort),
          channel_pins_(channelPins)
    {
        disableAllChannels();
    }

    bool select(I2CSwitchChannel channel)
    {
        if (!I2CSwitch<Transport, ResetPinShim>::select(channel))
            return false;

        pullUpChannelPin(channel);
        return true;
    }

    bool disableAll()
    {
        disableAllChannels();
        return I2CSwitch<Transport, ResetPinShim>::disableAll();
    }

private:
    GPIO_TypeDef* channel_port_;
    std::array<uint16_t, 4> channel_pins_;

    void pullUpChannelPin(I2CSwitchChannel channel)
    {
        disableAllChannels();

        switch (channel)
        {
        case I2CSwitchChannel::Channel0:
            HAL_GPIO_WritePin(channel_port_, channel_pins_[0], GPIO_PIN_SET);
            break;
        case I2CSwitchChannel::Channel1:
            HAL_GPIO_WritePin(channel_port_, channel_pins_[1], GPIO_PIN_SET);
            break;
        case I2CSwitchChannel::Channel2:
            HAL_GPIO_WritePin(channel_port_, channel_pins_[2], GPIO_PIN_SET);
            break;
        case I2CSwitchChannel::Channel3:
            HAL_GPIO_WritePin(channel_port_, channel_pins_[3], GPIO_PIN_SET);
            break;
        default:
            break;
        }
    }

    void disableAllChannels()
    {
        for (auto pin : channel_pins_)
            HAL_GPIO_WritePin(channel_port_, pin, GPIO_PIN_RESET);
    }
};

#endif // _CAMERA_SWITCH_HPP_
