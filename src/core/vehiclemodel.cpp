#include "vehiclemodel.h"

#include "signalcodec.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace can {

namespace {
constexpr double kPi = std::numbers::pi;
// Road speed per 1000 rpm in each gear (km/h).
constexpr double kKmhPer1000[] = {0, 7.0, 13.0, 20.0, 27.0, 34.0, 42.0};

int gearForSpeed(double v)
{
    if (v < 15) return 1;
    if (v < 30) return 2;
    if (v < 50) return 3;
    if (v < 70) return 4;
    if (v < 95) return 5;
    return 6;
}

double approach(double value, double target, double dt, double tau)
{
    return value + (target - value) * (1.0 - std::exp(-dt / tau));
}

// Full-load torque curve: ~140 Nm at idle, peak ~300 Nm around 3000 rpm.
double maxTorque(double rpm)
{
    const double x = (rpm - 3000.0) / 1800.0;
    return 140.0 + 160.0 * std::exp(-x * x);
}

// Internal friction / pumping losses (positive number, Nm).
double friction(double rpm)
{
    return 8.0 + rpm * 0.006;
}
} // namespace

VehicleModel::VehicleModel(std::uint64_t seed)
    : m_rng(seed ^ 0x5EEDCA7ull)
{
    enter(Phase::Start);
}

double VehicleModel::uniform(double lo, double hi)
{
    const double u = static_cast<double>(m_rng.next() >> 11) * 0x1.0p-53;
    return lo + (hi - lo) * u;
}

void VehicleModel::enter(Phase phase)
{
    m_phase = phase;
    m_phaseStart = m_t;
    switch (phase) {
    case Phase::Start:
        m_phaseDuration = 2.0;
        break;
    case Phase::Idle:
        m_phaseDuration = uniform(1.5, 3.0);
        break;
    case Phase::Accelerate:
        m_targetSpeed = uniform(55.0, 120.0);
        m_accel = uniform(7.0, 10.0);
        m_throttleTarget = uniform(45.0, 65.0);
        break;
    case Phase::Cruise:
        m_phaseDuration = uniform(5.0, 9.0);
        break;
    case Phase::Brake:
        m_decel = uniform(10.0, 14.0);
        m_abs = m_speed > 85.0;
        m_absUntil = m_t + 0.6;
        break;
    }
}

void VehicleModel::advanceTo(std::int64_t timeUs)
{
    double remaining = static_cast<double>(timeUs - m_lastUs) / 1e6;
    if (remaining <= 0)
        return;
    m_lastUs = timeUs;
    while (remaining > 0) {
        const double dt = std::min(remaining, 0.01);
        step(dt);
        remaining -= dt;
    }
}

