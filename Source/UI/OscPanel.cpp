#include "OscPanel.h"
#include <cmath>

namespace
{
bool isSupportedTerrainHeightmapFile(const juce::File& file)
{
    const auto ext = file.getFileExtension().toLowerCase();
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp";
}

bool isSupportedGranularAudioFile(const juce::File& file)
{
    const auto ext = file.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff"
        || ext == ".flac" || ext == ".ogg";
}
}

// ── VAWaveDisplay ─────────────────────────────────────────────────────────────
// Shape indices match VA combo order: Sine=0 Saw=1 RevSaw=2 Square=3 Triangle=4 Pulse=5
void VAWaveDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(2.f);
    g.setColour(juce::Colour(0xff0a0c0e));
    g.fillRoundedRectangle(b, 3.f);
    g.setColour(accent.withAlpha(0.35f));
    g.drawRoundedRectangle(b, 3.f, 1.f);

    const int kPts = 128;
    juce::Path wave;
    float cx = b.getX(), cy = b.getCentreY(), w = b.getWidth(), h = b.getHeight() * 0.42f;

    for (int i = 0; i <= kPts; ++i)
    {
        float t  = (float)i / kPts;
        float ph = std::fmod(t * 2.f, 1.f);   // two cycles for visual interest
        float v  = 0.f;
        switch (shape)
        {
            case 0: v = std::sin(ph * juce::MathConstants<float>::twoPi); break;
            case 1: v = 1.f - 2.f * ph;                                    break;
            case 2: v = 2.f * ph - 1.f;                                    break;
            case 3: v = ph < 0.5f ? 1.f : -1.f;                            break;
            case 4: v = (ph < 0.5f) ? (4.f * ph - 1.f) : (3.f - 4.f * ph); break;
            case 5: v = ph < 0.25f ? 1.f : -1.f;                           break;
            default: break;
        }
        float x = cx + t * w;
        float y = cy - v * h;
        if (i == 0) wave.startNewSubPath(x, y); else wave.lineTo(x, y);
    }

    g.setColour(accent);
    g.strokePath(wave, juce::PathStrokeType(1.5f));
}

void GranularWaveformDisplay::setSample(
    std::shared_ptr<const GranularOsc::SampleData> newSample, juce::String newName)
{
    sample = std::move(newSample);
    sampleName = newName.isNotEmpty() ? std::move(newName) : "SAMPLE";
    repaint();
}

void GranularWaveformDisplay::setRegion(float start, float end, int slices,
                                        int selectedSlice, bool useTransientSlices)
{
    regionStart = juce::jlimit(0.f, 1.f, start);
    regionEnd = juce::jlimit(regionStart, 1.f, end);
    sliceCount = juce::jlimit(1, 32, slices);
    sliceIndex = juce::jlimit(0, sliceCount - 1, selectedSlice);
    transientSlices = useTransientSlices;
    repaint();
}

void GranularWaveformDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff07090a));
    g.fillRoundedRectangle(bounds, 4.f);
    g.setColour(accent.withAlpha(0.3f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.f, 1.f);

    auto graph = bounds.reduced(5.f, 16.f);
    g.setColour(accent.withAlpha(0.12f));
    g.drawHorizontalLine(juce::roundToInt(graph.getCentreY()),
                         graph.getX(), graph.getRight());

    if (sample != nullptr && graph.getWidth() > 1.f)
    {
        juce::Path waveform;
        const int columns = juce::jmax(1, juce::roundToInt(graph.getWidth()));
        for (int x = 0; x < columns; ++x)
        {
            const int first = x * GranularOsc::kSourceSamples / columns;
            const int last = juce::jmax(first + 1,
                (x + 1) * GranularOsc::kSourceSamples / columns);
            float lo = 1.f;
            float hi = -1.f;
            for (int i = first; i < juce::jmin(last, GranularOsc::kSourceSamples); ++i)
            {
                lo = juce::jmin(lo, (*sample)[(size_t)i]);
                hi = juce::jmax(hi, (*sample)[(size_t)i]);
            }
            const float px = graph.getX() + (float)x;
            waveform.startNewSubPath(px, graph.getCentreY() - hi * graph.getHeight() * 0.45f);
            waveform.lineTo(px, graph.getCentreY() - lo * graph.getHeight() * 0.45f);
        }
        g.setColour(accent.withAlpha(0.9f));
        g.strokePath(waveform, juce::PathStrokeType(1.f));
    }

    const float regionX = graph.getX() + graph.getWidth() * regionStart;
    const float regionRight = graph.getX() + graph.getWidth() * regionEnd;
    g.setColour(juce::Colours::black.withAlpha(0.48f));
    g.fillRect(graph.withRight(regionX));
    g.fillRect(graph.withLeft(regionRight));
    std::array<float, GranularOsc::kMaxTransientMarkers + 2> boundaries {};
    int boundaryCount = 1;
    boundaries[0] = regionStart;
    if (transientSlices && sample != nullptr)
    {
        for (int i = 1; i < sample->transientMarkerCount - 1; ++i)
        {
            const float marker = sample->transientMarkers[(size_t)i];
            if (marker > regionStart && marker < regionEnd)
                boundaries[(size_t)boundaryCount++] = marker;
        }
    }
    else
    {
        for (int i = 1; i < sliceCount; ++i)
            boundaries[(size_t)boundaryCount++]
                = regionStart + (regionEnd - regionStart) * (float)i / (float)sliceCount;
    }
    boundaries[(size_t)boundaryCount++] = regionEnd;
    const int selected = juce::jlimit(0, boundaryCount - 2, sliceIndex);
    const float selectedX = graph.getX() + graph.getWidth() * boundaries[(size_t)selected];
    const float selectedRight = graph.getX()
        + graph.getWidth() * boundaries[(size_t)selected + 1];
    g.setColour(accent.withAlpha(0.13f));
    g.fillRect(juce::Rectangle<float>(selectedX, graph.getY(),
                                      selectedRight - selectedX, graph.getHeight()));
    g.setColour(accent.brighter(0.3f).withAlpha(0.85f));
    g.drawVerticalLine(juce::roundToInt(regionX), graph.getY(), graph.getBottom());
    g.drawVerticalLine(juce::roundToInt(regionRight), graph.getY(), graph.getBottom());
    g.setColour(accent.withAlpha(0.35f));
    for (int i = 1; i < boundaryCount - 1; ++i)
        g.drawVerticalLine(juce::roundToInt(
                               graph.getX() + graph.getWidth() * boundaries[(size_t)i]),
                           graph.getY(), graph.getBottom());

    g.setColour(accent.withAlpha(0.75f));
    g.setFont(8.f);
    g.drawFittedText(sampleName.toUpperCase(), getLocalBounds().reduced(5, 2),
                     juce::Justification::centredTop, 1);
}

// ── 3D field display ─────────────────────────────────────────────────────────
Osc3DFieldDisplay::Osc3DFieldDisplay(juce::AudioProcessorValueTreeState& apvts,
                                     const juce::String& parameterPrefix,
                                     juce::Colour accentColour)
    : accent(accentColour)
{
    auto get = [&](const char* suffix)
    {
        return apvts.getRawParameterValue(parameterPrefix + suffix);
    };

    field = get("field");
    path = get("path");
    radius = get("radius");
    depth = get("depth");
    warp = get("warp");
    rotation = get("rotation");
    chaos = get("chaos");
    morph = get("morph");
    fieldFreq = get("field_freq");
    zRatio = get("z_ratio");
    fold = get("fold");
    drive = get("drive");
    previewOsc.prepare(48000.0);
    startTimerHz(20);
}

float Osc3DFieldDisplay::value(std::atomic<float>* parameter, float fallback) const noexcept
{
    return parameter != nullptr ? parameter->load() : fallback;
}

juce::Point<float> Osc3DFieldDisplay::project(Vec3 p, juce::Rectangle<float> bounds) const noexcept
{
    const float scaleX = bounds.getWidth() * 0.24f;
    const float scaleY = bounds.getHeight() * 0.22f;
    return {
        bounds.getCentreX() + (p.x - p.y) * scaleX,
        bounds.getCentreY() + (p.x + p.y) * scaleY - p.z * bounds.getHeight() * 0.28f
    };
}

