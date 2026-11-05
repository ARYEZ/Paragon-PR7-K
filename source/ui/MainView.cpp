#include "MainView.h"

#include "ui/Finish.h"

#include "ModDestinations.h"

#include "Parameters.h"
#include "ui/Logo.h"

namespace
{
    // The header was measured off the PR7 family's mockup (1672 px wide);
    // our canvas is 1600. Header positions below are in the mockup's pixels, scaled by this.
    constexpr float mockScale = (float) MainView::width / 1672.0f;

    juce::Rectangle<float> mockF (float left, float top, float right, float bottom)
    {
        return juce::Rectangle<float>::leftTopRightBottom (left, top, right, bottom) * mockScale;
    }

    juce::Rectangle<int> mock (float left, float top, float right, float bottom)
    {
        return mockF (left, top, right, bottom).toNearestInt();
    }

    const auto tabsGroup = mockF (336, 10, 1087, 55);   // (wider in PR7-K 0.15 for ARP, 0.17 for FX; DRUMS' room shared out in 0.54.0)
    const auto rigGroup = mockF (1102, 10, 1399, 55);

    // The synth page, in canvas pixels.
    const juce::Rectangle<int> oscillatorArea { 12, 68, 888, 372 };
    const juce::Rectangle<int> filterArea { 908, 68, 580, 372 };
    constexpr int lowerTop = 448, lowerHeight = 292;
    const juce::Rectangle<int> filterEnvelopeArea { 12, lowerTop, 366, lowerHeight };
    const juce::Rectangle<int> ampEnvelopeArea { 386, lowerTop, 366, lowerHeight };
    const juce::Rectangle<int> ampArea { 760, lowerTop, 356, lowerHeight };
    const juce::Rectangle<int> playArea { 1124, lowerTop, 364, lowerHeight };
    const juce::Rectangle<int> outputArea { 1496, 68, 92, 672 };
    const juce::Rectangle<int> keyboardArea { 12, 750, 1576, 138 };
    const juce::Rectangle<int> pageArea { 12, 68, 1476, 672 };   // PRESETS and SETTINGS, beside the OUTPUT strip
}

//==============================================================================
GainColumn::GainColumn (const juce::String& title, juce::AudioProcessorValueTreeState& parameters,
                        const juce::String& parameterId, LevelMeterSource& meterSource)
    : attachment (parameters, parameterId, knob),
      meter (meterSource)
{
    titleLabel.setText (title, juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::FontOptions (14.5f, juce::Font::bold));

    // Set on the knob itself: the editor's look-and-feel is attached after the
    // knob's value box has already been created, so it wouldn't pick these up.
    knob.setColour (juce::Slider::textBoxTextColourId, ParagonColours::accent);
    knob.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 22);
    auto* parameter = parameters.getParameter (parameterId);
    knob.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));

    addAndMakeVisible (titleLabel);
    addAndMakeVisible (knob);
    addAndMakeVisible (meter);
}

void GainColumn::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
}

void GainColumn::resized()
{
    auto area = getLocalBounds();
    titleLabel.setBounds (area.withHeight (26).withY (14));
    knob.setBounds (juce::Rectangle<int> (76, 92).withCentre ({ area.getCentreX(), 86 }));
    meter.setBounds (juce::Rectangle<int> (46, 0).withX (area.getCentreX() - 26).withY (146).withBottom (area.getBottom() - 22));
}

