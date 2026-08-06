#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <bit>
#include <cstdint>
#include <memory>
#include <vector>
#include "HalfbandDecimator.h"

struct PitchResult
{
    float frequency = 0.0f;
    float confidence = 0.0f;
};

class YinPitchDetector
{
public:
    YinPitchDetector();
    ~YinPitchDetector();

    void prepare(double sampleRate);

    inline void feedSample(float sample)
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite(1, start1, size1, start2, size2);
        if (size1 > 0)
            fifoBuffer[static_cast<size_t>(start1)] = sample;
        else if (size2 > 0)
            fifoBuffer[static_cast<size_t>(start2)] = sample;
        else
            fifoOverflowed.store(true, std::memory_order_release);
        fifo.finishedWrite(size1 + size2);
    }

    inline PitchResult getResult() const
    {
        return unpackResult(atomicResult.load(std::memory_order_acquire));
    }

    void flushForTest();

private:
    struct AnalysisWorkspace
    {
        int halfWindow = 0;
        int windowSize = 0;
        int fftSize = 0;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> linearBuffer;
        std::vector<float> fftInput;
        std::vector<float> fftOutput;
        std::vector<float> diff;
        std::vector<float> cmndf;
    };

    struct PitchCandidate
    {
        PitchResult result;
        bool thresholdQualified = false;
        bool boundary = false;
    };

    void stopAnalysisThread();
    void prepareWorkspace(AnalysisWorkspace& workspace, double minimumFrequency);
    PitchCandidate analyse(AnalysisWorkspace& workspace, int samplesToAnalyse);
    void considerCandidate(const PitchCandidate& candidate);
    void publishResult(PitchResult result);
    void clearResult();

    static uint64_t packResult(PitchResult result)
    {
        auto frequency = std::bit_cast<uint32_t>(result.frequency);
        auto confidence = std::bit_cast<uint32_t>(result.confidence);
        return static_cast<uint64_t>(frequency) | (static_cast<uint64_t>(confidence) << 32);
    }

    static PitchResult unpackResult(uint64_t packed)
    {
        return { std::bit_cast<float>(static_cast<uint32_t>(packed)),
                 std::bit_cast<float>(static_cast<uint32_t>(packed >> 32)) };
    }

    class AnalysisThread;
    friend class AnalysisThread;

    double analysisSR = 44100.0;
    int quickHopSize = 0;
    int guitarAuditSize = 0;
    int lowHopSize = 0;
    int lowAuditSize = 0;
    int maxBacklogSamples = 0;

    std::vector<float> buffer;
    int writePos = 0;
    int activeWindowSize = 0;
    int quickHopCounter = 0;
    int guitarAuditCounter = 0;
    int lowAnalysisCounter = 0;
    float activityEnvelope = 0.0f;
    float activityRelease = 0.995f;

    AnalysisWorkspace quickAnalysis;
    AnalysisWorkspace guitarAnalysis;
    AnalysisWorkspace lowAnalysis;

    PitchResult lastResult;
    PitchCandidate pendingCandidate;
    int pendingCount = 0;
    int missedCandidates = 0;

    static constexpr float threshold = 0.15f;
    static constexpr float silenceThreshold = 1e-5f;
    static constexpr float activityReleaseSeconds = 0.009f;
    static constexpr float quickMinimumFrequency = 200.0f;
    static constexpr float guitarMinimumFrequency = 80.0f;
    static constexpr float lowSelectionMaximumFrequency = 82.0f;
    static constexpr float lowAnalysisMinimumFrequency = 18.0f;
    static constexpr float minimumOutputFrequency = 20.0f;
    static constexpr float maximumOutputFrequency = 5000.0f;

    juce::AbstractFifo fifo { 0 };
    std::vector<float> fifoBuffer;
    std::atomic<bool> fifoOverflowed { false };

    std::unique_ptr<AnalysisThread> analysisThread;

    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    std::atomic<uint64_t> atomicResult { 0 };

    HalfbandDecimator decimator;
};
