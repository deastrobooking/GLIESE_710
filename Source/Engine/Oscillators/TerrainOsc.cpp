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
    const int maxQuality = (int)Quality::NumModes - 1;
    params.quality = static_cast<Quality>(juce::jlimit(0, maxQuality, (int)params.quality));
    params.centerX = wrap01(params.centerX);
    params.centerY = wrap01(params.centerY);
    params.radiusX = juce::jlimit(0.f, 0.5f, params.radiusX);
    params.radiusY = juce::jlimit(0.f, 0.5f, params.radiusY);
    params.zoom = juce::jlimit(1.f, 16.f, params.zoom);
    params.rotation = wrap01(params.rotation);
    params.pathSize = juce::jlimit(0.f, 2.f, params.pathSize);
    params.ratioX = juce::jlimit(0.125f, 8.f, params.ratioX);
    params.ratioY = juce::jlimit(0.125f, 8.f, params.ratioY);
    params.scanRate = juce::jlimit(0.05f, 8.f, params.scanRate);
    params.phaseOffset = wrap01(params.phaseOffset);
    params.spiral = juce::jlimit(0.f, 1.f, params.spiral);
    params.drive = juce::jlimit(0.1f, 8.f, params.drive);
    params.offset = juce::jlimit(-1.f, 1.f, params.offset);
    params.smoothing = juce::jlimit(0.f, 1.f, params.smoothing);
    params.morph = juce::jlimit(0.f, 1.f, params.morph);
    params.gain = juce::jlimit(0.f, 2.f, params.gain);

    updateDerived();
}

void TerrainOsc::updateDerived()
{
    phaseInc = (freq * params.scanRate) / (float)sr;
}

float TerrainOsc::processSample()
{
    const float t = wrap01(phase + params.phaseOffset);
    auto [x, y] = computePath(t);

    const float smooth = params.smoothing;
    const auto& activeMips = sharedTerrain ? sharedTerrain->mipLevels : mipLevels;
    const auto& activeTerrain = sharedTerrain ? sharedTerrain->terrain : terrain;
    const auto& activeBlurred = sharedTerrain ? sharedTerrain->blurredTerrain : blurredTerrain;
    const int activeWidth = sharedTerrain ? sharedTerrain->width : width;
    const int activeHeight = sharedTerrain ? sharedTerrain->height : height;
    const int mipIndex = chooseMipLevel();
    const auto* mip = !activeMips.empty() ? &activeMips[(size_t)mipIndex] : nullptr;
    const auto& rawTerrain = mip != nullptr ? mip->terrain : activeTerrain;
    const int blurIndex = smoothingBlurIndex();
    const auto& blurTerrain = mip != nullptr
        ? mip->blurredTerrain[(size_t) blurIndex]
        : activeBlurred[(size_t) blurIndex];
    const int sampleWidth = mip != nullptr ? mip->width : activeWidth;
    const int sampleHeight = mip != nullptr ? mip->height : activeHeight;

    float raw = sampleTerrain(rawTerrain, sampleWidth, sampleHeight, x, y);
    if (smooth > 0.001f && blurTerrain.size() == rawTerrain.size())
        raw += (sampleTerrain(blurTerrain, sampleWidth, sampleHeight, x, y) - raw) * smooth;

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
    const float radiusX = params.radiusX * params.pathSize;
    const float radiusY = params.radiusY * params.pathSize;
    float x = 0.f;
    float y = 0.f;

    switch (params.pathMode)
    {
        case PathMode::Circle:
            x = radiusX * std::cos(twoPi * t);
            y = radiusX * std::sin(twoPi * t);
            break;

        case PathMode::Ellipse:
            x = radiusX * std::cos(aX);
            y = radiusY * std::sin(aY);
            break;

        case PathMode::Lissajous:
            x = radiusX * std::sin(aX + twoPi * 0.125f);
            y = radiusY * std::sin(aY);
            break;

        case PathMode::Spiral:
        {
            const float r = 1.f - params.spiral + params.spiral * t;
            x = radiusX * r * std::cos(aX);
            y = radiusY * r * std::sin(aY);
            break;
        }

        case PathMode::Line:
            x = radiusX * (2.f * t - 1.f);
            y = radiusY * 0.2f * std::sin(aY);
            break;

        case PathMode::OrbitNoise:
        {
            const float nX = valueNoise(t * params.ratioX * 4.f + 11.7f, orbitStateX);
            const float nY = valueNoise(t * params.ratioY * 4.f + 23.1f, orbitStateY);
            const float orbit = twoPi * t;
            x = radiusX * (0.72f * std::cos(orbit) + 0.56f * nX);
            y = radiusY * (0.72f * std::sin(orbit * 1.21f) + 0.56f * nY);
            orbitStateX = wrap01(orbitStateX + 0.000073f + params.spiral * 0.0002f);
            orbitStateY = wrap01(orbitStateY + 0.000091f + params.spiral * 0.00017f);
            break;
        }

        case PathMode::NumModes:
        default:
            break;
    }

    const float angle = params.rotation * twoPi;
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float rotatedX = x * c - y * s;
    const float rotatedY = x * s + y * c;

    return { wrap01(params.centerX + rotatedX / params.zoom),
             wrap01(params.centerY + rotatedY / params.zoom) };
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

    sharedTerrain.reset();
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

    auto data = std::make_shared<TerrainData>();
    data->width = width;
    data->height = height;
    data->terrain = std::move(terrain);
    data->blurredTerrain = std::move(blurredTerrain);
    data->mipLevels = std::move(mipLevels);
    sharedTerrain = std::move(data);
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

    auto at = [&](const std::vector<float>& data, int x, int y)
    {
        x = (x % width + width) % width;
        y = (y % height + height) % height;
        return data[(size_t)y * (size_t)width + (size_t)x];
    };

    std::vector<float> current = terrain;
    std::vector<float> tmp = current;

    for (int pass = 0; pass < kBlurPassCount; ++pass)
    {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                float sum = at(current, x, y) * 4.f;
                sum += at(current, x - 1, y) + at(current, x + 1, y);
                sum += at(current, x, y - 1) + at(current, x, y + 1);
                tmp[(size_t)y * (size_t)width + (size_t)x] = sum * 0.125f;
            }
        }
        current.swap(tmp);
        blurredTerrain[(size_t) pass] = current;
    }

    lastSmoothing = params.smoothing;
    rebuildMipLevels();
}

