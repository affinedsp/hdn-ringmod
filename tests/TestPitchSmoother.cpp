#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/PitchSmoother.h"
#include <cmath>

TEST_CASE("PitchSmoother: returns 0 before any valid input")
{
    PitchSmoother smoother;
    smoother.prepare(44100.0);
    smoother.setSensitivity(0.5f);

    float result = smoother.process(0.0f, 0.0f);
    REQUIRE(result == 0.0f);
}

TEST_CASE("PitchSmoother: converges to steady-state frequency")
{
    PitchSmoother smoother;
    smoother.prepare(44100.0);
    smoother.setSmoothingAmount(0.1f);
    smoother.setSensitivity(1.0f);

    float lastOutput = 0.0f;
    for (int i = 0; i < 44100; ++i)
        lastOutput = smoother.process(440.0f, 1.0f);

    REQUIRE_THAT(static_cast<double>(lastOutput),
                 Catch::Matchers::WithinAbs(440.0, 1.0));
}

TEST_CASE("PitchSmoother: holds last frequency when confidence drops")
{
    PitchSmoother smoother;
    smoother.prepare(44100.0);
    smoother.setSmoothingAmount(0.0f);
    smoother.setSensitivity(0.5f);

    float output = smoother.process(440.0f, 1.0f);
    REQUIRE_THAT(static_cast<double>(output),
                 Catch::Matchers::WithinAbs(440.0, 0.1));

    float held = smoother.process(0.0f, 0.0f);
    REQUIRE_THAT(static_cast<double>(held),
                 Catch::Matchers::WithinAbs(440.0, 0.1));
}

TEST_CASE("PitchSmoother: releases a stale pitch after invalid input")
{
    PitchSmoother smoother;
    smoother.prepare(44100.0);
    smoother.setSmoothingAmount(0.0f);
    smoother.setSensitivity(0.5f);

    smoother.process(440.0f, 1.0f);
    float held = 0.0f;
    for (int i = 0; i < 2204; ++i)
        held = smoother.process(0.0f, 0.0f);

    REQUIRE(held > 0.0f);
    REQUIRE(smoother.process(0.0f, 0.0f) == 0.0f);
}

TEST_CASE("PitchSmoother: prepare resets state")
{
    PitchSmoother smoother;
    smoother.prepare(44100.0);
    smoother.setSmoothingAmount(0.0f);
    smoother.setSensitivity(1.0f);

    smoother.process(440.0f, 1.0f);

    smoother.prepare(44100.0);
    float result = smoother.process(0.0f, 0.0f);
    REQUIRE(result == 0.0f);
}

TEST_CASE("PitchSmoother: sensitivity threshold gates low-confidence input")
{
    PitchSmoother smoother;
    smoother.prepare(44100.0);
    smoother.setSmoothingAmount(0.0f);
    smoother.setSensitivity(0.5f);

    float result = smoother.process(440.0f, 0.3f);
    REQUIRE(result == 0.0f);
}

TEST_CASE("PitchSmoother: maximum sensitivity rejects low-confidence input")
{
    PitchSmoother smoother;
    smoother.prepare(44100.0);
    smoother.setSmoothingAmount(0.0f);
    smoother.setSensitivity(1.0f);

    float result = smoother.process(440.0f, 0.1f);
    REQUIRE(result == 0.0f);
}

TEST_CASE("PitchSmoother: zero sensitivity accepts low-confidence input")
{
    PitchSmoother smoother;
    smoother.prepare(44100.0);
    smoother.setSmoothingAmount(0.0f);
    smoother.setSensitivity(0.0f);

    float result = smoother.process(440.0f, 0.1f);
    REQUIRE_THAT(static_cast<double>(result),
                 Catch::Matchers::WithinAbs(440.0, 0.1));
}

