#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>

class TerrainOsc
{
public:
    enum class PathMode
    {
        Circle = 0,
        Ellipse,
        Lissajous,
        Spiral,
        Line,
        OrbitNoise,
        NumModes
    };

    enum class Quality
    {
        Off = 0,
        Auto,
        Soft,
        NumModes
    };

    struct Params
    {
        PathMode pathMode = PathMode::Circle;
        Quality quality = Quality::Auto;
        float centerX = 0.5f;
        float centerY = 0.5f;
        float radiusX = 0.35f;
        float radiusY = 0.35f;
        float zoom = 1.0f;
        float rotation = 0.0f;
        float pathSize = 1.0f;
        float ratioX = 1.0f;
        float ratioY = 1.5f;
        float scanRate = 1.0f;
        float phaseOffset = 0.0f;
        float spiral = 0.0f;
        float drive = 1.5f;
        float offset = 0.0f;
        float smoothing = 0.0f;
        float morph = 0.0f;
        float gain = 1.0f;
    };

    void prepare(double sampleRate);
    void reset();
    void setFrequency(float hz);
    void setParams(const Params& params);

    float processSample();

    bool setHeightmapFromImage(const juce::Image& image);
    void resetHeightmap();
    bool importHeightmapFile(const juce::File& file);
    void shareTerrainFrom(const TerrainOsc& other) noexcept { sharedTerrain = other.sharedTerrain; }

    int getWidth() const noexcept { return width; }
    int getHeight() const noexcept { return height; }

private:
    static constexpr int kDefaultSize = 128;
    static constexpr int kMaxImportedSize = 512;
    static constexpr int kBlurPassCount = 5;

    double sr = 48000.0;
    float freq = 440.f;
    float phase = 0.f;
    float phaseInc = 440.f / 48000.f;
    float orbitStateX = 0.37f;
    float orbitStateY = 0.61f;
    float lastSmoothing = -1.f;

    struct MipLevel
    {
        int width = 0;
        int height = 0;
        std::vector<float> terrain;
        std::array<std::vector<float>, kBlurPassCount> blurredTerrain;
    };

    struct TerrainData
    {
        int width = 0;
        int height = 0;
        std::vector<float> terrain;
        std::array<std::vector<float>, kBlurPassCount> blurredTerrain;
        std::vector<MipLevel> mipLevels;
    };

    int width = 0;
    int height = 0;
    Params params;
    std::vector<float> terrain;
    std::array<std::vector<float>, kBlurPassCount> blurredTerrain;
    std::vector<MipLevel> mipLevels;
    std::shared_ptr<const TerrainData> sharedTerrain;

    void updateDerived();
    void generateDefaultTerrain();
    void publishTerrain();
    void rebuildBlurredTerrain();
    void rebuildMipLevels();
    int chooseMipLevel() const noexcept;
    int smoothingBlurIndex() const noexcept;
    std::pair<float, float> computePath(float t);

    float sampleTerrain(const std::vector<float>& data, int dataWidth, int dataHeight, float x, float y) const;
    float proceduralTerrain(float x, float y, float t) const noexcept;
    float valueNoise(float x, float y) const noexcept;

    static float wrap01(float v) noexcept;
};
