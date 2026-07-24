#include "Osc3D.h"
#include <cmath>

static constexpr float kPi  = juce::MathConstants<float>::pi;
static constexpr float kTau = juce::MathConstants<float>::twoPi;

//==============================================================================
void Osc3D::prepare(double sampleRate)
{
    sr = sampleRate;
    updateDerived();
    reset();
}

void Osc3D::reset()
{
    phase  = 0.f;
    rwPos  = {0.f, 0.f, 0.f};
    rwVel  = {0.1f, 0.07f, 0.13f};
    lorenz = {1.f, 1.f, 1.f};
}

void Osc3D::setParams(const Params& p)
{
    params = p;
    updateDerived();
}

void Osc3D::setFrequency(float hz)
{
    freq = juce::jlimit(20.f, (float)(sr * 0.49), hz);
    updateDerived();
}

//==============================================================================
void Osc3D::updateDerived()
{
    phaseInc = freq / (float)sr;
    float rotation = params.rotation * kTau;
    rotationCos = std::cos(rotation);
    rotationSin = std::sin(rotation);
    cachedWarpAmt = params.warpAmt;
    cachedZRatio = juce::jlimit(0.25f, 4.0f, params.zRatio);
    cachedFoldAmt = juce::jlimit(0.f, 1.f, params.foldAmt);
    cachedFieldFreq = juce::jlimit(0.25f, 16.f, params.fieldFreq);
    cachedDrive = juce::jlimit(0.25f, 4.f, params.drive);

    // Field frequency is a harmonic-density control rather than oscillator
    // pitch. Raise the internal rate only when that density (or strong
    // nonlinear shaping) makes aliasing likely, keeping ordinary patches cheap.
    oversampleFactor = cachedFieldFreq > 8.f ? 4
                         : cachedFieldFreq > 4.f ? 2
                         : 1;
    if (cachedFoldAmt > 0.5f || cachedDrive > 2.f)
        oversampleFactor = juce::jmax(oversampleFactor, 2);
}

//──────────────────────────────────────────────────────────────────────────────
Vec3 Osc3D::computePath(float t, int rateFactor)
{
    Vec3 p{};
    float r = params.radius;
    float d = params.depth;
    float zRatio = cachedZRatio;

    switch (params.path)
    {
        case PathType::Circle:
            p = { r * std::cos(t), r * std::sin(t), d * std::sin(t * 0.5f * zRatio) };
            break;

        case PathType::Spiral:
        {
            float ramp = 0.5f + 0.5f * std::sin(t * 0.25f);
            float zPhase = t * zRatio / kTau;
            p = { r * ramp * std::cos(t), r * ramp * std::sin(t), d * (zPhase - std::floor(zPhase) - 0.5f) };
            break;
        }

        case PathType::Lissajous:
            p = { r * std::cos(3.f * t + kPi * 0.5f),
                  r * std::sin(2.f * t),
                  d * std::sin(5.f * zRatio * t) };
            break;

        case PathType::RandomWalk:
        {
            float dt = 1.f / ((float)sr * (float)rateFactor);
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
            float zLimit = juce::jmax(0.001f, d);
            rwPos.z = std::tanh(rwPos.z / zLimit) * zLimit;
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
                  d * std::sin(angle * 2.f * zRatio) };
            break;
        }

        case PathType::NumPaths:
        default: break;
    }

    // Apply global rotation in XY plane
    p = { rotationCos * p.x - rotationSin * p.y,
          rotationSin * p.x + rotationCos * p.y,
          p.z };

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
    float w = cachedWarpAmt;
    if (w <= 0.f) return p;
    p.x += w * std::sin(p.y * 3.0f + p.z * 1.5f);
    p.y += w * std::cos(p.z * 2.0f + p.x * 1.2f);
    p.z += w * std::sin(p.x * 4.0f + p.y * 0.8f);
    return p;
}

float Osc3D::foldCoordinate(float v)
{
    v = std::fmod(v + 1.f, 4.f);
    if (v < 0.f)
        v += 4.f;

    return v <= 2.f ? v - 1.f : 3.f - v;
}

