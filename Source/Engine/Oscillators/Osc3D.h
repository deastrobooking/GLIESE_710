#pragma once
#include <JuceHeader.h>

struct Vec3 { float x, y, z; };

class Osc3D
{
public:
    enum class FieldType
    {
        // ── Original geometric fields ─────────────────────────────────────────
        SinSumXYZ = 0,     // sin(x)+cos(y)+sin(z)
        SinProduct,        // sin(x*y)+cos(z)
        SphericalRipple,   // sin(sqrt(x²+y²+z²))
        Torus,             // torus surface scan
        VortexField,       // curl-like attractor
        TurbulenceNoise,   // value-noise field
        LorenzAttractor,   // Lorenz system
        MagneticDipole,    // dipole B-field magnitude
        FoldedSpace,       // abs-fold fractal
        ChebyshevMorph,    // Chebyshev T4+T5+T6
        // ── Musical / harmonic fields ─────────────────────────────────────────
        HarmonicLattice,   // sin(x·F) + a·sin(2y·F+z) + b·sin(3z·F+x) — harmonic series
        ChordField,        // x=root y=third z=fifth — chordal axis ratios
        ResonanceShell,    // sin(k·dist·F) — spherical harmonic shells
        IntervalGravity,   // gravity wells at root / fifth / octave positions
        FormantVowel,      // Gaussian formant envelopes along axes
        MetallicRatio,     // blend harmonic vs irrational (sqrt2/phi) ratios
        TensionResolution, // blend stable harmonic + tense nonlinear
        BodyEdgeAir,       // x=body y=edge z=air — timbral axis model
        NumFields
    };

    enum class PathType
    {
        Circle = 0,
        Spiral,
        Lissajous,
        RandomWalk,
        PhysicsOrbit,
        NumPaths
    };

    struct Params
    {
        FieldType field     = FieldType::SinSumXYZ;
        PathType  path      = PathType::Circle;
        float     radius    = 1.0f;
        float     depth     = 1.0f;
        float     warpAmt   = 0.25f;
        float     rotation  = 0.0f;  // field rotation angle
        float     chaos     = 0.0f;  // noise injected into path
        float     morphT    = 0.0f;  // 0..1 blend to next FieldType
        float     fieldFreq = 1.0f;  // harmonic density multiplier
        float     zRatio    = 1.0f;  // z-axis motion ratio multiplier
        float     foldAmt   = 0.0f;  // 0..1 coordinate fold blend
        float     drive     = 1.0f;  // output shaper drive
    };

    void prepare(double sampleRate);
    void setParams(const Params& p);
    void setFrequency(float hz);
    void reset();

    float processSample();  // mono output

    // Sub-rate divisor: field evaluation runs every N audio samples (ZOH).
    // 4 → 12 kHz effective field rate; sufficient for all audible harmonics.
    static constexpr int kSubRate = 4;

private:
    double sr    = 48000.0;
    float  freq  = 440.f;
    float  phase = 0.f;
    float  phaseInc = 440.f / 48000.f;
    float  rotationCos = 1.f;
    float  rotationSin = 0.f;
    float  cachedWarpAmt = 0.25f;
    float  cachedZRatio = 1.f;
    float  cachedFoldAmt = 0.f;
    float  cachedFieldFreq = 1.f;
    float  cachedDrive = 1.f;

    // Sub-rate state
    int   subCount  = 0;
    float lastOut   = 0.f;

    Params params;

    // RandomWalk state
    Vec3 rwPos   = {0, 0, 0};
    Vec3 rwVel   = {0, 0, 0};

    // Lorenz integration state
    Vec3 lorenz  = {1.f, 1.f, 1.f};

    // Noise seed (LCG)
    uint32_t noiseSeed = 12345;

    Vec3   computePath(float t);
    Vec3   warpCoordinates(Vec3 p) const;
    Vec3   foldCoordinates(Vec3 p) const;
    float  evaluateField(Vec3 p, FieldType ft) const;
    float  blend(Vec3 p) const;

    float  whiteSample();
    void   updateDerived();

    static float foldCoordinate(float v);
};
