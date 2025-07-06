#ifndef INC_TASKRESPONDGETINFO_HPP_
#define INC_TASKSGP4_HPP_

#include "Task.hpp"
#include "RegistrationManager.hpp"
#include "Logger.hpp"
#include "TimeUtils.hpp"

#include "sgp4_tle.hpp"
#include "SGP4.h"

#include "nunavut_assert.h"
#include "_4111Spyglass.h"
#include "_4111spyglass/sat/data/SPG4TLE_0_1.h"
#include "_4111spyglass/sat/model/PositionVelocity_0_1.h"
#include <cstring>
#include <chrono>
#include <cstdint>

template <typename... Adapters>
class TaskSGP4 : public TaskPublishReceive<Adapters...>
{
public:
    TaskSGP4() = delete;
    TaskSGP4(RTC_HandleTypeDef *hrtc, uint32_t interval, uint32_t tick, CyphalTransferID transfer_id, std::tuple<Adapters...> &adapters) : TaskPublishReceive<Adapters...>(interval, tick, transfer_id, adapters), hrtc_(hrtc), tle_({}) {}

    virtual void registerTask(RegistrationManager *manager, std::shared_ptr<Task> task) override;
    virtual void unregisterTask(RegistrationManager *manager, std::shared_ptr<Task> task) override;
    virtual void handleTaskImpl() override;

    void setSGP4TLE(const SGP4TwoLineElement &tle)
    {
        tle_ = tle;
    }

    SGP4TwoLineElement getSGP4TLE() const
    {
        return tle_;
    }

private:
    void processTLEMessages();
    bool predictSGP4(float r[3], float v[3], uint64_t &timestamp);

private:
    RTC_HandleTypeDef *hrtc_;
    SGP4TwoLineElement tle_;
};

template <typename... Adapters>
void TaskSGP4<Adapters...>::processTLEMessages()
{
    size_t count = TaskSGP4<Adapters...>::buffer_.size();
    for (size_t i = 0; i < count; ++i)
    {
        std::shared_ptr<CyphalTransfer> transfer = TaskSGP4<Adapters...>::buffer_.pop();
        size_t payload_size = transfer->payload_size;
        _4111spyglass_sat_data_SPG4TLE_0_1 data;
        int8_t result = _4111spyglass_sat_data_SPG4TLE_0_1_deserialize_(&data, static_cast<const uint8_t *>(transfer->payload), &payload_size);
        if (result != 0)
        {
            log(LOG_LEVEL_ERROR, "TaskSGP4: deserialization error\r\n");
            return;
        }
        tle_ = {
            .satelliteNumber = data.satelliteNumber,
            .elementNumber = data.elementNumber,
            .ephemerisType = data.ephemerisType,
            .epochYear = data.epochYear,
            .epochDay = data.epochDay,
            .meanMotionDerivative1 = data.meanMotionDerivative1,
            .meanMotionDerivative2 = data.meanMotionDerivative2,
            .bStarDrag = data.bStarDrag,
            .inclination = data.inclination,
            .rightAscensionAscendingNode = data.rightAscensionAscendingNode,
            .eccentricity = data.eccentricity,
            .argumentOfPerigee = data.argumentOfPerigee,
            .meanAnomaly = data.meanAnomaly,
            .meanMotion = data.meanMotion,
            .revolutionNumberAtEpoch = data.revolutionNumberAtEpoch};
    }
}

