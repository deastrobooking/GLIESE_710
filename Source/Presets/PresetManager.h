#pragma once
#include <JuceHeader.h>
#include <functional>

class PresetManager
{
public:
    static constexpr const char* kPresetExtension = ".g710";
    static constexpr const char* kBankExtension   = ".g710bank";
    static constexpr const char* kPresetDirName   = "GLIESE_710";

    enum class Category
    {
        All = 0,
        Bass, Arp, Pad, Keyboard, Percussion, Lead, Other,
        NumCategories
    };
    static juce::String       categoryToString(Category c);
    static Category           categoryFromString(const juce::String& s);
    static juce::StringArray  allCategoryNames();   // excluding "All"

    struct PatchInfo
    {
        juce::String name;
        juce::String bankName;   // empty = root (no bank)
        Category     category  = Category::Other;
        juce::String author;
        juce::File   file;
    };

    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts);
    void setStateLoadedCallback(std::function<void()> callback)
    {
        stateLoadedCallback = std::move(callback);
    }

    // ── Current patch ─────────────────────────────────────────────────────────
    const juce::String& getCurrentPresetName() const { return currentPresetName; }
    Category            getCurrentCategory()   const { return currentCategory; }

    // ── Save / Load ───────────────────────────────────────────────────────────
    void savePreset(const juce::String& name,
                    Category cat         = Category::Other,
                    const juce::String&  bankName = {});
    bool loadPreset(const juce::File& file);
    bool loadPreset(const PatchInfo& info);

    // ── Browse ────────────────────────────────────────────────────────────────
    juce::Array<PatchInfo> getAllPatches()                                  const;
    juce::Array<PatchInfo> getPatchesByCategory(Category cat)              const;
    juce::Array<PatchInfo> getPatchesInBank(const juce::String& bankName)  const;
    juce::StringArray      getBankNames()                                   const;
    juce::File             getPresetsDirectory()                            const;

    // ── Delete ────────────────────────────────────────────────────────────────
    void deletePreset(const PatchInfo& info);

    // ── Bank management ───────────────────────────────────────────────────────
    bool createBank(const juce::String& bankName);
    bool renameBank(const juce::String& oldName, const juce::String& newName);
    void deleteBank(const juce::String& bankName);

    // ── Import / Export ───────────────────────────────────────────────────────
    bool importPatch(const juce::File& srcFile,
                     const juce::String& bankName = {});
    void exportPatch(const PatchInfo& info, const juce::File& destFile);
    bool importBank(const juce::File& bankFile);
    void exportBank(const juce::String& bankName, const juce::File& destFile);

    // ── Legacy (used by existing PresetBrowser header) ────────────────────────
    juce::StringArray getUserPresetNames() const;
    bool loadPresetByName(const juce::String& name);

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String currentPresetName { "Init" };
    Category     currentCategory   { Category::Other };
    juce::File   presetsDir;
    std::function<void()> stateLoadedCallback;

    void ensureDirectoryExists(const juce::File& dir);
    static bool isSafeName(const juce::String& name);
    bool isInsidePresetsDirectory(const juce::File& file) const;

    // Wrap APVTS state in our metadata envelope
    juce::XmlElement* buildPresetXml(const juce::String& name, Category cat) const;
    // Parse a .g710 file — handles both wrapped (v2) and raw (v1) formats
    bool parsePresetFile(const juce::File& file,
                         juce::String& nameOut, Category& catOut, juce::String& authorOut,
                         juce::ValueTree& stateOut) const;

    juce::File bankDirectory(const juce::String& bankName) const;
    juce::File presetFile(const juce::String& name,
                          const juce::String& bankName) const;

    juce::Array<PatchInfo> scanDirectory(const juce::File& dir,
                                         const juce::String& bankName) const;
};