Vec3 Osc3DFieldDisplay::pathPoint(float phase, const Osc3D::Params& params) const noexcept
{
    const float t = phase * juce::MathConstants<float>::twoPi;
    const float r = juce::jmin(params.radius, 2.f) * 0.55f;
    const float d = juce::jmin(params.depth, 2.f) * 0.45f;
    Vec3 p {};

    switch (params.path)
    {
        case Osc3D::PathType::Circle:
            p = { r * std::cos(t), r * std::sin(t), d * std::sin(t * 0.5f * params.zRatio) };
            break;
        case Osc3D::PathType::Spiral:
        {
            const float ramp = 0.25f + phase * 0.75f;
            p = { r * ramp * std::cos(t), r * ramp * std::sin(t), d * (phase - 0.5f) };
            break;
        }
        case Osc3D::PathType::Lissajous:
            p = { r * std::cos(3.f * t), r * std::sin(2.f * t),
                  d * std::sin(5.f * params.zRatio * t) };
            break;
        case Osc3D::PathType::RandomWalk:
            p = { r * std::sin(1.7f * t + 0.4f * std::sin(5.3f * t)),
                  r * std::sin(2.3f * t + 0.3f * std::cos(4.1f * t)),
                  d * std::sin(1.1f * t + 0.5f * std::sin(3.7f * t)) };
            break;
        case Osc3D::PathType::PhysicsOrbit:
        {
            const float orbit = r * (1.f - 0.3f * params.chaos * std::cos(t));
            p = { orbit * std::cos(t), orbit * std::sin(t),
                  d * std::sin(2.f * params.zRatio * t) };
            break;
        }
        case Osc3D::PathType::NumPaths:
            break;
    }

    const float angle = params.rotation * juce::MathConstants<float>::twoPi;
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return { c * p.x - s * p.y, s * p.x + c * p.y, p.z };
}

void Osc3DFieldDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff07090a));
    g.fillRoundedRectangle(bounds, 4.f);
    g.setColour(accent.withAlpha(0.28f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.f, 0.8f);

    Osc3D::Params params;
    params.field = static_cast<Osc3D::FieldType>(
        juce::jlimit(0, (int)Osc3D::FieldType::NumFields - 1,
                     juce::roundToInt(value(field, 0.f))));
    params.path = static_cast<Osc3D::PathType>(
        juce::jlimit(0, (int)Osc3D::PathType::NumPaths - 1,
                     juce::roundToInt(value(path, 0.f))));
    params.radius = value(radius, 1.f);
    params.depth = value(depth, 1.f);
    params.warpAmt = value(warp, 0.25f);
    params.rotation = value(rotation, 0.f);
    params.chaos = value(chaos, 0.f);
    params.morphT = value(morph, 0.f);
    params.fieldFreq = value(fieldFreq, 1.f);
    params.zRatio = value(zRatio, 1.f);
    params.foldAmt = value(fold, 0.f);
    params.drive = value(drive, 1.f);
    previewOsc.setParams(params);

    const auto graph = bounds.reduced(7.f, 5.f);
    constexpr int gridSize = 11;
    for (int axis = 0; axis < 2; ++axis)
    {
        for (int line = 0; line < gridSize; ++line)
        {
            juce::Path wire;
            for (int point = 0; point < gridSize; ++point)
            {
                const float a = juce::jmap((float)line, 0.f, (float)(gridSize - 1), -1.1f, 1.1f);
                const float b = juce::jmap((float)point, 0.f, (float)(gridSize - 1), -1.1f, 1.1f);
                const float x = axis == 0 ? a : b;
                const float y = axis == 0 ? b : a;
                const float z = previewOsc.evaluatePreview({ x, y, 0.f }) * 0.62f;
                const auto projected = project({ x, y, z }, graph);
                if (point == 0) wire.startNewSubPath(projected);
                else            wire.lineTo(projected);
            }
            g.setColour(accent.withAlpha(axis == 0 ? 0.28f : 0.16f));
            g.strokePath(wire, juce::PathStrokeType(0.7f));
        }
    }

    juce::Path orbit;
    constexpr int pathPoints = 72;
    for (int i = 0; i <= pathPoints; ++i)
    {
        auto p = pathPoint((float)i / (float)pathPoints, params);
        p.z += previewOsc.evaluatePreview(p) * 0.18f;
        const auto projected = project(p, graph);
        if (i == 0) orbit.startNewSubPath(projected);
        else        orbit.lineTo(projected);
    }
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.strokePath(orbit, juce::PathStrokeType(2.8f));
    g.setColour(accent.brighter(0.25f));
    g.strokePath(orbit, juce::PathStrokeType(1.1f));

    const float animatedPhase = std::fmod(
        (float)(juce::Time::getMillisecondCounterHiRes() * 0.00012), 1.f);
    auto active = pathPoint(animatedPhase, params);
    active.z += previewOsc.evaluatePreview(active) * 0.18f;
    const auto dot = project(active, graph);
    g.setColour(accent);
    g.fillEllipse(dot.x - 2.5f, dot.y - 2.5f, 5.f, 5.f);
}

void Osc3DFieldDisplay::timerCallback()
{
    if (isShowing())
        repaint();
}

// ── TerrainHeightmapDisplay ─────────────────────────────────────────────────
TerrainHeightmapDisplay::TerrainHeightmapDisplay(juce::Colour accentColour)
    : accent(accentColour)
{
    rebuildProceduralPreview();
    startTimerHz(24);
}

void TerrainHeightmapDisplay::setHeightmapImage(const juce::Image& image)
{
    if (!image.isValid())
        return;

    const int maxDim = 192;
    const float scale = (float)maxDim / (float)juce::jmax(image.getWidth(), image.getHeight());
    const int w = juce::jmax(2, juce::roundToInt((float)image.getWidth() * juce::jmin(1.0f, scale)));
    const int h = juce::jmax(2, juce::roundToInt((float)image.getHeight() * juce::jmin(1.0f, scale)));
    auto source = image.rescaled(w, h, juce::Graphics::mediumResamplingQuality);

    heightmapPreview = juce::Image(juce::Image::RGB, w, h, true);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const auto c = source.getPixelAt(x, y);
            const float brightness = (c.getFloatRed() + c.getFloatGreen() + c.getFloatBlue()) / 3.f;
            heightmapPreview.setPixelAt(x, y, juce::Colour::fromFloatRGBA(brightness, brightness, brightness, 1.f));
        }
    }

    repaint();
}

void TerrainHeightmapDisplay::setScanState(const ScanState& newState)
{
    state = newState;
    repaint();
}

void TerrainHeightmapDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(2.f);
    g.setColour(juce::Colour(0xff080a0d));
    g.fillRoundedRectangle(b, 4.f);
    g.setColour(accent.withAlpha(0.35f));
    g.drawRoundedRectangle(b, 4.f, 1.f);

    auto map = b.reduced(7.f);
    auto image = heightmapPreview.isValid() ? heightmapPreview : proceduralPreview;
    if (image.isValid())
        g.drawImage(image, map);

    g.setColour(juce::Colours::black.withAlpha(0.22f));
    for (int i = 1; i < 4; ++i)
    {
        const float x = map.getX() + map.getWidth() * (float)i / 4.f;
        const float y = map.getY() + map.getHeight() * (float)i / 4.f;
        g.drawVerticalLine(juce::roundToInt(x), map.getY(), map.getBottom());
        g.drawHorizontalLine(juce::roundToInt(y), map.getX(), map.getRight());
    }

    juce::Path path;
    const int steps = 120;
    for (int i = 0; i <= steps; ++i)
    {
        const auto [px, py] = pointFor((float)i / (float)steps);
        const float x = map.getX() + px * map.getWidth();
        const float y = map.getY() + py * map.getHeight();
        if (i == 0) path.startNewSubPath(x, y);
        else        path.lineTo(x, y);
    }

    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.strokePath(path, juce::PathStrokeType(3.5f));
    g.setColour(accent);
    g.strokePath(path, juce::PathStrokeType(1.5f));

    const double now = juce::Time::getMillisecondCounterHiRes() * 0.000035;
    const auto [dotX, dotY] = pointFor(wrap01((float)(now * (double)state.scanRate)));
    const float cx = map.getX() + dotX * map.getWidth();
    const float cy = map.getY() + dotY * map.getHeight();
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.fillEllipse(cx - 5.f, cy - 5.f, 10.f, 10.f);
    g.setColour(accent.brighter(0.35f));
    g.fillEllipse(cx - 3.f, cy - 3.f, 6.f, 6.f);

    const float centerX = map.getX() + wrap01(state.centerX) * map.getWidth();
    const float centerY = map.getY() + wrap01(state.centerY) * map.getHeight();
    g.setColour(accent.withAlpha(0.75f));
    g.drawLine(centerX - 5.f, centerY, centerX + 5.f, centerY, 1.f);
    g.drawLine(centerX, centerY - 5.f, centerX, centerY + 5.f, 1.f);
}

void TerrainHeightmapDisplay::timerCallback()
{
    repaint();
}

