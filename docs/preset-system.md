# Preset System

## File Format

Presets use the `.g710` extension. Internally they are plain XML — human-readable and diff-friendly.

Example:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<State>
  <PARAM id="master_volume" value="0.8"/>
  <PARAM id="osc_3d_field" value="2"/>
  <PARAM id="osc_3d_radius" value="1.5"/>
  <PARAM id="filter_a_cutoff" value="1200.0"/>
  ...
</State>
```

All APVTS-registered parameters are included. Parameters added in future versions default to their APVTS default value when loading older presets — forward compatibility is automatic.

## Storage Locations

| Type | Location |
|---|---|
| User presets | `~/Music/GLIESE_710/Presets/*.g710` |
| Factory presets | Embedded in the plugin binary via `juce_add_binary_data` |
| DAW session state | Opaque blob managed by host (`getStateInformation`) |

Imported terrain heightmaps are encoded into custom properties on the APVTS state, capped to 512 pixels, and therefore travel with both `.g710` presets and DAW session blobs. Terrain DSP data is rebuilt on load and shared across voices.

## Factory Content

Six starter patches ship in the read-only **Factory** bank: Init, Alien Sequence, Glass Lattice, Nebula Pad, Orbital Bass, and Pulse Vector. The compact embedded files contain parameter overrides which are merged onto the synth's captured default state when loaded. This guarantees that loading one factory patch after another cannot inherit unspecified values from the previous patch.

Factory patches appear in both preset browsers, are marked with a diamond in the header selector, and can be exported as normal `.g710` files. Delete and factory-bank mutation actions are disabled.

## Migration Requirements

- Never reuse or reorder existing parameter IDs.
- New parameters must have musically safe defaults.
- Preset format changes require an explicit version and compatibility path.
- Add round-trip tests for APVTS state, embedded terrain data, corrupt files, and older fixture presets.

## API — PresetManager

```cpp
// Save current state to ~/Music/GLIESE_710/Presets/MyPreset.g710
bool saved = presetManager.savePreset("MyPreset");

// Load from file
bool ok = presetManager.loadPreset(file);

// Load by name (looks in user presets directory)
bool ok = presetManager.loadPresetByName("MyPreset");

// List all user preset names (sorted)
juce::StringArray names = presetManager.getUserPresetNames();

// Get current preset name (for display)
juce::String name = presetManager.getCurrentPresetName();
```

## DAW Session Recall

The host calls `getStateInformation` / `setStateInformation` on save/load. These bridge through the APVTS:

```cpp
void getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    auto xml   = state.createXml();
    copyXmlToBinary(*xml, dest);
}

void setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.isValid())
            apvts.replaceState(tree);
    }
}
```

This means presets and DAW session state use the same XML format. A `.g710` file can be opened in a text editor, modified, and loaded — there is no binary encoding to decode.

## Thread Safety

`savePreset` and `loadPreset` must be called from the **message thread** (UI thread), never from the audio thread. The APVTS `replaceState()` method is designed for message-thread use and propagates changes to the audio thread via its atomic parameter mechanism.

In the UI, always trigger preset operations from button callbacks or `ComboBox::onChange`, never from `processBlock`.

## Versioning

The APVTS parameter ID includes a version number (currently `1`):

```cpp
juce::ParameterID{"master_volume", 1}
```

If a parameter is renamed or type-changed in a future version, increment the version. JUCE preserves unknown parameters in the ValueTree, so old presets loaded into a new build will retain any parameters that still exist and ignore removed ones.

## Factory Presets

Factory presets are embedded at build time via `juce_add_binary_data`:

```cmake
juce_add_binary_data(GLIESE_710_Assets
    SOURCES Source/Presets/FactoryPresets/Init.g710
            Source/Presets/FactoryPresets/SphereOrb.g710
            ...
)
```

`PresetManager::getFactoryPatches()` discovers the generated resources, parses their metadata, and exposes them as read-only `PatchInfo` entries in the Factory bank. Loading merges the resource's parameter overrides into a pristine default APVTS state before replacing the active state.

## Creating Presets for Distribution

1. Dial in a patch in the standalone app
2. Click **Save** in the preset browser, give it a name
3. The file appears in `~/Music/GLIESE_710/Presets/`
4. Copy the `.g710` file to `Source/Presets/FactoryPresets/`
5. Add it to `juce_add_binary_data` in `CMakeLists.txt`
6. Rebuild — it ships embedded in the plugin binary

## Import and save integrity

`savePreset` returns false when the name is invalid or writing fails, and updates the current preset only after a successful write. Both save dialogs display failures.

Bank imports validate patch state roots before writing. Existing files are preserved by choosing a numbered filename for collisions. A corrupt bank is rejected before any patch is written; an I/O failure during import returns false, and patches already written during that import may remain.

Loading state without embedded granular layers resets layer A to the built-in source and layer B to layer A. Missing or invalid terrain maps restore the procedural terrain. This prevents imported assets from the previously loaded patch leaking into a new patch.
