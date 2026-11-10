//==============================================================================
// In the plugin: the parameters reach the engine; the SIGNATURE slot (0.58.0, the owner's design): MOD / LO-FI is four
// units again, the fourth one of ECHOES, CRACKLE, DOWNPOUR, SIROCCO at a time, every one still playing when on; a
// preset's own signature shown when it loads; SIROCCO's page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ui/FxPage.h"
#include "ui/MainView.h"
#include "ui/RackSpecs.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    constexpr int phrygianDominant = 14;   // arp::scaleNames
    constexpr int d = 2;                   // sirocco::keyNames

    void setParam (ParagonX1AudioProcessor& p, const juce::String& id, float value)
    {
        auto* parameter = p.getParameters().getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }
}

TEST_CASE ("SIROCCO in the plugin: the answers sound, LEVEL is a matrix destination", "[sirocco]")
{
    ParagonX1AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    processor.seedRandomForTest (1);
    setParam (processor, ParamIDs::siroccoOn, 1.0f);
    setParam (processor, ParamIDs::siroccoKey, (float) d);
    setParam (processor, ParamIDs::siroccoScale, (float) phrygianDominant);
    setParam (processor, ParamIDs::siroccoLevel, 1.0f);
    processor.getKeyboardState().noteOn (1, 62, 0.8f);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    for (int b = 0; b < 20; ++b)
    {
        for (int c = 0; c < 2; ++c)
            juce::FloatVectorOperations::clear (buffer.getWritePointer (c), blockSize);
        processor.processBlock (buffer, midi);
    }
    const auto flags = processor.getSoundingNoteFlags();
    CHECK (flags[62] == 1);   // D played
    CHECK (flags[66] == 2);   // F#, its answer
    processor.getKeyboardState().noteOff (1, 62, 0.0f);

    for (const auto* id : { ParamIDs::siroccoLevel, ParamIDs::siroccoDetune, ParamIDs::siroccoSpread })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Sirocco");
    }
}

TEST_CASE ("The SIGNATURE slot: four units on MOD / LO-FI, one signature shown, the preset's own when it loads", "[sirocco][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    CHECK (signatureNames() == juce::StringArray { "ECHOES", "CRACKLE", "DOWNPOUR", "SIROCCO", "INFERNAL", "AFTERGLOW", "LIGHTTRAIL", "BLACKHOLE", "DILATION", "REMNANT", "PROXIMITY", "HAZE", "SPECTRE", "LEVIATHAN", "NUCLEARSPRING", "HYPERJUMP", "BLOOM", "LEGION", "OVERCHARGE", "AFTERMATH", "SUBMERGE", "LOCKSTEP", "COLOSSUS", "CANYON", "JACK-IN", "NAVICOMPUTER", "SWEEP", "REDLINE" });
    for (int s = 0; s < signatureNames().size(); ++s)
    {
        fx.showSignature (s);
        int visible = 0;
        for (int k = 0; k < 3 + signatureNames().size(); ++k)   // MOD, MOD 2, LO-FI and every signature effect
            visible += fx.getModFxUnit (k).isVisible() ? 1 : 0;
        CHECK (visible == 4);
        auto& unit = fx.getModFxUnit (3 + s);
        CHECK (unit.isVisible());
        CHECK (unit.getSpec().name == signatureNames()[s]);
        CHECK (unit.getBounds() == fx.getModFxUnit (3).getBounds());   // all in the fourth place
        CHECK (unit.getBounds().getHeight() == fx.getUnit (0).getBounds().getHeight());   // the MASTER units' size
        CHECK (unit.getBottom() <= fx.getHeight());
        CHECK (unit.getSpec().onFootClick != nullptr);
    }
    // Two on at once (MELTDOWN's Warning Tone has ECHOES and CRACKLE): the screen says so.
    setParam (processor, ParamIDs::echoOn, 1.0f);
    setParam (processor, ParamIDs::crackleOn, 1.0f);
    CHECK (fx.getModFxUnit (4).getSpec().footText() == "CRACKLE +1");

    // A preset with SIROCCO on loads: the slot shows SIROCCO.
    fx.showSignature (0);
    setParam (processor, ParamIDs::echoOn, 0.0f);
    setParam (processor, ParamIDs::crackleOn, 0.0f);
    setParam (processor, ParamIDs::siroccoOn, 1.0f);
    fx.followSignature ("a preset with SIROCCO");
    CHECK (fx.getSignatureShown() == 3);

    fx.getModFxUnit (6).getSpec().onEdit();
    auto& page = fx.getSiroccoPage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 9);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}

