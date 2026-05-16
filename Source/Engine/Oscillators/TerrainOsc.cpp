#include "TerrainOsc.h"
#include <cmath>

void TerrainOsc::prepare(double sampleRate)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    generateDefaultTerrain();
    updateDerived();
    reset();
}

void TerrainOsc::reset()
{
    phase = wrap01(params.phaseOffset);
    orbitStateX = 0.37f;
    orbitStateY = 0.61f;
}

void TerrainOsc::setFrequency(float hz)
{
    freq = juce::jlimit(0.01f, (float)sr * 0.45f, hz);
    updateDerived();
}

void TerrainOsc::setParams(const Params& newParams)
{
    params = newParams;
    const int maxMode = (int)PathMode::NumModes - 1;
    params.pathMode = static_cast<PathMode>(juce::jlimit(0, maxMode, (int)params.pathMode));
    params.centerX = wrap01(params.centerX);
    params.centerY = wrap01(params.centerY);
    params.radiusX = juce::jlimit(0.f, 0.5f, params.radiusX);
    params.radiusY = juce::jlimit(0.f, 0.5f, params.radiusY);
    params.ratioX = juce::jlimit(0.125f, 8.f, params.ratioX);
    params.ratioY = juce::jlimit(0.125f, 8.f, params.ratioY);
    params.phaseOffset = wrap01(params.phaseOffset);
    params.spiral = juce::jlimit(0.f, 1.f, params.spiral);
    params.drive = juce::jlimit(0.1f, 8.f, params.drive);
    params.offset = juce::jlimit(-1.f, 1.f, params.offset);
    params.smoothing = juce::jlimit(0.f, 1.f, params.smoothing);
    params.morph = juce::jlimit(0.f, 1.f, params.morph);
    params.gain = juce::jlimit(0.f, 2.f, params.gain);

    if (std::abs(params.smoothing - lastSmoothing) > 0.001f)
        rebuildBlurredTerrain();
}

void TerrainOsc::updateDerived()
{
    phaseInc = freq / (float)sr;
}

float TerrainOsc::processSample()
{
    const float t = wrap01(phase + params.phaseOffset);
    auto [x, y] = computePath(t);

    const float smooth = params.smoothing;
    float raw = sampleTerrain(terrain, x, y);
    if (smooth > 0.001f && blurredTerrain.size() == terrain.size())
        raw += (sampleTerrain(blurredTerrain, x, y) - raw) * smooth;

    const float proc = proceduralTerrain(x, y, t);
    float out = raw + (proc - raw) * params.morph;
    out = std::tanh((out + params.offset) * params.drive) * params.gain;

    phase += phaseInc;
    phase -= std::floor(phase);

    return juce::jlimit(-2.f, 2.f, out);
}

std::pair<float, float> TerrainOsc::computePath(float t)
{
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float aX = twoPi * t * params.ratioX;
    const float aY = twoPi * t * params.ratioY;
    float x = params.centerX;
    float y = params.centerY;

    switch (params.pathMode)
    {
        case PathMode::Circle:
            x += params.radiusX * std::cos(twoPi * t);
            y += params.radiusX * std::sin(twoPi * t);
            break;

        case PathMode::Ellipse:
            x += params.radiusX * std::cos(aX);
            y += params.radiusY * std::sin(aY);
            break;

        case PathMode::Lissajous:
            x += params.radiusX * std::sin(aX + twoPi * 0.125f);
            y += params.radiusY * std::sin(aY);
            break;

        case PathMode::Spiral:
        {
            const float r = 1.f - params.spiral + params.spiral * t;
            x += params.radiusX * r * std::cos(aX);
            y += params.radiusY * r * std::sin(aY);
            break;
        }

        case PathMode::Line:
            x += params.radiusX * (2.f * t - 1.f);
            y += params.radiusY * 0.2f * std::sin(aY);
            break;

        case PathMode::OrbitNoise:
        {
            const float nX = valueNoise(t * params.ratioX * 4.f + 11.7f, orbitStateX);
            const float nY = valueNoise(t * params.ratioY * 4.f + 23.1f, orbitStateY);
            const float orbit = twoPi * t;
            x += params.radiusX * (0.72f * std::cos(orbit) + 0.56f * nX);
            y += params.radiusY * (0.72f * std::sin(orbit * 1.21f) + 0.56f * nY);
            orbitStateX = wrap01(orbitStateX + 0.000073f + params.spiral * 0.0002f);
            orbitStateY = wrap01(orbitStateY + 0.000091f + params.spiral * 0.00017f);
            break;
        }

        case PathMode::NumModes:
        default:
            break;
    }

    return { wrap01(x), wrap01(y) };
}

bool TerrainOsc::setHeightmapFromImage(const juce::Image& image)
{
    if (!image.isValid() || image.getWidth() <= 1 || image.getHeight() <= 1)
        return false;

    juce::Image source = image;
    const int maxDim = juce::jmax(image.getWidth(), image.getHeight());
    if (maxDim > kMaxImportedSize)
    {
        const float scale = (float)kMaxImportedSize / (float)maxDim;
        source = image.rescaled(juce::jmax(2, juce::roundToInt((float)image.getWidth() * scale)),
                                juce::jmax(2, juce::roundToInt((float)image.getHeight() * scale)),
                                juce::Graphics::mediumResamplingQuality);
    }

    width = source.getWidth();
    height = source.getHeight();
    terrain.assign((size_t)width * (size_t)height, 0.f);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const auto c = source.getPixelAt(x, y);
            const float brightness = (c.getFloatRed() + c.getFloatGreen() + c.getFloatBlue()) / 3.f;
            terrain[(size_t)y * (size_t)width + (size_t)x] = brightness * 2.f - 1.f;
        }
    }

    lastSmoothing = -1.f;
    rebuildBlurredTerrain();
    return true;
}

