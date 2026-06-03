#include "OscPanel.h"
#include <cmath>

namespace
{
bool isSupportedTerrainHeightmapFile(const juce::File& file)
{
    const auto ext = file.getFileExtension().toLowerCase();
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp";
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
                   int terrainSlotIndex)
    : apvts(a),
      oscId(id),
      label(lbl),
      accent(IndustrialTheme::accentForOsc(id)),
      terrainImport(std::move(terrainImportCallback)),
      terrainIndex(terrainSlotIndex)
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

void OscPanel::paint(juce::Graphics& g)
{
    IndustrialTheme::paintPanel(g, getLocalBounds(), accent, oscId.startsWith("3d"));
}

void OscPanel::paintOverChildren(juce::Graphics& g)
{
    if (!terrainDropActive || terrainDisplay == nullptr)
        return;

    const auto b = terrainDisplay->getBounds().toFloat().expanded(3.f);
    g.setColour(accent.withAlpha(0.16f));
    g.fillRoundedRectangle(b, 5.f);
    g.setColour(accent.brighter(0.45f).withAlpha(0.92f));
    g.drawRoundedRectangle(b, 5.f, 1.5f);
}

bool OscPanel::isInterestedInFileDrag(const juce::StringArray& files)
{
    if (!hasTerrainImportButton || !terrainImport || terrainIndex < 0)
        return false;

    for (const auto& path : files)
        if (isSupportedTerrainHeightmapFile(juce::File(path)))
            return true;

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
