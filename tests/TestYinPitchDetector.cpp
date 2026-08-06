#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/YinPitchDetector.h"
#include <cmath>

static constexpr double twoPi = 6.283185307179586476925;
static constexpr int decimationFactor = 2;
static constexpr double minimumTrackedFrequency = 80.0;
static constexpr double hopSeconds = 0.003;

static int computeWindowSize(double sampleRate)
{
    double decimatedSR = sampleRate / decimationFactor;
    int halfWindow = static_cast<int>(std::ceil(decimatedSR / minimumTrackedFrequency));
    return 2 * halfWindow;
}

static int computeHopSize(double sampleRate)
{
    double decimatedSR = sampleRate / decimationFactor;
    return static_cast<int>(std::ceil(decimatedSR * hopSeconds));
}

static void feedSine(YinPitchDetector& yin, double sampleRate, float freq, int numSamples)
{
    double phase = 0.0;
    double inc = twoPi * static_cast<double>(freq) / sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        float sample = static_cast<float>(std::sin(phase));
        yin.feedSample(sample);
        phase += inc;
    }
    yin.flushForTest();
}

static void feedHarmonicComplex(YinPitchDetector& yin, double sampleRate, float freq, int numSamples, float amplitude)
{
    double phase = 0.0;
    double inc = twoPi * static_cast<double>(freq) / sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        double s = std::sin(phase)
                 + 0.8 * std::sin(2.0 * phase)
                 + 0.6 * std::sin(3.0 * phase)
                 + 0.5 * std::sin(4.0 * phase)
                 + 0.3 * std::sin(5.0 * phase);
        yin.feedSample(static_cast<float>(s * static_cast<double>(amplitude)));
        phase += inc;
    }
    yin.flushForTest();
}

static void feedUpperHarmonics(YinPitchDetector& yin, double sampleRate, float freq,
                               int numSamples, double fundamentalAmplitude, double& phase)
{
    double inc = twoPi * static_cast<double>(freq) / sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        double s = fundamentalAmplitude * std::sin(phase)
                 + std::sin(2.0 * phase)
                 + 0.7 * std::sin(3.0 * phase)
                 + 0.4 * std::sin(4.0 * phase);
        yin.feedSample(static_cast<float>(s * 0.2));
        phase += inc;
    }
}

static void feedWeakFundamental(YinPitchDetector& yin, double sampleRate, float freq, int numSamples)
{
    double phase = 0.0;
    feedUpperHarmonics(yin, sampleRate, freq, numSamples, 0.2, phase);
    yin.flushForTest();
}

static bool withinCents(float actual, float expected, float cents)
{
    return actual > 0.0f
        && std::abs(std::log2(actual / expected)) <= cents / 1200.0f;
}

static int feedSineUntilDetection(YinPitchDetector& yin, double sampleRate, float freq, int maxSamples)
{
    double phase = 0.0;
    double inc = twoPi * static_cast<double>(freq) / sampleRate;
    int hopOriginal = computeHopSize(sampleRate) * decimationFactor;
    int totalFed = 0;

    while (totalFed < maxSamples)
    {
        int toFeed = std::min(hopOriginal, maxSamples - totalFed);
        for (int i = 0; i < toFeed; ++i)
        {
            float sample = static_cast<float>(std::sin(phase));
            yin.feedSample(sample);
            phase += inc;
        }
        totalFed += toFeed;
        yin.flushForTest();

        if (yin.getResult().frequency > 0.0f)
            return totalFed;
    }
    return maxSamples;
}

TEST_CASE("YIN: detects 440 Hz sine at 44100 Hz")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    feedSine(yin, 44100.0, 440.0f, 44100);

    auto result = yin.getResult();
    REQUIRE(result.frequency > 0.0f);
    REQUIRE_THAT(static_cast<double>(result.frequency),
                 Catch::Matchers::WithinRel(440.0, 0.01));
    REQUIRE(result.confidence > 0.5f);
}

TEST_CASE("YIN: detects 220 Hz sine at 44100 Hz")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    feedSine(yin, 44100.0, 220.0f, 44100);

    auto result = yin.getResult();
    REQUIRE(result.frequency > 0.0f);
    REQUIRE_THAT(static_cast<double>(result.frequency),
                 Catch::Matchers::WithinRel(220.0, 0.01));
    REQUIRE(result.confidence > 0.5f);
}

