#pragma once

namespace affine::shading
{
// Small, allocation-free helpers for the offline-style renderers. Everything
// here runs on the message thread while building cached layers.

struct V3
{
    float x = 0.0f, y = 0.0f, z = 0.0f;

    V3 operator+ (V3 o) const noexcept { return { x + o.x, y + o.y, z + o.z }; }
    V3 operator- (V3 o) const noexcept { return { x - o.x, y - o.y, z - o.z }; }
    V3 operator* (float s) const noexcept { return { x * s, y * s, z * s }; }
    V3 operator* (V3 o) const noexcept { return { x * o.x, y * o.y, z * o.z }; }
    V3& operator+= (V3 o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }
    float dot (V3 o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    float length() const noexcept { return std::sqrt (dot (*this)); }
    V3 normalised() const noexcept
    {
        const auto l = length();
        return l > 1.0e-9f ? *this * (1.0f / l) : V3 { 0.0f, 0.0f, 1.0f };
    }
};

inline float saturate (float v) noexcept { return juce::jlimit (0.0f, 1.0f, v); }

inline float smoothstep (float e0, float e1, float x) noexcept
{
    const auto t = saturate ((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

inline float mix (float a, float b, float t) noexcept { return a + (b - a) * t; }
inline V3 mix (V3 a, V3 b, float t) noexcept { return a + (b - a) * t; }

inline float toLinear (float srgb) noexcept
{
    return srgb <= 0.04045f ? srgb / 12.92f : std::pow ((srgb + 0.055f) / 1.055f, 2.4f);
}

inline float toSrgb (float linear) noexcept
{
    linear = saturate (linear);
    return linear <= 0.0031308f ? linear * 12.92f : 1.055f * std::pow (linear, 1.0f / 2.4f) - 0.055f;
}

inline V3 linear (juce::Colour c) noexcept
{
    return { toLinear (c.getFloatRed()), toLinear (c.getFloatGreen()), toLinear (c.getFloatBlue()) };
}

inline V3 reflect (V3 incident, V3 n) noexcept { return incident - n * (2.0f * incident.dot (n)); }

// Deterministic integer hash noise: identical on every platform and run.
inline float hash (int x, int y, int seed = 0) noexcept
{
    auto h = static_cast<uint32_t> (x) * 374761393u + static_cast<uint32_t> (y) * 668265263u
           + static_cast<uint32_t> (seed) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<float> (h & 0xffffffu) / 16777215.0f;
}

inline float valueNoise (float x, float y, int seed = 0) noexcept
{
    const auto xi = static_cast<int> (std::floor (x)), yi = static_cast<int> (std::floor (y));
    const auto fx = x - static_cast<float> (xi), fy = y - static_cast<float> (yi);
    const auto sx = fx * fx * (3.0f - 2.0f * fx), sy = fy * fy * (3.0f - 2.0f * fy);
    const auto a = hash (xi, yi, seed), b = hash (xi + 1, yi, seed);
    const auto c = hash (xi, yi + 1, seed), d = hash (xi + 1, yi + 1, seed);
    return mix (mix (a, b, sx), mix (c, d, sx), sy);
}

inline float fbm (float x, float y, int octaves, int seed = 0) noexcept
{
    float sum = 0.0f, amplitude = 0.5f, norm = 0.0f;
    for (int i = 0; i < octaves; ++i)
    {
        sum += amplitude * valueNoise (x, y, seed + i * 31);
        norm += amplitude;
        x *= 2.03f;
        y *= 2.03f;
        amplitude *= 0.5f;
    }
    return sum / norm;
}

// Premultiplied ARGB writer for juce::Image::BitmapData in either pixel order.
inline void writePixel (juce::Image::BitmapData& data, int x, int y, V3 linearRgb, float alpha) noexcept
{
    alpha = saturate (alpha);
    const auto r = static_cast<juce::uint8> (juce::roundToInt (toSrgb (linearRgb.x) * alpha * 255.0f));
    const auto g = static_cast<juce::uint8> (juce::roundToInt (toSrgb (linearRgb.y) * alpha * 255.0f));
    const auto b = static_cast<juce::uint8> (juce::roundToInt (toSrgb (linearRgb.z) * alpha * 255.0f));
    const auto a = static_cast<juce::uint8> (juce::roundToInt (alpha * 255.0f));
    auto* pixel = reinterpret_cast<juce::PixelARGB*> (data.getPixelPointer (x, y));
    pixel->setARGB (a, r, g, b);
}
} // namespace affine::shading
