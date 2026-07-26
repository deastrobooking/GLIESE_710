#include "PresetManager.h"
#include <BinaryData.h>

//──────────────────────────────────────────────────────────────────────────────
// Category helpers
//──────────────────────────────────────────────────────────────────────────────

juce::String PresetManager::categoryToString(Category c)
{
    switch (c)
    {
        case Category::All:        return "All";
        case Category::Bass:       return "Bass";
        case Category::Arp:        return "Arp";
        case Category::Pad:        return "Pad";
        case Category::Keyboard:   return "Keyboard";
        case Category::Percussion: return "Percussion";
        case Category::Lead:       return "Lead";
        case Category::Other:      return "Other";
        case Category::NumCategories:
            break;
    }

    return "Other";
}

PresetManager::Category PresetManager::categoryFromString(const juce::String& s)
{
    if (s.equalsIgnoreCase("Bass"))       return Category::Bass;
    if (s.equalsIgnoreCase("Arp"))        return Category::Arp;
    if (s.equalsIgnoreCase("Pad"))        return Category::Pad;
    if (s.equalsIgnoreCase("Keyboard"))   return Category::Keyboard;
    if (s.equalsIgnoreCase("Percussion")) return Category::Percussion;
    if (s.equalsIgnoreCase("Lead"))       return Category::Lead;
    return Category::Other;
}

juce::StringArray PresetManager::allCategoryNames()
{
    return { "Bass","Arp","Pad","Keyboard","Percussion","Lead","Other" };
}

//──────────────────────────────────────────────────────────────────────────────
// Construction
//──────────────────────────────────────────────────────────────────────────────

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    factoryBaselineState = apvts.copyState();
    presetsDir = getPresetsDirectory();
    ensureDirectoryExists(presetsDir);
}

void PresetManager::ensureDirectoryExists(const juce::File& dir)
{
    if (!dir.exists())
        dir.createDirectory();
}

bool PresetManager::isSafeName(const juce::String& name)
{
    const auto trimmed = name.trim();
    return trimmed.isNotEmpty()
        && trimmed != "."
        && trimmed != ".."
        && !trimmed.containsAnyOf("/\\:")
        && juce::File::createLegalFileName(trimmed) == trimmed;
}

bool PresetManager::isInsidePresetsDirectory(const juce::File& file) const
{
    return file == presetsDir || file.isAChildOf(presetsDir);
}

juce::File PresetManager::getPresetsDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userMusicDirectory)
               .getChildFile(kPresetDirName)
               .getChildFile("Presets");
}

juce::File PresetManager::bankDirectory(const juce::String& bankName) const
{
    return presetsDir.getChildFile(bankName);
}

juce::File PresetManager::presetFile(const juce::String& name,
                                      const juce::String& bankName) const
{
    if (bankName.isEmpty())
        return presetsDir.getChildFile(name + kPresetExtension);
    return bankDirectory(bankName).getChildFile(name + kPresetExtension);
}

//──────────────────────────────────────────────────────────────────────────────
// XML helpers
//──────────────────────────────────────────────────────────────────────────────

juce::XmlElement* PresetManager::buildPresetXml(const juce::String& name, Category cat) const
{
    auto* root = new juce::XmlElement("GLIESE710Preset");
    root->setAttribute("name",     name);
    root->setAttribute("category", categoryToString(cat));
    root->setAttribute("version",  2);

    auto state = apvts.copyState();
    if (auto* stateXml = state.createXml().release())
    {
        root->addChildElement(stateXml);
    }
    return root;
}

bool PresetManager::parsePresetFile(const juce::File& file,
                                     juce::String& nameOut, Category& catOut,
                                     juce::String& authorOut,
                                     juce::ValueTree& stateOut) const
{
    auto xml = juce::XmlDocument::parse(file);
    if (!xml) return false;

    if (xml->getTagName() == "GLIESE710Preset")
    {
        // v2 wrapped format
        nameOut   = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
        catOut    = categoryFromString(xml->getStringAttribute("category", "Other"));
        authorOut = xml->getStringAttribute("author", "");
        if (auto* stateEl = xml->getFirstChildElement())
        {
            stateOut = juce::ValueTree::fromXml(*stateEl);
            return stateOut.isValid();
        }
        return false;
    }

    // v1 — raw APVTS XML
    nameOut   = file.getFileNameWithoutExtension();
    catOut    = Category::Other;
    authorOut = "";
    stateOut  = juce::ValueTree::fromXml(*xml);
    return stateOut.isValid();
}