void TerrainHeightmapDisplay::rebuildProceduralPreview()
{
    const int size = 128;
    proceduralPreview = juce::Image(juce::Image::RGB, size, size, true);
    for (int y = 0; y < size; ++y)
    {
        const float ny = (float)y / (float)(size - 1);
        for (int x = 0; x < size; ++x)
        {
            const float nx = (float)x / (float)(size - 1);
            const float dx = nx - 0.5f;
            const float dy = ny - 0.5f;
            const float r = std::sqrt(dx * dx + dy * dy);
            float v = 0.52f * std::sin((nx * 5.f + ny * 2.f) * juce::MathConstants<float>::twoPi);
            v += 0.34f * std::sin((r * 13.f - ny * 1.5f) * juce::MathConstants<float>::twoPi);
            v += 0.18f * noise(nx * 9.f, ny * 9.f);
            const float grey = juce::jmap(juce::jlimit(-1.f, 1.f, v), -1.f, 1.f, 0.08f, 0.92f);
            proceduralPreview.setPixelAt(x, y, juce::Colour::fromFloatRGBA(grey, grey, grey, 1.f));
        }
    }
}

std::pair<float, float> TerrainHeightmapDisplay::pointFor(float t) const noexcept
{
    t = wrap01(t + state.phaseOffset);
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float aX = twoPi * t * state.ratioX;
    const float aY = twoPi * t * state.ratioY;
    const float radiusX = state.radiusX * state.pathSize;
    const float radiusY = state.radiusY * state.pathSize;
    float x = 0.f;
    float y = 0.f;

    switch (state.pathMode)
    {
        case 0:
            x = radiusX * std::cos(twoPi * t);
            y = radiusX * std::sin(twoPi * t);
            break;
        case 1:
            x = radiusX * std::cos(aX);
            y = radiusY * std::sin(aY);
            break;
        case 2:
            x = radiusX * std::sin(aX + twoPi * 0.125f);
            y = radiusY * std::sin(aY);
            break;
        case 3:
        {
            const float r = 1.f - state.spiral + state.spiral * t;
            x = radiusX * r * std::cos(aX);
            y = radiusY * r * std::sin(aY);
            break;
        }
        case 4:
            x = radiusX * (2.f * t - 1.f);
            y = radiusY * 0.2f * std::sin(aY);
            break;
        case 5:
            x = radiusX * (0.72f * std::cos(twoPi * t)
                + 0.56f * noise(t * state.ratioX * 4.f + 11.7f, 0.37f));
            y = radiusY * (0.72f * std::sin(twoPi * t * 1.21f)
                + 0.56f * noise(t * state.ratioY * 4.f + 23.1f, 0.61f));
            break;
        default:
            break;
    }

    const float angle = state.rotation * twoPi;
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float rotatedX = x * c - y * s;
    const float rotatedY = x * s + y * c;
    const float zoom = juce::jlimit(1.f, 16.f, state.zoom);

    return { wrap01(state.centerX + rotatedX / zoom),
             wrap01(state.centerY + rotatedY / zoom) };
}

float TerrainHeightmapDisplay::wrap01(float v) noexcept
{
    v -= std::floor(v);
    return v;
}

float TerrainHeightmapDisplay::noise(float x, float y) noexcept
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

