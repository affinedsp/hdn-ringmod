#include "YinPitchDetector.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace
{
constexpr float candidateToleranceOctaves = 50.0f / 1200.0f;
constexpr float movingCandidateToleranceOctaves = 150.0f / 1200.0f;
constexpr float quickPathFrequency = 300.0f;
constexpr float strongCandidateConfidence = 0.9f;
constexpr float qualifiedCandidateConfidence = 0.8f;
constexpr float fallbackCandidateConfidence = 0.75f;
constexpr float confidenceSelectionMargin = 0.1f;
constexpr float guitarSelectionMargin = 0.05f;
constexpr float lowTrackingRangeRatio = 0.6f;
constexpr int maximumWorkerBatchSize = 512;
constexpr int lowCandidateMissLimit = 8;

bool hasPitch(PitchResult result)
{
    return result.frequency > 0.0f && result.confidence > 0.0f;
}

bool frequenciesAreWithin(float first, float second, float toleranceOctaves)
{
    if (first <= 0.0f || second <= 0.0f)
        return false;

    return std::abs(std::log2(first / second)) <= toleranceOctaves;
}

bool frequenciesAreClose(float first, float second)
{
    return frequenciesAreWithin(first, second, candidateToleranceOctaves);
}
}

class YinPitchDetector::AnalysisThread : public juce::Thread
{
public:
    AnalysisThread(YinPitchDetector& owner)
        : Thread("PitchAnalysis"), o(owner) {}

    void run() override
    {
        while (!threadShouldExit())
        {
            if (o.fifoOverflowed.exchange(false, std::memory_order_acq_rel))
            {
                discardSamples(o.fifo.getNumReady());
                resetStreamState();
                continue;
            }

            int ready = o.fifo.getNumReady();
            if (ready > o.maxBacklogSamples)
            {
                discardSamples(ready - o.maxBacklogSamples);
                resetStreamState();
                ready = o.fifo.getNumReady();
            }

            if (ready == 0)
            {
                wait(1);
                continue;
            }

            ready = std::min(ready, maximumWorkerBatchSize);
            int start1, size1, start2, size2;
            o.fifo.prepareToRead(ready, start1, size1, start2, size2);

            int processed = 0;
            bool overflowed = false;
            for (int i = 0; i < size1 && !threadShouldExit(); ++i)
            {
                if (o.fifoOverflowed.load(std::memory_order_acquire))
                {
                    overflowed = true;
                    break;
                }
                processSample(o.fifoBuffer[static_cast<size_t>(start1 + i)]);
                ++processed;
            }
            for (int i = 0; i < size2 && !threadShouldExit() && !overflowed; ++i)
            {
                if (o.fifoOverflowed.load(std::memory_order_acquire))
                {
                    overflowed = true;
                    break;
                }
                processSample(o.fifoBuffer[static_cast<size_t>(start2 + i)]);
                ++processed;
            }

            o.fifo.finishedRead(processed);
            if (overflowed || o.fifoOverflowed.exchange(false, std::memory_order_acq_rel))
            {
                discardSamples(o.fifo.getNumReady());
                resetStreamState();
            }
        }
    }

private:
    void discardSamples(int count)
    {
        int start1, size1, start2, size2;
        o.fifo.prepareToRead(count, start1, size1, start2, size2);
        o.fifo.finishedRead(size1 + size2);
    }

    void resetStreamState()
    {
        o.decimator.reset();
        o.activeWindowSize = 0;
        o.quickHopCounter = 0;
        o.guitarAuditCounter = 0;
        o.lowAnalysisCounter = 0;
        o.activityEnvelope = 0.0f;
        o.clearResult();
    }