TEST_CASE("PitchSmoother: midpoint sensitivity accepts only midpoint confidence or higher")
{
    PitchSmoother smoother;
    smoother.prepare(44100.0);
    smoother.setSmoothingAmount(0.0f);
    smoother.setSensitivity(0.5f);

    REQUIRE(smoother.process(440.0f, 0.49f) == 0.0f);

    float result = smoother.process(440.0f, 0.5f);
    REQUIRE_THAT(static_cast<double>(result),
                 Catch::Matchers::WithinAbs(440.0, 0.1));
}

TEST_CASE("PitchSmoother: zero smoothing passes frequency through immediately")
{
    PitchSmoother smoother;
    smoother.prepare(44100.0);
    smoother.setSmoothingAmount(0.0f);
    smoother.setSensitivity(1.0f);

    float result = smoother.process(440.0f, 1.0f);
    REQUIRE_THAT(static_cast<double>(result),
                 Catch::Matchers::WithinAbs(440.0, 0.1));
}

TEST_CASE("PitchSmoother: smoothing slows convergence within a semitone")
{
    PitchSmoother fast, slow;
    fast.prepare(44100.0);
    slow.prepare(44100.0);

    fast.setSmoothingAmount(0.1f);
    fast.setSensitivity(1.0f);
    slow.setSmoothingAmount(0.9f);
    slow.setSensitivity(1.0f);

    fast.process(440.0f, 1.0f);
    slow.process(440.0f, 1.0f);

    float fastOut = 0.0f, slowOut = 0.0f;
    for (int i = 0; i < 100; ++i)
    {
        fastOut = fast.process(443.0f, 1.0f);
        slowOut = slow.process(443.0f, 1.0f);
    }

    REQUIRE(std::abs(fastOut - 443.0f) < std::abs(slowOut - 443.0f));
}

TEST_CASE("PitchSmoother: smoothing operates in log-frequency domain")
{
    PitchSmoother smoother;
    smoother.prepare(44100.0);
    smoother.setSmoothingAmount(0.5f);
    smoother.setSensitivity(1.0f);

    smoother.process(440.0f, 1.0f);

    float output = 0.0f;
    for (int i = 0; i < 100; ++i)
        output = smoother.process(445.0f, 1.0f);

    REQUIRE(output > 440.0f);
    REQUIRE(output < 445.0f);
}

TEST_CASE("PitchSmoother: instant lock on note change")
{
    PitchSmoother smoother;
    smoother.prepare(44100.0);
    smoother.setSmoothingAmount(1.0f);
    smoother.setSensitivity(1.0f);

    smoother.process(440.0f, 1.0f);

    float output = smoother.process(880.0f, 1.0f);
    REQUIRE_THAT(static_cast<double>(output),
                 Catch::Matchers::WithinAbs(880.0, 0.1));
}

TEST_CASE("PitchSmoother: holds during alternating confidence gaps")
{
    PitchSmoother smoother;
    smoother.prepare(44100.0);
    smoother.setSmoothingAmount(0.0f);
    smoother.setSensitivity(0.5f);

    smoother.process(440.0f, 1.0f);

    for (int i = 0; i < 10; ++i)
    {
        float held = smoother.process(0.0f, 0.0f);
        REQUIRE_THAT(static_cast<double>(held),
                     Catch::Matchers::WithinAbs(440.0, 0.1));

        float active = smoother.process(440.0f, 1.0f);
        REQUIRE_THAT(static_cast<double>(active),
                     Catch::Matchers::WithinAbs(440.0, 0.1));
    }
}

TEST_CASE("PitchSmoother: stable output on sustained note")
{
    PitchSmoother smoother;
    smoother.prepare(44100.0);
    smoother.setSmoothingAmount(0.5f);
    smoother.setSensitivity(1.0f);

    for (int i = 0; i < 1000; ++i)
        smoother.process(440.0f, 1.0f);

    float output = smoother.process(440.0f, 1.0f);
    REQUIRE_THAT(static_cast<double>(output),
                 Catch::Matchers::WithinAbs(440.0, 0.01));
}
