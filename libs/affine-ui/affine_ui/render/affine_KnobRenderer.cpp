namespace affine
{
namespace
{
using namespace shading;

enum class Zone { cap, chamfer, grip, skirt };

struct Surface
{
    V3 normal;
    Zone zone;
    float occlusion;
};

// Radii of the knob's zones as fractions of its radius.
struct Profile
{
    float cap, chamfer, grip, rim, dome;
};

Profile profileFor (const KnobFinish& finish) noexcept
{
    return { finish.capRatio, finish.capRatio + 0.036f, finish.gripRatio, 0.950f, finish.capDome };
}

const V3 keyLight = V3 { -0.30f, -0.78f, 0.62f }.normalised();
const V3 fillLight = V3 { 0.55f, 0.35f, 0.76f }.normalised();
const V3 softbox = V3 { -0.16f, -0.58f, 0.80f }.normalised();
const V3 halfway = (keyLight + V3 { 0.0f, 0.0f, 1.0f }).normalised();

// The studio seen in a reflection: one large softbox above the player, a dim
// room elsewhere. Every product shares this environment.
float environment (V3 r) noexcept
{
    const auto box = smoothstep (0.70f, 0.985f, r.dot (softbox));
    const auto sky = saturate (-r.y * 0.85f + 0.08f);
    return 0.030f + 1.35f * box + 0.20f * sky * sky;
}

Surface surfaceAt (float r, float phi, float gripAngle, int ridges, float ridgeDepth, bool shaped, const Profile& p) noexcept
{
    const auto c = std::cos (phi), s = std::sin (phi);
    const V3 radial { c, s, 0.0f }, tangent { -s, c, 0.0f }, up { 0.0f, 0.0f, 1.0f };
    float slope = 0.0f, occlusion = 1.0f;
    Zone zone = Zone::cap;

    if (r < p.cap)
    {
        // A slightly domed face keeps the reflection alive across the cap.
        slope = p.dome * r / p.cap;
    }
    else if (r < p.chamfer)
    {
        zone = Zone::chamfer;
        slope = mix (0.55f, 0.95f, (r - p.cap) / (p.chamfer - p.cap));
    }
    else if (r < p.grip)
    {
        zone = Zone::grip;
        slope = 1.00f;
        occlusion = mix (0.55f, 1.0f, smoothstep (p.grip, p.chamfer + 0.04f, r));
    }
    else if (r < p.rim)
    {
        zone = Zone::skirt;
        slope = 0.22f;
        occlusion = mix (0.30f, 1.0f, smoothstep (p.grip, p.grip + 0.07f, r));
    }
    else
    {
        zone = Zone::skirt;
        const auto t = saturate ((r - p.rim) / (1.0f - p.rim));
        slope = mix (0.22f, 1.35f, t * t);
    }

    auto normal = radial * std::sin (slope) + up * std::cos (slope);

    if (zone == Zone::grip && shaped && ridgeDepth > 0.0f)
    {
        const auto phase = (phi - gripAngle) * static_cast<float> (ridges);
        normal = (normal + tangent * (ridgeDepth * std::sin (phase))).normalised();
        occlusion *= 1.0f - 0.61f * ridgeDepth * (0.5f - 0.5f * std::cos (phase));
    }

    return { normal, zone, occlusion };
}

V3 shadeMaterial (KnobFinish::Material material, juce::Colour colour, const Surface& surface, float r, float phi) noexcept
{
    const auto n = surface.normal;
    const auto reflected = reflect ({ 0.0f, 0.0f, -1.0f }, n);
    const auto key = saturate (n.dot (keyLight));
    const auto fill = saturate (n.dot (fillLight)) * 0.18f;
    const auto specular = saturate (n.dot (halfway));
    const auto face = surface.zone == Zone::cap;
    const V3 white { 1.0f, 1.0f, 1.0f };
    const auto base = linear (colour);

    switch (material)
    {
        case KnobFinish::Material::spunAluminium:
        {
            if (face)
            {
                const V3 t { -std::sin (phi), std::cos (phi), 0.0f };
                const auto th = t.dot (halfway);
                const auto kajiyaKay = std::sqrt (std::max (0.0f, 1.0f - th * th));
                // Lathe grooves: fine concentric rings with slow variation in depth.
                const auto grooves = 0.72f + 0.56f * fbm (r * 310.0f, 3.7f, 3, 11);
                const auto sheen = std::pow (kajiyaKay, 64.0f) * grooves;
                const auto broad = std::pow (kajiyaKay, 9.0f) * 0.22f;
                const auto lit = base * (0.05f + 0.28f * key + fill) + base * (environment (reflected) * 0.42f)
                               + base * ((sheen * 0.95f + broad) * grooves);
                // A soft hot spot where the softbox reflects near the centre.
                const auto hot = std::exp (-std::pow ((r - 0.18f) / 0.30f, 2.0f)) * 0.05f;
                return (lit + white * hot) * surface.occlusion;
            }
            const auto bright = base * (0.10f + 0.30f * key + fill) + base * (environment (reflected) * 0.95f)
                              + white * (std::pow (specular, 90.0f) * 1.6f);
            return bright * surface.occlusion;
        }
        case KnobFinish::Material::anodised:
        default:
        {
            if (surface.zone == Zone::skirt)
                return (base * (0.30f + 1.6f * key + fill) + white * (environment (reflected) * 0.030f)
                        + white * (std::pow (specular, 14.0f) * 0.07f)) * surface.occlusion;
            return (base * (0.35f + 2.2f * key + fill) + white * (environment (reflected) * 0.070f)
                    + white * (std::pow (specular, 18.0f) * 0.20f)) * surface.occlusion;
        }
    }
}

V3 shade (const Surface& surface, float r, float phi, const KnobFinish& finish) noexcept
{
    const auto onCap = surface.zone == Zone::cap || surface.zone == Zone::chamfer;
    return shadeMaterial (onCap ? finish.capMaterial : finish.bodyMaterial,
                          onCap ? finish.cap : finish.body, surface, r, phi);
}

template <typename Fn>
void renderSupersampled (juce::Image& image, int size, int samples, juce::Rectangle<int> area, Fn&& sample)
{
    juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);
    const auto radius = static_cast<float> (size) * 0.5f;
    const auto inv = 1.0f / static_cast<float> (samples * samples);

    for (int y = area.getY(); y < area.getBottom(); ++y)
    {
        for (int x = area.getX(); x < area.getRight(); ++x)
        {
            V3 colour;
            float alpha = 0.0f;

            for (int sy = 0; sy < samples; ++sy)
            {
                for (int sx = 0; sx < samples; ++sx)
                {
                    const auto px = (static_cast<float> (x) + (static_cast<float> (sx) + 0.5f) / static_cast<float> (samples)) - radius;
                    const auto py = (static_cast<float> (y) + (static_cast<float> (sy) + 0.5f) / static_cast<float> (samples)) - radius;
                    float weight = 0.0f;
                    const auto c = sample (px / radius, py / radius, weight);
                    colour += c * weight;
                    alpha += weight;
                }
            }

            if (alpha > 0.0f)
                writePixel (data, x, y, colour * (1.0f / alpha), alpha * inv);
            else
                reinterpret_cast<juce::PixelARGB*> (data.getPixelPointer (x, y))->setARGB (0, 0, 0, 0);
        }
    }
}
} // namespace

