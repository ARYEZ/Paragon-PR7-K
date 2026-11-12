#include "FxPage.h"

#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/RackSpecs.h"

FxPage::FxPage (ParagonX1AudioProcessor& processor)
    : audio (processor), insertStrip (processor), matrix (processor), delayPage (processor, SendPage::Kind::delay), reverbPage (processor, SendPage::Kind::reverb),
      echoesPage (processor), cracklePage (processor), downpourPage (processor), siroccoPage (processor), infernalPage (processor), afterglowPage (processor), lightTrailPage (processor), blackHolePage (processor), dilationPage (processor), remnantPage (processor), proximityPage (processor), hazePage (processor), spectrePage (processor), leviathanPage (processor), nuclearSpringPage (processor), hyperJumpPage (processor), bloomPage (processor), legionPage (processor), overchargePage (processor), aftermathPage (processor), submergePage (processor), lockstepPage (processor), colossusPage (processor), canyonPage (processor), jackInPage (processor), naviComputerPage (processor), sweepPage (processor), redlinePage (processor), freezeTagPage (processor), crossroadsPage (processor), gearworksPage (processor), ragtimePage (processor), sortPage (processor), eqPage (processor), dominionPage (processor), compPage (processor), limiterPage (processor)
{
    RackActions actions;
    actions.openPage = [this] (int unit)
    {
        constexpr View views[] = { View::eq, View::dominion, View::comp, View::limiter };
        show (views[juce::jlimit (0, 3, unit)]);
    };
    actions.openMatch = [this]
    {
        show (View::eq);
        eqPage.showMatch (true);
    };
    actions.analyserOn = [this] { return eqPage.isAnalyserOn(); };
    actions.setAnalyser = [this] (bool on) { eqPage.setAnalyser (on); };
    auto specs = makeRackSpecs (processor, std::move (actions));
    for (size_t i = 0; i < units.size(); ++i)
    {
        units[i] = std::make_unique<RackUnit> (processor.getParameters(), std::move (specs[i]));
        addAndMakeVisible (*units[i]);
    }
    for (auto* page : std::initializer_list<juce::Component*> { &eqPage, &dominionPage, &compPage, &limiterPage })
        addChildComponent (page);

    // THE SENDS (stage 7b).
    auto sendSpecs = makeSendSpecs (processor, [this] (int send) { show (send == 0 ? View::delay : View::reverb); });
    for (size_t i = 0; i < sendUnits.size(); ++i)
    {
        sendUnits[i] = std::make_unique<RackUnit> (processor.getParameters(), std::move (sendSpecs[i]));
        addChildComponent (*sendUnits[i]);
    }
    addChildComponent (matrix);
    addChildComponent (delayPage);
    addChildComponent (reverbPage);
    delayPage.onClose = [this] { show (View::sends); };
    reverbPage.onClose = [this] { show (View::sends); };

    // MOD, MOD 2 and LO-FI (stage 7c).
    auto modFxSpecs = makeModFxSpecs (processor, [this] (int unit)
    {
        constexpr View views[] = { View::mod, View::mod2, View::loFi, View::echoes, View::crackle, View::downpour, View::sirocco, View::infernal, View::afterglow, View::lightTrail, View::blackHole, View::dilation, View::remnant, View::proximity, View::haze, View::spectre, View::leviathan, View::nuclearSpring, View::hyperJump, View::bloom, View::legion, View::overcharge, View::aftermath, View::submerge, View::lockstep, View::colossus, View::canyon, View::jackIn, View::naviComputer, View::sweep, View::redline, View::freezeTag, View::crossroads, View::gearworks, View::ragtime, View::sort };
        show (views[juce::jlimit (0, (int) std::size (views) - 1, unit)]);
    }, [this] { chooseSignature(); });
    constexpr ModFxPage::Kind kinds[] = { ModFxPage::Kind::mod, ModFxPage::Kind::mod2, ModFxPage::Kind::loFi };
    for (size_t i = 0; i < modFxUnits.size(); ++i)
    {
        modFxUnits[i] = std::make_unique<RackUnit> (processor.getParameters(), std::move (modFxSpecs[i]));
        addChildComponent (*modFxUnits[i]);
    }
    for (size_t i = 0; i < modFxPages.size(); ++i)
    {
        modFxPages[i] = std::make_unique<ModFxPage> (processor, kinds[i]);
        modFxPages[i]->onClose = [this] { show (View::modFx); };
        addChildComponent (*modFxPages[i]);
    }
    // ECHOES (0.53.0): its page.
    echoesPage.onClose = [this] { show (View::modFx); };
    addChildComponent (echoesPage);
    // CRACKLE (0.56.0): its page.
    cracklePage.onClose = [this] { show (View::modFx); };
    addChildComponent (cracklePage);
    // DOWNPOUR (0.57.0): its page.
    downpourPage.onClose = [this] { show (View::modFx); };
    addChildComponent (downpourPage);
    // SIROCCO (0.58.0): its page.
    siroccoPage.onClose = [this] { show (View::modFx); };
    addChildComponent (siroccoPage);
    // INFERNAL (0.59.0): its page.
    infernalPage.onClose = [this] { show (View::modFx); };
    addChildComponent (infernalPage);
    // AFTERGLOW (0.60.0): its page.
    afterglowPage.onClose = [this] { show (View::modFx); };
    addChildComponent (afterglowPage);
    // LIGHTTRAIL (0.61.0): its page.
    lightTrailPage.onClose = [this] { show (View::modFx); };
    addChildComponent (lightTrailPage);
    // BLACKHOLE (0.62.0): its page.
    blackHolePage.onClose = [this] { show (View::modFx); };
    addChildComponent (blackHolePage);
    // DILATION (0.63.0): its page.
    dilationPage.onClose = [this] { show (View::modFx); };
    addChildComponent (dilationPage);
    // REMNANT (0.64.0): its page.
    remnantPage.onClose = [this] { show (View::modFx); };
    addChildComponent (remnantPage);
    // PROXIMITY (0.65.0): its page.
    proximityPage.onClose = [this] { show (View::modFx); };
    addChildComponent (proximityPage);
    // HAZE (0.66.0): its page.
    hazePage.onClose = [this] { show (View::modFx); };
    addChildComponent (hazePage);
    // SPECTRE (0.67.0): its page.
    spectrePage.onClose = [this] { show (View::modFx); };
    addChildComponent (spectrePage);
    // LEVIATHAN (0.68.0): its page.
    leviathanPage.onClose = [this] { show (View::modFx); };
    addChildComponent (leviathanPage);
    // NUCLEARSPRING (0.69.0): its page.
    nuclearSpringPage.onClose = [this] { show (View::modFx); };
    addChildComponent (nuclearSpringPage);
    hyperJumpPage.onClose = [this] { show (View::modFx); };
    addChildComponent (hyperJumpPage);
    // BLOOM (0.74.0): its page.
    bloomPage.onClose = [this] { show (View::modFx); };
    addChildComponent (bloomPage);
    // LEGION (0.76.0): its page.
    legionPage.onClose = [this] { show (View::modFx); };
    addChildComponent (legionPage);
    // OVERCHARGE (0.77.0): its page.
    overchargePage.onClose = [this] { show (View::modFx); };
    addChildComponent (overchargePage);
    // AFTERMATH (0.79.0): its page.
    aftermathPage.onClose = [this] { show (View::modFx); };
    addChildComponent (aftermathPage);
    // SUBMERGE (0.80.0): its page.
    submergePage.onClose = [this] { show (View::modFx); };
    addChildComponent (submergePage);
    // LOCKSTEP (0.81.0): its page.
    lockstepPage.onClose = [this] { show (View::modFx); };
    addChildComponent (lockstepPage);
    // COLOSSUS (0.82.0): its page.
    colossusPage.onClose = [this] { show (View::modFx); };
    addChildComponent (colossusPage);
    // CANYON (0.84.0): its page.
    canyonPage.onClose = [this] { show (View::modFx); };
    addChildComponent (canyonPage);
    // JACK-IN (0.85.0): its page.
    jackInPage.onClose = [this] { show (View::modFx); };
    addChildComponent (jackInPage);
    naviComputerPage.onClose = [this] { show (View::modFx); };
    addChildComponent (naviComputerPage);
    sweepPage.onClose = [this] { show (View::modFx); };
    addChildComponent (sweepPage);
    redlinePage.onClose = [this] { show (View::modFx); };
    addChildComponent (redlinePage);
    freezeTagPage.onClose = [this] { show (View::modFx); };
    crossroadsPage.onClose = [this] { show (View::modFx); };
    addChildComponent (freezeTagPage);
    addChildComponent (crossroadsPage);
    gearworksPage.onClose = [this] { show (View::modFx); };
    addChildComponent (gearworksPage);
    ragtimePage.onClose = [this] { show (View::modFx); };
    addChildComponent (ragtimePage);
    sortPage.onClose = [this] { show (View::modFx); };
    addChildComponent (sortPage);
    // THE INSERT BUSES (stage 7d): every bus's units and pages made now, the strip's bus shown.
    for (int b = 0; b < ParamIDs::numInsertBuses; ++b)
    {
        auto specs = makeInsertSpecs (processor, b, [this] (int unit)
        {
            constexpr View views[] = { View::insertDrive, View::insertGlitch, View::insertMod, View::insertLoFi, View::insertWah };
            show (views[juce::jlimit (0, 4, unit)]);
        });
        for (size_t i = 0; i < specs.size(); ++i)
        {
            insertUnits[(size_t) b][i] = std::make_unique<RackUnit> (processor.getParameters(), std::move (specs[i]));
            addChildComponent (*insertUnits[(size_t) b][i]);
        }
        insertEffectPages[(size_t) b][0] = std::make_unique<InsertEffectPage> (processor, InsertEffectPage::Kind::drive, b);
        insertEffectPages[(size_t) b][1] = std::make_unique<InsertEffectPage> (processor, InsertEffectPage::Kind::glitch, b);
        insertEffectPages[(size_t) b][2] = std::make_unique<InsertEffectPage> (processor, InsertEffectPage::Kind::wah, b);
        insertModFxPages[(size_t) b][0] = std::make_unique<ModFxPage> (processor, ModFxPage::Kind::mod, b);
        insertModFxPages[(size_t) b][1] = std::make_unique<ModFxPage> (processor, ModFxPage::Kind::loFi, b);
        for (auto& page : insertEffectPages[(size_t) b])
        {
            page->onClose = [this] { show (View::inserts); };
            addChildComponent (*page);
        }
        for (auto& page : insertModFxPages[(size_t) b])
        {
            page->onClose = [this] { show (View::inserts); };
            addChildComponent (*page);
        }
    }
    addChildComponent (insertStrip);
    insertStrip.onBusChanged = [this] (int) { show (View::inserts); };
    for (auto* tab : { &insertsTab, &sendsTab, &modFxTab, &masterTab })
    {
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (971);
        tab->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0d0a0e));
        tab->setColour (juce::TextButton::buttonOnColourId, ParagonColours::accentDim.withAlpha (0.6f));
        tab->setColour (juce::TextButton::textColourOffId, ParagonColours::textDim);
        tab->setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
        addAndMakeVisible (*tab);
    }
    sendsTab.setTooltip ("The SENDS: DELAY and REVERB, fed by each oscillator's send; they come back in before the MASTER rack");
    masterTab.setTooltip ("The MASTER rack: ECHELON, DOMINION, CONSTRICTOR and the LIMITER, on everything");
    modFxTab.setTooltip ("MOD, MOD 2, LO-FI and ECHOES: PR7-G's modulation (twice) and lo-fi, and the past played back under the present, on everything, the echoes and reverb too, before the MASTER rack");
    insertsTab.setTooltip ("The INSERTS: buses A to D, each a WAH, DRIVE, GLITCH, MOD and LO-FI on the oscillators routed to it, before the sends");
    insertsTab.onClick = [this] { show (View::inserts); };
    sendsTab.onClick = [this] { show (View::sends); };
    modFxTab.onClick = [this] { show (View::modFx); };
    masterTab.onClick = [this] { show (View::rack); };
    show (View::rack);
    eqPage.onClose = [this] { show (View::rack); };
    dominionPage.onClose = [this] { show (View::rack); };
    compPage.onClose = [this] { show (View::rack); };
    limiterPage.onClose = [this] { show (View::rack); };
}