void TerrainOsc::rebuildMipLevels()
{
    mipLevels.clear();
    if (terrain.empty() || width <= 1 || height <= 1)
        return;

    MipLevel base;
    base.width = width;
    base.height = height;
    base.terrain = terrain;
    for (int i = 0; i < kBlurPassCount; ++i)
        base.blurredTerrain[(size_t) i] = blurredTerrain[(size_t) i].size() == terrain.size()
            ? blurredTerrain[(size_t) i]
            : terrain;
    mipLevels.push_back(std::move(base));

    auto downsample = [](const std::vector<float>& src, int srcW, int srcH, int dstW, int dstH)
    {
        std::vector<float> dst((size_t)dstW * (size_t)dstH, 0.f);
        auto at = [&](int x, int y)
        {
            x = (x % srcW + srcW) % srcW;
            y = (y % srcH + srcH) % srcH;
            return src[(size_t)y * (size_t)srcW + (size_t)x];
        };

        for (int y = 0; y < dstH; ++y)
        {
            for (int x = 0; x < dstW; ++x)
            {
                const int sx = x * 2;
                const int sy = y * 2;
                const float sum = at(sx, sy) + at(sx + 1, sy)
                                + at(sx, sy + 1) + at(sx + 1, sy + 1);
                dst[(size_t)y * (size_t)dstW + (size_t)x] = sum * 0.25f;
            }
        }

        return dst;
    };

    while (mipLevels.size() < 9)
    {
        const auto& prev = mipLevels.back();
        const int nextW = juce::jmax(2, prev.width / 2);
        const int nextH = juce::jmax(2, prev.height / 2);
        if (nextW == prev.width && nextH == prev.height)
            break;

        MipLevel next;
        next.width = nextW;
        next.height = nextH;
        next.terrain = downsample(prev.terrain, prev.width, prev.height, nextW, nextH);
        for (int i = 0; i < kBlurPassCount; ++i)
            next.blurredTerrain[(size_t) i] = downsample(prev.blurredTerrain[(size_t) i],
                                                         prev.width, prev.height, nextW, nextH);
        mipLevels.push_back(std::move(next));
    }
}

int TerrainOsc::smoothingBlurIndex() const noexcept
{
    return juce::jlimit(0, kBlurPassCount - 1,
                        juce::roundToInt(params.smoothing * (float)(kBlurPassCount - 1)));
}

int TerrainOsc::chooseMipLevel() const noexcept
{
    const auto& activeMips = sharedTerrain ? sharedTerrain->mipLevels : mipLevels;
    const int activeWidth = sharedTerrain ? sharedTerrain->width : width;
    const int activeHeight = sharedTerrain ? sharedTerrain->height : height;
    if (params.quality == Quality::Off || activeMips.size() <= 1)
        return 0;

    const float radius = juce::jmax(params.radiusX, params.radiusY) * juce::jmax(0.25f, params.pathSize);
    const float ratio = juce::jmax(params.ratioX, params.ratioY);
    const float pathScale = juce::jmax(1.f, juce::MathConstants<float>::twoPi * radius * ratio);
    const float texelsPerSample = std::abs(phaseInc) * (float)juce::jmax(activeWidth, activeHeight) * pathScale
                                / juce::jmax(1.f, params.zoom);

    float targetLevel = std::log2(juce::jmax(1.f, texelsPerSample));
    if (params.quality == Quality::Soft)
        targetLevel += 1.f;

    return juce::jlimit(0, (int)activeMips.size() - 1, juce::roundToInt(targetLevel));
}

float TerrainOsc::sampleTerrain(const std::vector<float>& data, int dataWidth, int dataHeight, float x, float y) const
{
    if (data.empty() || dataWidth <= 1 || dataHeight <= 1)
        return 0.f;

    x = wrap01(x) * (float)dataWidth;
    y = wrap01(y) * (float)dataHeight;
    const int x0 = (int)std::floor(x) % dataWidth;
    const int y0 = (int)std::floor(y) % dataHeight;
    const int x1 = (x0 + 1) % dataWidth;
    const int y1 = (y0 + 1) % dataHeight;
    const float tx = x - std::floor(x);
    const float ty = y - std::floor(y);

    const float a = data[(size_t)y0 * (size_t)dataWidth + (size_t)x0];
    const float b = data[(size_t)y0 * (size_t)dataWidth + (size_t)x1];
    const float c = data[(size_t)y1 * (size_t)dataWidth + (size_t)x0];
    const float d = data[(size_t)y1 * (size_t)dataWidth + (size_t)x1];
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