TEST_CASE("YIN: parabolic interpolation is accurate for 110 Hz sine")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    feedSine(yin, 44100.0, 110.0f, 44100);

    auto result = yin.getResult();
    REQUIRE(result.frequency > 0.0f);
    REQUIRE_THAT(static_cast<double>(result.frequency),
                 Catch::Matchers::WithinAbs(110.0, 0.2));
    REQUIRE(result.confidence > 0.5f);
}

TEST_CASE("YIN: detects E2 (82.4 Hz) at 44100 Hz")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    feedSine(yin, 44100.0, 82.4f, 44100);

    auto result = yin.getResult();
    REQUIRE(result.frequency > 0.0f);
    REQUIRE_THAT(static_cast<double>(result.frequency),
                 Catch::Matchers::WithinRel(82.4, 0.03));
}

TEST_CASE("YIN: detects eight-string and bass-range fundamentals")
{
    constexpr float frequencies[] { 81.0f, 80.0f, 79.0f, 46.25f, 41.20f, 30.87f, 23.12f, 20.0f };

    for (float frequency : frequencies)
    {
        YinPitchDetector yin;
        yin.prepare(44100.0);

        feedSine(yin, 44100.0, frequency, 44100);

        auto result = yin.getResult();
        CAPTURE(frequency, result.frequency, result.confidence);
        REQUIRE(result.frequency > 0.0f);
        REQUIRE_THAT(static_cast<double>(result.frequency),
                     Catch::Matchers::WithinRel(static_cast<double>(frequency), 0.03));
        REQUIRE(result.confidence > 0.8f);
    }
}

TEST_CASE("YIN: low tracking is stable across phase and level")
{
    constexpr double phases[] { 0.0, 0.7, 1.5707963267948966 };
    constexpr double amplitudes[] { 0.5, 0.01, 0.001 };

    for (double initialPhase : phases)
    {
        for (double amplitude : amplitudes)
        {
            YinPitchDetector yin;
            yin.prepare(44100.0);

            double phase = initialPhase;
            double increment = twoPi * 46.25 / 44100.0;
            for (int i = 0; i < 13230; ++i)
            {
                yin.feedSample(static_cast<float>(amplitude * std::sin(phase)));
                phase += increment;
            }
            yin.flushForTest();

            auto result = yin.getResult();
            CAPTURE(initialPhase, amplitude, result.frequency, result.confidence);
            REQUIRE_THAT(static_cast<double>(result.frequency),
                         Catch::Matchers::WithinRel(46.25, 0.03));
            REQUIRE(result.confidence > 0.8f);
        }
    }
}

TEST_CASE("YIN: resolution boundary is covered across sample rates and phases")
{
    constexpr double sampleRates[] { 44100.0, 48000.0, 96000.0 };
    constexpr double phases[] { 0.0, 0.7 };

    for (double sampleRate : sampleRates)
    {
        for (double initialPhase : phases)
        {
            YinPitchDetector yin;
            yin.prepare(sampleRate);

            double phase = initialPhase;
            double increment = twoPi * 80.0 / sampleRate;
            for (int i = 0; i < static_cast<int>(sampleRate * 0.3); ++i)
            {
                yin.feedSample(static_cast<float>(0.2 * std::sin(phase)));
                phase += increment;
            }
            yin.flushForTest();

            auto result = yin.getResult();
            CAPTURE(sampleRate, initialPhase, result.frequency, result.confidence);
            REQUIRE_THAT(static_cast<double>(result.frequency),
                         Catch::Matchers::WithinRel(80.0, 0.03));
            REQUIRE(result.confidence > 0.8f);
        }
    }
}

TEST_CASE("YIN: low tracking tolerates detector-path DC offset")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    double phase = 0.0;
    double increment = twoPi * 46.25 / 44100.0;
    for (int i = 0; i < 13230; ++i)
    {
        yin.feedSample(static_cast<float>(0.5 + 0.01 * std::sin(phase)));
        phase += increment;
    }
    yin.flushForTest();

    auto result = yin.getResult();
    REQUIRE_THAT(static_cast<double>(result.frequency),
                 Catch::Matchers::WithinRel(46.25, 0.03));
    REQUIRE(result.confidence > 0.8f);
}