//==============================================================================
MainView::MainView (ParagonX1AudioProcessor& processorToShow)
    : audioProcessor (processorToShow),
      oscillators (processorToShow),
      filter (processorToShow.getParameters()),
      face (processorToShow),
      filterEnvelope (processorToShow.getParameters(), "FILTER ENVELOPE",
                      { ParamIDs::fltAttack, ParamIDs::fltHold, ParamIDs::fltDecay, ParamIDs::fltSustain, ParamIDs::fltRelease,
                        ParamIDs::fltAttackCurve, ParamIDs::fltDecayCurve, ParamIDs::fltReleaseCurve }),
      ampEnvelope (processorToShow.getParameters(), "AMP ENVELOPE",
                   { ParamIDs::ampAttack, ParamIDs::ampHold, ParamIDs::ampDecay, ParamIDs::ampSustain, ParamIDs::ampRelease,
                     ParamIDs::ampAttackCurve, ParamIDs::ampDecayCurve, ParamIDs::ampReleaseCurve }),
      amp (processorToShow.getParameters()),
      play (processorToShow),
      outputColumn ("OUTPUT", processorToShow.getParameters(), ParamIDs::outputGain, processorToShow.getOutputMeter()),
      sources (processorToShow),
      keyboard (processorToShow.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard),
      modPage (processorToShow),
      arpPage (processorToShow),
      fxPage (processorToShow),
      rigBrowser (processorToShow),
      settingsPage (processorToShow)
{
    // Header
    for (auto* tab : { &synthTab, &modTab, &arpTab, &fxTab, &presetsTab, &settingsTab })
    {
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (100);
        addAndMakeVisible (*tab);
    }
    synthTab.setToggleState (true, juce::dontSendNotification);
    synthTab.setComponentID ("synthTab");
    presetsTab.setComponentID ("presetsTab");
    settingsTab.setComponentID ("settingsTab");
    synthTab.onClick = [this] { showPage (Page::synth); };
    modTab.setComponentID ("modTab");
    modTab.onClick = [this] { showPage (Page::modulation); };
    arpTab.setComponentID ("arpTab");
    arpTab.onClick = [this] { showPage (Page::arp); };
    fxTab.setComponentID ("fxTab");
    fxTab.onClick = [this] { showPage (Page::fx); };
    presetsTab.onClick = [this] { showPage (Page::presets); };
    settingsTab.onClick = [this] { showPage (Page::settings); };

    previousRig.onClick = [this] { audioProcessor.stepRig (-1); };
    nextRig.onClick = [this] { audioProcessor.stepRig (1); };
    rigNameButton.getProperties().set (ParagonLookAndFeel::plainTextProperty, 19.0f);
    rigNameButton.setColour (juce::TextButton::textColourOffId, ParagonColours::accent);
    rigNameButton.setTooltip ("The current preset. * means it has changed since it was loaded or saved. Click to browse presets.");
    rigNameButton.onClick = [this] { showPage (Page::presets); };

    undoButton.onClick = [this] { audioProcessor.undo(); };
    redoButton.onClick = [this] { audioProcessor.redo(); };
    saveButton.onClick = [this] { saveRig(); };
    settingsButton.onClick = [this] { showPage (page == Page::settings ? Page::synth : Page::settings); };

    for (auto* component : std::initializer_list<juce::Component*> { &previousRig, &nextRig, &rigNameButton,
                                                                      &undoButton, &redoButton, &saveButton, &settingsButton,
                                                                      &outputColumn })
        addAndMakeVisible (component);

    synthPage = { &filterEnvelope, &ampEnvelope, &amp, &play, &rackButton };
    for (auto* component : synthPage)
        addAndMakeVisible (component);

    // 0.49.0: the front panel, or the rack (OSCILLATORS and FILTER) in its place; the key swaps them, and the choice
    // is remembered.
    addChildComponent (oscillators);
    addChildComponent (filter);
    addChildComponent (face);
    face.onPrevious = [this] { audioProcessor.stepRig (-1); };
    face.onNext = [this] { audioProcessor.stepRig (1); };
    face.onScreenClicked = [this] { showPage (Page::presets); };
    rackButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
    rackButton.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
    rackButton.setTooltip ("Shows the full OSCILLATORS and FILTER panels (the rack), or the front panel in their place.");
    rackButton.onClick = [this] { showRack (! rackShown); };
    rackShown = Finish::rackShown();

    // The keyboard, in the plugin's colours: dark keys lit red-orange as they're played.
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffdcd7da));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff141115));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff6b6569));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, ParagonColours::accent.withAlpha (0.25f));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, ParagonColours::glow.withAlpha (0.85f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.5f));
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, juce::Colour (0xff151217));
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, ParagonColours::text);
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, ParagonColours::inkDim);
    keyboard.setAvailableRange (24, 107);   // C1 to B7 shown (notes outside it from the DAW still play)
    keyboard.setOctaveForMiddleC (4);
    keyboard.setWantsKeyboardFocus (false);   // computer-key playing would fight the DAW's own shortcuts
    keyboard.setScrollButtonsVisible (false);  // all seven octaves fit
    addAndMakeVisible (keyboard);
    addAndMakeVisible (sources);

    addChildComponent (modPage);
    addChildComponent (arpPage);
    addChildComponent (fxPage);
    addChildComponent (rigBrowser);
    addChildComponent (settingsPage);

    showPage (page);   // the front panel or the rack, as last left
    audioProcessor.rigBroadcaster.addChangeListener (this);
    refreshRig();
    startTimerHz (5);
    setWantsKeyboardFocus (true);
}