bool PresetManager::parsePresetData(const void* data, int size,
                                    const juce::String& fallbackName,
                                    juce::String& nameOut, Category& catOut,
                                    juce::String& authorOut,
                                    juce::ValueTree& stateOut) const
{
    if (data == nullptr || size <= 0)
        return false;

    auto xml = juce::XmlDocument::parse(
        juce::String::fromUTF8(static_cast<const char*>(data), size));
    if (!xml)
        return false;

    if (xml->getTagName() == "GLIESE710Preset")
    {
        nameOut = xml->getStringAttribute("name", fallbackName);
        catOut = categoryFromString(xml->getStringAttribute("category", "Other"));
        authorOut = xml->getStringAttribute("author", "");
        if (auto* stateEl = xml->getFirstChildElement())
        {
            stateOut = juce::ValueTree::fromXml(*stateEl);
            return stateOut.isValid();
        }
        return false;
    }

    nameOut = fallbackName;
    catOut = Category::Other;
    authorOut = {};
    stateOut = juce::ValueTree::fromXml(*xml);
    return stateOut.isValid();
}

//──────────────────────────────────────────────────────────────────────────────
// Save / Load
//──────────────────────────────────────────────────────────────────────────────

void PresetManager::savePreset(const juce::String& name, Category cat,
                                const juce::String& bankName)
{
    if (!isSafeName(name) || (bankName.isNotEmpty() && !isSafeName(bankName)))
        return;

    if (bankName.isNotEmpty())
        ensureDirectoryExists(bankDirectory(bankName));

    auto file = presetFile(name, bankName);
    std::unique_ptr<juce::XmlElement> xml(buildPresetXml(name, cat));
    xml->writeTo(file, juce::XmlElement::TextFormat{});

    currentPresetName = name;
    currentCategory   = cat;
}

bool PresetManager::loadPreset(const juce::File& file)
{
    juce::String name, author;
    Category cat;
    juce::ValueTree state;
    if (!parsePresetFile(file, name, cat, author, state)) return false;
    apvts.replaceState(state);
    if (stateLoadedCallback)
        stateLoadedCallback();
    currentPresetName = name;
    currentCategory   = cat;
    return true;
}

bool PresetManager::loadPreset(const PatchInfo& info)
{
    if (info.isFactory)
        return loadFactoryPreset(info);
    return loadPreset(info.file);
}

juce::ValueTree PresetManager::mergeFactoryState(const juce::ValueTree& overlay) const
{
    auto merged = factoryBaselineState.createCopy();
    if (!merged.isValid())
        return overlay.createCopy();

    for (int i = 0; i < overlay.getNumChildren(); ++i)
    {
        const auto source = overlay.getChild(i);
        auto target = merged.getChildWithProperty("id", source.getProperty("id"));
        if (target.isValid())
            target.copyPropertiesFrom(source, nullptr);
    }
    return merged;
}

bool PresetManager::loadFactoryPreset(const PatchInfo& info)
{
    int size = 0;
    const auto* data = FactoryPresetData::getNamedResource(
        info.resourceName.toRawUTF8(), size);

    juce::String name, author;
    Category cat = Category::Other;
    juce::ValueTree overlay;
    if (!parsePresetData(data, size, info.name, name, cat, author, overlay))
        return false;

    apvts.replaceState(mergeFactoryState(overlay));
    if (stateLoadedCallback)
        stateLoadedCallback();
    currentPresetName = name;
    currentCategory = cat;
    return true;
}

bool PresetManager::loadPresetByName(const juce::String& name)
{
    if (!isSafeName(name))
        return false;
    return loadPreset(presetsDir.getChildFile(name + kPresetExtension));
}

//──────────────────────────────────────────────────────────────────────────────
// Browse
//──────────────────────────────────────────────────────────────────────────────