TEST_CASE("YIN: detects F sharp 1 across supported sample rates")
{
    constexpr double sampleRates[] { 44100.0, 48000.0, 96000.0 };

    for (double sampleRate : sampleRates)
    {
        YinPitchDetector yin;
        yin.prepare(sampleRate);

        feedSine(yin, sampleRate, 46.25f, static_cast<int>(sampleRate));

        auto result = yin.getResult();
        CAPTURE(sampleRate, result.frequency, result.confidence);
        REQUIRE_THAT(static_cast<double>(result.frequency),
                     Catch::Matchers::WithinRel(46.25, 0.03));
        REQUIRE(result.confidence > 0.8f);
    }
}

TEST_CASE("YIN: detects 880 Hz sine at 48000 Hz")
{
    YinPitchDetector yin;
    yin.prepare(48000.0);

    feedSine(yin, 48000.0, 880.0f, 48000);

    auto result = yin.getResult();
    REQUIRE(result.frequency > 0.0f);
    REQUIRE_THAT(static_cast<double>(result.frequency),
                 Catch::Matchers::WithinRel(880.0, 0.03));
    REQUIRE(result.confidence > 0.5f);
}

TEST_CASE("YIN: detects pitch at 96000 Hz sample rate")
{
    YinPitchDetector yin;
    yin.prepare(96000.0);

    feedSine(yin, 96000.0, 440.0f, 96000);

    auto result = yin.getResult();
    REQUIRE(result.frequency > 0.0f);
    REQUIRE_THAT(static_cast<double>(result.frequency),
                 Catch::Matchers::WithinRel(440.0, 0.03));
    REQUIRE(result.confidence > 0.5f);
}

TEST_CASE("YIN: returns zero for silence")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    for (int i = 0; i < 44100; ++i)
        yin.feedSample(0.0f);
    yin.flushForTest();

    auto result = yin.getResult();
    REQUIRE(result.frequency == 0.0f);
}

TEST_CASE("YIN: prepare resets state")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    feedSine(yin, 44100.0, 440.0f, 44100);
    REQUIRE(yin.getResult().frequency > 0.0f);

    yin.prepare(44100.0);
    REQUIRE(yin.getResult().frequency == 0.0f);
}

TEST_CASE("YIN: rejects out-of-range frequencies")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    feedSine(yin, 44100.0, 10.0f, 44100);

    auto result = yin.getResult();
    REQUIRE(result.frequency == 0.0f);
}

TEST_CASE("YIN: confidence is clamped to [0, 1]")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    feedSine(yin, 44100.0, 440.0f, 44100);

    auto result = yin.getResult();
    REQUIRE(result.confidence >= 0.0f);
    REQUIRE(result.confidence <= 1.0f);
}

TEST_CASE("YIN: first detection within one window fill")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    int winOriginal = computeWindowSize(44100.0) * decimationFactor;
    int hopOriginal = computeHopSize(44100.0) * decimationFactor;
    int samplesNeeded = feedSineUntilDetection(yin, 44100.0, 440.0f, 44100);

    REQUIRE(samplesNeeded <= winOriginal + hopOriginal);
    REQUIRE(yin.getResult().frequency > 0.0f);
}

TEST_CASE("YIN: first 440 Hz detection is under 20 ms")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    feedSine(yin, 44100.0, 440.0f, 882);

    auto result = yin.getResult();
    REQUIRE(result.frequency > 0.0f);
    REQUIRE_THAT(static_cast<double>(result.frequency),
                 Catch::Matchers::WithinRel(440.0, 0.03));
}

TEST_CASE("YIN: locks to 440 Hz within 20 ms after silence")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    for (int i = 0; i < 44100; ++i)
        yin.feedSample(0.0f);
    yin.flushForTest();

    REQUIRE(yin.getResult().frequency == 0.0f);

    feedSine(yin, 44100.0, 440.0f, 882);

    auto result = yin.getResult();
    REQUIRE(result.frequency > 0.0f);
    REQUIRE_THAT(static_cast<double>(result.frequency),
                 Catch::Matchers::WithinRel(440.0, 0.03));
}

TEST_CASE("YIN: detects E2 within 30 ms")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    feedSine(yin, 44100.0, 82.4f, 1323);

    auto result = yin.getResult();
    REQUIRE(result.frequency > 0.0f);
    REQUIRE_THAT(static_cast<double>(result.frequency),
                 Catch::Matchers::WithinRel(82.4, 0.03));
}