// ─────────────────────────────────────────────────────────────────────────────
OscPanel::OscPanel(juce::AudioProcessorValueTreeState& a, const juce::String& id,
                   const juce::String& lbl,
                   TerrainImportCallback terrainImportCallback,
                   int terrainSlotIndex,
                   GranularImportCallback granularImportCallback,
                   int granularSlotIndex,
                   GranularCaptureCallback granularCaptureCallback)
    : apvts(a),
      oscId(id),
      label(lbl),
      accent(IndustrialTheme::accentForOsc(id)),
      terrainImport(std::move(terrainImportCallback)),
      terrainIndex(terrainSlotIndex),
      granularImport(std::move(granularImportCallback)),
      granularIndex(granularSlotIndex),
      granularCapture(std::move(granularCaptureCallback))
{
    titleLabel.setText(lbl, juce::dontSendNotification);
    IndustrialTheme::styleTitle(titleLabel, accent, 14.f);
    addAndMakeVisible(titleLabel);

    juce::String pfx = "osc_" + oscId + "_";

    // Enable / hard-shutoff toggle
    enableButton.setButtonText("ON");
    enableButton.setTooltip("Enable / disable this oscillator engine");
    enableButton.setColour(juce::ToggleButton::textColourId,    accent);
    enableButton.setColour(juce::ToggleButton::tickColourId,    accent);
    enableButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff555555));
    addAndMakeVisible(enableButton);
    enableAtt = std::make_unique<BAtt>(apvts, pfx + "on", enableButton);

    setupSlider(levelSlider,  "Oscillator Level");
    levelAtt = std::make_unique<Att>(apvts, pfx + "level", levelSlider);

    setupSlider(tuneSlider, "Fine Tune (cents)");
    tuneAtt = std::make_unique<Att>(apvts, pfx + "tune", tuneSlider);

    setupSlider(octaveSlider, "Octave Transpose");
    octaveAtt = std::make_unique<Att>(apvts, pfx + "octave", octaveSlider);

    setupADSRSlider(adsrA, "Attack");  attA = std::make_unique<Att>(apvts, pfx + "adsr_a", adsrA);
    setupADSRSlider(adsrD, "Decay");   attD = std::make_unique<Att>(apvts, pfx + "adsr_d", adsrD);
    setupADSRSlider(adsrS, "Sustain"); attS = std::make_unique<Att>(apvts, pfx + "adsr_s", adsrS);
    setupADSRSlider(adsrR, "Release"); attR = std::make_unique<Att>(apvts, pfx + "adsr_r", adsrR);

    if (oscId.startsWith("3d"))
    {
        modeBoxA.addItemList({"SinSum","SinProd","Ripple","Torus",
                              "Vortex","Turbulence","Lorenz","Dipole",
                              "Folded","Chebyshev",
                              "Harmonic","Chord","Shell","Gravity",
                              "Formant","Metallic","Tension","Body"}, 1);
        modeBoxB.addItemList({"Circle","Spiral","Lissajous","Random","Orbit"}, 1);
        modeBoxA.setTooltip("3D Field");
        modeBoxB.setTooltip("3D Path");
        IndustrialTheme::styleCombo(modeBoxA, accent);
        IndustrialTheme::styleCombo(modeBoxB, accent);
        addAndMakeVisible(modeBoxA);
        addAndMakeVisible(modeBoxB);
        modeAttA = std::make_unique<CAtt>(apvts, pfx + "field", modeBoxA);
        modeAttB = std::make_unique<CAtt>(apvts, pfx + "path",  modeBoxB);
        numModeBoxes = 2;

        setupExtraSlider(extra1,  pfx + "radius",    "3D Radius",       extraAtt1);
        setupExtraSlider(extra2,  pfx + "depth",     "3D Depth",        extraAtt2);
        setupExtraSlider(extra3,  pfx + "warp",      "3D Warp",         extraAtt3);
        setupExtraSlider(extra4,  pfx + "rotation",  "3D Rotation",     extraAtt4);
        setupExtraSlider(extra5,  pfx + "chaos",     "3D Chaos",        extraAtt5);
        setupExtraSlider(extra6,  pfx + "morph",     "3D Morph",        extraAtt6);
        setupExtraSlider(extra7,  pfx + "field_freq","Field Frequency",  extraAtt7);
        setupExtraSlider(extra8,  pfx + "z_ratio",   "Z Ratio",          extraAtt8);
        setupExtraSlider(extra9,  pfx + "fold",      "Coordinate Fold",  extraAtt9);
        setupExtraSlider(extra10, pfx + "drive",     "Field Drive",      extraAtt10);
        numExtraSliders = 10;

        field3dDisplay = std::make_unique<Osc3DFieldDisplay>(apvts, pfx, accent);
        addAndMakeVisible(*field3dDisplay);
    }
    else if (oscId.startsWith("ks"))
    {
        setupExtraSlider(extra1, pfx + "decay",      "Decay (0=short, 1=long)",         extraAtt1);
        setupExtraSlider(extra2, pfx + "brightness", "Brightness (loop LPF blend)",      extraAtt2);
        setupExtraSlider(extra3, pfx + "stiffness",  "Stiffness (all-pass dispersion)",  extraAtt3);
        setupExtraSlider(extra4, pfx + "morph",      "Excitation morph (noise to glitch)", extraAtt4);
        setupExtraSlider(extra5, pfx + "position",   "Pluck position",                   extraAtt5);
        numExtraSliders = 5;
    }
    else if (oscId.startsWith("grain"))
    {
        modeBoxA.addItemList({"Free Cloud","Burst","Audio Rate"}, 1);
        modeBoxB.addItemList({"Hann","Triangle","Gaussian","Tukey","Rect","Exp Attack","Exp Decay"}, 1);
        modeBoxC.addItemList({"Off","Glass","Metal","Wood","Plate","String","Drum","Pipe","Alien"}, 1);
        modeBoxA.setTooltip("Grain Trigger Mode");
        modeBoxB.setTooltip("Grain Window");
        modeBoxC.setTooltip("Physical Body Type");
        IndustrialTheme::styleCombo(modeBoxA, accent);
        IndustrialTheme::styleCombo(modeBoxB, accent);
        IndustrialTheme::styleCombo(modeBoxC, accent);
        addAndMakeVisible(modeBoxA);
        addAndMakeVisible(modeBoxB);
        addAndMakeVisible(modeBoxC);
        modeAttA = std::make_unique<CAtt>(apvts, pfx + "trigger_mode", modeBoxA);
        modeAttB = std::make_unique<CAtt>(apvts, pfx + "window", modeBoxB);
        modeAttC = std::make_unique<CAtt>(apvts, pfx + "body_type", modeBoxC);
        numModeBoxes = 3;

        setupExtraSlider(extra1,  pfx + "position", "Grain Position",          extraAtt1);
        setupExtraSlider(extra2,  pfx + "size",     "Grain Size",              extraAtt2);
        setupExtraSlider(extra3,  pfx + "density",  "Grain Density",           extraAtt3);
        setupExtraSlider(extra4,  pfx + "pitch",    "Grain Pitch",             extraAtt4);
        setupExtraSlider(extra5,  pfx + "spray",    "Position Spray",          extraAtt5);
        setupExtraSlider(extra6,  pfx + "jitter",   "Timing / Pitch Jitter",   extraAtt6);
        setupExtraSlider(extra7,  pfx + "pan",      "Pan Spread",              extraAtt7);
        setupExtraSlider(extra8,  pfx + "reverse",  "Reverse Probability",     extraAtt8);
        setupExtraSlider(extra9,  pfx + "scan",     "Position Scan Speed",     extraAtt9);
        setupExtraSlider(extra10, pfx + "freeze",   "Position Freeze Amount",  extraAtt10);
        setupExtraSlider(extra11, pfx + "body_root",   "Body Root Pitch",       extraAtt11);
        setupExtraSlider(extra12, pfx + "body_size",   "Body Size",             extraAtt12);
        setupExtraSlider(extra13, pfx + "body_decay",  "Body Decay",            extraAtt13);
        setupExtraSlider(extra14, pfx + "body_damp",   "Body Damping",          extraAtt14);
        setupExtraSlider(extra15, pfx + "body_bright", "Body Brightness",       extraAtt15);
        setupExtraSlider(extra16, pfx + "body_inharm", "Body Inharmonicity",    extraAtt16);
        setupExtraSlider(extra17, pfx + "body_excite", "Body Excitation",       extraAtt17);
        setupExtraSlider(extra18, pfx + "body_mix",    "Body Mix",              extraAtt18);
        setupExtraSlider(extra19, pfx + "body_spread", "Body Stereo Spread",    extraAtt19);
        setupExtraSlider(extra20, pfx + "body_modes",  "Body Mode Count",       extraAtt20);
        numExtraSliders = 20;

        granularWaveform = std::make_unique<GranularWaveformDisplay>(accent);
        addAndMakeVisible(*granularWaveform);
        const char* regionIds[] = {
            "region_start", "region_end", "slice_count", "slice_index", "root_note" };
        const char* regionNames[] = { "START", "END", "SLICES", "SELECT", "ROOT" };
        for (int i = 0; i < 5; ++i)
        {
            setupSlider(regionSliders[(size_t)i], regionNames[i]);
            regionAtts[(size_t)i] = std::make_unique<Att>(
                apvts, pfx + regionIds[i], regionSliders[(size_t)i]);
            regionLabels[(size_t)i].setText(regionNames[i], juce::dontSendNotification);
            IndustrialTheme::styleLabel(regionLabels[(size_t)i], accent, 8.f);
            addAndMakeVisible(regionLabels[(size_t)i]);
            regionSliders[(size_t)i].onValueChange = [this]
            {
                if (granularWaveform != nullptr)
                    granularWaveform->setRegion(
                        (float)regionSliders[0].getValue(),
                        (float)regionSliders[1].getValue(),
                        (int)regionSliders[2].getValue(),
                        (int)regionSliders[3].getValue(),
                        transientSlicesButton.getToggleState());
            };
        }
        transientSlicesButton.setTooltip("Use detected transients as slice boundaries");
        transientSlicesButton.setColour(juce::ToggleButton::textColourId, accent);
        transientSlicesButton.setColour(juce::ToggleButton::tickColourId, accent);
        transientSlicesButton.onClick = [this]
        {
            if (granularWaveform != nullptr)
                granularWaveform->setRegion(
                    (float)regionSliders[0].getValue(),
                    (float)regionSliders[1].getValue(),
                    (int)regionSliders[2].getValue(),
                    (int)regionSliders[3].getValue(),
                    transientSlicesButton.getToggleState());
        };
        addAndMakeVisible(transientSlicesButton);
        transientSlicesAtt = std::make_unique<BAtt>(
            apvts, pfx + "transient_slices", transientSlicesButton);

        granularSceneBox.addItemList(
            { "SCENE 1", "SCENE 2", "SCENE 3", "SCENE 4" }, 1);
        granularSceneBox.setTooltip("Recall a stored granular region scene");
        IndustrialTheme::styleCombo(granularSceneBox, accent);
        granularSceneBox.onChange = [this] { recallGranularScene(); };
        addAndMakeVisible(granularSceneBox);
        granularSceneAtt = std::make_unique<CAtt>(
            apvts, pfx + "scene", granularSceneBox);

        saveGranularSceneButton.setTooltip(
            "Store the current region, slice, AUTO, and root settings in this scene");
        IndustrialTheme::styleButton(saveGranularSceneButton, accent);
        saveGranularSceneButton.onClick = [this] { saveGranularScene(); };
        addAndMakeVisible(saveGranularSceneButton);

        sampleBrowserBox.setTextWhenNothingSelected("SAMPLE LIBRARY");
        sampleBrowserBox.setTextWhenNoChoicesAvailable("NO LIBRARY SAMPLES");
        sampleBrowserBox.setTooltip(
            "Audio files in Music/GLIESE_710/Samples; choose one to load");
        IndustrialTheme::styleCombo(sampleBrowserBox, accent);
        sampleBrowserBox.onChange = [this]
        {
            const int index = sampleBrowserBox.getSelectedItemIndex();
            if (index >= 0 && index < (int)sampleBrowserFiles.size())
                importGranularSampleFile(sampleBrowserFiles[(size_t)index]);
        };
        addAndMakeVisible(sampleBrowserBox);
        rescanSamplesButton.setTooltip("Rescan Music/GLIESE_710/Samples");
        IndustrialTheme::styleButton(rescanSamplesButton, accent);
        rescanSamplesButton.onClick = [this] { refreshGranularSampleBrowser(); };
        addAndMakeVisible(rescanSamplesButton);
        densitySyncButton.setTooltip("Sync grain triggers to host tempo");
        densitySyncButton.setColour(juce::ToggleButton::textColourId, accent);
        densitySyncButton.setColour(juce::ToggleButton::tickColourId, accent);
        addAndMakeVisible(densitySyncButton);
        densitySyncAtt = std::make_unique<BAtt>(
            apvts, pfx + "density_sync", densitySyncButton);
        densityDivisionBox.addItemList(
            { "4 BAR","2 BAR","1 BAR","1/2","1/4","1/8","1/8T",
              "1/16","1/16T","1/32" }, 1);
        densityDivisionBox.setTooltip("Tempo-synced grain trigger division");
        IndustrialTheme::styleCombo(densityDivisionBox, accent);
        addAndMakeVisible(densityDivisionBox);
        densityDivisionAtt = std::make_unique<CAtt>(
            apvts, pfx + "density_division", densityDivisionBox);
        pitchQuantizeButton.setTooltip("Quantize each grain pitch to the selected key and scale");
        pitchQuantizeButton.setColour(juce::ToggleButton::textColourId, accent);
        pitchQuantizeButton.setColour(juce::ToggleButton::tickColourId, accent);
        addAndMakeVisible(pitchQuantizeButton);
        pitchQuantizeAtt = std::make_unique<BAtt>(
            apvts, pfx + "pitch_quantize", pitchQuantizeButton);
        pitchCloudBox.addItemList({ "CLOUD OFF","±2","±4","±7","±12","±24" }, 1);
        pitchScaleBox.addItemList(
            { "CHROM","MAJOR","MINOR","DORIAN","MIXOLYD","PENTA" }, 1);
        pitchScaleRootBox.addItemList(
            { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }, 1);
        for (auto* box : { &pitchCloudBox, &pitchScaleBox, &pitchScaleRootBox })
        {
            IndustrialTheme::styleCombo(*box, accent);
            addAndMakeVisible(*box);
        }
        pitchCloudBox.setTooltip("Random pitch-cloud spread in semitones");
        pitchScaleBox.setTooltip("Pitch-cloud quantization scale");
        pitchScaleRootBox.setTooltip("Pitch-cloud musical key");
        pitchCloudAtt = std::make_unique<CAtt>(
            apvts, pfx + "pitch_cloud", pitchCloudBox);
        pitchScaleAtt = std::make_unique<CAtt>(
            apvts, pfx + "pitch_scale", pitchScaleBox);
        pitchScaleRootAtt = std::make_unique<CAtt>(
            apvts, pfx + "pitch_scale_root", pitchScaleRootBox);
        refreshGranularSampleBrowser();
        hasGranularImportButton = granularImport != nullptr && granularIndex >= 0;
        if (hasGranularImportButton)
        {
            importSampleButton.setTooltip("Load or drop a WAV, AIFF, FLAC, or OGG sample");
            IndustrialTheme::styleButton(importSampleButton, accent);
            importSampleButton.onClick = [this] { launchGranularImportChooser(); };
            addAndMakeVisible(importSampleButton);
            captureSampleButton.setTooltip(
                "Freeze the most recent eight seconds from the plugin input");
            IndustrialTheme::styleButton(captureSampleButton, accent);
            captureSampleButton.onClick = [this]
            {
                if (!granularCapture)
                    return;
                auto sample = granularCapture(granularIndex);
                if (sample != nullptr)
                {
                    granularWaveform->setSample(sample, "LIVE CAPTURE");
                    captureSampleButton.setButtonText("CAPTURED");
                }
                else
                {
                    captureSampleButton.setButtonText("NO INPUT");
                }
            };
            addAndMakeVisible(captureSampleButton);
        }
    }
    else if (oscId.startsWith("terrain"))
    {
        modeBoxA.addItemList({"Circle","Ellipse","Lissajous","Spiral","Line","Orbit Noise"}, 1);
        modeBoxB.addItemList({"Off","Auto","Soft"}, 1);
        modeBoxA.setTooltip("Wave-terrain scan path");
        modeBoxB.setTooltip("Terrain mipmap anti-aliasing quality");
        IndustrialTheme::styleCombo(modeBoxA, accent);
        IndustrialTheme::styleCombo(modeBoxB, accent);
        addAndMakeVisible(modeBoxA);
        addAndMakeVisible(modeBoxB);
        modeAttA = std::make_unique<CAtt>(apvts, pfx + "path", modeBoxA);
        modeAttB = std::make_unique<CAtt>(apvts, pfx + "quality", modeBoxB);
        numModeBoxes = 2;

        setupExtraSlider(extra1,  pfx + "center_x",  "Terrain Center X",       extraAtt1);
        setupExtraSlider(extra2,  pfx + "center_y",  "Terrain Center Y",       extraAtt2);
        setupExtraSlider(extra3,  pfx + "radius_x",  "Terrain Radius X",       extraAtt3);
        setupExtraSlider(extra4,  pfx + "radius_y",  "Terrain Radius Y",       extraAtt4);
        setupExtraSlider(extra5,  pfx + "zoom",      "Terrain Zoom",           extraAtt5);
        setupExtraSlider(extra6,  pfx + "rotation",  "Path Rotation",          extraAtt6);
        setupExtraSlider(extra7,  pfx + "path_size", "Path Shape Size",        extraAtt7);
        setupExtraSlider(extra8,  pfx + "ratio_x",   "Path Ratio X",           extraAtt8);
        setupExtraSlider(extra9,  pfx + "ratio_y",   "Path Ratio Y",           extraAtt9);
        setupExtraSlider(extra10, pfx + "scan_rate", "Terrain Scan Rate",      extraAtt10);
        setupExtraSlider(extra11, pfx + "phase",     "Phase Offset",           extraAtt11);
        setupExtraSlider(extra12, pfx + "spiral",    "Spiral Amount",          extraAtt12);
        setupExtraSlider(extra13, pfx + "drive",     "Terrain Drive",          extraAtt13);
        setupExtraSlider(extra14, pfx + "offset",    "Terrain Offset",         extraAtt14);
        setupExtraSlider(extra15, pfx + "smoothing", "Terrain Blur Smoothing", extraAtt15);
        setupExtraSlider(extra16, pfx + "morph",     "Procedural Morph",       extraAtt16);
        setupExtraSlider(extra17, pfx + "gain",      "Terrain Gain",           extraAtt17);
        numExtraSliders = 17;

        importHeightmapButton.setTooltip("Import a PNG, JPG, or BMP heightmap for this terrain oscillator");
        IndustrialTheme::styleButton(importHeightmapButton, accent);
        importHeightmapButton.onClick = [this] { launchTerrainImportChooser(); };
        addAndMakeVisible(importHeightmapButton);
        hasTerrainImportButton = true;

        terrainDisplay = std::make_unique<TerrainHeightmapDisplay>(accent);
        addAndMakeVisible(*terrainDisplay);

        auto updateDisplay = [this] { refreshTerrainDisplay(); };
        modeBoxA.onChange = updateDisplay;
        modeBoxB.onChange = updateDisplay;
        extra1.onValueChange = updateDisplay;
        extra2.onValueChange = updateDisplay;
        extra3.onValueChange = updateDisplay;
        extra4.onValueChange = updateDisplay;
        extra5.onValueChange = updateDisplay;
        extra6.onValueChange = updateDisplay;
        extra7.onValueChange = updateDisplay;
        extra8.onValueChange = updateDisplay;
        extra9.onValueChange = updateDisplay;
        extra10.onValueChange = updateDisplay;
        extra11.onValueChange = updateDisplay;
        extra12.onValueChange = updateDisplay;
    }
    else if (oscId.startsWith("va"))
    {
        modeBoxA.addItemList({"Native","Moog","Roland","Oberheim","Sequential"}, 1);
        modeBoxA.setTooltip("Virtual analog oscillator model");
        IndustrialTheme::styleCombo(modeBoxA, accent);
        addAndMakeVisible(modeBoxA);
        modeAttA = std::make_unique<CAtt>(apvts, pfx + "model", modeBoxA);
        numModeBoxes = 1;

        setupExtraSlider(extra1, pfx + "shape",  "Wave Shape Morph (0=Sine 1=Saw 2=RevSaw 3=Sqr 4=Tri 5=Pulse)", extraAtt1);
        setupExtraSlider(extra2, pfx + "pw",     "Pulse Width",      extraAtt2);
        setupExtraSlider(extra3, pfx + "detune", "Unison Detune",    extraAtt3);
        setupExtraSlider(extra4, pfx + "unison", "Unison Voices",    extraAtt4);
        setupExtraSlider(extra5, pfx + "spread", "Stereo Spread",    extraAtt5);
        setupExtraSlider(extra6, pfx + "ring",   "Ring Modulation",  extraAtt6);
        setupExtraSlider(extra7, pfx + "fold",   "Wave Folding",     extraAtt7);
        numExtraSliders = 7;

        waveDisplay = std::make_unique<VAWaveDisplay>(accent);
        addAndMakeVisible(*waveDisplay);
        waveDisplay->setShape(juce::roundToInt(extra1.getValue()));
        extra1.onValueChange = [this] {
            waveDisplay->setShape(juce::roundToInt(
                juce::jlimit(extra1.getMinimum(), extra1.getMaximum(), extra1.getValue())));
        };
    }
    else if (oscId.startsWith("wt"))
    {
        modeBoxA.addItemList({"Sine","Triangle","Saw","Square","Pulse",
                              "Half Sine","Abs Sine","Sine Cube",
                              "Hollow Odd","Even Glass","Vocal A","Metallic",
                              "Sync Saw","Reso Fold","Noisy Digital"}, 1);
        modeBoxB.addItemList({"Off","Bend+","Bend-","Bend +/-","Mirror",
                              "Sync","Window Sync","Quantize","Fold","Formant",
                              "PD Saw","PD Square","PD Pulse","PD Resonant","PD Formant"}, 1);
        modeBoxA.setTooltip("Wavetable Shape");
        modeBoxB.setTooltip("Wavetable Warp Mode");
        IndustrialTheme::styleCombo(modeBoxA, accent);
        IndustrialTheme::styleCombo(modeBoxB, accent);
        addAndMakeVisible(modeBoxA);
        addAndMakeVisible(modeBoxB);
        modeAttA = std::make_unique<CAtt>(apvts, pfx + "table", modeBoxA);
        modeAttB = std::make_unique<CAtt>(apvts, pfx + "warp_mode", modeBoxB);
        numModeBoxes = 2;

        setupExtraSlider(extra1, pfx + "pos",        "Wavetable Position",        extraAtt1);
        setupExtraSlider(extra2, pfx + "warp",       "Warp Amount",               extraAtt2);
        setupExtraSlider(extra3, pfx + "pd_break",   "Phase Distortion Breakpoint", extraAtt3);
        setupExtraSlider(extra4, pfx + "unison",     "Unison Voices",             extraAtt4);
        setupExtraSlider(extra5, pfx + "detune",     "Unison Detune",             extraAtt5);
        setupExtraSlider(extra6, pfx + "spread",     "Stereo Spread",             extraAtt6);
        setupExtraSlider(extra7, pfx + "pos_spread", "Wavetable Position Spread", extraAtt7);
        setupExtraSlider(extra8, pfx + "phase",      "Phase Offset",              extraAtt8);
        setupExtraSlider(extra9, pfx + "random",     "Random Phase Amount",       extraAtt9);
        numExtraSliders = 9;

        retrigButton.setButtonText("RT");
        retrigButton.setTooltip("Retrigger WT phase on note-on");
        retrigButton.setColour(juce::ToggleButton::textColourId, accent);
        retrigButton.setColour(juce::ToggleButton::tickColourId, accent);
        retrigButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff555555));
        addAndMakeVisible(retrigButton);
        retrigAtt = std::make_unique<BAtt>(apvts, pfx + "retrig", retrigButton);
        hasRetrigButton = true;
    }

    // ── Labels ────────────────────────────────────────────────────────────────
    auto makeLabel = [&](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification);
        IndustrialTheme::styleLabel(l, accent, 10.f);
        addAndMakeVisible(l);
    };

    makeLabel(levelLabel,  "LEVEL");
    makeLabel(tuneLabel,   "TUNE");
    makeLabel(octaveLabel, "OCTAVE");

    if (oscId.startsWith("3d"))
    {
        makeLabel(modeALabel, "FIELD");
        makeLabel(modeBLabel, "PATH");
        makeLabel(extraLabels[0], "RADIUS");
        makeLabel(extraLabels[1], "DEPTH");
        makeLabel(extraLabels[2], "WARP");
        makeLabel(extraLabels[3], "ROTAT");
        makeLabel(extraLabels[4], "CHAOS");
        makeLabel(extraLabels[5], "MORPH");
        makeLabel(extraLabels[6], "FREQ");
        makeLabel(extraLabels[7], "Z-RAT");
        makeLabel(extraLabels[8], "FOLD");
        makeLabel(extraLabels[9], "DRIVE");
    }
    else if (oscId.startsWith("ks"))
    {
        makeLabel(extraLabels[0], "DECAY");
        makeLabel(extraLabels[1], "BRIGHT");
        makeLabel(extraLabels[2], "STIFF");
        makeLabel(extraLabels[3], "MORPH");
        makeLabel(extraLabels[4], "POS");
    }
    else if (oscId.startsWith("grain"))
    {
        makeLabel(modeALabel, "TRIGGER");
        makeLabel(modeBLabel, "WINDOW");
        makeLabel(modeCLabel, "BODY");
        makeLabel(extraLabels[0], "POS");
        makeLabel(extraLabels[1], "SIZE");
        makeLabel(extraLabels[2], "DENS");
        makeLabel(extraLabels[3], "PITCH");
        makeLabel(extraLabels[4], "SPRAY");
        makeLabel(extraLabels[5], "JITTR");
        makeLabel(extraLabels[6], "PAN");
        makeLabel(extraLabels[7], "REV");
        makeLabel(extraLabels[8], "SCAN");
        makeLabel(extraLabels[9], "FREEZE");
        makeLabel(extraLabels[10], "ROOT");
        makeLabel(extraLabels[11], "BODY-SZ");
        makeLabel(extraLabels[12], "DECAY");
        makeLabel(extraLabels[13], "DAMP");
        makeLabel(extraLabels[14], "BRIGHT");
        makeLabel(extraLabels[15], "INHARM");
        makeLabel(extraLabels[16], "EXCITE");
        makeLabel(extraLabels[17], "B-MIX");
        makeLabel(extraLabels[18], "B-SPR");
        makeLabel(extraLabels[19], "MODES");
    }
    else if (oscId.startsWith("terrain"))
    {
        makeLabel(modeALabel, "PATH");
        makeLabel(modeBLabel, "QUAL");
        makeLabel(extraLabels[0], "X");
        makeLabel(extraLabels[1], "Y");
        makeLabel(extraLabels[2], "RAD-X");
        makeLabel(extraLabels[3], "RAD-Y");
        makeLabel(extraLabels[4], "ZOOM");
        makeLabel(extraLabels[5], "ROT");
        makeLabel(extraLabels[6], "SIZE");
        makeLabel(extraLabels[7], "RAT-X");
        makeLabel(extraLabels[8], "RAT-Y");
        makeLabel(extraLabels[9], "SPEED");
        makeLabel(extraLabels[10], "PHASE");
        makeLabel(extraLabels[11], "SPIRAL");
        makeLabel(extraLabels[12], "DRIVE");
        makeLabel(extraLabels[13], "OFFSET");
        makeLabel(extraLabels[14], "SMOOTH");
        makeLabel(extraLabels[15], "MORPH");
        makeLabel(extraLabels[16], "GAIN");
    }
    else if (oscId.startsWith("va"))
    {
        makeLabel(modeALabel, "MODEL");
        makeLabel(extraLabels[0], "SHAPE");
        makeLabel(extraLabels[1], "PW");
        makeLabel(extraLabels[2], "DETUNE");
        makeLabel(extraLabels[3], "UNISON");
        makeLabel(extraLabels[4], "SPREAD");
        makeLabel(extraLabels[5], "RING");
        makeLabel(extraLabels[6], "FOLD");
    }
    else if (oscId.startsWith("wt"))
    {
        makeLabel(modeALabel, "SHAPE");
        makeLabel(modeBLabel, "WARP");
        makeLabel(extraLabels[0], "POS");
        makeLabel(extraLabels[1], "WARP");
        makeLabel(extraLabels[2], "PD-BRK");
        makeLabel(extraLabels[3], "UNI");
        makeLabel(extraLabels[4], "DETUNE");
        makeLabel(extraLabels[5], "SPREAD");
        makeLabel(extraLabels[6], "P-SPR");
        makeLabel(extraLabels[7], "PHASE");
        makeLabel(extraLabels[8], "RAND");
        makeLabel(retrigLabel, "RETRIG");
    }

    const char* adsrNames[4] = { "A", "D", "S", "R" };
    for (int i = 0; i < 4; ++i)
    {
        adsrLabels[(size_t)i].setText(adsrNames[i], juce::dontSendNotification);
        IndustrialTheme::styleLabel(adsrLabels[(size_t)i], accent, 10.f);
        addAndMakeVisible(adsrLabels[(size_t)i]);
    }
}

