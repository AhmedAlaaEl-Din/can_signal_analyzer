#include "dbc.h"
#include "signalcodec.h"
#include "simulatorengine.h"

#include <QtTest>

#include <cmath>
#include <map>

using namespace can;

class TstVehicleSim : public QObject
{
    Q_OBJECT
private:
    struct Run
    {
        std::vector<CanFrame> frames;
        std::vector<SimulatorEngine::Intended> intended;
    };
    static Run run(std::uint64_t seed, std::int64_t endUs, std::int64_t stepUs, int rate = 0)
    {
        SimulatorEngine e(seed, SimulatorEngine::Mode::Vehicle);
        e.setTargetRate(rate);
        Run r;
        for (std::int64_t t = stepUs; t < endUs; t += stepUs)
            e.advanceTo(t, r.frames, &r.intended);
        e.advanceTo(endUs, r.frames, &r.intended);
        return r;
    }
    // Physical values per signal name over a run, decoded from the frames.
    static std::map<QString, std::vector<double>> decodedSeries(const Run &r)
    {
        const auto db = demoDatabase();
        std::map<QString, std::vector<double>> series;
        for (const auto &f : r.frames) {
            const MessageDef *m = db->find(f.id, f.extended);
            for (const auto &d : Decoder::decode(*m, f))
                series[m->signalList[std::size_t(d.signalIndex)].name].push_back(d.physical);
        }
        return series;
    }

private slots:
    void onlyDatabaseMessages()
    {
        const auto r = run(1, 3'000'000, 10'000);
        const auto db = demoDatabase();
        QVERIFY(r.frames.size() > 500);
        for (const auto &f : r.frames) {
            const MessageDef *m = db->find(f.id, f.extended);
            QVERIFY2(m, qPrintable(QString::number(f.id, 16)));
            QCOMPARE(int(f.dlc), m->dlc);
        }
        QCOMPARE(r.frames.size(), r.intended.size());
    }

    void periodsFollowCycleTimes()
    {
        const auto r = run(2, 10'000'000, 10'000);
        const auto db = demoDatabase();
        std::map<std::uint32_t, int> counts;
        for (const auto &f : r.frames)
            ++counts[f.key()];
        for (const auto &m : db->messages) {
            const double expected = 10000.0 / m.cycleTimeMs;
            QVERIFY2(std::abs(counts[m.key()] - expected) <= std::max(2.0, expected * 0.03),
                     qPrintable(m.name));
        }
    }

    void deterministic()
    {
        const auto a = run(42, 4'000'000, 10'000);
        const auto b = run(42, 4'000'000, 1'337); // different chunking
        QCOMPARE(a.frames.size(), b.frames.size());
        QVERIFY(a.frames == b.frames);
        const auto c = run(43, 4'000'000, 10'000);
        QVERIFY(a.frames != c.frames);
    }

    // Generate -> decode -> compare with the value the model intended.
    void roundTrip()
    {
        const auto r = run(7, 20'000'000, 10'000, 1000);
        const auto db = demoDatabase();
        int compared = 0;
        for (std::size_t i = 0; i < r.frames.size(); ++i) {
            const CanFrame &f = r.frames[i];
            const MessageDef *m = db->find(f.id, f.extended);
            const auto decoded = Decoder::decode(*m, f);
            for (const auto &[name, intended] : r.intended[i]) {
                const SignalDef *s = m->findSignal(name);
                QVERIFY(s);
                auto it = std::find_if(decoded.begin(), decoded.end(), [&](const DecodedSignal &d) {
                    return m->signalList[std::size_t(d.signalIndex)].name == name;
                });
                QVERIFY2(it != decoded.end(), qPrintable(name));
                // Quantisation error is at most half a step.
                const double tol = std::abs(s->factor) / 2 + 1e-9;
                QVERIFY2(std::abs(it->physical - intended) <= tol,
                         qPrintable(QStringLiteral("%1: decoded %2, intended %3")
                                        .arg(name).arg(it->physical).arg(intended)));
                QVERIFY2(!it->outOfRange, qPrintable(name));
                ++compared;
            }
        }
        QVERIFY(compared > 20000);
    }

    void plausibleBehaviour()
    {
        const auto series = decodedSeries(run(11, 60'000'000, 10'000));
        // Speed ramps up from standstill well above 50 km/h and back to 0.
        const auto &speed = series.at(QStringLiteral("VehicleSpeed"));
        QCOMPARE(speed.front(), 0.0);
        QVERIFY(*std::max_element(speed.begin(), speed.end()) > 50.0);
        QVERIFY(std::count(speed.begin(), speed.end(), 0.0) > 100); // stops at idle
        // Engine speed: idle around 800 rpm, higher when moving, never absurd.
        const auto &rpm = series.at(QStringLiteral("EngineSpeed"));
        QVERIFY(*std::max_element(rpm.begin(), rpm.end()) < 6000.0);
        QVERIFY(*std::max_element(rpm.begin(), rpm.end()) > 1500.0);
        // Coolant warms up monotonically (never drops while running).
        const auto &coolant = series.at(QStringLiteral("CoolantTemp"));
        QVERIFY(coolant.back() > coolant.front() + 20.0);
        for (std::size_t i = 1; i < coolant.size(); ++i)
            QVERIFY(coolant[i] >= coolant[i - 1]);
        // Brake flag toggles, steering sweeps both ways, several gears are used.
        const auto &brake = series.at(QStringLiteral("BrakePedalPressed"));
        int toggles = 0;
        for (std::size_t i = 1; i < brake.size(); ++i)
            toggles += brake[i] != brake[i - 1];
        QVERIFY(toggles >= 4);
        const auto &steer = series.at(QStringLiteral("SteeringAngle"));
        QVERIFY(*std::min_element(steer.begin(), steer.end()) < -20.0);
        QVERIFY(*std::max_element(steer.begin(), steer.end()) > 20.0);
        const auto &gear = series.at(QStringLiteral("CurrentGear"));
        QVERIFY(*std::max_element(gear.begin(), gear.end()) >= 4.0);
        // Multiplexed pages all appear.
        QVERIFY(series.count(QStringLiteral("Odometer")) && series.count(QStringLiteral("FuelLevel"))
                && series.count(QStringLiteral("TripDistance")));
    }

    void rpmFollowsSpeedAndGear()
    {
        // While moving in gear, rpm ~= speed * 1000 / (km/h per 1000 rpm of that gear).
        const auto r = run(5, 40'000'000, 10'000);
        const auto db = demoDatabase();
        double lastSpeed = -1, lastGear = -1, shifting = 0;
        int checked = 0;
        for (const auto &f : r.frames) {
            const MessageDef *m = db->find(f.id, f.extended);
            for (const auto &d : Decoder::decode(*m, f)) {
                const QString &n = m->signalList[std::size_t(d.signalIndex)].name;
                if (n == QLatin1String("VehicleSpeed")) lastSpeed = d.physical;
                if (n == QLatin1String("CurrentGear")) lastGear = d.physical;
                if (n == QLatin1String("ShiftInProgress")) shifting = d.physical;
                if (n == QLatin1String("EngineSpeed") && lastSpeed > 20 && lastGear >= 1 && shifting == 0) {
                    static const double kmhPer1000[] = {0, 7, 13, 20, 27, 34, 42};
                    const double base = lastSpeed * 1000 / kmhPer1000[int(lastGear)];
                    QVERIFY2(d.physical > base * 0.8 && d.physical < base * 1.1 + 450,
                             qPrintable(QStringLiteral("rpm %1 at %2 km/h gear %3").arg(d.physical).arg(lastSpeed).arg(lastGear)));
                    ++checked;
                }
            }
        }
        QVERIFY(checked > 500);
    }

    // Every analog signal must move with the drive cycle and must not sit at a
    // limit of its DBC range for more than a few seconds.
    void analogSignalsVaryAndStayOffLimits()
    {
        const auto db = demoDatabase();
        const auto r = run(21, 60'000'000, 10'000);
        struct Track
        {
            double lo = 1e300, hi = -1e300;
            std::int64_t limitSince = -1;
            std::int64_t longestAtLimitUs = 0;
        };
        std::map<QString, Track> tracks;
        auto isAnalog = [](const SignalDef &s) {
            return s.valueTable.empty() && !s.name.contains(QLatin1String("Counter"))
                   && s.muxRole != SignalDef::MuxRole::Multiplexor && s.length >= 8 && !s.unit.isEmpty();
        };
        for (const auto &f : r.frames) {
            const MessageDef *m = db->find(f.id, f.extended);
            for (const auto &d : Decoder::decode(*m, f)) {
                const SignalDef &s = m->signalList[std::size_t(d.signalIndex)];
                if (!isAnalog(s))
                    continue;
                Track &t = tracks[s.name];
                t.lo = std::min(t.lo, d.physical);
                t.hi = std::max(t.hi, d.physical);
                const double tol = std::abs(s.factor) / 2 + 1e-9;
                const bool atLimit = s.hasRange()
                                     && (std::abs(d.physical - s.minimum) <= tol || std::abs(d.physical - s.maximum) <= tol);
                if (atLimit) {
                    if (t.limitSince < 0)
                        t.limitSince = f.timestampUs;
                    t.longestAtLimitUs = std::max(t.longestAtLimitUs, f.timestampUs - t.limitSince);
                } else {
                    t.limitSince = -1;
                }
            }
        }
        QVERIFY(tracks.size() >= 25);
        for (const auto &[name, t] : tracks) {
            const SignalDef *s = nullptr;
            for (const auto &m : db->messages)
                if (!s)
                    s = m.findSignal(name);
            QVERIFY2(t.hi - t.lo >= 2 * std::abs(s->factor) - 1e-9,
                     qPrintable(QStringLiteral("%1 does not vary: %2..%3").arg(name).arg(t.lo).arg(t.hi)));
            QVERIFY2(t.longestAtLimitUs <= 6'000'000,
                     qPrintable(QStringLiteral("%1 stays at a range limit for %2 s").arg(name).arg(t.longestAtLimitUs / 1e6)));
        }
        // The specific complaint from the phase 2 demo: torque was flat at 180 Nm.
        QVERIFY(tracks.at(QStringLiteral("EngineTorque")).hi - tracks.at(QStringLiteral("EngineTorque")).lo > 100.0);
        QVERIFY(tracks.at(QStringLiteral("EngineTorque")).lo < 0.0); // engine braking
        QVERIFY(tracks.at(QStringLiteral("ThrottlePosition")).hi - tracks.at(QStringLiteral("ThrottlePosition")).lo > 30.0);
    }

    void throttleAndTorqueAreNotFlat()
    {
        // Within any 4 s window while driving, throttle and torque must change.
        const auto series = decodedSeries(run(4, 60'000'000, 10'000));
        for (const QString name : {QStringLiteral("ThrottlePosition"), QStringLiteral("EngineTorque"),
                                   QStringLiteral("EngineLoad")}) {
            const auto &v = series.at(name);
            const std::size_t window = 400; // 100 Hz message -> 4 s
            for (std::size_t i = 0; i + window < v.size(); i += window / 2) {
                const auto [mn, mx] = std::minmax_element(v.begin() + std::ptrdiff_t(i), v.begin() + std::ptrdiff_t(i + window));
                QVERIFY2(*mx - *mn > 0.0, qPrintable(QStringLiteral("%1 flat at sample %2").arg(name).arg(i)));
            }
        }
    }

    void rawModeStillAvailable()
    {
        SimulatorEngine e(3); // default: raw
        QVERIFY(e.mode() == SimulatorEngine::Mode::Raw);
        std::vector<CanFrame> out;
        e.advanceTo(1'000'000, out);
        QVERIFY(std::any_of(out.begin(), out.end(), [](const CanFrame &f) { return f.id == 0x0A0; }));
        e.setMode(SimulatorEngine::Mode::Vehicle);
        out.clear();
        e.advanceTo(2'000'000, out);
        QVERIFY(std::all_of(out.begin(), out.end(), [](const CanFrame &f) { return demoDatabase()->find(f.id, f.extended); }));
        QVERIFY(out.front().timestampUs > 1'000'000);
    }
};

QTEST_GUILESS_MAIN(TstVehicleSim)
#include "tst_vehiclesim.moc"
