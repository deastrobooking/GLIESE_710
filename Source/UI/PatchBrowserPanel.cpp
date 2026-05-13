#include "PatchBrowserPanel.h"
#include "IndustrialTheme.h"

//──────────────────────────────────────────────────────────────────────────────
// BankModel
//──────────────────────────────────────────────────────────────────────────────

int PatchBrowserPanel::BankModel::getNumRows()
{
    return owner->bankNames.size() + 1;   // row 0 = "ALL BANKS"
}

void PatchBrowserPanel::BankModel::paintListBoxItem(int row, juce::Graphics& g,
                                                     int w, int h, bool sel)
{
    auto accent = IndustrialTheme::accentMaster();
    if (sel)
        g.fillAll(accent.withAlpha(0.25f));
    g.setColour(sel ? accent : IndustrialTheme::text());
    g.setFont(juce::Font(juce::FontOptions(11.f)));
    juce::String label = (row == 0) ? "ALL BANKS" : owner->bankNames[row - 1];
    g.drawText(label, 8, 0, w - 8, h, juce::Justification::centredLeft);
}

void PatchBrowserPanel::BankModel::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    if (row == 0)
        owner->setBankFilter({});
    else if (row - 1 < owner->bankNames.size())
        owner->setBankFilter(owner->bankNames[row - 1]);
}

//──────────────────────────────────────────────────────────────────────────────
// TableListBoxModel
//──────────────────────────────────────────────────────────────────────────────

int PatchBrowserPanel::getNumRows() { return filteredPatches.size(); }

void PatchBrowserPanel::paintRowBackground(juce::Graphics& g, int row,
                                            int /*w*/, int /*h*/, bool selected)
{
    if (selected)
        g.fillAll(IndustrialTheme::accentMaster().withAlpha(0.22f));
    else
        g.fillAll((row % 2 == 0) ? juce::Colour(0xff131517) : juce::Colour(0xff0f1113));
}

void PatchBrowserPanel::paintCell(juce::Graphics& g, int row, int col,
                                   int w, int h, bool selected)
{
    if (row >= filteredPatches.size()) return;
    const auto& p = filteredPatches.getReference(row);
    g.setColour(selected ? IndustrialTheme::accentMaster() : IndustrialTheme::text());
    g.setFont(juce::Font(juce::FontOptions(11.f)));
    juce::String text;
    switch (col)
    {
        case 0: text = p.name;                                                              break;
        case 1: text = PresetManager::categoryToString(p.category);                         break;
        case 2: text = p.bankName.isEmpty() ? juce::String("Root") : p.bankName;           break;
        default: break;
    }
    g.drawText(text, 4, 0, w - 8, h, juce::Justification::centredLeft);
}

void PatchBrowserPanel::selectedRowsChanged(int lastRow)
{
    selectedRow = lastRow;
    updateActionStates();
}

void PatchBrowserPanel::cellDoubleClicked(int row, int, const juce::MouseEvent&)
{
    selectedRow = row;
    doLoad();
}

//──────────────────────────────────────────────────────────────────────────────
// TextEditor::Listener
//──────────────────────────────────────────────────────────────────────────────

void PatchBrowserPanel::textEditorTextChanged(juce::TextEditor& e)
{
    searchText = e.getText();
    applyFilter();
}

//──────────────────────────────────────────────────────────────────────────────
// Constructor
//──────────────────────────────────────────────────────────────────────────────