void VehicleModel::step(double dt)
{
    m_t += dt;
    const double inPhase = m_t - m_phaseStart;

    // --- drive cycle --------------------------------------------------------
    switch (m_phase) {
    case Phase::Start:
        m_engineState = inPhase < 0.6 ? 1 : 2; // cranking, then running
        m_brake = true;
        if (inPhase >= m_phaseDuration)
            enter(Phase::Idle);
        break;
    case Phase::Idle:
        m_brake = true;
        m_speed = 0;
        if (inPhase >= m_phaseDuration)
            enter(Phase::Accelerate);
        break;
    case Phase::Accelerate: {
        m_brake = false;
        // The driver eases off the pedal while approaching the target speed,
        // with small pedal movements on top.
        const double progress = std::clamp(m_speed / m_targetSpeed, 0.0, 1.0);
        m_throttleCommand = m_throttleTarget * (1.0 - 0.55 * progress * progress)
                            + 5.0 * std::sin(2 * kPi * m_t / 2.3);
        m_speed += m_accel * (0.45 + 0.55 * m_throttle / 60.0) * (1.0 - m_speed / 190.0) * dt;
        if (m_speed >= m_targetSpeed)
            enter(Phase::Cruise);
        break;
    }
    case Phase::Cruise:
        m_brake = false;
        // Road load plus a slowly changing grade and small corrections.
        m_throttleCommand = 8.0 + 0.18 * m_speed + 6.0 * std::sin(2 * kPi * inPhase / 3.7)
                            + 4.0 * std::sin(2 * kPi * m_t / 9.0);
        m_speed += (0.9 * std::sin(2 * kPi * inPhase / 4.0) + 0.5 * std::sin(2 * kPi * m_t / 9.0)) * dt;
        if (inPhase >= m_phaseDuration)
            enter(Phase::Brake);
        break;
    case Phase::Brake:
        m_brake = true;
        m_speed -= m_decel * dt;
        if (m_t > m_absUntil)
            m_abs = false;
        if (m_speed <= 0) {
            m_speed = 0;
            m_abs = false;
            enter(Phase::Idle);
        }
        break;
    }
    m_speed = std::clamp(m_speed, 0.0, 250.0);
    m_distance += m_speed / 3600.0 * dt;

    // --- powertrain ---------------------------------------------------------
    const int gear = m_phase == Phase::Start ? 0 : gearForSpeed(m_speed);
    if (gear != m_gear) {
        if (m_gear > 0 && gear > 0)
            m_shiftUntil = m_t + 0.35;
        m_gear = gear;
    }
    // Idle-air / coasting pedal position is a few percent, never exactly 0.
    const bool pedalUp = m_phase == Phase::Start || m_phase == Phase::Idle || m_phase == Phase::Brake;
    const double command = pedalUp ? 3.0 + 0.6 * std::sin(2 * kPi * m_t / 5.0) : m_throttleCommand;
    m_throttle = approach(m_throttle, std::clamp(command, 2.0, 92.0), dt, 0.3);
    double rpmTarget = 0;
    if (m_engineState == 1) {
        rpmTarget = 250;
    } else if (m_engineState == 2) {
        rpmTarget = 800;
        if (m_gear > 0 && m_speed > 0.5)
            rpmTarget = std::max(800.0, m_speed * 1000.0 / kKmhPer1000[m_gear]);
        rpmTarget += m_throttle * 6.0;
        if (m_t < m_shiftUntil)
            rpmTarget *= 0.92;
    }
    m_rpm = approach(m_rpm, rpmTarget, dt, m_engineState == 1 ? 0.25 : 0.15);

    // Torque follows throttle and the torque curve; negative while coasting
    // in gear (engine braking), small positive at idle.
    double torqueTarget = 0;
    if (m_engineState == 2) {
        if (pedalUp && m_speed > 3.0)
            torqueTarget = -(friction(m_rpm) * 1.6);
        else if (pedalUp)
            torqueTarget = 14.0 + 2.0 * std::sin(2 * kPi * m_t / 1.7);
        else
            torqueTarget = m_throttle / 100.0 * maxTorque(m_rpm) + 18.0 - friction(m_rpm);
    }
    m_torque = approach(m_torque, torqueTarget, dt, 0.12);

    // --- brakes -------------------------------------------------------------
    // Small residual pressure when released (never exactly 0).
    const double pressureTarget = !m_brake ? 0.4 + 0.15 * std::sin(2 * kPi * m_t / 2.9)
                                           : (m_phase == Phase::Brake ? 22.0 + m_decel : 12.0);
    m_brakePressure = approach(m_brakePressure, pressureTarget, dt, 0.08);

    // --- steering: slow sweep, larger at low speed ---------------------------
    const double amplitude = 8.0 + 170.0 * std::exp(-m_speed / 25.0);
    m_steering = amplitude * std::sin(2 * kPi * m_t / 11.0) + 6.0 * std::sin(2 * kPi * m_t / 3.1);

    // --- temperatures, battery ---------------------------------------------
    const bool running = m_engineState == 2;
    m_coolant = approach(m_coolant, running ? 90.0 : 18.0, dt, 70.0);
    m_oil = approach(m_oil, running ? 98.0 : 18.0, dt, 120.0);
    m_transOil = approach(m_transOil, running ? 80.0 : 18.0, dt, 150.0);
    m_cabin = approach(m_cabin, 22.0, dt, 120.0);
    if (running) {
        m_soc = std::min(100.0, m_soc + 0.03 * dt);
        m_fuelUsed += (0.008 + 0.0004 * m_throttle) * dt; // demo-accelerated consumption
    }
}