TEST_CASE("YIN: detects pitch change after silence")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    int winOriginal = computeWindowSize(44100.0) * decimationFactor;
    int hopOriginal = computeHopSize(44100.0) * decimationFactor;

    for (int i = 0; i < winOriginal * 2; ++i)
        yin.feedSample(0.0f);
    yin.flushForTest();

    REQUIRE(yin.getResult().frequency == 0.0f);

    int samplesNeeded = feedSineUntilDetection(yin, 44100.0, 440.0f, 44100);

    REQUIRE(samplesNeeded <= winOriginal + hopOriginal);

    auto result = yin.getResult();
    REQUIRE(result.frequency > 0.0f);
    REQUIRE_THAT(static_cast<double>(result.frequency),
                 Catch::Matchers::WithinRel(440.0, 0.03));
}

TEST_CASE("YIN: tracks frequency sweep across hop intervals")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    int winOriginal = computeWindowSize(44100.0) * decimationFactor;

    feedSine(yin, 44100.0, 440.0f, winOriginal + 4000);

    auto resultBefore = yin.getResult();
    REQUIRE(resultBefore.frequency > 0.0f);
    REQUIRE_THAT(static_cast<double>(resultBefore.frequency),
                 Catch::Matchers::WithinRel(440.0, 0.03));

    feedSine(yin, 44100.0, 880.0f, winOriginal + 4000);

    auto resultAfter = yin.getResult();
    REQUIRE(resultAfter.frequency > 0.0f);
    REQUIRE_THAT(static_cast<double>(resultAfter.frequency),
                 Catch::Matchers::WithinRel(880.0, 0.05));
}

TEST_CASE("YIN: fallback detects harmonically complex low signal")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    feedHarmonicComplex(yin, 44100.0, 82.4f, 44100, 0.15f);

    auto result = yin.getResult();
    REQUIRE(result.frequency > 0.0f);
    REQUIRE(result.frequency > 40.0f);
    REQUIRE(result.frequency < 200.0f);
}

TEST_CASE("YIN: detects a weak fundamental on the lowest eight-string note")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    feedWeakFundamental(yin, 44100.0, 46.25f, 44100);

    auto result = yin.getResult();
    REQUIRE_THAT(static_cast<double>(result.frequency),
                 Catch::Matchers::WithinRel(46.25, 0.03));
    REQUIRE(result.confidence > 0.8f);
}

TEST_CASE("YIN: detects a missing fundamental on the lowest eight-string note")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    double phase = 0.0;
    feedUpperHarmonics(yin, 44100.0, 46.25f, 44100, 0.0, phase);
    yin.flushForTest();

    auto result = yin.getResult();
    REQUIRE_THAT(static_cast<double>(result.frequency),
                 Catch::Matchers::WithinRel(46.25, 0.03));
    REQUIRE(result.confidence > 0.8f);
}

TEST_CASE("YIN: rejects periodic input below the supported range")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    feedSine(yin, 44100.0, 15.0f, 44100);

    auto result = yin.getResult();
    REQUIRE(result.frequency == 0.0f);
    REQUIRE(result.confidence == 0.0f);
}

TEST_CASE("YIN: acquires F sharp 1 within two periods plus one low-resolution hop")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    int samplesNeeded = feedSineUntilDetection(yin, 44100.0, 46.25f, 6615);
    int bound = static_cast<int>(std::ceil(2.0 * 44100.0 / 46.25))
              + static_cast<int>(std::ceil(44100.0 * 0.012));

    CAPTURE(samplesNeeded, bound);
    REQUIRE(samplesNeeded <= bound);
    REQUIRE_THAT(static_cast<double>(yin.getResult().frequency),
                 Catch::Matchers::WithinRel(46.25, 0.03));
}

TEST_CASE("YIN: continuous fast pitch step does not publish intermediate guesses")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    double phase = 0.0;
    feedUpperHarmonics(yin, 44100.0, 164.81f, 11025, 0.35, phase);
    yin.flushForTest();
    REQUIRE(withinCents(yin.getResult().frequency, 164.81f, 50.0f));

    bool reachedTarget = false;
    for (int chunk = 0; chunk < 15; ++chunk)
    {
        feedUpperHarmonics(yin, 44100.0, 110.0f, 132, 0.35, phase);
        yin.flushForTest();

        auto result = yin.getResult();
        CAPTURE(chunk, result.frequency, result.confidence);
        bool heldPrevious = withinCents(result.frequency, 164.81f, 50.0f);
        bool reachedNew = withinCents(result.frequency, 110.0f, 50.0f);
        REQUIRE((heldPrevious || reachedNew));
        if (chunk == 0)
            REQUIRE(heldPrevious);
        reachedTarget = reachedTarget || reachedNew;
    }

    REQUIRE(reachedTarget);
}