void FxPage::show (View newView)
{
    view = newView;
    for (auto& unit : units)
        unit->setVisible (view == View::rack);
    for (auto& unit : sendUnits)
        unit->setVisible (view == View::sends);
    matrix.setVisible (view == View::sends);
    for (size_t i = 0; i < modFxUnits.size(); ++i)   // MOD, MOD 2, LO-FI, and the SIGNATURE slot's chosen one
        modFxUnits[i]->setVisible (view == View::modFx && (i < 3 || (int) i == 3 + signatureShown));
    modFxPages[0]->setVisible (view == View::mod);
    modFxPages[1]->setVisible (view == View::mod2);
    modFxPages[2]->setVisible (view == View::loFi);
    echoesPage.setVisible (view == View::echoes);
    cracklePage.setVisible (view == View::crackle);
    downpourPage.setVisible (view == View::downpour);
    siroccoPage.setVisible (view == View::sirocco);
    infernalPage.setVisible (view == View::infernal);
    afterglowPage.setVisible (view == View::afterglow);
    lightTrailPage.setVisible (view == View::lightTrail);
    blackHolePage.setVisible (view == View::blackHole);
    dilationPage.setVisible (view == View::dilation);
    remnantPage.setVisible (view == View::remnant);
    proximityPage.setVisible (view == View::proximity);
    hazePage.setVisible (view == View::haze);
    spectrePage.setVisible (view == View::spectre);
    leviathanPage.setVisible (view == View::leviathan);
    nuclearSpringPage.setVisible (view == View::nuclearSpring);
    hyperJumpPage.setVisible (view == View::hyperJump);
    bloomPage.setVisible (view == View::bloom);
    legionPage.setVisible (view == View::legion);
    overchargePage.setVisible (view == View::overcharge);
    aftermathPage.setVisible (view == View::aftermath);
    submergePage.setVisible (view == View::submerge);
    lockstepPage.setVisible (view == View::lockstep);
    colossusPage.setVisible (view == View::colossus);
    canyonPage.setVisible (view == View::canyon);
    jackInPage.setVisible (view == View::jackIn);
    naviComputerPage.setVisible (view == View::naviComputer);
    sweepPage.setVisible (view == View::sweep);
    redlinePage.setVisible (view == View::redline);
    freezeTagPage.setVisible (view == View::freezeTag);
    crossroadsPage.setVisible (view == View::crossroads);
    gearworksPage.setVisible (view == View::gearworks);
    ragtimePage.setVisible (view == View::ragtime);
    sortPage.setVisible (view == View::sort);
    const auto bus = insertStrip.getBus();
    insertStrip.setVisible (view == View::inserts);
    for (int b = 0; b < ParamIDs::numInsertBuses; ++b)
    {
        for (auto& unit : insertUnits[(size_t) b])
            unit->setVisible (view == View::inserts && b == bus);
        insertEffectPages[(size_t) b][0]->setVisible (view == View::insertDrive && b == bus);
        insertEffectPages[(size_t) b][1]->setVisible (view == View::insertGlitch && b == bus);
        insertEffectPages[(size_t) b][2]->setVisible (view == View::insertWah && b == bus);
        insertModFxPages[(size_t) b][0]->setVisible (view == View::insertMod && b == bus);
        insertModFxPages[(size_t) b][1]->setVisible (view == View::insertLoFi && b == bus);
    }
    delayPage.setVisible (view == View::delay);
    reverbPage.setVisible (view == View::reverb);
    const auto onRackView = view == View::rack || view == View::sends || view == View::modFx || view == View::inserts;
    for (auto* tab : { &insertsTab, &sendsTab, &modFxTab, &masterTab })
        tab->setVisible (onRackView);
    (view == View::inserts ? insertsTab : view == View::sends ? sendsTab : view == View::modFx ? modFxTab : masterTab)
        .setToggleState (true, juce::dontSendNotification);
    eqPage.setVisible (view == View::eq);
    dominionPage.setVisible (view == View::dominion);
    compPage.setVisible (view == View::comp);
    limiterPage.setVisible (view == View::limiter);
    if (view == View::eq)
        eqPage.getDisplay().grabKeyboardFocus();
    else if (view == View::dominion)
        dominionPage.getDisplay().grabKeyboardFocus();
    else if (view == View::comp)
        compPage.getDisplay().grabKeyboardFocus();
}