PatchBrowserPanel::PatchBrowserPanel(PresetManager& presetManager) : pm(presetManager)
{
    setVisible(false);

    auto accent = IndustrialTheme::accentMaster();

    // ── Category sidebar ──────────────────────────────────────────────────────
    catHeaderLabel.setText("CATEGORY", juce::dontSendNotification);
    IndustrialTheme::styleLabel(catHeaderLabel, accent, 10.f,
                                juce::Justification::centredLeft);
    addAndMakeVisible(catHeaderLabel);

    static const char* kCatNames[kNumCats] = {
        "ALL", "BASS", "ARP", "PAD", "KEYBOARD", "PERCUSSION", "LEAD", "OTHER"
    };
    for (int i = 0; i < kNumCats; ++i)
    {
        catButtons[(size_t)i].setButtonText(kCatNames[i]);
        catButtons[(size_t)i].setClickingTogglesState(true);
        catButtons[(size_t)i].setRadioGroupId(1001);
        IndustrialTheme::styleButton(catButtons[(size_t)i], accent);
        addAndMakeVisible(catButtons[(size_t)i]);
        auto cat = static_cast<PresetManager::Category>(i);
        catButtons[(size_t)i].onClick = [this, cat] { setCategoryFilter(cat); };
    }
    catButtons[0].setToggleState(true, juce::dontSendNotification);

    // ── Bank sidebar ──────────────────────────────────────────────────────────
    bankHeaderLabel.setText("BANK", juce::dontSendNotification);
    IndustrialTheme::styleLabel(bankHeaderLabel, accent, 10.f,
                                juce::Justification::centredLeft);
    addAndMakeVisible(bankHeaderLabel);

    bankModel.owner = this;
    bankListBox.setModel(&bankModel);
    bankListBox.setRowHeight(22);
    bankListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff080a0c));
    bankListBox.setOutlineThickness(1);
    addAndMakeVisible(bankListBox);

    for (auto* btn : { &newBankButton, &renameBankButton, &deleteBankButton })
        IndustrialTheme::styleButton(*btn, accent);
    newBankButton.onClick    = [this] { doNewBank(); };
    renameBankButton.onClick = [this] { doRenameBank(); };
    deleteBankButton.onClick = [this] { doDeleteBank(); };
    addAndMakeVisible(newBankButton);
    addAndMakeVisible(renameBankButton);
    addAndMakeVisible(deleteBankButton);

    // ── Search ────────────────────────────────────────────────────────────────
    searchLabel.setText("SEARCH", juce::dontSendNotification);
    IndustrialTheme::styleLabel(searchLabel, accent, 10.f,
                                juce::Justification::centredLeft);
    addAndMakeVisible(searchLabel);

    searchEditor.setColour(juce::TextEditor::backgroundColourId,     juce::Colour(0xff080a0c));
    searchEditor.setColour(juce::TextEditor::outlineColourId,        accent.withAlpha(0.35f));
    searchEditor.setColour(juce::TextEditor::focusedOutlineColourId, accent);
    searchEditor.setColour(juce::TextEditor::textColourId,           IndustrialTheme::text());
    searchEditor.setFont(juce::Font(juce::FontOptions(11.f)));
    searchEditor.addListener(this);
    addAndMakeVisible(searchEditor);

    // ── Patch table ───────────────────────────────────────────────────────────
    patchTable.setModel(this);
    patchTable.setRowHeight(22);
    auto& hdr = patchTable.getHeader();
    hdr.addColumn("NAME",     1, 200, 80,  -1, juce::TableHeaderComponent::defaultFlags);
    hdr.addColumn("CATEGORY", 2, 100, 60,  -1, juce::TableHeaderComponent::defaultFlags);
    hdr.addColumn("BANK",     3, 120, 60,  -1, juce::TableHeaderComponent::defaultFlags);
    hdr.setStretchToFitActive(true);
    patchTable.setColour(juce::TableListBox::backgroundColourId, juce::Colour(0xff080a0c));
    patchTable.setOutlineThickness(1);
    addAndMakeVisible(patchTable);

    patchCountLabel.setText("0 patches", juce::dontSendNotification);
    IndustrialTheme::styleLabel(patchCountLabel, IndustrialTheme::mutedText(), 10.f,
                                juce::Justification::centredLeft);
    addAndMakeVisible(patchCountLabel);

    // ── Action buttons ────────────────────────────────────────────────────────
    for (auto* btn : { &loadButton, &saveButton, &deleteButton,
                       &importPatchButton, &exportPatchButton,
                       &importBankButton,  &exportBankButton })
        IndustrialTheme::styleButton(*btn, accent);

    closeButton.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xff280000));
    closeButton.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff500000));
    closeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);

    loadButton.onClick        = [this] { doLoad(); };
    saveButton.onClick        = [this] { doSave(); };
    deleteButton.onClick      = [this] { doDelete(); };
    importPatchButton.onClick = [this] { doImportPatch(); };
    exportPatchButton.onClick = [this] { doExportPatch(); };
    importBankButton.onClick  = [this] { doImportBank(); };
    exportBankButton.onClick  = [this] { doExportBank(); };
    closeButton.onClick       = [this] { closeBrowser(); };

    addAndMakeVisible(loadButton);
    addAndMakeVisible(saveButton);
    addAndMakeVisible(deleteButton);
    addAndMakeVisible(importPatchButton);
    addAndMakeVisible(exportPatchButton);
    addAndMakeVisible(importBankButton);
    addAndMakeVisible(exportBankButton);
    addAndMakeVisible(closeButton);

    updateActionStates();
}

//──────────────────────────────────────────────────────────────────────────────
// Open / Close
//──────────────────────────────────────────────────────────────────────────────

