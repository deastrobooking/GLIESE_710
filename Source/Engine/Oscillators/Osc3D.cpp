#include "Osc3D.h"
#include <cmath>

static constexpr float kPi  = juce::MathConstants<float>::pi;
static constexpr float kTau = juce::MathConstants<float>::twoPi;

//==============================================================================
void Osc3D::prepare(double sampleRate)
{
    sr = sampleRate;
    reset();
}

void Osc3D::reset()
{
    phase  = 0.f;
    rwPos  = {0.f, 0.f, 0.f};
    rwVel  = {0.1f, 0.07f, 0.13f};
    lorenz = {1.f, 1.f, 1.f};
}

void Osc3D::setParams(const Params& p) { params = p; }
void Osc3D::setFrequency(float hz)     { freq = juce::jlimit(20.f, (float)(sr * 0.49), hz); }

//==============================================================================
Vec3 Osc3D::rotateXY(Vec3 p, float angle)
{
    float c = std::cos(angle), s = std::sin(angle);
    return { c * p.x - s * p.y, s * p.x + c * p.y, p.z };
}

//──────────────────────────────────────────────────────────────────────────────
Vec3 Osc3D::computePath(float t)
{
    Vec3 p{};
    float r = params.radius;
    float d = params.depth;

    switch (params.path)
    {
        case PathType::Circle:
            p = { r * std::cos(t), r * std::sin(t), d * std::sin(t * 0.5f) };
            break;

        case PathType::Spiral:
        {
            float ramp = 0.5f + 0.5f * std::sin(t * 0.25f);
            p = { r * ramp * std::cos(t), r * ramp * std::sin(t), d * (t / kTau - std::floor(t / kTau) - 0.5f) };
            break;
        }

        case PathType::Lissajous:
            p = { r * std::cos(3.f * t + kPi * 0.5f),
                  r * std::sin(2.f * t),
                  d * std::sin(5.f * t) };
            break;

        case PathType::RandomWalk:
        {
            float dt = 1.f / (float)sr;
            float noiseScale = 0.05f + params.chaos * 0.5f;
            rwVel.x += noiseScale * whiteSample();
            rwVel.y += noiseScale * whiteSample();
            rwVel.z += noiseScale * whiteSample();
            // Dampen velocity (orbit around origin)
            float damp = 0.995f;
            rwVel.x *= damp; rwVel.y *= damp; rwVel.z *= damp;
            rwPos.x += rwVel.x * dt * 500.f;
            rwPos.y += rwVel.y * dt * 500.f;
            rwPos.z += rwVel.z * dt * 500.f;
            // Soft limit to keep in [-r, r]
            rwPos.x = std::tanh(rwPos.x / r) * r;
            rwPos.y = std::tanh(rwPos.y / r) * r;
            rwPos.z = std::tanh(rwPos.z / d) * d;
            p = rwPos;
            break;
        }

        case PathType::PhysicsOrbit:
        {
            // Simple circular orbit perturbed by gravity well
            float angle  = t;
            float ecc    = 0.3f * params.chaos;
            float rOrbit = r * (1.f - ecc * std::cos(t));
            p = { rOrbit * std::cos(angle),
                  rOrbit * std::sin(angle),
                  d * std::sin(angle * 2.f) };
            break;
        }

        default: break;
    }

    // Apply global rotation in XY plane
    p = rotateXY(p, params.rotation * kTau);

    // Inject chaos noise into coordinates
    if (params.chaos > 0.f)
    {
        float c = params.chaos * 0.3f;
        p.x += c * std::sin(p.y * 3.7f + t);
        p.y += c * std::cos(p.z * 2.9f + t);
        p.z += c * std::sin(p.x * 4.1f);
    }

    return p;
}

//──────────────────────────────────────────────────────────────────────────────
Vec3 Osc3D::warpCoordinates(Vec3 p) const
{
    float w = params.warpAmt;
    if (w <= 0.f) return p;
    p.x += w * std::sin(p.y * 3.0f + p.z * 1.5f);
    p.y += w * std::cos(p.z * 2.0f + p.x * 1.2f);
    p.z += w * std::sin(p.x * 4.0f + p.y * 0.8f);
    return p;
}