juce::Array<PresetManager::PatchInfo>
PresetManager::scanDirectory(const juce::File& dir, const juce::String& bankName) const
{
    juce::Array<PatchInfo> result;
    for (auto& f : dir.findChildFiles(juce::File::findFiles, false,
                                       "*" + juce::String(kPresetExtension)))
    {
        juce::String name, author;
        Category cat = Category::Other;
        juce::ValueTree dummy;
        if (!parsePresetFile(f, name, cat, author, dummy))
            continue;

        PatchInfo info;
        info.name     = name;
        info.bankName = bankName;
        info.category = cat;
        info.author   = author;
        info.file     = f;
        result.add(info);
    }
    struct Cmp {
        static int compareElements(const PatchInfo& a, const PatchInfo& b)
        { return a.name.compareIgnoreCase(b.name); }
    } cmp;
    result.sort(cmp, false);
    return result;
}

juce::Array<PresetManager::PatchInfo> PresetManager::getAllPatches() const
{
    auto all = getFactoryPatches();
    all.addArray(scanDirectory(presetsDir, {}));
    for (const auto& bankName : getBankNames())
        if (bankName != "Factory")
            all.addArray(scanDirectory(bankDirectory(bankName), bankName));
    return all;
}

juce::Array<PresetManager::PatchInfo> PresetManager::getFactoryPatches() const
{
    juce::Array<PatchInfo> result;
    for (int i = 0; i < FactoryPresetData::namedResourceListSize; ++i)
    {
        const juce::String resourceName = FactoryPresetData::namedResourceList[i];
        int size = 0;
        const auto* data = FactoryPresetData::getNamedResource(
            resourceName.toRawUTF8(), size);

        juce::String name, author;
        Category cat = Category::Other;
        juce::ValueTree state;
        if (!parsePresetData(data, size, resourceName, name, cat, author, state))
            continue;

        PatchInfo info;
        info.name = name;
        info.bankName = "Factory";
        info.category = cat;
        info.author = author;
        info.resourceName = resourceName;
        info.isFactory = true;
        result.add(info);
    }

    struct Cmp {
        static int compareElements(const PatchInfo& a, const PatchInfo& b)
        { return a.name.compareIgnoreCase(b.name); }
    } cmp;
    result.sort(cmp, false);
    return result;
}

juce::Array<PresetManager::PatchInfo>
PresetManager::getPatchesByCategory(Category cat) const
{
    juce::Array<PatchInfo> result;
    for (const auto& p : getAllPatches())
        if (cat == Category::All || p.category == cat)
            result.add(p);
    return result;
}

juce::Array<PresetManager::PatchInfo>
PresetManager::getPatchesInBank(const juce::String& bankName) const
{
    if (bankName.isEmpty() || bankName == "Root")
        return scanDirectory(presetsDir, {});
    if (bankName == "Factory")
        return getFactoryPatches();
    if (!isSafeName(bankName))
        return {};
    return scanDirectory(bankDirectory(bankName), bankName);
}

juce::StringArray PresetManager::getBankNames() const
{
    juce::StringArray banks { "Factory" };
    for (auto& dir : presetsDir.findChildFiles(juce::File::findDirectories, false))
    {
        if (!dir.getFileName().startsWith("_"))
            banks.add(dir.getFileName());
    }
    banks.sort(false);
    return banks;
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;
    for (const auto& p : scanDirectory(presetsDir, {}))
        names.add(p.name);
    return names;
}

//──────────────────────────────────────────────────────────────────────────────
// Delete
//──────────────────────────────────────────────────────────────────────────────

void PresetManager::deletePreset(const PatchInfo& info)
{
    if (info.isFactory)
        return;
    if (isInsidePresetsDirectory(info.file) && info.file.existsAsFile())
        info.file.deleteFile();
}

//──────────────────────────────────────────────────────────────────────────────
// Bank management
//──────────────────────────────────────────────────────────────────────────────

bool PresetManager::createBank(const juce::String& bankName)
{
    if (!isSafeName(bankName)) return false;
    auto dir = bankDirectory(bankName);
    if (dir.exists()) return false;
    return dir.createDirectory();
}

