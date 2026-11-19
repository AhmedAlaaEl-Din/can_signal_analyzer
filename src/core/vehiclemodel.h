#pragma once

#include "canframe.h"
#include "dbc.h"
#include "simulatorengine.h"

#include <QHash>
#include <QString>

#include <cstdint>
#include <utility>
#include <vector>

namespace can {

// Deterministic, physically plausible car behind the demo database:
// a repeating drive cycle (idle, accelerate, cruise, brake), engine speed that
// follows road speed and gear, a slow coolant warm-up, a sweeping steering wheel,
// battery, body and trip-computer values.
//
// The model is advanced to each frame's timestamp before that frame is filled,
// so the output depends only on the seed and the frame times.
class VehicleModel
{
public:
    using Intended = std::vector<std::pair<QString, double>>;

    explicit VehicleModel(std::uint64_t seed);

    void advanceTo(std::int64_t timeUs);
    // Encodes every applicable signal of `message` into `frame` (id/dlc/data).
    // `intended` (optional) receives the physical values that were encoded.
    void fill(const MessageDef &message, CanFrame &frame, Intended *intended = nullptr);
    // Current physical value of a demo signal (NaN if the name is unknown).
    double value(const QString &signal) const;

    double speedKmh() const { return m_speed; }

private:
    enum class Phase { Start, Idle, Accelerate, Cruise, Brake };
    void step(double dt);
    void enter(Phase phase);
    double uniform(double lo, double hi);

    SplitMix64 m_rng;
    double m_t = 0;          // seconds
    std::int64_t m_lastUs = 0;

    Phase m_phase = Phase::Start;
    double m_phaseStart = 0;
    double m_phaseDuration = 0;
    double m_targetSpeed = 0;
    double m_accel = 8;      // km/h per s
    double m_decel = 12;
    double m_throttleTarget = 0;  // pedal aim for the current acceleration
    double m_throttleCommand = 0; // pedal command this step

    double m_speed = 0;      // km/h
    double m_distance = 0;   // km
    double m_rpm = 0;
    double m_throttle = 0;   // %
    int m_gear = 0;
    double m_shiftUntil = -1;
    double m_steering = 0;   // deg
    bool m_brake = true;
    double m_brakePressure = 0;
    bool m_abs = false;
    double m_absUntil = -1;
    int m_engineState = 0;
    double m_torque = 0;     // Nm, follows throttle and the torque curve
    double m_fuelUsed = 0;   // % of tank
    double m_coolant = 18, m_oil = 18, m_transOil = 18, m_cabin = 26;
    double m_soc = 78;

    QHash<QString, int> m_counters; // per message: alive counters / mux page
};

} // namespace can
