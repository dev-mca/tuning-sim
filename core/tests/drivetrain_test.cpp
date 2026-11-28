// Clutch, gearbox, car on the roller dyno, launch and flat shift (research.md §8, §3.4); mirrors
// lab/tests/test_drivetrain.py.

#include "domain/common/units.hpp"
#include "domain/drivetrain/car_simulation.hpp"
#include "domain/dyno/dyno.hpp"
#include "domain/dyno/roller.hpp"
#include "test_support.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

using namespace simcore;

namespace {

constexpr int kStepsPerSecond = 1000;
const LaunchSettings kLaunch{true, units::rpmToRadS(4000.0), 10.0, true};

const EngineDefinition& engine() {
    return test::referenceEngine().definition;
}

CarSimulation tunedCar(const LaunchSettings& launch = kLaunch) {
    Tune tune = Tune::fromTables(engine().tables);
    tune.launch = launch;
    return CarSimulation(engine(), tune);
}

std::vector<CarState> run(CarSimulation& car, double throttle, double seconds) {
    std::vector<CarState> states;
    const auto steps = static_cast<int>(seconds * kStepsPerSecond + 0.5);
    for (int i = 0; i < steps; ++i) {
        states.push_back(car.step(throttle));
    }
    return states;
}

double rpm(const CarState& state) {
    return units::radSToRpm(state.engine.speed_rad_s);
}

/// 1st gear, clutch pressed, 2-step held at full throttle for 2 s, then the clutch released.
std::vector<CarState> launch(CarSimulation& car) {
    car.drivetrain().setClutchPedal(true);
    REQUIRE(car.drivetrain().shift(1));
    run(car, 0.0, 0.5);
    run(car, 1.0, 2.0);
    car.drivetrain().setClutchPedal(false);
    return run(car, 1.0, 3.0);
}

} // namespace

TEST_CASE("road load and ratios are plausible (research.md §8.1, §8.2)") {
    const DrivetrainParameters& p = engine().drivetrain;
    CHECK(p.overallRatio(1) == doctest::Approx(4.11 * 3.15));
    CHECK(roadLoad_N(p, 0.0) == 0.0);
    const double at100 = roadLoad_N(p, units::kmhToMs(100.0));
    CHECK(at100 > 400.0);
    CHECK(at100 < 600.0);
}

TEST_CASE("in neutral the engine revs freely and the car stands") {
    CarSimulation car(engine());
    double peak = 0.0;
    for (const CarState& state : run(car, 1.0, 3.0)) {
        peak = std::max(peak, rpm(state));
        CHECK(state.drivetrain.vehicleSpeed_m_s == 0.0);
        CHECK(state.drivetrain.clutchState == ClutchState::Open);
    }
    CHECK(peak > 6500.0);
}

TEST_CASE("the clutch pedal arms the 2-step only while the car stands") {
    CarSimulation car = tunedCar();
    car.drivetrain().setClutchPedal(true);
    static_cast<void>(car.drivetrain().shift(1));
    const std::vector<CarState> held = run(car, 1.0, 2.0);
    CHECK(held.back().engine.command.launchActive);
    CHECK(rpm(held.back()) > 3700.0);
    CHECK(rpm(held.back()) < 4150.0);

    CarSimulation moving = tunedCar();
    launch(moving);
    moving.drivetrain().setClutchPedal(true);
    CHECK_FALSE(moving.step(1.0).engine.command.launchActive);
}