TEST_CASE("YIN: low-to-high pitch step switches without octave glitches")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    double phase = 0.0;
    feedUpperHarmonics(yin, 44100.0, 46.25f, 11025, 0.2, phase);
    yin.flushForTest();
    REQUIRE(withinCents(yin.getResult().frequency, 46.25f, 50.0f));

    bool reachedTarget = false;
    for (int chunk = 0; chunk < 7; ++chunk)
    {
        feedUpperHarmonics(yin, 44100.0, 220.0f, 132, 0.2, phase);
        yin.flushForTest();

        auto result = yin.getResult();
        CAPTURE(chunk, result.frequency, result.confidence);
        bool heldPrevious = withinCents(result.frequency, 46.25f, 50.0f);
        bool reachedNew = withinCents(result.frequency, 220.0f, 50.0f);
        REQUIRE((heldPrevious || reachedNew));
        if (chunk == 0)
            REQUIRE(heldPrevious);
        reachedTarget = reachedTarget || reachedNew;
    }

    REQUIRE(reachedTarget);
}

TEST_CASE("YIN: direct step to tracking floor reacquires without an octave guess")
{
    constexpr double sampleRates[] { 44100.0, 48000.0, 96000.0 };

    for (double sampleRate : sampleRates)
    {
        YinPitchDetector yin;
        yin.prepare(sampleRate);

        double phase = 0.0;
        feedUpperHarmonics(yin, sampleRate, 46.25f,
                           static_cast<int>(sampleRate * 0.25), 0.2, phase);
        yin.flushForTest();
        REQUIRE(withinCents(yin.getResult().frequency, 46.25f, 50.0f));

        bool reachedTarget = false;
        int chunkSize = static_cast<int>(std::ceil(sampleRate * hopSeconds));
        for (int chunk = 0; chunk < 40; ++chunk)
        {
            feedUpperHarmonics(yin, sampleRate, 20.0f, chunkSize, 0.2, phase);
            yin.flushForTest();

            auto result = yin.getResult();
            CAPTURE(sampleRate, chunk, result.frequency, result.confidence);
            bool heldPrevious = withinCents(result.frequency, 46.25f, 50.0f);
            bool reachedNew = withinCents(result.frequency, 20.0f, 50.0f);
            REQUIRE((heldPrevious || reachedNew));
            reachedTarget = reachedTarget || reachedNew;
        }

        CAPTURE(sampleRate);
        REQUIRE(reachedTarget);
    }
}

TEST_CASE("YIN: tracked low pitch releases after an unsupported downward step")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    double phase = 0.0;
    feedUpperHarmonics(yin, 44100.0, 46.25f, 11025, 0.2, phase);
    yin.flushForTest();
    REQUIRE(withinCents(yin.getResult().frequency, 46.25f, 50.0f));

    bool released = false;
    for (int chunk = 0; chunk < 15; ++chunk)
    {
        feedUpperHarmonics(yin, 44100.0, 15.0f, 528, 0.2, phase);
        yin.flushForTest();

        auto result = yin.getResult();
        CAPTURE(chunk, result.frequency, result.confidence);
        REQUIRE((result.frequency == 0.0f
                 || withinCents(result.frequency, 46.25f, 50.0f)));
        released = released || result.frequency == 0.0f;
    }

    REQUIRE(released);
}