MainView::~MainView()
{
    audioProcessor.rigBroadcaster.removeChangeListener (this);
}

void MainView::showPage (Page newPage)
{
    page = newPage;
    auto& tab = newPage == Page::presets ? presetsTab : newPage == Page::settings ? settingsTab : newPage == Page::modulation ? modTab
              : newPage == Page::arp ? arpTab : newPage == Page::fx ? fxTab : synthTab;
    tab.setToggleState (true, juce::dontSendNotification);
    for (auto* component : synthPage)
        component->setVisible (newPage == Page::synth);
    oscillators.setVisible (newPage == Page::synth && rackShown);
    filter.setVisible (newPage == Page::synth && rackShown);
    face.setVisible (newPage == Page::synth && ! rackShown);
    rackButton.setButtonText (rackShown ? "HIDE RACK" : "SHOW RACK");
    modPage.setVisible (newPage == Page::modulation);
    arpPage.setVisible (newPage == Page::arp);
    fxPage.setVisible (newPage == Page::fx);
    rigBrowser.setVisible (newPage == Page::presets);
    settingsPage.setVisible (newPage == Page::settings);
}

void MainView::showRack (bool shown, bool remember)
{
    rackShown = shown;
    if (remember)
        Finish::setRackShown (shown);
    showPage (page);
}

void MainView::refreshRig()
{
    const auto rig = audioProcessor.getCurrentRig();
    const auto headerText = rig.name + (audioProcessor.isRigModified() ? " *" : "");
    if (rigNameButton.getButtonText() != headerText && ! audioProcessor.getLibraryDownload().isRunning())
        rigNameButton.setButtonText (headerText);

    // The front panel's screen: the preset's slot, bank and notes (looked up in the library), and its macros' names.
    SynthFace::Screen screen;
    screen.name = rig.name;
    screen.modified = audioProcessor.isRigModified();
    for (const auto& entry : audioProcessor.getRigLibrary().getEntries())
        if ((rig.isUserRig() && entry.file == rig.file) || (! rig.isUserRig() && entry.isFactory && entry.rig.name == rig.factoryName))
        {
            screen.slot = entry.rig.getSlotName();
            screen.bank = Rig::bankName (entry.rig.bank);
            screen.notes = entry.rig.notes;
            break;
        }
    // 0.73.1: a library downloading shows on the screen and in the header on every page (it showed only on an
    // oscillator's SFZ line, which a preset playing its synthesized fallback doesn't even have).
    {
        const auto& download = audioProcessor.getLibraryDownload();
        using DS = libraries::Download::State;
        if (download.isRunning())
        {
            const auto percent = juce::String (juce::roundToInt (100.0f * download.getProgress())) + "%";
            screen.status = (download.getState() == DS::downloading ? "DOWNLOADING THE " : "UNPACKING THE ") + download.getLibrary().what
                          + " " + percent;
            screen.progress = download.getProgress();
            const auto header = download.getLibrary().what + " " + percent + "  \xc2\xb7  " + headerText;   // (first: a narrow header trims its end)
            if (rigNameButton.getButtonText() != juce::String (juce::CharPointer_UTF8 (header.toRawUTF8())))
                rigNameButton.setButtonText (juce::String (juce::CharPointer_UTF8 (header.toRawUTF8())));
        }
        else if (download.getState() == DS::failed)
        {
            screen.notes = "THE " + download.getLibrary().what + " DIDN'T DOWNLOAD: " + download.getMessage() + "      " + screen.notes;
            if (! downloadFailureSeen)   // offer it again (once), saying why the last try failed
            {
                downloadFailureSeen = true;
                libraryOfferedFor.clear();
            }
        }
        else
            downloadFailureSeen = false;
    }
    face.setScreen (screen);
    fxPage.followSignature (rig.name + "|" + rig.file.getFullPathName());   // 0.58.0: the SIGNATURE slot shows the preset's own
    const auto& matrix = audioProcessor.getModMatrix();
    face.setMacroNames ({ matrix.macroName (0), matrix.macroName (1), matrix.macroName (2) });
    offerMissingLibrary();
}