void OscPanel::setupSlider(juce::Slider& s, const juce::String& tooltip)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 44, 14);
    s.setTooltip(tooltip);
    IndustrialTheme::styleSlider(s, accent);
    addAndMakeVisible(s);
}

void OscPanel::setupADSRSlider(juce::Slider& s, const juce::String& tooltip)
{
    s.setSliderStyle(juce::Slider::LinearVertical);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    s.setTooltip(tooltip);
    IndustrialTheme::styleSlider(s, accent);
    addAndMakeVisible(s);
}

void OscPanel::setupExtraSlider(juce::Slider& slider, const juce::String& paramId,
                                const juce::String& tooltip, std::unique_ptr<Att>& attachment)
{
    setupSlider(slider, tooltip);
    attachment = std::make_unique<Att>(apvts, paramId, slider);
}

void OscPanel::launchTerrainImportChooser()
{
    if (!terrainImport || terrainIndex < 0)
        return;

    auto chooser = std::make_shared<juce::FileChooser>(
        "Import Terrain Heightmap",
        juce::File{},
        "*.png;*.jpg;*.jpeg;*.bmp");

    terrainChooser = chooser;
    auto safeThis = juce::Component::SafePointer<OscPanel>(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                           | juce::FileBrowserComponent::canSelectFiles,
        [safeThis, chooser](const juce::FileChooser& fc)
        {
            if (safeThis == nullptr)
                return;

            auto file = fc.getResult();
            if (!file.existsAsFile())
                return;

            safeThis->importTerrainHeightmapFile(file);
        });
}