    void processSample(float sample)
    {
        if (!std::isfinite(sample))
            sample = 0.0f;

        if (!o.decimator.processSample(sample))
            return;

        float decimated = o.decimator.getOutput();
        o.buffer[static_cast<size_t>(o.writePos)] = decimated;
        if (++o.writePos >= static_cast<int>(o.buffer.size()))
            o.writePos = 0;

        o.activityEnvelope = std::max(std::abs(decimated), o.activityEnvelope * o.activityRelease);
        if (o.activityEnvelope < o.silenceThreshold)
        {
            o.activeWindowSize = 0;
            o.quickHopCounter = 0;
            o.guitarAuditCounter = 0;
            o.lowAnalysisCounter = 0;
            o.clearResult();
            return;
        }

        o.activeWindowSize = std::min(o.activeWindowSize + 1, static_cast<int>(o.buffer.size()));
        o.quickHopCounter = std::min(o.quickHopCounter + 1, o.quickHopSize);
        o.guitarAuditCounter = std::min(o.guitarAuditCounter + 1, o.guitarAuditSize);
        o.lowAnalysisCounter = std::min(o.lowAnalysisCounter + 1, o.lowAuditSize);

        if (o.activeWindowSize < o.quickAnalysis.windowSize || o.quickHopCounter < o.quickHopSize)
            return;

        o.quickHopCounter = 0;
        auto quick = o.analyse(o.quickAnalysis, o.quickAnalysis.windowSize);
        auto selected = quick;

        bool quickNeedsGuitar = !hasPitch(quick.result)
                             || quick.boundary
                             || !quick.thresholdQualified
                             || quick.result.confidence < strongCandidateConfidence
                             || quick.result.frequency < quickPathFrequency
                             || (o.lastResult.frequency > 0.0f
                                 && o.lastResult.frequency < quickPathFrequency);

        PitchCandidate guitar;
        bool ranGuitar = o.activeWindowSize >= o.guitarAnalysis.windowSize
                      && (quickNeedsGuitar || o.guitarAuditCounter >= o.guitarAuditSize);

        if (ranGuitar)
        {
            o.guitarAuditCounter = 0;
            guitar = o.analyse(o.guitarAnalysis, o.guitarAnalysis.windowSize);

            if (hasPitch(guitar.result) && !guitar.boundary)
            {
                if (!hasPitch(quick.result) || quick.boundary)
                {
                    selected = guitar;
                }
                else if (frequenciesAreClose(quick.result.frequency, guitar.result.frequency))
                {
                    selected = guitar.result.confidence >= quick.result.confidence ? guitar : quick;
                }
                else if ((guitar.thresholdQualified && !quick.thresholdQualified)
                      || (guitar.result.frequency < quickPathFrequency
                          && guitar.result.confidence + guitarSelectionMargin
                             >= quick.result.confidence))
                {
                    selected = guitar;
                }
            }
        }

        bool selectedNeedsLow = !hasPitch(selected.result)
                             || selected.boundary
                             || !selected.thresholdQualified
                             || selected.result.confidence < qualifiedCandidateConfidence
                             || selected.result.frequency <= lowSelectionMaximumFrequency
                             || (o.lastResult.frequency > 0.0f
                                 && o.lastResult.frequency <= lowSelectionMaximumFrequency);
        int lowInterval = selectedNeedsLow ? o.lowHopSize : o.lowAuditSize;

        bool ranLow = false;
        if (o.activeWindowSize > o.guitarAnalysis.windowSize
            && o.lowAnalysisCounter >= lowInterval)
        {
            ranLow = true;
            o.lowAnalysisCounter = 0;

            int lowWindowSize = std::min(o.activeWindowSize, o.lowAnalysis.windowSize);
            float lowReference = o.lastResult.frequency <= lowSelectionMaximumFrequency
                               ? o.lastResult.frequency
                               : o.pendingCandidate.result.frequency;
            if (lowReference > 0.0f
                && lowReference <= lowSelectionMaximumFrequency
                && o.missedCandidates == 0
                && (!hasPitch(o.pendingCandidate.result)
                    || o.pendingCandidate.result.frequency
                       >= lowReference * lowTrackingRangeRatio))
            {
                float trackingMinimum = std::max(minimumOutputFrequency,
                                                 lowReference * lowTrackingRangeRatio);
                int trackingHalfWindow = static_cast<int>(std::ceil(o.analysisSR
                                                                   / trackingMinimum));
                lowWindowSize = std::min(lowWindowSize, 2 * trackingHalfWindow);
            }

            auto low = o.analyse(o.lowAnalysis, lowWindowSize);

            bool trustedLow = hasPitch(low.result)
                           && !low.boundary
                           && low.result.frequency <= lowSelectionMaximumFrequency
                           && ((low.thresholdQualified
                                && low.result.confidence >= qualifiedCandidateConfidence)
                               || low.result.confidence >= fallbackCandidateConfidence);

            if (trustedLow)
            {
                bool shortResultClearlyStronger = hasPitch(selected.result)
                                               && selected.thresholdQualified
                                               && selected.result.confidence
                                                  > low.result.confidence + confidenceSelectionMargin;
                if (!shortResultClearlyStronger)
                    selected = low;
            }
        }

        bool retainingLowCandidate = (o.lastResult.frequency > 0.0f
                                    && o.lastResult.frequency <= lowSelectionMaximumFrequency)
                                  || (o.pendingCandidate.result.frequency > 0.0f
                                      && o.pendingCandidate.result.frequency <= lowSelectionMaximumFrequency);
        if (retainingLowCandidate
            && !ranLow
            && (!hasPitch(selected.result)
                || !selected.thresholdQualified
                || selected.result.confidence < strongCandidateConfidence))
            return;

        o.considerCandidate(selected);
    }