void MainView::offerMissingLibrary()
{
    const auto rig = audioProcessor.getCurrentRig();
    const auto key = rig.name + "|" + rig.file.getFullPathName();
    if (! offerLibrary || key == libraryOfferedFor || audioProcessor.getLibraryDownload().isRunning())
        return;
    const libraries::Library* missing = nullptr;
    for (int i = 0; i < ParamIDs::numOscillators && missing == nullptr; ++i)
        missing = audioProcessor.missingLibrary (i);
    // 0.72.0: a preset with a sampled version, playing its synthesized fallback.
    auto fallback = false;
    if (missing == nullptr)
        if (const auto* entry = audioProcessor.findCurrentRigEntry(); entry != nullptr && entry->isFactory && entry->rig.sampled != nullptr)
            if (const auto* needs = ParagonX1AudioProcessor::libraryOf (*entry->rig.sampled); needs != nullptr && ! audioProcessor.isLibraryInstalled (*needs))
            {
                missing = needs;
                fallback = true;
            }
    if (missing == nullptr)
        return;
    libraryOfferedFor = key;
    const auto kind = missing->what.toLowerCase();
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "GET THE " + missing->what,
        (fallback ? "\"" + rig.name + "\" can play the " + missing->name + ", a free sampled " + kind + " that isn't on this computer "
                    "yet. Until it is, it plays PR7-K's own synthesized sounds.\n\n"
                  : "\"" + rig.name + "\" plays the " + missing->name + ", a free sampled " + kind + " that isn't on this computer yet.\n\n")
        + "Download it now? It's about " + juce::String (missing->bytes / 1000000) + " MB (it unpacks to about "
        + juce::String (missing->unpackedMb) + " MB, in PR7-K's presets folder). It only has to be downloaded once. "
          "The screen and the header show how far it has got."
        + (audioProcessor.getLibraryDownload().getState() == libraries::Download::State::failed
               ? "\n\nThe last try didn't work: " + audioProcessor.getLibraryDownload().getMessage() : juce::String()),
        "DOWNLOAD", "NOT NOW", this,
        juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<MainView> (this), missing] (int result)
        {
            if (safe != nullptr && result == 1)
                safe->audioProcessor.startLibraryDownload (*missing);
        }));
}

void MainView::timerCallback()
{
    refreshRig();
    undoButton.setEnabled (audioProcessor.canUndo());
    redoButton.setEnabled (audioProcessor.canRedo());

    // The knobs' rings and MIDI marks follow the matrix, the links and learning.
    const auto& matrix = audioProcessor.getModMatrix();
    if (matrix.routes != shownRoutes || ! (audioProcessor.getMidiLinks() == shownLinks) || audioProcessor.getMidiLearnTarget() != shownLearning)
    {
        shownRoutes = matrix.routes;
        shownLinks = audioProcessor.getMidiLinks();
        shownLearning = audioProcessor.getMidiLearnTarget();
        repaint();
    }
}