bool TerrainOsc::importHeightmapFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    return setHeightmapFromImage(juce::ImageFileFormat::loadFrom(file));
}

void TerrainOsc::generateDefaultTerrain()
{
    width = kDefaultSize;
    height = kDefaultSize;
    terrain.assign((size_t)width * (size_t)height, 0.f);

    for (int y = 0; y < height; ++y)
    {
        const float ny = (float)y / (float)(height - 1);
        for (int x = 0; x < width; ++x)
        {
            const float nx = (float)x / (float)(width - 1);
            const float dx = nx - 0.5f;
            const float dy = ny - 0.5f;
            const float r = std::sqrt(dx * dx + dy * dy);
            float v = 0.52f * std::sin((nx * 5.0f + ny * 2.0f) * juce::MathConstants<float>::twoPi);
            v += 0.34f * std::sin((r * 13.0f - ny * 1.5f) * juce::MathConstants<float>::twoPi);
            v += 0.18f * valueNoise(nx * 9.0f, ny * 9.0f);
            terrain[(size_t)y * (size_t)width + (size_t)x] = juce::jlimit(-1.f, 1.f, v);
        }
    }

    lastSmoothing = -1.f;
    rebuildBlurredTerrain();
}

void TerrainOsc::rebuildBlurredTerrain()
{
    if (terrain.empty() || width <= 0 || height <= 0)
        return;

    blurredTerrain = terrain;
    std::vector<float> tmp = blurredTerrain;
    const int passes = juce::jlimit(1, 5, 1 + juce::roundToInt(params.smoothing * 4.f));

    auto at = [&](const std::vector<float>& data, int x, int y)
    {
        x = (x % width + width) % width;
        y = (y % height + height) % height;
        return data[(size_t)y * (size_t)width + (size_t)x];
    };

    for (int pass = 0; pass < passes; ++pass)
    {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                float sum = at(blurredTerrain, x, y) * 4.f;
                sum += at(blurredTerrain, x - 1, y) + at(blurredTerrain, x + 1, y);
                sum += at(blurredTerrain, x, y - 1) + at(blurredTerrain, x, y + 1);
                tmp[(size_t)y * (size_t)width + (size_t)x] = sum * 0.125f;
            }
        }
        blurredTerrain.swap(tmp);
    }

    lastSmoothing = params.smoothing;
}

float TerrainOsc::sampleTerrain(const std::vector<float>& data, float x, float y) const
{
    if (data.empty() || width <= 1 || height <= 1)
        return 0.f;

    x = wrap01(x) * (float)width;
    y = wrap01(y) * (float)height;
    const int x0 = (int)std::floor(x) % width;
    const int y0 = (int)std::floor(y) % height;
    const int x1 = (x0 + 1) % width;
    const int y1 = (y0 + 1) % height;
    const float tx = x - std::floor(x);
    const float ty = y - std::floor(y);

    const float a = data[(size_t)y0 * (size_t)width + (size_t)x0];
    const float b = data[(size_t)y0 * (size_t)width + (size_t)x1];
    const float c = data[(size_t)y1 * (size_t)width + (size_t)x0];
    const float d = data[(size_t)y1 * (size_t)width + (size_t)x1];
    const float ab = a + (b - a) * tx;
    const float cd = c + (d - c) * tx;
    return ab + (cd - ab) * ty;
}

float TerrainOsc::proceduralTerrain(float x, float y, float t) const noexcept
{
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float dx = x - 0.5f;
    const float dy = y - 0.5f;
    const float r = std::sqrt(dx * dx + dy * dy);
    float v = 0.45f * std::sin(twoPi * (x * 3.0f + t));
    v += 0.35f * std::sin(twoPi * (y * 5.0f - t * 0.5f));
    v += 0.30f * std::sin(twoPi * (r * 12.0f + t * 0.25f));
    return juce::jlimit(-1.f, 1.f, v);
}

float TerrainOsc::valueNoise(float x, float y) const noexcept
{
    auto hash = [](int xi, int yi)
    {
        uint32_t h = (uint32_t)xi * 374761393u + (uint32_t)yi * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float)((h ^ (h >> 16)) & 0x00ffffffu) / (float)0x00ffffffu;
    };

    const int x0 = (int)std::floor(x);
    const int y0 = (int)std::floor(y);
    const float tx = x - (float)x0;
    const float ty = y - (float)y0;
    const float sx = tx * tx * (3.f - 2.f * tx);
    const float sy = ty * ty * (3.f - 2.f * ty);

    const float a = hash(x0, y0);
    const float b = hash(x0 + 1, y0);
    const float c = hash(x0, y0 + 1);
    const float d = hash(x0 + 1, y0 + 1);
    const float ab = a + (b - a) * sx;
    const float cd = c + (d - c) * sx;
    return (ab + (cd - ab) * sy) * 2.f - 1.f;
}

float TerrainOsc::wrap01(float v) noexcept
{
    v -= std::floor(v);
    return v;
}
