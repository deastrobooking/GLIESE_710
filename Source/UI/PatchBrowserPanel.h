#pragma once
#include <JuceHeader.h>
#include "../Presets/PresetManager.h"

class PatchBrowserPanel : public juce::Component,
                          private juce::TableListBoxModel,
                          private juce::TextEditor::Listener
{
public:
    explicit PatchBrowserPanel(PresetManager& pm);

    // Show/hide the overlay; refreshes the patch list on show
    void openBrowser();
    void closeBrowser();

    // Called after a preset is loaded so the header can update
    std::function<void()> onPresetLoaded;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PresetManager& pm;

    // ── Left sidebar ──────────────────────────────────────────────────────────
    juce::Label catHeaderLabel;
    static constexpr int kNumCats = 8;   // All + 7 categories
    std::array<juce::TextButton, kNumCats> catButtons;

    juce::Label      bankHeaderLabel;
    juce::ListBox    bankListBox;
    juce::TextButton newBankButton   { "NEW BANK" };
    juce::TextButton renameBankButton{ "RENAME" };
    juce::TextButton deleteBankButton{ "DEL BANK" };

    // ── Center patch table ────────────────────────────────────────────────────
    juce::TextEditor searchEditor;
    juce::Label      searchLabel;
    juce::TableListBox patchTable;
    juce::Label      patchCountLabel;

    // ── Action bar ────────────────────────────────────────────────────────────
    juce::TextButton loadButton        { "LOAD" };
    juce::TextButton saveButton        { "SAVE AS" };
    juce::TextButton deleteButton      { "DELETE" };
    juce::TextButton importPatchButton { "IMPORT PATCH" };
    juce::TextButton exportPatchButton { "EXPORT PATCH" };
    juce::TextButton importBankButton  { "IMPORT BANK" };
    juce::TextButton exportBankButton  { "EXPORT BANK" };
    juce::TextButton closeButton       { "  ✕  CLOSE" };

    // ── State ─────────────────────────────────────────────────────────────────
    PresetManager::Category  filterCategory  = PresetManager::Category::All;
    juce::String             filterBank;      // empty = all banks
    juce::String             searchText;

    juce::Array<PresetManager::PatchInfo>  allPatches;
    juce::Array<PresetManager::PatchInfo>  filteredPatches;
    juce::StringArray                      bankNames;
    int selectedRow = -1;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void refreshAll();
    void applyFilter();
    void updateActionStates();

    void doLoad();
    void doSave();
    void doDelete();
    void doImportPatch();
    void doExportPatch();
    void doImportBank();
    void doExportBank();
    void doNewBank();
    void doRenameBank();
    void doDeleteBank();

    void setCategoryFilter(PresetManager::Category cat);
    void setBankFilter(const juce::String& bankName);

    juce::String selectedBankName() const;

    // ── TableListBoxModel ─────────────────────────────────────────────────────
    int  getNumRows() override;
    void paintRowBackground(juce::Graphics&, int row, int w, int h, bool selected) override;
    void paintCell(juce::Graphics&, int row, int col, int w, int h, bool selected) override;
    void selectedRowsChanged(int lastRow) override;
    void cellDoubleClicked(int row, int, const juce::MouseEvent&) override;

    // ── TextEditor::Listener ──────────────────────────────────────────────────
    void textEditorTextChanged(juce::TextEditor&) override;

    // ── Bank ListBoxModel ─────────────────────────────────────────────────────
    struct BankModel : public juce::ListBoxModel
    {
        PatchBrowserPanel* owner = nullptr;
        int getNumRows()                 override;
        void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool sel) override;
        void listBoxItemClicked(int row, const juce::MouseEvent&)               override;
    } bankModel;
};