void OscPanel::importTerrainHeightmapFile(const juce::File& file)
{
    if (!terrainImport || terrainIndex < 0 || !file.existsAsFile())
        return;

    const bool ok = terrainImport(terrainIndex, file);
    importHeightmapButton.setButtonText(ok ? "MAP LOADED" : "IMPORT FAILED");

    if (ok && terrainDisplay != nullptr)
    {
        const auto image = juce::ImageFileFormat::loadFrom(file);
        if (image.isValid())
            terrainDisplay->setHeightmapImage(image);
    }
}

void OscPanel::refreshTerrainDisplay()
{
    if (terrainDisplay == nullptr)
        return;

    TerrainHeightmapDisplay::ScanState s;
    s.pathMode = modeBoxA.getSelectedItemIndex();
    s.centerX = (float)extra1.getValue();
    s.centerY = (float)extra2.getValue();
    s.radiusX = (float)extra3.getValue();
    s.radiusY = (float)extra4.getValue();
    s.zoom = (float)extra5.getValue();
    s.rotation = (float)extra6.getValue();
    s.pathSize = (float)extra7.getValue();
    s.ratioX = (float)extra8.getValue();
    s.ratioY = (float)extra9.getValue();
    s.scanRate = (float)extra10.getValue();
    s.phaseOffset = (float)extra11.getValue();
    s.spiral = (float)extra12.getValue();
    terrainDisplay->setScanState(s);
}