Vec3 Osc3D::foldCoordinates(Vec3 p) const
{
    float amt = cachedFoldAmt;
    if (amt <= 0.f)
        return p;

    Vec3 folded { foldCoordinate(p.x), foldCoordinate(p.y), foldCoordinate(p.z) };
    return {
        p.x + (folded.x - p.x) * amt,
        p.y + (folded.y - p.y) * amt,
        p.z + (folded.z - p.z) * amt
    };
}

//──────────────────────────────────────────────────────────────────────────────
float Osc3D::evaluateField(Vec3 p, FieldType ft) const
{
    float f       = cachedFieldFreq;
    float a       = params.radius;    // primary harmonic weight
    float b       = params.depth;     // secondary weight
    float tension = params.morphT;    // morphT reused as tension/vowel/morph

    switch (ft)
    {
        // ── Original geometric fields ─────────────────────────────────────────
        case FieldType::SinSumXYZ:
            return std::sin(p.x * 4.f * f) + std::cos(p.y * 3.f * f) + std::sin(p.z * 5.f * f);

        case FieldType::SinProduct:
            return std::sin(p.x * p.y * 2.f * f) + std::cos(p.z * 3.f * f);

        case FieldType::SphericalRipple:
        {
            float r = std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z) + 1e-6f;
            return std::sin(r * 6.f * f) / r;
        }

        case FieldType::Torus:
        {
            float ta = 0.7f, tb = 0.3f;
            float dist = std::sqrt(p.x*p.x + p.y*p.y);
            float dx = dist - ta;
            float d  = std::sqrt(dx*dx + p.z*p.z) - tb;
            return std::sin(d * kTau * 2.f * f);
        }

        case FieldType::VortexField:
        {
            Vec3 v { p.y - p.z, p.z - p.x, p.x - p.y };
            return std::sin(v.x * 2.f * f) + std::cos(v.y * 3.f * f);
        }

        case FieldType::TurbulenceNoise:
        {
            float n  = std::sin((p.x * 12.9f + p.y * 4.1f) * f) * 43758.5453f;
            n -= std::floor(n);
            float n2 = std::sin((p.y * 7.8f + p.z * 9.3f) * f) * 43758.5453f;
            n2 -= std::floor(n2);
            return (n + n2 - 1.f) * 0.7f;
        }

        case FieldType::LorenzAttractor:
        {
            float dist = std::sqrt(p.x*p.x + p.y*p.y + (p.z-27.f)*(p.z-27.f));
            return std::sin(dist * 0.5f * f);
        }

        case FieldType::MagneticDipole:
        {
            float r2 = p.x*p.x + p.y*p.y + p.z*p.z + 0.1f;
            float r  = std::sqrt(r2);
            float cosTheta = p.z / r;
            return ((3.f * cosTheta * cosTheta - 1.f) / (r2 * r)) * std::sqrt(f);
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
            return std::sin(px * 5.f * f) + std::cos(py * 5.f * f) + std::sin(pz * 5.f * f);
        }

        case FieldType::ChebyshevMorph:
        {
            float xc = juce::jlimit(-1.f, 1.f, p.x * f);
            float yc = juce::jlimit(-1.f, 1.f, p.y * f);
            float zc = juce::jlimit(-1.f, 1.f, p.z * f);
            float T4x = 8.f*xc*xc*xc*xc - 8.f*xc*xc + 1.f;
            float T5y = 16.f*yc*yc*yc*yc*yc - 20.f*yc*yc*yc + 5.f*yc;
            float T6z = 32.f*zc*zc*zc*zc*zc*zc - 48.f*zc*zc*zc*zc + 18.f*zc*zc - 1.f;
            return (T4x + T5y + T6z) * 0.333f;
        }

        // ── Musical / harmonic fields ─────────────────────────────────────────
        case FieldType::HarmonicLattice:
        {
            float wa = juce::jlimit(0.f, 2.f, a) * 0.5f;
            float wb = juce::jlimit(0.f, 2.f, b) * 0.25f;
            return std::sin(p.x * f)
                 + wa * std::sin(2.f * p.y * f + p.z)
                 + wb * std::sin(3.f * p.z * f + p.x);
        }

        case FieldType::ChordField:
        {
            float third = 1.25f - tension * 0.05f;   // 5/4 → 6/5 (major→minor)
            float wa = juce::jlimit(0.f, 1.f, a * 0.25f);
            float wb = juce::jlimit(0.f, 1.f, b * 0.25f);
            return (std::sin(p.x * f)
                  + wa * std::sin(p.y * third * f)
                  + wb * std::sin(p.z * 1.5f * f)) * 0.6f;
        }

        case FieldType::ResonanceShell:
        {
            float d = std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z) + 1e-6f;
            float k = juce::jmax(0.5f, a + b * 2.f);
            return std::sin(d * k * f);
        }

        case FieldType::IntervalGravity:
        {
            Vec3 root   { 0.f,        0.f,         0.f   };
            Vec3 fifth  { 0.7f * a,   0.2f,       -0.1f  };
            Vec3 octave {-0.4f,        0.6f * b,   0.3f  };
            auto dist3 = [](Vec3 u, Vec3 v) {
                return std::sqrt((u.x-v.x)*(u.x-v.x)+(u.y-v.y)*(u.y-v.y)+(u.z-v.z)*(u.z-v.z));
            };
            return std::sin(dist3(p, root) * f)
                 + 0.6f * std::sin(dist3(p, fifth)  * 1.5f * f)
                 + 0.4f * std::sin(dist3(p, octave) * 2.0f * f);
        }

        case FieldType::FormantVowel:
        {
            auto gauss = [](float x, float c, float w) {
                float d = x - c;
                return std::exp(-(d*d) / (2.f * w * w + 1e-6f));
            };
            float f1 = gauss(p.x,  0.2f + tension * 0.2f,  0.3f + params.warpAmt * 0.1f);
            float f2 = gauss(p.y, -0.1f + tension * 0.2f,  0.25f);
            float f3 = gauss(p.z,  0.5f - tension * 0.2f,  0.2f);
            return f1 * std::sin(p.x * 3.f  * f)
                 + f2 * std::sin(p.y * 6.f  * f)
                 + f3 * std::sin(p.z * 10.f * f);
        }

        case FieldType::MetallicRatio:
        {
            constexpr float kSqrt2 = 1.41421356f;
            constexpr float kPhi   = 1.61803398f;
            float metal = juce::jlimit(0.f, 1.f, params.chaos);
            float r2 = 2.f + metal * (kSqrt2 - 2.f);
            float r3 = 3.f + metal * (kPhi   - 3.f);
            return std::sin(p.x * f)
                 + 0.5f  * std::sin(p.y * r2 * f)
                 + 0.25f * std::sin(p.z * r3 * f);
        }

        case FieldType::TensionResolution:
        {
            float stable = std::sin(p.x * f) + 0.5f * std::sin(2.f * p.y * f);
            float tense  = std::sin(p.x * p.y * p.z * 8.f * f);
            return stable * (1.f - tension) + tense * tension;
        }

        case FieldType::BodyEdgeAir:
        {
            float body = std::sin(p.x * f);
            float edge = std::sin(p.x * f + 3.f * std::sin(p.y * f));
            float air  = std::sin(8.f * p.z * f + p.x * p.y * f);
            return body
                 + params.warpAmt * 0.5f * edge
                 + params.chaos   * 0.3f * air;
        }

        case FieldType::NumFields:
        default:
            return std::sin(p.x * f) + std::cos(p.y * f);
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
float Osc3D::processSubSample(int rateFactor)
{
    phase += phaseInc / (float)rateFactor;
    if (phase >= 1.f) phase -= 1.f;

    float t  = phase * kTau;
    Vec3  p  = computePath(t, rateFactor);
    p        = warpCoordinates(p);
    p        = foldCoordinates(p);
    float s  = blend(p);
    return std::tanh(s * cachedDrive);
}

float Osc3D::processSample()
{
    float sum = 0.f;
    for (int i = 0; i < oversampleFactor; ++i)
        sum += processSubSample(oversampleFactor);

    // Integrating the evenly spaced high-rate evaluations is a short box FIR.
    // Besides restoring unity gain, it suppresses content above the host
    // Nyquist frequency before returning to the host sample rate.
    return sum / (float)oversampleFactor;
}