//==============================================================================
std::vector<ControlHost::RouteInfo> MainView::routesTo (const juce::String& parameterId) const
{
    std::vector<RouteInfo> found;
    const auto& routes = audioProcessor.getModMatrix().routes;
    for (size_t i = 0; i < routes.size(); ++i)
    {
        const auto& r = routes[i];
        if (! r.on || r.destination != parameterId)
            continue;
        const auto source = synth::mod::sourceFromId (r.source);
        found.push_back ({ (int) i, synth::mod::sourceNames[(size_t) source], r.amount, r.percent });
    }
    return found;
}

bool MainView::canModulate (const juce::String& parameterId) const
{
    return ModDestinations::find (parameterId) != nullptr;
}

void MainView::addRoute (const juce::String& sourceId, const juce::String& parameterId)
{
    auto matrix = audioProcessor.getModMatrix();
    if ((int) matrix.routes.size() >= synth::mod::maxRoutes || ! canModulate (parameterId))
        return;
    ModRouteData route;
    route.source = sourceId;
    route.destination = parameterId;
    route.amount = 0.5f;
    matrix.routes.push_back (route);
    audioProcessor.setModMatrix (matrix);
    audioProcessor.modMatrixEditFinished();
    repaint();
}

void MainView::removeRoute (int index)
{
    auto matrix = audioProcessor.getModMatrix();
    if (index < 0 || index >= (int) matrix.routes.size())
        return;
    matrix.routes.erase (matrix.routes.begin() + index);
    audioProcessor.setModMatrix (matrix);
    audioProcessor.modMatrixEditFinished();
    repaint();
}

int MainView::linkedController (const juce::String& parameterId) const { return audioProcessor.getMidiLinks().controllerFor (parameterId); }
bool MainView::isLearning (const juce::String& parameterId) const { return audioProcessor.getMidiLearnTarget() == parameterId; }

void MainView::learn (const juce::String& parameterId)
{
    if (isLearning (parameterId))
        audioProcessor.cancelMidiLearn();
    else
        audioProcessor.startMidiLearn (parameterId);
    repaint();
}

void MainView::forget (const juce::String& parameterId)
{
    auto links = audioProcessor.getMidiLinks();
    links.forget (parameterId);
    audioProcessor.setMidiLinks (links);
    repaint();
}

void MainView::linkController (int controller, const juce::String& parameterId)
{
    auto links = audioProcessor.getMidiLinks();
    links.link (controller, parameterId);
    audioProcessor.setMidiLinks (links);
    repaint();
}

void MainView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshRig();
}

void MainView::saveRig()
{
    if (! audioProcessor.saveCurrentRig())
        rigBrowser.promptSaveAs();
    refreshRig();
}

bool MainView::keyPressed (const juce::KeyPress& key)
{
    const auto modifiers = key.getModifiers();
    if (! modifiers.isCommandDown())
        return false;

    const auto keyCode = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) key.getKeyCode());
    if (keyCode == 'Z')
    {
        if (modifiers.isShiftDown())
            audioProcessor.redo();
        else
            audioProcessor.undo();
        return true;
    }
    if (keyCode == 'Y')
    {
        audioProcessor.redo();
        return true;
    }
    return false;
}

