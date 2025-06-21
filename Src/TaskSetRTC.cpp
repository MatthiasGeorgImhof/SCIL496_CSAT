/*
 * TaskSetRTC.cpp
 *
 *  Created on: Dec 4, 2024
 *      Author: mgi
 */

#include <TaskSetRTC.hpp>
#include "ServiceManager.hpp"
#include "TimeUtils.hpp"
#ifdef __arm__
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "utilities.h"
#elif __x86_64__
#include "mock_hal.h"
#endif


void TaskSetRTC::registerTask(RegistrationManager *manager, std::shared_ptr<Task> task)
{
	manager->subscribe(PURE_HANDLER, task);
}

void TaskSetRTC::unregisterTask(RegistrationManager *manager, std::shared_ptr<Task> task)
{
	manager->unsubscribe(PURE_HANDLER, task);
}

void TaskSetRTC::handleTaskImpl()
{
	{
	    std::optional<UTCTime> utctime = gnss.GetNavTimeUTC();
        if (!utctime.has_value())
        {
            return;
        }

        struct TimeUtils::DateTimeComponents dateTimeComponents {
            .year = utctime.value().year,
            .month = utctime.value().month,
            .day = utctime.value().day,
            .hour = utctime.value().hour,
            .minute = utctime.value().min,
            .second = utctime.value().sec,
            .millisecond = utctime.value().nano / 1000000
        };

        struct TimeUtils::RTCDateTimeSubseconds rtc = TimeUtils::to_rtc(TimeUtils::to_epoch_duration(dateTimeComponents), hrtc->Init.SynchPrediv);
        HAL_RTC_SetTime(hrtc, &rtc.time, RTC_FORMAT_BIN);
        HAL_RTC_SetDate(hrtc, &rtc.date, RTC_FORMAT_BIN);
        HAL_RTCEx_SetSynchroShift(RTC_SHIFTADD1S_RESET, rtc.time.SubSeconds);
    }
}