void FxPage::paint (juce::Graphics& g)
{
    if (view == View::rack || view == View::sends || view == View::modFx || view == View::inserts)
        DockStyle::drawPanel (g, getLocalBounds().toFloat());
}

void FxPage::resized()
{
    // The tabs, then the four units, one to a row as in a rack, each as wide as the page allows at its drawn proportions.
    auto area = getLocalBounds().reduced (12, 10);
    auto tabs = area.removeFromTop (26);
    insertsTab.setBounds (tabs.removeFromLeft (110));
    tabs.removeFromLeft (4);
    sendsTab.setBounds (tabs.removeFromLeft (110));
    tabs.removeFromLeft (4);
    modFxTab.setBounds (tabs.removeFromLeft (130));
    tabs.removeFromLeft (4);
    masterTab.setBounds (tabs.removeFromLeft (110));
    area.removeFromTop (8);
    const auto rackArea = area;
    constexpr int gap = 8;
    const auto byHeight = (float) (area.getHeight() - 3 * gap) / 4.0f;
    const auto width = juce::jmin ((float) area.getWidth(), byHeight * RackUnit::referenceWidth / RackUnit::referenceHeight);
    const auto height = width * RackUnit::referenceHeight / RackUnit::referenceWidth;
    const auto top = (float) area.getY() + ((float) area.getHeight() - 4.0f * height - 3.0f * gap) * 0.5f;
    for (size_t i = 0; i < units.size(); ++i)
        units[i]->setBounds (juce::Rectangle<float> ((float) area.getCentreX() - width * 0.5f, top + (float) i * (height + gap), width, height).toNearestInt());
    for (auto* page : std::initializer_list<juce::Component*> { &eqPage, &dominionPage, &compPage, &limiterPage, &delayPage, &reverbPage,
                                                                modFxPages[0].get(), modFxPages[1].get(), modFxPages[2].get(), &echoesPage, &cracklePage, &downpourPage, &siroccoPage, &infernalPage, &afterglowPage, &lightTrailPage, &blackHolePage, &dilationPage, &remnantPage, &proximityPage, &hazePage, &spectrePage, &leviathanPage, &nuclearSpringPage, &hyperJumpPage, &bloomPage, &legionPage, &overchargePage, &aftermathPage, &submergePage, &lockstepPage, &colossusPage, &canyonPage, &jackInPage, &naviComputerPage, &sweepPage, &redlinePage, &freezeTagPage, &crossroadsPage, &gearworksPage, &ragtimePage, &sortPage })
        page->setBounds (getLocalBounds());
    // INSERTS: the strip, then the bus's five units in the order they play (the WAH, last in the list, first), sized
    // to fit what's left.
    {
        auto insertArea = rackArea;
        insertStrip.setBounds (insertArea.removeFromTop (ParamControl::knobHeight + 8));
        constexpr int insertGap = 6;
        insertArea.removeFromTop (insertGap);
        const auto count = (float) insertUnits[0].size();
        const auto byHeightNow = ((float) insertArea.getHeight() - (count - 1.0f) * insertGap) / count;
        const auto w = juce::jmin ((float) insertArea.getWidth(), byHeightNow * RackUnit::referenceWidth / RackUnit::referenceHeight);
        const auto h = w * RackUnit::referenceHeight / RackUnit::referenceWidth;
        for (auto& busUnits : insertUnits)
            for (size_t i = 0; i < busUnits.size(); ++i)
            {
                const auto row = (float) ((i + 1) % busUnits.size());   // the WAH (4) on row 0, DRIVE (0) on row 1...
                busUnits[i]->setBounds (juce::Rectangle<float> ((float) insertArea.getCentreX() - w * 0.5f, (float) insertArea.getY() + row * (h + insertGap),
                                                                w, h).toNearestInt());
            }
        for (int b = 0; b < ParamIDs::numInsertBuses; ++b)
        {
            for (auto& page : insertEffectPages[(size_t) b])
                page->setBounds (getLocalBounds());
            for (auto& page : insertModFxPages[(size_t) b])
                page->setBounds (getLocalBounds());
        }
    }
    // MOD / LO-FI: four places as the MASTER's (0.58.0, the owner's design): MOD, MOD 2, LO-FI, and the SIGNATURE slot,
    // whose four effects share the fourth place (one shown at a time).
    for (size_t i = 0; i < modFxUnits.size(); ++i)
    {
        const auto row = (float) juce::jmin ((int) i, 3);
        modFxUnits[i]->setBounds (juce::Rectangle<float> ((float) area.getCentreX() - width * 0.5f, top + row * (height + gap), width, height).toNearestInt());
    }

    // SENDS: the two units at the master units' size, the matrix under them.
    auto sendsArea = rackArea;
    for (size_t i = 0; i < sendUnits.size(); ++i)
    {
        sendUnits[i]->setBounds (juce::Rectangle<float> ((float) sendsArea.getCentreX() - width * 0.5f, (float) sendsArea.getY(), width, height).toNearestInt());
        sendsArea.removeFromTop (juce::roundToInt (height) + gap);
    }
    matrix.setBounds (sendsArea.withSizeKeepingCentre (juce::roundToInt (width), sendsArea.getHeight()));
}

//==============================================================================
void FxPage::showSignature (int signature)
{
    signatureShown = juce::jlimit (0, signatureNames().size() - 1, signature);
    if (view == View::modFx)
        show (View::modFx);
}

void FxPage::chooseSignature()
{
    // The SIGNATURE slot's menu: each effect, ticked if shown, marked if on.
    juce::PopupMenu menu;
    menu.addSectionHeader ("SIGNATURE");
    for (int k = 0; k < signatureNames().size(); ++k)
        menu.addItem (k + 1, signatureNames()[k] + (signatureOn (audio, k) ? "  (ON)" : ""), true, k == signatureShown);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (modFxUnits[(size_t) (3 + signatureShown)].get()),
                        [safe = juce::Component::SafePointer<FxPage> (this)] (int chosen)
                        {
                            if (safe != nullptr && chosen > 0)
                                safe->showSignature (chosen - 1);
                        });
}

void FxPage::followSignature (const juce::String& rigKey)
{
    if (rigKey == followedRig)
        return;
    followedRig = rigKey;
    if (signatureOn (audio, signatureShown))
        return;
    for (int k = 0; k < signatureNames().size(); ++k)
        if (signatureOn (audio, k))
        {
            showSignature (k);
            return;
        }
}