void PatchBrowserPanel::openBrowser()
{
    refreshAll();
    setVisible(true);
    toFront(true);
}

void PatchBrowserPanel::closeBrowser()
{
    setVisible(false);
}

//──────────────────────────────────────────────────────────────────────────────
// Paint / Resized
//──────────────────────────────────────────────────────────────────────────────

void PatchBrowserPanel::paint(juce::Graphics& g)
{
    // Dark overlay
    g.fillAll(juce::Colour(0xd2050607));

    auto panel = getLocalBounds().reduced(20);
    g.setColour(juce::Colour(0xff0b0e10));
    g.fillRoundedRectangle(panel.toFloat(), 6.f);

    auto accent = IndustrialTheme::accentMaster();
    g.setColour(accent.withAlpha(0.55f));
    g.drawRoundedRectangle(panel.toFloat(), 6.f, 1.5f);

    // Title bar background
    auto titleRect = panel.removeFromTop(34).toFloat();
    g.setColour(juce::Colour(0xff0d1012));
    g.fillRect(titleRect);
    g.setColour(accent.withAlpha(0.4f));
    g.drawLine(titleRect.getX(), titleRect.getBottom(),
               titleRect.getRight(), titleRect.getBottom(), 1.f);

    g.setColour(accent);
    g.setFont(juce::Font(juce::FontOptions(14.f)));
    g.drawText("PATCH BROWSER", titleRect, juce::Justification::centred);
}

void PatchBrowserPanel::resized()
{
    auto panel = getLocalBounds().reduced(20);
    panel.removeFromTop(34);   // title bar consumed in paint
    panel.reduce(8, 6);

    // Action bar
    auto actionBar = panel.removeFromBottom(30);
    panel.removeFromBottom(6);

    // Left sidebar
    const int sideW = 172;
    auto sidebar = panel.removeFromLeft(sideW);
    panel.removeFromLeft(8);

    // Category
    catHeaderLabel.setBounds(sidebar.removeFromTop(14));
    sidebar.removeFromTop(3);
    for (int i = 0; i < kNumCats; ++i)
        catButtons[(size_t)i].setBounds(sidebar.removeFromTop(23).reduced(0, 1));
    sidebar.removeFromTop(8);

    // Bank
    bankHeaderLabel.setBounds(sidebar.removeFromTop(14));
    sidebar.removeFromTop(3);
    auto bankBtnRow = sidebar.removeFromBottom(24);
    int bw = bankBtnRow.getWidth() / 3;
    newBankButton   .setBounds(bankBtnRow.removeFromLeft(bw).reduced(1, 0));
    renameBankButton.setBounds(bankBtnRow.removeFromLeft(bw).reduced(1, 0));
    deleteBankButton.setBounds(bankBtnRow.reduced(1, 0));
    sidebar.removeFromBottom(4);
    bankListBox.setBounds(sidebar);

    // Center: search + table + count
    searchLabel.setBounds(panel.removeFromTop(14));
    panel.removeFromTop(2);
    searchEditor.setBounds(panel.removeFromTop(22));
    panel.removeFromTop(4);
    patchCountLabel.setBounds(panel.removeFromBottom(14));
    patchTable.setBounds(panel);

    // Action bar layout (right → left for most, load on far left)
    closeButton      .setBounds(actionBar.removeFromRight(100).reduced(2));
    actionBar.removeFromRight(8);
    exportBankButton .setBounds(actionBar.removeFromRight(90).reduced(2));
    importBankButton .setBounds(actionBar.removeFromRight(90).reduced(2));
    exportPatchButton.setBounds(actionBar.removeFromRight(90).reduced(2));
    importPatchButton.setBounds(actionBar.removeFromRight(90).reduced(2));
    actionBar.removeFromRight(12);
    deleteButton     .setBounds(actionBar.removeFromRight(70).reduced(2));
    saveButton       .setBounds(actionBar.removeFromRight(70).reduced(2));
    loadButton       .setBounds(actionBar.removeFromLeft(70).reduced(2));
}

//──────────────────────────────────────────────────────────────────────────────
// Helpers
//──────────────────────────────────────────────────────────────────────────────

void PatchBrowserPanel::refreshAll()
{
    allPatches = pm.getAllPatches();
    bankNames  = pm.getBankNames();
    bankListBox.updateContent();

    // Restore bank list selection
    if (filterBank.isEmpty())
    {
        bankListBox.selectRow(0, false, true);
    }
    else
    {
        int idx = bankNames.indexOf(filterBank);
        bankListBox.selectRow(idx >= 0 ? idx + 1 : 0, false, true);
    }

    applyFilter();
}