TEST_CASE("YIN: fast downward pitch bend does not jump upward")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    double phase = 0.0;
    feedUpperHarmonics(yin, 44100.0, 220.0f, 11025, 0.2, phase);
    yin.flushForTest();

    float previousPublished = yin.getResult().frequency;
    constexpr int chunkSize = 132;
    constexpr int chunks = 40;
    for (int chunk = 0; chunk < chunks; ++chunk)
    {
        for (int i = 0; i < chunkSize; ++i)
        {
            double progress = static_cast<double>(chunk * chunkSize + i)
                            / static_cast<double>(chunks * chunkSize - 1);
            double frequency = 220.0 * std::pow(46.25 / 220.0, progress);
            double sample = 0.2 * std::sin(phase)
                          + std::sin(2.0 * phase)
                          + 0.7 * std::sin(3.0 * phase);
            yin.feedSample(static_cast<float>(sample * 0.2));
            phase += twoPi * frequency / 44100.0;
        }
        yin.flushForTest();

        auto result = yin.getResult();
        CAPTURE(chunk, previousPublished, result.frequency, result.confidence);
        if (result.frequency > 0.0f)
        {
            REQUIRE(result.frequency <= previousPublished * 1.08f);
            previousPublished = result.frequency;
        }
    }

    feedUpperHarmonics(yin, 44100.0, 46.25f, 6615, 0.2, phase);
    yin.flushForTest();
    REQUIRE(withinCents(yin.getResult().frequency, 46.25f, 50.0f));
}

TEST_CASE("YIN: fast bend below eight-string range keeps tracking downward")
{
    constexpr double sampleRates[] { 44100.0, 48000.0, 96000.0 };
    constexpr int chunks = 40;

    for (double sampleRate : sampleRates)
    {
        YinPitchDetector yin;
        yin.prepare(sampleRate);

        double phase = 0.0;
        feedUpperHarmonics(yin, sampleRate, 46.25f,
                           static_cast<int>(sampleRate * 0.25), 0.2, phase);
        yin.flushForTest();

        int chunkSize = static_cast<int>(std::ceil(sampleRate * hopSeconds));
        float previousPublished = yin.getResult().frequency;
        float halfwayResult = previousPublished;
        float threeQuarterResult = previousPublished;
        int downwardUpdates = 0;
        for (int chunk = 0; chunk < chunks; ++chunk)
        {
            for (int i = 0; i < chunkSize; ++i)
            {
                double progress = static_cast<double>(chunk * chunkSize + i)
                                / static_cast<double>(chunks * chunkSize - 1);
                double frequency = 46.25 * std::pow(30.0 / 46.25, progress);
                double sample = 0.2 * std::sin(phase)
                              + std::sin(2.0 * phase)
                              + 0.7 * std::sin(3.0 * phase);
                yin.feedSample(static_cast<float>(sample * 0.2));
                phase += twoPi * frequency / sampleRate;
            }
            yin.flushForTest();

            auto result = yin.getResult();
            CAPTURE(sampleRate, chunk, previousPublished,
                    result.frequency, result.confidence);
            if (result.frequency > 0.0f)
            {
                REQUIRE(result.frequency <= previousPublished * 1.08f);
                if (result.frequency < previousPublished * std::exp2(-10.0f / 1200.0f))
                    ++downwardUpdates;
                previousPublished = result.frequency;
            }
            if (chunk == chunks / 2 - 1)
                halfwayResult = result.frequency;
            if (chunk == 3 * chunks / 4 - 1)
                threeQuarterResult = result.frequency;
        }

        CAPTURE(sampleRate, halfwayResult, threeQuarterResult, downwardUpdates);
        REQUIRE(halfwayResult < 46.0f);
        REQUIRE(threeQuarterResult < 43.0f);
        REQUIRE(downwardUpdates >= 2);

        auto bentResult = yin.getResult();
        CAPTURE(sampleRate, bentResult.frequency, bentResult.confidence);
        REQUIRE(bentResult.frequency < 40.0f);
        REQUIRE(bentResult.frequency > 25.0f);

        feedUpperHarmonics(yin, sampleRate, 30.0f,
                           static_cast<int>(sampleRate * 0.1), 0.2, phase);
        yin.flushForTest();
        REQUIRE(withinCents(yin.getResult().frequency, 30.0f, 50.0f));
    }
}

TEST_CASE("YIN: silence still returns zero with fallback")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    for (int i = 0; i < 44100; ++i)
        yin.feedSample(0.0f);
    yin.flushForTest();

    auto result = yin.getResult();
    REQUIRE(result.frequency == 0.0f);
    REQUIRE(result.confidence == 0.0f);
}

TEST_CASE("YIN: threshold path still preferred for clean signals")
{
    YinPitchDetector yin;
    yin.prepare(44100.0);

    feedSine(yin, 44100.0, 440.0f, 44100);

    auto result = yin.getResult();
    REQUIRE(result.confidence > 0.8f);
    REQUIRE_THAT(static_cast<double>(result.frequency),
                 Catch::Matchers::WithinRel(440.0, 0.01));
}