//──────────────────────────────────────────────────────────────────────────────
float Osc3D::evaluateField(Vec3 p, FieldType ft) const
{
    switch (ft)
    {
        case FieldType::SinSumXYZ:
            return std::sin(p.x * 4.f) + std::cos(p.y * 3.f) + std::sin(p.z * 5.f);

        case FieldType::SinProduct:
            return std::sin(p.x * p.y * 2.f) + std::cos(p.z * 3.f);

        case FieldType::SphericalRipple:
        {
            float r = std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z) + 1e-6f;
            return std::sin(r * 6.f) / r;
        }

        case FieldType::Torus:
        {
            float a = 0.7f, b = 0.3f;
            float dist = std::sqrt(p.x*p.x + p.y*p.y);
            float dx = dist - a;
            float d  = std::sqrt(dx*dx + p.z*p.z) - b;
            return std::sin(d * kTau * 2.f);
        }

        case FieldType::VortexField:
        {
            // Cross-product vortex: (y-z, z-x, x-y) → dot with normalised input
            Vec3 v { p.y - p.z, p.z - p.x, p.x - p.y };
            return std::sin(v.x * 2.f) + std::cos(v.y * 3.f);
        }

        case FieldType::TurbulenceNoise:
        {
            // Value noise approximation via trig
            float n  = std::sin(p.x * 12.9f + p.y * 4.1f) * 43758.5453f;
            n -= std::floor(n);
            float n2 = std::sin(p.y * 7.8f + p.z * 9.3f) * 43758.5453f;
            n2 -= std::floor(n2);
            return (n + n2 - 1.f) * 0.7f;
        }

        case FieldType::LorenzAttractor:
        {
            // Return normalised distance from strange attractor centroid
            float dist = std::sqrt((p.x-0.f)*(p.x-0.f) + (p.y-0.f)*(p.y-0.f) + (p.z-27.f)*(p.z-27.f));
            return std::sin(dist * 0.5f);
        }

        case FieldType::MagneticDipole:
        {
            float r2 = p.x*p.x + p.y*p.y + p.z*p.z + 0.1f;
            float r  = std::sqrt(r2);
            float cosTheta = p.z / r;
            return (3.f * cosTheta * cosTheta - 1.f) / (r2 * r);
        }

        case FieldType::FoldedSpace:
        {
            float px = p.x, py = p.y, pz = p.z;
            for (int i = 0; i < 4; ++i)
            {
                px = std::abs(px) - 0.7f;
                py = std::abs(py) - 0.7f;
                pz = std::abs(pz) - 0.7f;
            }
            return std::sin(px * 5.f) + std::cos(py * 5.f) + std::sin(pz * 5.f);
        }

        case FieldType::ChebyshevMorph:
        {
            // T4(x) + T5(y) + T6(z) Chebyshev polynomials
            float xc = juce::jlimit(-1.f, 1.f, p.x);
            float yc = juce::jlimit(-1.f, 1.f, p.y);
            float zc = juce::jlimit(-1.f, 1.f, p.z);
            float T4x = 8.f*xc*xc*xc*xc - 8.f*xc*xc + 1.f;
            float T5y = 16.f*yc*yc*yc*yc*yc - 20.f*yc*yc*yc + 5.f*yc;
            float T6z = 32.f*zc*zc*zc*zc*zc*zc - 48.f*zc*zc*zc*zc + 18.f*zc*zc - 1.f;
            return (T4x + T5y + T6z) * 0.333f;
        }

        default: return 0.f;
    }
}

//──────────────────────────────────────────────────────────────────────────────
float Osc3D::blend(Vec3 p) const
{
    auto fieldA = params.field;
    auto fieldB = static_cast<FieldType>(((int)fieldA + 1) % (int)FieldType::NumFields);

    if (params.morphT <= 0.f) return evaluateField(p, fieldA);
    if (params.morphT >= 1.f) return evaluateField(p, fieldB);

    float sA = evaluateField(p, fieldA);
    float sB = evaluateField(p, fieldB);
    return sA + params.morphT * (sB - sA);
}

//──────────────────────────────────────────────────────────────────────────────
float Osc3D::whiteSample()
{
    noiseSeed = noiseSeed * 1664525u + 1013904223u;
    return ((float)(noiseSeed >> 16) / 32767.5f) - 1.f;
}

//──────────────────────────────────────────────────────────────────────────────
float Osc3D::processSample()
{
    float phaseInc = freq / (float)sr;
    float t = phase * kTau;

    Vec3 p = computePath(t);
    p      = warpCoordinates(p);
    float s = blend(p);

    phase += phaseInc;
    if (phase >= 1.f) phase -= 1.f;

    return std::tanh(s);   // soft clip to ±1
}