void OscPanel::launchGranularImportChooser()
{
    if (!granularImport || granularIndex < 0)
        return;

    const auto library = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
        .getChildFile("GLIESE_710").getChildFile("Samples");
    auto chooser = std::make_shared<juce::FileChooser>(
        "Load Granular Sample", library, "*.wav;*.aif;*.aiff;*.flac;*.ogg");
    granularChooser = chooser;
    auto safeThis = juce::Component::SafePointer<OscPanel>(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                           | juce::FileBrowserComponent::canSelectFiles,
        [safeThis, chooser](const juce::FileChooser& fc)
        {
            if (safeThis != nullptr && fc.getResult().existsAsFile())
                safeThis->importGranularSampleFile(fc.getResult());
        });
}

void OscPanel::refreshGranularSampleBrowser()
{
    if (!oscId.startsWith("grain"))
        return;

    const auto library = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
        .getChildFile("GLIESE_710").getChildFile("Samples");
    if (!library.exists())
        library.createDirectory();

    juce::Array<juce::File> found;
    library.findChildFiles(found, juce::File::findFiles, true,
                           "*.wav;*.aif;*.aiff;*.flac;*.ogg");
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b)
    {
        return a.getFileName().compareNatural(b.getFileName()) < 0;
    });

    sampleBrowserFiles.assign(found.begin(), found.end());
    sampleBrowserBox.clear(juce::dontSendNotification);
    for (int i = 0; i < (int)sampleBrowserFiles.size(); ++i)
    {
        auto relative = sampleBrowserFiles[(size_t)i].getRelativePathFrom(library);
        sampleBrowserBox.addItem(relative, i + 1);
    }
}

void OscPanel::saveGranularScene()
{
    const int scene = granularSceneBox.getSelectedItemIndex();
    if (scene < 0)
        return;

    const auto pfx = "osc_" + oscId + "_";
    const auto scenePfx = pfx + "scene" + juce::String(scene + 1) + "_";
    const juce::String suffixes[] = {
        "region_start", "region_end", "slice_count", "slice_index", "root_note" };

    for (int i = 0; i < 5; ++i)
    {
        auto* destination = apvts.getParameter(scenePfx + suffixes[i]);
        if (destination == nullptr)
            continue;
        destination->beginChangeGesture();
        destination->setValueNotifyingHost(
            destination->convertTo0to1((float)regionSliders[(size_t)i].getValue()));
        destination->endChangeGesture();
    }

    if (auto* destination = apvts.getParameter(scenePfx + "transient_slices"))
    {
        destination->beginChangeGesture();
        destination->setValueNotifyingHost(
            destination->convertTo0to1(transientSlicesButton.getToggleState() ? 1.f : 0.f));
        destination->endChangeGesture();
    }
    saveGranularSceneButton.setButtonText("SAVED");
}

void OscPanel::recallGranularScene()
{
    if (recallingGranularScene)
        return;
    const int scene = granularSceneBox.getSelectedItemIndex();
    if (scene < 0)
        return;

    const juce::ScopedValueSetter<bool> guard(recallingGranularScene, true);
    const auto pfx = "osc_" + oscId + "_";
    const auto scenePfx = pfx + "scene" + juce::String(scene + 1) + "_";
    const juce::String suffixes[] = {
        "region_start", "region_end", "slice_count", "slice_index", "root_note" };

    for (int i = 0; i < 5; ++i)
    {
        auto* source = apvts.getRawParameterValue(scenePfx + suffixes[i]);
        auto* destination = apvts.getParameter(pfx + suffixes[i]);
        if (source == nullptr || destination == nullptr)
            continue;
        destination->beginChangeGesture();
        destination->setValueNotifyingHost(
            destination->convertTo0to1(source->load()));
        destination->endChangeGesture();
    }

    auto* source = apvts.getRawParameterValue(scenePfx + "transient_slices");
    auto* destination = apvts.getParameter(pfx + "transient_slices");
    if (source != nullptr && destination != nullptr)
    {
        destination->beginChangeGesture();
        destination->setValueNotifyingHost(
            destination->convertTo0to1(source->load()));
        destination->endChangeGesture();
    }
    saveGranularSceneButton.setButtonText("SAVE");
}

void OscPanel::importGranularSampleFile(const juce::File& file)
{
    if (!granularImport || granularIndex < 0 || !file.existsAsFile()
        || !isSupportedGranularAudioFile(file))
        return;

    importSampleButton.setButtonText("LOADING...");
    auto safeThis = juce::Component::SafePointer<OscPanel>(this);
    sampleLoadPool.addJob(std::function<void()>([safeThis, file]
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        std::shared_ptr<const GranularOsc::SampleData> data;

        if (reader != nullptr && reader->lengthInSamples > 0)
        {
            const auto maxSamples = (juce::int64) juce::jmax(1.0, reader->sampleRate * 120.0);
            const int samples = (int) juce::jmin(reader->lengthInSamples, maxSamples);
            const int channels = juce::jlimit(1, 2, (int)reader->numChannels);
            juce::AudioBuffer<float> decoded(channels, samples);
            if (reader->read(&decoded, 0, samples, 0, true, channels > 1))
            {
                std::vector<float> left((size_t)samples);
                std::vector<float> right((size_t)samples);
                for (int i = 0; i < samples; ++i)
                {
                    left[(size_t)i] = decoded.getSample(0, i);
                    right[(size_t)i] = channels > 1
                        ? decoded.getSample(1, i) : left[(size_t)i];
                }
                data = GranularOsc::createSampleData(
                    left.data(), right.data(), samples);
            }
        }

        juce::MessageManager::callAsync([safeThis, data, name = file.getFileNameWithoutExtension()]
        {
            if (safeThis == nullptr)
                return;
            if (data != nullptr)
            {
                safeThis->granularImport(safeThis->granularIndex, data);
                safeThis->granularWaveform->setSample(data, name);
                safeThis->importSampleButton.setButtonText("SAMPLE LOADED");
            }
            else
            {
                safeThis->importSampleButton.setButtonText("LOAD FAILED");
            }
        });
    }));
}

void OscPanel::paint(juce::Graphics& g)
{
    IndustrialTheme::paintPanel(g, getLocalBounds(), accent, oscId.startsWith("3d"));
}

void OscPanel::paintOverChildren(juce::Graphics& g)
{
    if (!terrainDropActive)
        return;

    auto* target = granularWaveform != nullptr
        ? static_cast<juce::Component*>(granularWaveform.get())
        : static_cast<juce::Component*>(terrainDisplay.get());
    if (target == nullptr)
        return;
    const auto b = target->getBounds().toFloat().expanded(3.f);
    g.setColour(accent.withAlpha(0.16f));
    g.fillRoundedRectangle(b, 5.f);
    g.setColour(accent.brighter(0.45f).withAlpha(0.92f));
    g.drawRoundedRectangle(b, 5.f, 1.5f);
}

bool OscPanel::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        const juce::File file(path);
        if (hasGranularImportButton && granularImport && granularIndex >= 0
            && isSupportedGranularAudioFile(file))
            return true;
        if (hasTerrainImportButton && terrainImport && terrainIndex >= 0
            && isSupportedTerrainHeightmapFile(file))
            return true;
    }

    return false;
}