void MainView::paint (juce::Graphics& g)
{
    // Background: near black, with a faint red warmth behind the oscillators.
    g.fillAll (ParagonColours::background);
    const auto centre = oscillatorArea.getCentre().toFloat();
    g.setGradientFill (juce::ColourGradient (ParagonColours::glow.withAlpha (0.06f), centre,
                                             ParagonColours::glow.withAlpha (0.0f), centre.translated (900.0f, 0.0f), true));
    g.fillRect (getLocalBounds());

    // Header: the emblem and name.
    auto emblem = mockF (33, 13, 73, 50);
    juce::Path triangle;
    triangle.addTriangle (emblem.getTopLeft(), emblem.getTopRight(), { emblem.getCentreX(), emblem.getBottom() });
    g.setColour (ParagonColours::glow.withAlpha (0.3f));
    g.strokePath (triangle, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (ParagonColours::glow);
    g.strokePath (triangle, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    triangle.applyTransform (juce::AffineTransform::scale (0.5f, 0.5f, emblem.getCentreX(), emblem.getCentreY() - 3.0f));
    g.strokePath (triangle, juce::PathStrokeType (1.5f));

    const auto nameArea = mockF (84, 10, 330, 55);
    const auto nameFont = juce::FontOptions (21.0f);
    g.setFont (nameFont);
    g.setColour (ParagonColours::text);
    g.drawText ("PARAGON", nameArea, juce::Justification::centredLeft, false);
    const auto nameWidth = (float) juce::GlyphArrangement::getStringWidthInt (nameFont, "PARAGON ");
    Logo::drawMark (g, nameArea.withTrimmedLeft (nameWidth).withSizeKeepingCentre (nameArea.getWidth() - nameWidth, 17.0f),
                   juce::Justification::centredLeft, { ParagonColours::glow, ParagonColours::glow, {}, {}, 1.0f, true });

    // Framed groups: the tabs and the preset selector.
    for (const auto& group : { tabsGroup, rigGroup })
    {
        g.setColour (juce::Colour (0xff0d0a0e));
        g.fillRoundedRectangle (group, 5.0f);
        g.setColour (ParagonColours::panelEdge);
        g.drawRoundedRectangle (group.reduced (0.5f), 5.0f, 1.0f);
    }
    for (auto x : { 450.0f, 563.0f, 685.0f, 807.0f, 946.0f })
        g.drawVerticalLine (juce::roundToInt (x * mockScale), tabsGroup.getY() + 8.0f, tabsGroup.getBottom() - 8.0f);

    g.setColour (ParagonColours::panelEdge);
    g.drawHorizontalLine (juce::roundToInt (64.0f * mockScale), 0.0f, (float) getWidth());

    // The keyboard's frame.
    DockStyle::drawPanel (g, keyboardArea.toFloat().expanded (0.0f, 0.0f));
}

void MainView::resized()
{
    // Header
    synthTab.setBounds (mock (338, 12, 449, 53));
    modTab.setBounds (mock (451, 12, 562, 53));
    arpTab.setBounds (mock (564, 12, 684, 53));
    fxTab.setBounds (mock (686, 12, 806, 53));
    presetsTab.setBounds (mock (808, 12, 945, 53));
    settingsTab.setBounds (mock (947, 12, 1085, 53));
    previousRig.setBounds (mock (1110, 14, 1150, 51));
    nextRig.setBounds (mock (1351, 14, 1391, 51));
    rigNameButton.setBounds (mock (1152, 14, 1349, 51));
    undoButton.setBounds (mock (1444, 14, 1482, 50));
    redoButton.setBounds (mock (1496, 14, 1534, 50));
    saveButton.setBounds (mock (1551, 14, 1589, 50));
    settingsButton.setBounds (mock (1611, 14, 1649, 50));

    oscillators.setBounds (oscillatorArea);
    filter.setBounds (filterArea);
    face.setBounds (oscillatorArea.getUnion (filterArea));
    filterEnvelope.setBounds (filterEnvelopeArea);
    ampEnvelope.setBounds (ampEnvelopeArea);
    amp.setBounds (ampArea);
    play.setBounds (playArea);
    // OUTPUT, with the rack key under it (on the SYNTH page).
    outputColumn.setBounds (outputArea.withTrimmedBottom (44));
    rackButton.setBounds (outputArea.withTop (outputArea.getBottom() - 38));

    // Seven octaves from C1, filling the strip (the width first: changing it moves the view).
    auto keys = keyboardArea.reduced (8, 6);
    sources.setBounds (keys.removeFromTop (24));
    keys.removeFromTop (4);
    keyboard.setBounds (keys);
    keyboard.setKeyWidth ((float) keys.getWidth() / 49.0f);
    keyboard.setLowestVisibleKey (24);

    modPage.setBounds (pageArea);
    arpPage.setBounds (pageArea);
    fxPage.setBounds (pageArea);
    rigBrowser.setBounds (pageArea);
    settingsPage.setBounds (pageArea);
}