    YinPitchDetector& o;
};

YinPitchDetector::YinPitchDetector() = default;

YinPitchDetector::~YinPitchDetector()
{
    stopAnalysisThread();
}

void YinPitchDetector::stopAnalysisThread()
{
    if (!analysisThread)
        return;

    analysisThread->signalThreadShouldExit();
    analysisThread->notify();
    if (!analysisThread->waitForThreadToExit(1000))
        analysisThread->stopThread(-1);
    analysisThread.reset();
}

void YinPitchDetector::prepare(double sampleRate)
{
    stopAnalysisThread();

    analysisSR = sampleRate / 2.0;
    activityRelease = std::exp(-1.0f / (static_cast<float>(analysisSR) * activityReleaseSeconds));
    decimator.reset();

    prepareWorkspace(quickAnalysis, quickMinimumFrequency);
    prepareWorkspace(guitarAnalysis, guitarMinimumFrequency);
    prepareWorkspace(lowAnalysis, lowAnalysisMinimumFrequency);

    buffer.assign(static_cast<size_t>(lowAnalysis.windowSize), 0.0f);

    quickHopSize = static_cast<int>(std::ceil(analysisSR * 0.003));
    guitarAuditSize = static_cast<int>(std::ceil(analysisSR * 0.012));
    lowHopSize = static_cast<int>(std::ceil(analysisSR * 0.012));
    lowAuditSize = static_cast<int>(std::ceil(analysisSR * 0.096));
    maxBacklogSamples = static_cast<int>(std::ceil(sampleRate * 0.25));

    int fifoSize = std::max(8192, static_cast<int>(sampleRate * 2.5));
    fifo.setTotalSize(fifoSize);
    fifoBuffer.resize(static_cast<size_t>(fifoSize));

    fifoOverflowed.store(false, std::memory_order_relaxed);
    writePos = 0;
    activeWindowSize = 0;
    quickHopCounter = 0;
    guitarAuditCounter = 0;
    lowAnalysisCounter = 0;
    activityEnvelope = 0.0f;
    clearResult();

    analysisThread = std::make_unique<AnalysisThread>(*this);
    analysisThread->startThread(juce::Thread::Priority::normal);
}