void PatchBrowserPanel::applyFilter()
{
    filteredPatches.clear();
    auto searchLower = searchText.toLowerCase();

    for (const auto& p : allPatches)
    {
        if (filterCategory != PresetManager::Category::All && p.category != filterCategory)
            continue;
        if (filterBank.isNotEmpty() && p.bankName != filterBank)
            continue;
        if (searchText.isNotEmpty() && !p.name.toLowerCase().contains(searchLower))
            continue;
        filteredPatches.add(p);
    }

    patchTable.updateContent();
    patchTable.deselectAllRows();
    selectedRow = -1;

    int n = filteredPatches.size();
    patchCountLabel.setText(juce::String(n) + (n == 1 ? " patch" : " patches"),
                            juce::dontSendNotification);
    updateActionStates();
}

void PatchBrowserPanel::updateActionStates()
{
    bool hasSel  = (selectedRow >= 0 && selectedRow < filteredPatches.size());
    bool hasBank = filterBank.isNotEmpty();

    loadButton       .setEnabled(hasSel);
    deleteButton     .setEnabled(hasSel);
    exportPatchButton.setEnabled(hasSel);
    renameBankButton .setEnabled(hasBank);
    deleteBankButton .setEnabled(hasBank);
    exportBankButton .setEnabled(true);
}

void PatchBrowserPanel::setCategoryFilter(PresetManager::Category cat)
{
    filterCategory = cat;
    applyFilter();
}

void PatchBrowserPanel::setBankFilter(const juce::String& bankName)
{
    filterBank = bankName;
    applyFilter();
}

juce::String PatchBrowserPanel::selectedBankName() const
{
    int row = bankListBox.getSelectedRow();
    if (row <= 0 || row - 1 >= bankNames.size()) return {};
    return bankNames[row - 1];
}

//──────────────────────────────────────────────────────────────────────────────
// Actions
//──────────────────────────────────────────────────────────────────────────────

void PatchBrowserPanel::doLoad()
{
    if (selectedRow < 0 || selectedRow >= filteredPatches.size()) return;
    pm.loadPreset(filteredPatches.getReference(selectedRow));
    if (onPresetLoaded) onPresetLoaded();
    closeBrowser();
}

void PatchBrowserPanel::doSave()
{
    auto* window = new juce::AlertWindow("Save Preset", {},
                                         juce::AlertWindow::NoIcon);
    window->addTextEditor("name", pm.getCurrentPresetName(), "Name:");

    auto catNames = PresetManager::allCategoryNames();
    window->addComboBox("category", catNames, "Category:");
    if (auto* c = window->getComboBoxComponent("category"))
    {
        int catIdx = juce::jmax(0, static_cast<int>(pm.getCurrentCategory()) - 1);
        c->setSelectedItemIndex(catIdx, juce::dontSendNotification);
    }

    juce::StringArray bankOpts = { "(Root)" };
    bankOpts.addArray(bankNames);
    window->addComboBox("bank", bankOpts, "Bank:");
    if (auto* c = window->getComboBoxComponent("bank"))
    {
        if (filterBank.isEmpty())
            c->setSelectedItemIndex(0, juce::dontSendNotification);
        else if (int idx = bankNames.indexOf(filterBank); idx >= 0)
            c->setSelectedItemIndex(idx + 1, juce::dontSendNotification);
    }

    window->addButton("SAVE",   1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    window->enterModalState(true,
        juce::ModalCallbackFunction::create([this, window](int result)
        {
            if (result == 1)
            {
                auto name = window->getTextEditorContents("name").trim();
                if (name.isNotEmpty())
                {
                    int catIdx = 0;
                    if (auto* c = window->getComboBoxComponent("category"))
                        catIdx = c->getSelectedItemIndex();
                    auto cat = static_cast<PresetManager::Category>(
                        juce::jlimit(1, (int)PresetManager::Category::Other, catIdx + 1));

                    juce::String bank;
                    if (auto* c = window->getComboBoxComponent("bank"))
                    {
                        int bankIdx = c->getSelectedItemIndex();
                        if (bankIdx > 0 && bankIdx - 1 < bankNames.size())
                            bank = bankNames[bankIdx - 1];
                    }

                    pm.savePreset(name, cat, bank);
                    if (onPresetLoaded) onPresetLoaded();
                    refreshAll();
                }
            }
            delete window;
        }), false);
}

void PatchBrowserPanel::doDelete()
{
    if (selectedRow < 0 || selectedRow >= filteredPatches.size()) return;
    auto patchName = filteredPatches.getReference(selectedRow).name;

    auto* window = new juce::AlertWindow("Delete Preset",
        "Delete \"" + patchName + "\"? This cannot be undone.",
        juce::AlertWindow::WarningIcon);
    window->addButton("DELETE", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    window->enterModalState(true,
        juce::ModalCallbackFunction::create([this, window, patchName](int result)
        {
            if (result == 1 && selectedRow >= 0 && selectedRow < filteredPatches.size())
            {
                if (filteredPatches.getReference(selectedRow).name == patchName)
                    pm.deletePreset(filteredPatches.getReference(selectedRow));
            }
            delete window;
            refreshAll();
        }), false);
}

void PatchBrowserPanel::doImportPatch()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Import Patch", juce::File{},
        "*" + juce::String(PresetManager::kPresetExtension));

    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto files = fc.getResults();
            if (files.isEmpty()) return;
            pm.importPatch(files[0], filterBank);
            refreshAll();
        });
}