TEST_CASE("after a launch the clutch slips briefly, then locks without chatter") {
    CarSimulation car = tunedCar();
    const std::vector<CarState> after = launch(car);
    int changes = 0;
    int firstLock = -1;
    for (std::size_t i = 1; i < after.size(); ++i) {
        if (after[i].drivetrain.clutchState != after[i - 1].drivetrain.clutchState) {
            ++changes;
        }
        if (firstLock < 0 && after[i].drivetrain.clutchState == ClutchState::Locked) {
            firstLock = static_cast<int>(i);
        }
    }
    CHECK(firstLock >= 0);
    CHECK(firstLock < kStepsPerSecond);
    CHECK(changes <= 3);
    const CarState& last = after.back();
    REQUIRE(last.drivetrain.clutchState == ClutchState::Locked);
    const DrivetrainParameters& p = engine().drivetrain;
    // Locked: the car follows the crank kinematically.
    CHECK(last.drivetrain.vehicleSpeed_m_s ==
          doctest::Approx(last.engine.speed_rad_s * p.wheelRadius_m / p.overallRatio(1)));
}

TEST_CASE("flat shift cuts the spark and keeps the engine from flaring up") {
    const auto shiftAt5000 = [](bool flatShift) {
        LaunchSettings settings = kLaunch;
        settings.flatShift = flatShift;
        CarSimulation car = tunedCar(settings);
        car.drivetrain().setClutchPedal(true);
        static_cast<void>(car.drivetrain().shift(1));
        run(car, 1.0, 2.0);
        car.drivetrain().setClutchPedal(false);
        CarState state = car.step(1.0);
        while (rpm(state) < 5000.0 || state.drivetrain.clutchState != ClutchState::Locked) {
            state = car.step(1.0);
        }
        REQUIRE(car.drivetrain().shift(2));
        return run(car, 1.0, engine().drivetrain.shiftTime_s);
    };
    const std::vector<CarState> flat = shiftAt5000(true);
    const std::vector<CarState> plain = shiftAt5000(false);
    const auto rise = [](const std::vector<CarState>& states) {
        double peak = 0.0;
        for (const CarState& state : states) {
            peak = std::max(peak, rpm(state));
        }
        return peak - rpm(states.front());
    };
    CHECK(flat.front().engine.command.flatShiftActive);
    CHECK(flat.front().engine.command.sparkCut);
    CHECK_FALSE(plain.front().engine.command.flatShiftActive);
    CHECK(rise(flat) < 50.0);
    CHECK(rise(plain) > 300.0);
}

TEST_CASE("shifting checks the gear and changes it after the interruption") {
    CarSimulation car(engine());
    CHECK_FALSE(car.drivetrain().shift(7));
    CHECK(car.drivetrain().shift(1));
    CHECK_FALSE(car.drivetrain().shift(2)); // still shifting
    const std::vector<CarState> states = run(car, 0.0, engine().drivetrain.shiftTime_s + 0.01);
    CHECK(states.front().drivetrain.gear == 0);
    CHECK(states.back().drivetrain.gear == 1);
}

TEST_CASE("a stalled engine restarts once the clutch is pressed") {
    CarSimulation car(engine());
    static_cast<void>(car.drivetrain().shift(1));
    run(car, 0.0, 3.0);
    car.engine().setSpeed_rad_s(0.0);
    car.drivetrain().setClutchPedal(true);
    static_cast<void>(car.step(0.0));
    CHECK(car.engine().speed_rad_s() == engine().idleSpeed_rad_s);
}

TEST_CASE("the roller shows less power at the wheels than the engine dyno") {
    const std::vector<RollerSample> roller = runRoller(engine(), {4, 3000.0, 6800.0, 200.0});
    const std::vector<SweepSample> sweep = runSweep(engine(), {3000.0, 6800.0, 300.0, 1.0, 200.0});
    double wheelPeak = 0.0;
    double enginePeak = 0.0;
    for (const RollerSample& s : roller) {
        wheelPeak = std::max(wheelPeak, s.wheelPower_W());
    }
    for (const SweepSample& s : sweep) {
        enginePeak = std::max(enginePeak, s.power_W());
    }
    CHECK(wheelPeak / enginePeak > 0.85);
    CHECK(wheelPeak / enginePeak < engine().drivetrain.efficiency);
    CHECK_THROWS_AS(static_cast<void>(runRoller(engine(), {0, 3000.0, 6800.0})),
                    std::invalid_argument);
}
