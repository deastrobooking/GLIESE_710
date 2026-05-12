#pragma once
#include <JuceHeader.h>

struct Vec3 { float x, y, z; };

class Osc3D
{
public:
    enum class FieldType
    {
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

private:
    double sr    = 48000.0;
    float  freq  = 440.f;
    float  phase = 0.f;

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

    static float foldCoordinate(float v);
    static Vec3 rotateXY(Vec3 p, float angle);
};