void OscPanel::fileDragEnter(const juce::StringArray& files, int, int)
{
    const bool shouldHighlight = isInterestedInFileDrag(files);
    if (terrainDropActive == shouldHighlight)
        return;

    terrainDropActive = shouldHighlight;
    repaint();
}

void OscPanel::fileDragExit(const juce::StringArray&)
{
    if (!terrainDropActive)
        return;

    terrainDropActive = false;
    repaint();
}

void OscPanel::filesDropped(const juce::StringArray& files, int, int)
{
    terrainDropActive = false;
    repaint();

    if (!isInterestedInFileDrag(files))
        return;

    for (const auto& path : files)
    {
        const juce::File file(path);
        if (hasGranularImportButton && isSupportedGranularAudioFile(file)
            && file.existsAsFile())
        {
            importGranularSampleFile(file);
            return;
        }
        if (isSupportedTerrainHeightmapFile(file) && file.existsAsFile())
        {
            importTerrainHeightmapFile(file);
            return;
        }
    }
}

void OscPanel::resized()
{
    // Evenly distributes rect 'r' into n equal slices, returning the i-th
    auto portion = [](juce::Rectangle<int> r, int i, int n) {
        int x  = r.getX() + r.getWidth() * i / n;
        int x2 = r.getX() + r.getWidth() * (i + 1) / n;
        return r.withX(x).withWidth(x2 - x);
    };

    auto area = getLocalBounds().reduced(6);
    {
        auto titleRow = area.removeFromTop(20);
        enableButton.setBounds(titleRow.removeFromRight(44));
        titleLabel  .setBounds(titleRow);
    }
    area.removeFromTop(2);

    // ── Level + pitch controls ────────────────────────────────────────────────
    auto lr0 = area.removeFromTop(13);
    const int topKnobH = juce::jlimit(48, 60, getHeight() / 7);
    auto sr0 = area.removeFromTop(topKnobH);
    levelLabel  .setBounds(portion(lr0, 0, 3));
    tuneLabel   .setBounds(portion(lr0, 1, 3));
    octaveLabel .setBounds(portion(lr0, 2, 3));
    levelSlider .setBounds(portion(sr0, 0, 3));
    tuneSlider  .setBounds(portion(sr0, 1, 3));
    octaveSlider.setBounds(portion(sr0, 2, 3));
    area.removeFromTop(4);

    // ── VA Waveform display ───────────────────────────────────────────────────
    if (waveDisplay != nullptr)
    {
        waveDisplay->setBounds(area.removeFromTop(52));
        area.removeFromTop(2);
    }

    // ── Mode Combo Boxes ──────────────────────────────────────────────────────
    if (numModeBoxes > 0)
    {
        int n = numModeBoxes;
        auto lr = area.removeFromTop(13);
        auto mr = area.removeFromTop(24);
        modeALabel.setBounds(portion(lr, 0, n));
        modeBoxA  .setBounds(portion(mr, 0, n).reduced(1, 0));
        if (n > 1)
        {
            modeBLabel.setBounds(portion(lr, 1, n));
            modeBoxB  .setBounds(portion(mr, 1, n).reduced(1, 0));
        }
        if (n > 2)
        {
            modeCLabel.setBounds(portion(lr, 2, n));
            modeBoxC  .setBounds(portion(mr, 2, n).reduced(1, 0));
        }
        area.removeFromTop(4);
    }

    if (hasRetrigButton)
    {
        auto rr = area.removeFromTop(22);
        retrigLabel.setBounds(rr.removeFromLeft(58));
        retrigButton.setBounds(rr.removeFromLeft(50));
        area.removeFromTop(4);
    }

    if (field3dDisplay != nullptr)
    {
        const int previewH = juce::jlimit(68, 104, area.getHeight() / 3);
        field3dDisplay->setBounds(area.removeFromTop(previewH).reduced(2));
        area.removeFromTop(4);
    }

    if (hasGranularImportButton)
    {
        auto actions = area.removeFromTop(22);
        auto loadActions = portion(actions, 0, 2);
        importSampleButton.setBounds(portion(loadActions, 0, 2).reduced(1, 0));
        captureSampleButton.setBounds(portion(loadActions, 1, 2).reduced(1, 0));
        auto sceneActions = portion(actions, 1, 2);
        granularSceneBox.setBounds(sceneActions.removeFromLeft(
            juce::roundToInt((float)sceneActions.getWidth() * 0.64f)).reduced(1, 0));
        saveGranularSceneButton.setBounds(sceneActions.reduced(1, 0));
        area.removeFromTop(2);

        auto browser = area.removeFromTop(22);
        rescanSamplesButton.setBounds(browser.removeFromRight(44).reduced(1, 0));
        densityDivisionBox.setBounds(browser.removeFromRight(68).reduced(1, 0));
        densitySyncButton.setBounds(browser.removeFromRight(54).reduced(1, 1));
        sampleBrowserBox.setBounds(browser.reduced(2, 0));
        area.removeFromTop(2);

        auto pitchCloud = area.removeFromTop(22);
        pitchQuantizeButton.setBounds(pitchCloud.removeFromLeft(48).reduced(1, 1));
        pitchScaleRootBox.setBounds(pitchCloud.removeFromLeft(48).reduced(1, 0));
        pitchScaleBox.setBounds(pitchCloud.removeFromLeft(
            juce::jmax(64, pitchCloud.getWidth() / 2)).reduced(1, 0));
        pitchCloudBox.setBounds(pitchCloud.reduced(1, 0));
        area.removeFromTop(2);
    }

    if (granularWaveform != nullptr)
    {
        const int previewH = juce::jlimit(54, 78, area.getHeight() / 5);
        granularWaveform->setBounds(area.removeFromTop(previewH).reduced(2));
        area.removeFromTop(4);
    }

    if (granularWaveform != nullptr)
    {
        auto labels = area.removeFromTop(11);
        auto controls = area.removeFromTop(44);
        for (int i = 0; i < 5; ++i)
        {
            regionLabels[(size_t)i].setBounds(portion(labels, i, 6));
            regionSliders[(size_t)i].setBounds(portion(controls, i, 6));
        }
        transientSlicesButton.setBounds(portion(controls, 5, 6).reduced(2, 7));
        area.removeFromTop(3);
    }

    if (hasTerrainImportButton)
    {
        importHeightmapButton.setBounds(area.removeFromTop(24).reduced(2, 0));
        area.removeFromTop(4);
    }

    if (terrainDisplay != nullptr)
    {
        const int previewH = juce::jlimit(84, 142, area.getHeight() / 3);
        terrainDisplay->setBounds(area.removeFromTop(previewH).reduced(2));
        area.removeFromTop(4);
        refreshTerrainDisplay();
    }

    // ── Extra Sliders ─────────────────────────────────────────────────────────
    if (numExtraSliders > 0)
    {
        std::array<juce::Slider*, 20> sliders {
            &extra1, &extra2, &extra3, &extra4, &extra5,
            &extra6, &extra7, &extra8, &extra9, &extra10,
            &extra11, &extra12, &extra13, &extra14, &extra15,
            &extra16, &extra17, &extra18, &extra19, &extra20
        };

        const int cols = oscId.startsWith("grain") || oscId.startsWith("terrain") ? 5
                       : numExtraSliders >= 7 ? 4
                       : 3;
        const int rows = (numExtraSliders + cols - 1) / cols;
        const int reservedForAdsr = juce::jlimit(76, 118, getHeight() / 4);
        const int available = juce::jmax(0, area.getHeight() - reservedForAdsr);
        const int rowLabelH = 11;
        const int rowGap = 1;
        const int rowSliderH = juce::jlimit(26, 52,
            (available - rows * (rowLabelH + rowGap)) / juce::jmax(1, rows));

        for (int row = 0; row < rows; ++row)
        {
            if (row > 0) area.removeFromTop(rowGap);
            const int start = row * cols;
            const int n = juce::jmin(cols, numExtraSliders - start);
            auto lr = area.removeFromTop(rowLabelH);
            auto sr = area.removeFromTop(rowSliderH);
            for (int i = 0; i < n; ++i)
            {
                const int index = start + i;
                extraLabels[(size_t)index].setBounds(portion(lr, i, n));
                sliders[(size_t)index]->setBounds(portion(sr, i, n));
            }
        }
        area.removeFromTop(4);
    }

    // ── ADSR — vertical sliders filling remaining space ───────────────────────
    juce::Slider* adsrSliders[4] = { &adsrA, &adsrD, &adsrS, &adsrR };
    for (int i = 0; i < 4; ++i)
    {
        auto col = portion(area, i, 4).reduced(2, 0);
        adsrLabels[(size_t)i].setBounds(col.removeFromTop(13));
        adsrSliders[i]->setBounds(col);
    }
}