void YinPitchDetector::prepareWorkspace(AnalysisWorkspace& workspace, double minimumFrequency)
{
    workspace.halfWindow = static_cast<int>(std::ceil(analysisSR / minimumFrequency));
    workspace.windowSize = 2 * workspace.halfWindow;

    int requiredFftSize = 2 * workspace.halfWindow;
    int fftOrder = static_cast<int>(std::ceil(std::log2(static_cast<double>(requiredFftSize))));
    workspace.fftSize = 1 << fftOrder;
    workspace.fft = std::make_unique<juce::dsp::FFT>(fftOrder);

    workspace.linearBuffer.resize(static_cast<size_t>(workspace.windowSize));
    workspace.fftInput.resize(static_cast<size_t>(workspace.fftSize * 2), 0.0f);
    workspace.fftOutput.resize(static_cast<size_t>(workspace.fftSize * 2), 0.0f);
    workspace.diff.resize(static_cast<size_t>(workspace.halfWindow));
    workspace.cmndf.resize(static_cast<size_t>(workspace.halfWindow));
}

void YinPitchDetector::flushForTest()
{
    if (analysisThread)
        analysisThread->notify();
    while (fifo.getNumReady() > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

YinPitchDetector::PitchCandidate YinPitchDetector::analyse(AnalysisWorkspace& workspace,
                                                            int samplesToAnalyse)
{
    PitchCandidate candidate;
    int activeHalfWindow = std::clamp(samplesToAnalyse / 2, 2, workspace.halfWindow);
    int activeWindow = 2 * activeHalfWindow;
    auto n = static_cast<size_t>(activeHalfWindow);

    int start = writePos - activeWindow;
    if (start < 0)
        start += static_cast<int>(buffer.size());

    int tail = std::min(activeWindow, static_cast<int>(buffer.size()) - start);
    std::copy_n(buffer.data() + start, tail, workspace.linearBuffer.data());
    std::copy_n(buffer.data(), activeWindow - tail, workspace.linearBuffer.data() + tail);

    juce::FloatVectorOperations::clear(workspace.fftInput.data(), workspace.fftSize * 2);
    std::copy_n(workspace.linearBuffer.data(), activeHalfWindow, workspace.fftInput.data());
    workspace.fft->performRealOnlyForwardTransform(workspace.fftInput.data(), true);

    juce::FloatVectorOperations::clear(workspace.fftOutput.data(), workspace.fftSize * 2);
    std::copy_n(workspace.linearBuffer.data(), activeWindow, workspace.fftOutput.data());
    workspace.fft->performRealOnlyForwardTransform(workspace.fftOutput.data(), true);

    for (int k = 0; k <= workspace.fftSize / 2; ++k)
    {
        float aRe = workspace.fftInput[static_cast<size_t>(2 * k)];
        float aIm = workspace.fftInput[static_cast<size_t>(2 * k + 1)];
        float bRe = workspace.fftOutput[static_cast<size_t>(2 * k)];
        float bIm = workspace.fftOutput[static_cast<size_t>(2 * k + 1)];
        workspace.fftInput[static_cast<size_t>(2 * k)] = aRe * bRe + aIm * bIm;
        workspace.fftInput[static_cast<size_t>(2 * k + 1)] = aRe * bIm - aIm * bRe;
    }

    workspace.fft->performRealOnlyInverseTransform(workspace.fftInput.data());

    float powerTerm0 = 0.0f;
    for (size_t j = 0; j < n; ++j)
        powerTerm0 += workspace.linearBuffer[j] * workspace.linearBuffer[j];

    float powerTermTau = powerTerm0;
    workspace.diff[0] = 0.0f;
    for (size_t tau = 1; tau < n; ++tau)
    {
        powerTermTau += workspace.linearBuffer[n + tau - 1]
                      * workspace.linearBuffer[n + tau - 1]
                      - workspace.linearBuffer[tau - 1]
                      * workspace.linearBuffer[tau - 1];
        float difference = powerTerm0 + powerTermTau - 2.0f * workspace.fftInput[tau];
        workspace.diff[tau] = std::max(0.0f, difference);
    }

    workspace.cmndf[0] = 1.0f;
    float runningSum = 0.0f;
    for (size_t tau = 1; tau < n; ++tau)
    {
        runningSum += workspace.diff[tau];
        workspace.cmndf[tau] = runningSum > 0.0f
                             ? workspace.diff[tau] * static_cast<float>(tau) / runningSum
                             : 1.0f;
    }

    auto firstTau = static_cast<size_t>(std::max(
        2, static_cast<int>(std::floor(analysisSR / maximumOutputFrequency))));
    size_t tauEstimate = 0;

    for (size_t tau = firstTau; tau < n; ++tau)
    {
        if (workspace.cmndf[tau] < threshold)
        {
            while (tau + 1 < n && workspace.cmndf[tau + 1] < workspace.cmndf[tau])
                ++tau;
            tauEstimate = tau;
            candidate.thresholdQualified = true;
            break;
        }
    }

    if (tauEstimate == 0)
    {
        float minValue = 1.0f;
        for (size_t tau = firstTau; tau < n; ++tau)
        {
            if (workspace.cmndf[tau] < minValue)
            {
                minValue = workspace.cmndf[tau];
                tauEstimate = tau;
            }
        }
    }

    if (tauEstimate == 0)
        return candidate;

    candidate.boundary = tauEstimate + 2 >= n;
    float betterTau = static_cast<float>(tauEstimate);

    if (tauEstimate > 0 && tauEstimate < n - 1)
    {
        float s0 = workspace.cmndf[tauEstimate - 1];
        float s1 = workspace.cmndf[tauEstimate];
        float s2 = workspace.cmndf[tauEstimate + 1];
        float denominator = 2.0f * (2.0f * s1 - s2 - s0);
        if (std::abs(denominator) > 1e-12f)
            betterTau += (s2 - s0) / denominator;
    }

    if (betterTau < 1.0f)
        return {};

    float frequency = static_cast<float>(analysisSR) / betterTau;
    float confidence = 1.0f - workspace.cmndf[tauEstimate];

    if (!std::isfinite(frequency) || !std::isfinite(confidence)
        || frequency < minimumOutputFrequency || frequency > maximumOutputFrequency)
        return {};

    candidate.result = { frequency, std::clamp(confidence, 0.0f, 1.0f) };
    return candidate;
}

void YinPitchDetector::considerCandidate(const PitchCandidate& candidate)
{
    if (!hasPitch(candidate.result) || candidate.boundary)
    {
        pendingCandidate = {};
        pendingCount = 0;
        int missLimit = (lastResult.frequency > 0.0f
                      && lastResult.frequency <= lowSelectionMaximumFrequency)
                      ? lowCandidateMissLimit : 2;
        if (++missedCandidates >= missLimit)
            publishResult({});
        return;
    }

    missedCandidates = 0;

    if (lastResult.frequency <= 0.0f)
    {
        if (candidate.thresholdQualified
            && candidate.result.confidence >= qualifiedCandidateConfidence)
        {
            publishResult(candidate.result);
            return;
        }
    }
    else if (frequenciesAreClose(lastResult.frequency, candidate.result.frequency))
    {
        publishResult(candidate.result);
        return;
    }
    else if (!candidate.thresholdQualified
             && candidate.result.confidence < fallbackCandidateConfidence)
    {
        pendingCandidate = {};
        pendingCount = 0;
        return;
    }

    int confirmationsRequired = (candidate.thresholdQualified
                              || pendingCandidate.thresholdQualified) ? 2 : 4;
    if (hasPitch(pendingCandidate.result)
        && frequenciesAreWithin(pendingCandidate.result.frequency,
                                candidate.result.frequency,
                                movingCandidateToleranceOctaves))
    {
        pendingCandidate = candidate;
        if (++pendingCount >= confirmationsRequired)
            publishResult(candidate.result);
    }
    else
    {
        pendingCandidate = candidate;
        pendingCount = 1;
    }
}

void YinPitchDetector::publishResult(PitchResult result)
{
    lastResult = result;
    pendingCandidate = {};
    pendingCount = 0;
    atomicResult.store(packResult(result), std::memory_order_release);
}

void YinPitchDetector::clearResult()
{
    lastResult = {};
    pendingCandidate = {};
    pendingCount = 0;
    missedCandidates = 0;
    atomicResult.store(packResult({}), std::memory_order_release);
}