void KnobRenderer::prepare (int diameterPx)
{
    diameterPx = juce::jmax (8, diameterPx);
    if (diameterPx == diameter)
        return;

    diameter = diameterPx;
    shadowMargin = juce::roundToInt (static_cast<float> (diameter) * 0.26f) + 2;
    // Keep ridge pitch roughly constant in physical pixels, within a believable range.
    ridges = finish.ridges > 0 ? finish.ridges
                               : juce::jlimit (24, 60, juce::roundToInt (static_cast<float> (diameter) * 0.36f));
    gripAngle = std::numeric_limits<float>::quiet_NaN();
    renderBody();
    renderShadow();
}

void KnobRenderer::renderBody()
{
    body = juce::Image (juce::Image::ARGB, diameter, diameter, true);
    const auto profile = profileFor (finish);
    renderSupersampled (body, diameter, 3, body.getBounds(), [this, profile] (float x, float y, float& weight)
    {
        const auto r = std::sqrt (x * x + y * y);
        if (r > 1.0f)
            return V3 {};
        weight = 1.0f;
        const auto phi = std::atan2 (y, x);
        return shade (surfaceAt (r, phi, 0.0f, ridges, finish.ridgeDepth, false, profile), r, phi, finish);
    });
}

void KnobRenderer::renderShadow()
{
    const auto size = diameter + shadowMargin * 2;
    shadow = juce::Image (juce::Image::ARGB, size, size, true);
    juce::Image::BitmapData data (shadow, juce::Image::BitmapData::writeOnly);
    const auto radius = static_cast<float> (diameter) * 0.5f;
    const auto centre = static_cast<float> (size) * 0.5f;

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const auto dx = static_cast<float> (x) + 0.5f - centre;
            const auto dy = static_cast<float> (y) + 0.5f - centre;
            // Soft key-light shadow, cast down and slightly right.
            const auto drop = std::hypot (dx - radius * 0.05f, dy - radius * 0.16f) / radius;
            const auto soft = 1.0f - smoothstep (0.70f, 1.42f, drop);
            // Tight contact shadow where the skirt meets the panel.
            const auto contact = std::hypot (dx, dy - radius * 0.035f) / radius;
            const auto tight = 1.0f - smoothstep (0.93f, 1.07f, contact);
            const auto alpha = saturate (soft * 0.58f + tight * 0.62f - soft * tight * 0.58f * 0.62f);
            auto* pixel = reinterpret_cast<juce::PixelARGB*> (data.getPixelPointer (x, y));
            pixel->setARGB (static_cast<juce::uint8> (juce::roundToInt (alpha * 255.0f)), 0, 0, 0);
        }
    }
}

const juce::Image& KnobRenderer::getGrip (float angle)
{
    if (! juce::exactlyEqual (angle, gripAngle))
        renderGrip (angle);
    return grip;
}

void KnobRenderer::renderGrip (float angle)
{
    gripAngle = angle;
    if (! grip.isValid() || grip.getWidth() != diameter)
        grip = juce::Image (juce::Image::ARGB, diameter, diameter, true);

    const auto radius = static_cast<float> (diameter) * 0.5f;
    const auto overlap = 1.5f / radius;
    const auto profile = profileFor (finish);
    const auto inner = profile.chamfer - overlap, outer = profile.grip + overlap;
    // atan2 has 0 pointing right; knob angles have 0 pointing up.
    const auto ridgeAngle = angle - juce::MathConstants<float>::halfPi;

    renderSupersampled (grip, diameter, 2, grip.getBounds(), [&] (float x, float y, float& weight)
    {
        const auto r = std::sqrt (x * x + y * y);
        if (r < inner || r > outer)
            return V3 {};
        // Fade the band edges over matching body pixels so the seam is invisible.
        weight = smoothstep (inner, inner + overlap, r) * (1.0f - smoothstep (outer - overlap, outer, r));
        const auto phi = std::atan2 (y, x);
        return shade (surfaceAt (r, phi, ridgeAngle, ridges, finish.ridgeDepth, true, profile), r, phi, finish);
    });
}
} // namespace affine