double VehicleModel::value(const QString &s) const
{
    const double v = m_speed;
    const double ms = v / 3.6;
    const double yaw = std::clamp(ms * std::tan(m_steering / 15.0 * kPi / 180.0) / 2.7 * 180.0 / kPi, -100.0, 100.0);
    const bool running = m_engineState == 2;
    const bool cranking = m_engineState == 1;

    if (s == QLatin1String("VehicleSpeed")) return v;
    if (s == QLatin1String("SteeringAngle")) return m_steering;
    if (s == QLatin1String("YawRate")) return yaw;
    if (s == QLatin1String("LateralAccel")) return std::clamp(ms * yaw * kPi / 180.0, -10.0, 10.0);
    if (s == QLatin1String("EngineSpeed")) return m_rpm;
    if (s == QLatin1String("ThrottlePosition")) return m_throttle;
    if (s == QLatin1String("EngineLoad"))
        return running ? std::clamp(9.0 + 90.0 * std::max(m_torque, 0.0) / maxTorque(m_rpm), 0.0, 100.0) : 0.0;
    if (s == QLatin1String("EngineTorque")) return m_torque;
    if (s == QLatin1String("EngineState")) return m_engineState;
    if (s == QLatin1String("CoolantTemp")) return m_coolant;
    if (s == QLatin1String("OilTemp")) return m_oil;
    if (s == QLatin1String("IntakeAirTemp")) return 24.0 + 0.08 * m_throttle + 3.0 * (1.0 - std::exp(-m_t / 60.0));
    if (s == QLatin1String("OilPressure"))
        return running ? std::clamp(1.0 + m_rpm / 1500.0 - (m_oil - 20.0) * 0.006, 0.0, 6.0) : 0.0;
    if (s == QLatin1String("GearLever")) return m_t < 2.0 ? 0 : 3; // P, then D
    if (s == QLatin1String("CurrentGear")) return m_gear;
    if (s == QLatin1String("TransOilTemp")) return m_transOil;
    if (s == QLatin1String("ShiftInProgress")) return m_t < m_shiftUntil ? 1 : 0;
    if (s == QLatin1String("ConverterSlip"))
        return running ? 15.0 + 0.6 * value(QStringLiteral("EngineLoad"))
                             + (m_gear == 1 ? std::max(0.0, 1.0 - v / 20.0) * 240.0 : 0.0)
                       : 0.0;
    if (s == QLatin1String("BrakePedalPressed")) return m_brake ? 1 : 0;
    if (s == QLatin1String("ABSActive")) return m_abs ? 1 : 0;
    if (s == QLatin1String("BrakePressure")) return m_brakePressure;
    if (s == QLatin1String("BatteryVoltage"))
        return cranking ? 10.6 : running ? 14.1 + 0.04 * std::sin(2 * kPi * m_t / 1.3) : 12.4;
    if (s == QLatin1String("BatteryCurrent"))
        return cranking ? -150.0 : running ? 6.0 + 9.0 * std::exp(-m_t / 25.0) - 0.04 * m_throttle : -2.0;
    if (s == QLatin1String("StateOfCharge")) return m_soc;
    if (s == QLatin1String("BatteryTemp")) return 24.0 + 6.0 * (1.0 - std::exp(-m_t / 120.0)) + 0.4 * std::sin(m_t / 7.0);
    if (s == QLatin1String("ChargingState")) return cranking ? 2 : running ? 1 : 0;
    if (s == QLatin1String("DoorOpenDriver")) return 0;
    if (s == QLatin1String("DoorOpenPassenger")) return 0;
    if (s == QLatin1String("TrunkOpen")) return 0;
    if (s == QLatin1String("HeadlightMode")) return 2;
    if (s == QLatin1String("TurnIndicator"))
        return v < 60 && m_steering > 90 ? 1 : (v < 60 && m_steering < -90 ? 2 : 0);
    if (s == QLatin1String("WiperMode")) return 0;
    if (s == QLatin1String("Odometer")) return 48213.4 + m_distance;
    if (s == QLatin1String("FuelLevel")) return std::max(0.0, 63.2 - m_fuelUsed);
    if (s == QLatin1String("AvgConsumption")) return 6.4 + 0.01 * m_throttle;
    if (s == QLatin1String("RangeEstimate")) return std::max(0.0, 63.2 - m_fuelUsed) * 9.5;
    if (s == QLatin1String("TripDistance")) return 1.27 + m_distance;
    if (s == QLatin1String("AmbientTemp")) return 21.5 + 0.8 * std::sin(m_t / 40.0);
    if (s == QLatin1String("CabinTemp")) return m_cabin;
    if (s == QLatin1String("Humidity")) return 44.0 + 2.0 * std::sin(m_t / 40.0);
    return std::numeric_limits<double>::quiet_NaN();
}

void VehicleModel::fill(const MessageDef &message, CanFrame &frame, Intended *intended)
{
    frame.id = message.id;
    frame.extended = message.extended;
    frame.dlc = static_cast<std::uint8_t>(message.dlc);
    frame.data = {};
    int &counter = m_counters[message.name];
    const int muxPage = message.multiplexor() ? counter % 3 : -1;

    for (const SignalDef &s : message.signalList) {
        double phys;
        if (s.muxRole == SignalDef::MuxRole::Multiplexor)
            phys = muxPage;
        else if (s.muxRole == SignalDef::MuxRole::Multiplexed && s.muxValue != muxPage)
            continue;
        else if (s.name.endsWith(QLatin1String("AliveCounter")))
            phys = counter & 0x0F;
        else
            phys = value(s.name);
        if (std::isnan(phys))
            continue;
        codec::encode(s, phys, frame.data.data());
        if (intended)
            intended->emplace_back(s.name, phys);
    }
    ++counter;
}

} // namespace can