template <typename... Adapters>
bool TaskSGP4<Adapters...>::predictSGP4(float r[3], float v[3], uint64_t &timestamp)
{
    elsetrec satrec{};
    sprintf(satrec.satnum, "%05d", tle_.satelliteNumber);
    satrec.epochyr = tle_.epochYear;
    satrec.epochdays = tle_.epochDay;
    satrec.ndot = tle_.meanMotionDerivative1;
    satrec.nddot = tle_.meanMotionDerivative2;
    satrec.bstar = tle_.bStarDrag;
    satrec.ephtype = tle_.ephemerisType;
    satrec.elnum = tle_.elementNumber;
    satrec.inclo = tle_.inclination;
    satrec.nodeo = tle_.rightAscensionAscendingNode;
    satrec.ecco = tle_.eccentricity;
    satrec.argpo = tle_.argumentOfPerigee;
    satrec.mo = tle_.meanAnomaly;
    satrec.no_kozai = tle_.meanMotion;
    satrec.revnum = tle_.revolutionNumberAtEpoch;

    TimeUtils::RTCDateTimeSubseconds rtc;
    HAL_RTC_GetTime(hrtc_, &rtc.time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(hrtc_, &rtc.date, RTC_FORMAT_BIN);

    TimeUtils::DateTimeComponents dtc = {
        .year = static_cast<uint16_t>(static_cast<uint16_t>(rtc.date.Year) + TimeUtils::EPOCH_YEAR),
        .month = rtc.date.Month,
        .day = rtc.date.Date,
        .hour = rtc.time.Hours,
        .minute = rtc.time.Minutes,
        .second = rtc.time.Seconds,
        .millisecond = static_cast<uint16_t>(static_cast<uint64_t>(1000 * (rtc.time.SecondFraction - rtc.time.SubSeconds) / (rtc.time.SecondFraction + 1)))};

    std::chrono::system_clock::time_point now = TimeUtils::to_timepoint(dtc);
    std::chrono::system_clock::time_point epoch = TimeUtils::to_timepoint(static_cast<uint16_t>(tle_.epochYear) + TimeUtils::EPOCH_YEAR, tle_.epochDay);
    float fractional_days_since_epoch = TimeUtils::to_fractional_days(epoch, now);
    
    std::chrono::milliseconds milliseconds = TimeUtils::from_rtc(rtc, hrtc_->Init.SynchPrediv);
    timestamp = 1000 * static_cast<uint64_t>(milliseconds.count());

    gravconsttype whichconst = wgs84; // Choose the gravity model (wgs72old, wgs72, wgs84)
    char opsmode = 'i';               // Operation mode ('a' for AFSPC, 'i' for improved)
    SGP4Funcs::satrec2rv(opsmode, whichconst, satrec);
    return SGP4Funcs::sgp4(satrec, fractional_days_since_epoch, r, v);
}

template <typename... Adapters>
void TaskSGP4<Adapters...>::handleTaskImpl()
{
    // Process all transfers in the buffer
    processTLEMessages();
    if (tle_.satelliteNumber == 0)
    {
        return;
    }

    uint64_t timestamp;
    float r[3], v[3];
    if (!predictSGP4(r, v, timestamp))
    {
        return;
    };

    for (int i = 0; i < 3; ++i)
    {
        r[i] *= 1000.0f;
        v[i] *= 1000.0f;
    }

    _4111spyglass_sat_model_PositionVelocity_0_1 data;
    data.timestamp.microsecond = timestamp;
    memcpy(data.position_m, r, sizeof(r));
    memcpy(data.velocity_ms, v, sizeof(v));

    constexpr size_t PAYLOAD_SIZE = _4111spyglass_sat_model_PositionVelocity_0_1_SERIALIZATION_BUFFER_SIZE_BYTES_;
    uint8_t payload[PAYLOAD_SIZE];

    TaskSGP4<Adapters...>::publish(PAYLOAD_SIZE, payload, &data,
                                   reinterpret_cast<int8_t (*)(const void *const, uint8_t *const, size_t *const)>(_4111spyglass_sat_model_PositionVelocity_0_1_serialize_),
                                   _4111spyglass_sat_model_PositionVelocity_0_1_PORT_ID_);
}

template <typename... Adapters>
void TaskSGP4<Adapters...>::registerTask(RegistrationManager *manager, std::shared_ptr<Task> task)
{
    manager->server(_4111spyglass_sat_data_SGP4_0_1_PORT_ID_, task);
}

template <typename... Adapters>
void TaskSGP4<Adapters...>::unregisterTask(RegistrationManager *manager, std::shared_ptr<Task> task)
{
    manager->unserver(_4111spyglass_sat_data_SGP4_0_1_PORT_ID_, task);
}

static_assert(std::is_standard_layout_v<SGP4TwoLineElement>, "SGP4TwoLineElement is not a standard layout type");                                 // Important for some comparisons
static_assert(std::is_standard_layout_v<_4111spyglass_sat_data_SPG4TLE_0_1>, "_4111spyglass_sat_data_SPG4TLE_0_1 is not a standard layout type"); // Important for some comparisons
static_assert(sizeof(SGP4TwoLineElement) == sizeof(_4111spyglass_sat_data_SPG4TLE_0_1), "SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different sizes!");
static_assert(offsetof(SGP4TwoLineElement, satelliteNumber) == offsetof(_4111spyglass_sat_data_SPG4TLE_0_1, satelliteNumber), "satelliteNumber: SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different offsets!");
static_assert(offsetof(SGP4TwoLineElement, epochYear) == offsetof(_4111spyglass_sat_data_SPG4TLE_0_1, epochYear), "epochYear: SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different offsets!");
static_assert(offsetof(SGP4TwoLineElement, epochDay) == offsetof(_4111spyglass_sat_data_SPG4TLE_0_1, epochDay), "epochDay: SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different offsets!");
static_assert(offsetof(SGP4TwoLineElement, meanMotionDerivative1) == offsetof(_4111spyglass_sat_data_SPG4TLE_0_1, meanMotionDerivative1), "meanMotionDerivative1: SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different offsets!");
static_assert(offsetof(SGP4TwoLineElement, meanMotionDerivative2) == offsetof(_4111spyglass_sat_data_SPG4TLE_0_1, meanMotionDerivative2), "meanMotionDerivative2: SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different offsets!");
static_assert(offsetof(SGP4TwoLineElement, bStarDrag) == offsetof(_4111spyglass_sat_data_SPG4TLE_0_1, bStarDrag), "bStarDrag: SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different offsets!");
static_assert(offsetof(SGP4TwoLineElement, ephemerisType) == offsetof(_4111spyglass_sat_data_SPG4TLE_0_1, ephemerisType), "ephemerisType: SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different offsets!");
static_assert(offsetof(SGP4TwoLineElement, elementNumber) == offsetof(_4111spyglass_sat_data_SPG4TLE_0_1, elementNumber), "elementNumber: SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different offsets!");
static_assert(offsetof(SGP4TwoLineElement, inclination) == offsetof(_4111spyglass_sat_data_SPG4TLE_0_1, inclination), "inclination: SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different offsets!");
static_assert(offsetof(SGP4TwoLineElement, rightAscensionAscendingNode) == offsetof(_4111spyglass_sat_data_SPG4TLE_0_1, rightAscensionAscendingNode), "rightAscensionAscendingNode: SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different offsets!");
static_assert(offsetof(SGP4TwoLineElement, eccentricity) == offsetof(_4111spyglass_sat_data_SPG4TLE_0_1, eccentricity), "eccentricity: SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different offsets!");
static_assert(offsetof(SGP4TwoLineElement, argumentOfPerigee) == offsetof(_4111spyglass_sat_data_SPG4TLE_0_1, argumentOfPerigee), "argumentOfPerigee: SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different offsets!");
static_assert(offsetof(SGP4TwoLineElement, meanAnomaly) == offsetof(_4111spyglass_sat_data_SPG4TLE_0_1, meanAnomaly), "meanAnomaly: SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different offsets!");
static_assert(offsetof(SGP4TwoLineElement, meanMotion) == offsetof(_4111spyglass_sat_data_SPG4TLE_0_1, meanMotion), "meanMotion: SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different offsets!");
static_assert(offsetof(SGP4TwoLineElement, revolutionNumberAtEpoch) == offsetof(_4111spyglass_sat_data_SPG4TLE_0_1, revolutionNumberAtEpoch), "revolutionNumberAtEpoch: SGP4TwoLineElement and _4111spyglass_sat_data_SPG4TLE_0_1 have different offsets!");
#endif // INC_TASKSGP4_HPP_