void PatchBrowserPanel::doExportPatch()
{
    if (selectedRow < 0 || selectedRow >= filteredPatches.size()) return;
    auto info = filteredPatches.getReference(selectedRow);

    auto chooser = std::make_shared<juce::FileChooser>(
        "Export Patch",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
            .getChildFile(info.name + PresetManager::kPresetExtension),
        "*" + juce::String(PresetManager::kPresetExtension));

    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser, info](const juce::FileChooser& fc)
        {
            auto dest = fc.getResult();
            if (dest.getFullPathName().isNotEmpty())
                pm.exportPatch(info, dest);
        });
}

void PatchBrowserPanel::doImportBank()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Import Bank", juce::File{},
        "*" + juce::String(PresetManager::kBankExtension));

    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto files = fc.getResults();
            if (files.isEmpty()) return;
            pm.importBank(files[0]);
            refreshAll();
        });
}

void PatchBrowserPanel::doExportBank()
{
    juce::String bankToExport = filterBank;
    juce::String defaultName  = bankToExport.isEmpty() ? "AllPatches" : bankToExport;

    auto chooser = std::make_shared<juce::FileChooser>(
        "Export Bank",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
            .getChildFile(defaultName + PresetManager::kBankExtension),
        "*" + juce::String(PresetManager::kBankExtension));

    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser, bankToExport](const juce::FileChooser& fc)
        {
            auto dest = fc.getResult();
            if (dest.getFullPathName().isNotEmpty())
                pm.exportBank(bankToExport, dest);
        });
}

void PatchBrowserPanel::doNewBank()
{
    auto* window = new juce::AlertWindow("New Bank", {},
                                         juce::AlertWindow::NoIcon);
    window->addTextEditor("name", {}, "Bank Name:");
    window->addButton("CREATE", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    window->enterModalState(true,
        juce::ModalCallbackFunction::create([this, window](int result)
        {
            if (result == 1)
            {
                auto name = window->getTextEditorContents("name").trim();
                if (name.isNotEmpty())
                    pm.createBank(name);
            }
            delete window;
            refreshAll();
        }), false);
}

void PatchBrowserPanel::doRenameBank()
{
    auto oldName = selectedBankName();
    if (oldName.isEmpty()) return;

    auto* window = new juce::AlertWindow("Rename Bank", {},
                                         juce::AlertWindow::NoIcon);
    window->addTextEditor("name", oldName, "New Name:");
    window->addButton("RENAME", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    window->enterModalState(true,
        juce::ModalCallbackFunction::create([this, window, oldName](int result)
        {
            if (result == 1)
            {
                auto newName = window->getTextEditorContents("name").trim();
                if (newName.isNotEmpty() && newName != oldName)
                {
                    pm.renameBank(oldName, newName);
                    if (filterBank == oldName)
                        filterBank = newName;
                }
            }
            delete window;
            refreshAll();
        }), false);
}

void PatchBrowserPanel::doDeleteBank()
{
    auto bankName = selectedBankName();
    if (bankName.isEmpty()) return;

    auto* window = new juce::AlertWindow("Delete Bank",
        "Delete bank \"" + bankName + "\" and ALL its patches?\nThis cannot be undone.",
        juce::AlertWindow::WarningIcon);
    window->addButton("DELETE", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    window->enterModalState(true,
        juce::ModalCallbackFunction::create([this, window, bankName](int result)
        {
            if (result == 1)
            {
                pm.deleteBank(bankName);
                if (filterBank == bankName)
                    filterBank = {};
            }
            delete window;
            refreshAll();
        }), false);
}
