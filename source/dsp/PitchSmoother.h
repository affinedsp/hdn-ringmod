#pragma once

#include <algorithm>
#include <cmath>

class PitchSmoother
{
public:
    inline void prepare(double sampleRate)
    {
        sr = std::max(1.0f, static_cast<float>(sampleRate));
        hasValue = false;
        smoothed = 0.0f;
        cachedFreq = 0.0f;
        lastDetectedFreq = 0.0f;
        cachedLogFreq = 0.0f;
        invalidSamples = 0;
        releaseSamples = std::max(1, static_cast<int>(sr * 0.05f));
        recomputeAlpha();
    }

    inline void setSmoothingAmount(float amount01)
    {
        if (amount01 == smoothingAmount)
            return;
        smoothingAmount = amount01;
        recomputeAlpha();
    }

    inline void setSensitivity(float sensitivity01)
    {
        sensitivityThreshold = std::clamp(sensitivity01, 0.0f, 1.0f);
    }

    inline float process(float detectedFreq, float confidence)
    {
        if (detectedFreq <= 0.0f || confidence < sensitivityThreshold)
        {
            if (hasValue && ++invalidSamples >= releaseSamples)
            {
                hasValue = false;
                smoothed = 0.0f;
                cachedFreq = 0.0f;
            }
            return cachedFreq;
        }

        invalidSamples = 0;
        if (detectedFreq != lastDetectedFreq)
        {
            lastDetectedFreq = detectedFreq;
            cachedLogFreq = std::log2(detectedFreq);
        }
        float logFreq = cachedLogFreq;

        if (!hasValue)
        {
            smoothed = logFreq;
            hasValue = true;
            cachedFreq = std::exp2(smoothed);
            return cachedFreq;
        }

        float delta = std::abs(logFreq - smoothed);
        float effectiveAlpha = delta > 0.08f ? 1.0f : alpha;

        float prev = smoothed;
        smoothed += effectiveAlpha * (logFreq - smoothed);

        if (smoothed != prev)
            cachedFreq = std::exp2(smoothed);

        return cachedFreq;
    }

private:
    inline void recomputeAlpha()
    {
        float tau = smoothingAmount * 0.05f;
        if (tau < 1e-6f)
            alpha = 1.0f;
        else
            alpha = 1.0f - std::exp(-1.0f / (sr * tau));
    }

    float sr = 44100.0f;
    float smoothingAmount = 0.5f;
    float smoothed = 0.0f;
    float alpha = 0.5f;
    float sensitivityThreshold = 0.5f;
    float cachedFreq = 0.0f;
    float lastDetectedFreq = 0.0f;
    float cachedLogFreq = 0.0f;
    int invalidSamples = 0;
    int releaseSamples = 2205;
    bool hasValue = false;
};