bool PresetManager::renameBank(const juce::String& oldName, const juce::String& newName)
{
    if (!isSafeName(oldName) || !isSafeName(newName)) return false;
    auto src = bankDirectory(oldName);
    auto dst = bankDirectory(newName);
    if (!src.exists() || dst.exists()) return false;
    return src.moveFileTo(dst);
}

void PresetManager::deleteBank(const juce::String& bankName)
{
    if (!isSafeName(bankName))
        return;
    auto dir = bankDirectory(bankName);
    if (dir.isAChildOf(presetsDir) && dir != presetsDir)
        dir.deleteRecursively();
}

//──────────────────────────────────────────────────────────────────────────────
// Import / Export
//──────────────────────────────────────────────────────────────────────────────

bool PresetManager::importPatch(const juce::File& srcFile, const juce::String& bankName)
{
    if (!srcFile.existsAsFile()) return false;
    if (bankName.isNotEmpty() && !isSafeName(bankName)) return false;
    auto destDir = bankName.isNotEmpty() ? bankDirectory(bankName) : presetsDir;
    ensureDirectoryExists(destDir);
    auto dest = destDir.getChildFile(srcFile.getFileName());
    // avoid overwrite — append number if collision
    if (dest.exists())
    {
        int n = 1;
        while (dest.exists())
            dest = destDir.getChildFile(srcFile.getFileNameWithoutExtension()
                                         + "_" + juce::String(n++) + kPresetExtension);
    }
    return srcFile.copyFileTo(dest);
}

void PresetManager::exportPatch(const PatchInfo& info, const juce::File& destFile)
{
    if (info.isFactory)
    {
        int size = 0;
        if (const auto* data = FactoryPresetData::getNamedResource(
                info.resourceName.toRawUTF8(), size))
            destFile.replaceWithData(data, (size_t) size);
        return;
    }
    info.file.copyFileTo(destFile);
}

bool PresetManager::importBank(const juce::File& bankFile)
{
    auto xml = juce::XmlDocument::parse(bankFile);
    if (!xml || xml->getTagName() != "GLIESE710Bank") return false;

    juce::String bankName = xml->getStringAttribute("name",
                                bankFile.getFileNameWithoutExtension());
    if (!isSafeName(bankName))
        return false;
    auto dir = bankDirectory(bankName);
    ensureDirectoryExists(dir);

    bool anyOk = false;
    for (auto* patchEl : xml->getChildIterator())
    {
        if (patchEl->getTagName() != "Patch") continue;
        juce::String pName = patchEl->getStringAttribute("name", "Unnamed");
        juce::String pCat  = patchEl->getStringAttribute("category", "Other");
        if (!isSafeName(pName))
            continue;

        auto* root = new juce::XmlElement("GLIESE710Preset");
        root->setAttribute("name",    pName);
        root->setAttribute("category", pCat);
        root->setAttribute("version",  2);

        if (auto* stateEl = patchEl->getFirstChildElement())
            root->addChildElement(new juce::XmlElement(*stateEl));

        auto dest = dir.getChildFile(pName + kPresetExtension);
        std::unique_ptr<juce::XmlElement> owned(root);
        owned->writeTo(dest, juce::XmlElement::TextFormat{});
        anyOk = true;
    }
    return anyOk;
}

void PresetManager::exportBank(const juce::String& bankName, const juce::File& destFile)
{
    if (bankName.isNotEmpty() && !isSafeName(bankName))
        return;
    juce::XmlElement root("GLIESE710Bank");
    root.setAttribute("name", bankName.isEmpty() ? "Export" : bankName);
    root.setAttribute("version", 1);

    auto patches = bankName.isEmpty() ? scanDirectory(presetsDir, {})
                                      : scanDirectory(bankDirectory(bankName), bankName);
    for (const auto& info : patches)
    {
        juce::String name, author;
        Category cat;
        juce::ValueTree state;
        if (!parsePresetFile(info.file, name, cat, author, state)) continue;

        auto* patchEl = root.createNewChildElement("Patch");
        patchEl->setAttribute("name",     name);
        patchEl->setAttribute("category", categoryToString(cat));
        if (auto* stateXml = state.createXml().release())
            patchEl->addChildElement(stateXml);
    }
    root.writeTo(destFile, juce::XmlElement::TextFormat{});
}
