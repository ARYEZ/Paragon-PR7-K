#pragma once

#include "PluginProcessor.h"
#include "ui/CompPage.h"
#include "ui/DominionPage.h"
#include "ui/EchoesPage.h"
#include "ui/CracklePage.h"
#include "ui/DownpourPage.h"
#include "ui/SiroccoPage.h"
#include "ui/InfernalPage.h"
#include "ui/AfterglowPage.h"
#include "ui/LightTrailPage.h"
#include "ui/BlackHolePage.h"
#include "ui/DilationPage.h"
#include "ui/RemnantPage.h"
#include "ui/ProximityPage.h"
#include "ui/HazePage.h"
#include "ui/SpectrePage.h"
#include "ui/LeviathanPage.h"
#include "ui/NuclearSpringPage.h"
#include "ui/BloomPage.h"
#include "ui/LegionPage.h"
#include "ui/OverchargePage.h"
#include "ui/AftermathPage.h"
#include "ui/SubmergePage.h"
#include "ui/LockstepPage.h"
#include "ui/ColossusPage.h"
#include "ui/CanyonPage.h"
#include "ui/JackInPage.h"
#include "ui/HyperJumpPage.h"
#include "ui/NaviComputerPage.h"
#include "ui/SweepPage.h"
#include "ui/RedlinePage.h"
#include "ui/FreezeTagPage.h"
#include "ui/EqPage.h"
#include "ui/InsertPages.h"
#include "ui/LimiterPage.h"
#include "ui/ModFxPages.h"
#include "ui/RackUnit.h"
#include "ui/SendPages.h"

// The FX page: four views, INSERTS, SENDS, MOD / LO-FI and MASTER (tabs at the top, in the sound's order).
//   INSERTS (stage 7d): the insert buses A to D (one shown at a time: its strip with the oscillators routed to it and
//   its sends, then its DRIVE, GLITCH, MOD and LO-FI units).
//   SENDS (stage 7b): PR7-G's DELAY and REVERB as two rack units, and the send matrix (each oscillator's sends,
//   DELAY > REVERB). Their returns join the synth before the inserts.
//   MOD / LO-FI (stage 7c): PR7-G's MOD block twice and its LO-FI, on the whole sound (the returns too), before MASTER.
//   MASTER (stage 7a): THE RACK, PR7-E 0.18.1's four rack units in the signal chain's order (ECHELON, DOMINION,
//   CONSTRICTOR, the LIMITER), each with its POWER switch, meter, screen, keys and knobs.
// A unit's EDIT (or its name) opens that effect's big page here; the page's BACK TO RACK returns to its view.
class FxPage final : public juce::Component
{
public:
    explicit FxPage (ParagonX1AudioProcessor&);

    enum class View { rack, eq, dominion, comp, limiter,   // rack: the MASTER view
                      sends, delay, reverb,                // stage 7b
                      modFx, mod, mod2, loFi,              // stage 7c
                      inserts, insertDrive, insertGlitch, insertMod, insertLoFi, insertWah,   // stage 7d (the strip's bus)
                      echoes,                                               // 0.53.0 (on the MOD / LO-FI tab)
                      crackle,                                              // 0.56.0 (on the MOD / LO-FI tab)
                      downpour,                                             // 0.57.0 (on the MOD / LO-FI tab)
                      sirocco,                                              // 0.58.0 (the SIGNATURE slot)
                      infernal,                                             // 0.59.0 (the SIGNATURE slot)
                      afterglow,                                            // 0.60.0 (the SIGNATURE slot)
                      lightTrail,                                           // 0.61.0 (the SIGNATURE slot)
                      blackHole,                                            // 0.62.0 (the SIGNATURE slot)
                      dilation,                                             // 0.63.0 (the SIGNATURE slot)
                      remnant,                                              // 0.64.0 (the SIGNATURE slot)
                      proximity,                                            // 0.65.0 (the SIGNATURE slot)
                      haze,                                                 // 0.66.0 (the SIGNATURE slot)
                      spectre,                                              // 0.67.0 (the SIGNATURE slot)
                      leviathan,                                            // 0.68.0 (the SIGNATURE slot)
                      nuclearSpring, hyperJump, bloom, legion, overcharge, aftermath, submerge, lockstep, colossus, canyon, jackIn, naviComputer, sweep, redline, freezeTag };                                      // 0.69.0 (the SIGNATURE slot)
    void show (View);
    View getView() const noexcept { return view; }

    RackUnit& getUnit (int index) noexcept { return *units[(size_t) index]; }
    RackUnit& getSendUnit (int index) noexcept { return *sendUnits[(size_t) index]; }
    SendPage& getDelayPage() noexcept { return delayPage; }
    SendPage& getReverbPage() noexcept { return reverbPage; }
    SendMatrix& getSendMatrix() noexcept { return matrix; }
    RackUnit& getModFxUnit (int index) noexcept { return *modFxUnits[(size_t) index]; }   // MOD, MOD 2, LO-FI, ECHOES, CRACKLE, DOWNPOUR, SIROCCO
    // The SIGNATURE slot (0.58.0): the fourth place shows one of ECHOES, CRACKLE, DOWNPOUR, SIROCCO (signatureNames).
    void showSignature (int signature);
    int getSignatureShown() const noexcept { return signatureShown; }
    // A preset loaded (its key changed): the slot shows the signature effect it has on (if the one shown is off).
    void followSignature (const juce::String& rigKey);
    SiroccoPage& getSiroccoPage() noexcept { return siroccoPage; }
    InfernalPage& getInfernalPage() noexcept { return infernalPage; }
    AfterglowPage& getAfterglowPage() noexcept { return afterglowPage; }
    LightTrailPage& getLightTrailPage() noexcept { return lightTrailPage; }
    BlackHolePage& getBlackHolePage() noexcept { return blackHolePage; }
    DilationPage& getDilationPage() noexcept { return dilationPage; }
    RemnantPage& getRemnantPage() noexcept { return remnantPage; }
    ProximityPage& getProximityPage() noexcept { return proximityPage; }
    HazePage& getHazePage() noexcept { return hazePage; }
    SpectrePage& getSpectrePage() noexcept { return spectrePage; }
    LeviathanPage& getLeviathanPage() noexcept { return leviathanPage; }
    NuclearSpringPage& getNuclearSpringPage() noexcept { return nuclearSpringPage; }
    HyperJumpPage& getHyperJumpPage() noexcept { return hyperJumpPage; }
    NaviComputerPage& getNaviComputerPage() noexcept { return naviComputerPage; }
    SweepPage& getSweepPage() noexcept { return sweepPage; }
    RedlinePage& getRedlinePage() noexcept { return redlinePage; }
    FreezeTagPage& getFreezeTagPage() noexcept { return freezeTagPage; }
    BloomPage& getBloomPage() noexcept { return bloomPage; }
    LegionPage& getLegionPage() noexcept { return legionPage; }
    OverchargePage& getOverchargePage() noexcept { return overchargePage; }
    AftermathPage& getAftermathPage() noexcept { return aftermathPage; }
    SubmergePage& getSubmergePage() noexcept { return submergePage; }
    LockstepPage& getLockstepPage() noexcept { return lockstepPage; }
    ColossusPage& getColossusPage() noexcept { return colossusPage; }
    CanyonPage& getCanyonPage() noexcept { return canyonPage; }
    JackInPage& getJackInPage() noexcept { return jackInPage; }
    EchoesPage& getEchoesPage() noexcept { return echoesPage; }
    CracklePage& getCracklePage() noexcept { return cracklePage; }
    DownpourPage& getDownpourPage() noexcept { return downpourPage; }
    ModFxPage& getModFxPage (int index) noexcept { return *modFxPages[(size_t) index]; }
    juce::TextButton& getModFxTab() noexcept { return modFxTab; }
    juce::TextButton& getInsertsTab() noexcept { return insertsTab; }
    InsertBusStrip& getInsertStrip() noexcept { return insertStrip; }
    RackUnit& getInsertUnit (int bus, int index) noexcept { return *insertUnits[(size_t) bus][(size_t) index]; }
    InsertEffectPage& getInsertEffectPage (int bus, int index) noexcept { return *insertEffectPages[(size_t) bus][(size_t) index]; }   // DRIVE, GLITCH, WAH
    ModFxPage& getInsertModFxPage (int bus, int index) noexcept { return *insertModFxPages[(size_t) bus][(size_t) index]; }       // MOD, LO-FI
    juce::TextButton& getSendsTab() noexcept { return sendsTab; }
    juce::TextButton& getMasterTab() noexcept { return masterTab; }
    EqPage& getEqPage() noexcept { return eqPage; }
    DominionPage& getDominionPage() noexcept { return dominionPage; }
    CompPage& getCompPage() noexcept { return compPage; }
    LimiterPage& getLimiterPage() noexcept { return limiterPage; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ParagonX1AudioProcessor& audio;   // (declared first: set before the pages are built)
    View view = View::rack;
    std::array<std::unique_ptr<RackUnit>, 4> units;
    std::array<std::unique_ptr<RackUnit>, 2> sendUnits;
    std::array<std::unique_ptr<RackUnit>, 32> modFxUnits;   // MOD, MOD 2, LO-FI, then the SIGNATURE slot's four
    int signatureShown = 0;
    juce::String followedRig;
    void chooseSignature();
    std::array<std::unique_ptr<ModFxPage>, 3> modFxPages;
    juce::TextButton insertsTab { "INSERTS" }, sendsTab { "SENDS" }, modFxTab { "MOD / LO-FI" }, masterTab { "MASTER" };
    InsertBusStrip insertStrip;
    std::array<std::array<std::unique_ptr<RackUnit>, 5>, ParamIDs::numInsertBuses> insertUnits;   // DRIVE, GLITCH, MOD, LO-FI, WAH (drawn first)
    std::array<std::array<std::unique_ptr<InsertEffectPage>, 3>, ParamIDs::numInsertBuses> insertEffectPages;   // DRIVE, GLITCH, WAH
    std::array<std::array<std::unique_ptr<ModFxPage>, 2>, ParamIDs::numInsertBuses> insertModFxPages;
    SendMatrix matrix;
    SendPage delayPage, reverbPage;
    EchoesPage echoesPage;
    CracklePage cracklePage;
    DownpourPage downpourPage;
    SiroccoPage siroccoPage;
    InfernalPage infernalPage;
    AfterglowPage afterglowPage;
    LightTrailPage lightTrailPage;
    BlackHolePage blackHolePage;
    DilationPage dilationPage;
    RemnantPage remnantPage;
    ProximityPage proximityPage;
    HazePage hazePage;
    SpectrePage spectrePage;
    LeviathanPage leviathanPage;
    NuclearSpringPage nuclearSpringPage;
    HyperJumpPage hyperJumpPage;     // 0.86.0 (LIGHTSPEED's place)
    BloomPage bloomPage;
    LegionPage legionPage;   // 0.76.0
    OverchargePage overchargePage;   // 0.77.0
    AftermathPage aftermathPage;     // 0.79.0
    SubmergePage submergePage;       // 0.80.0
    LockstepPage lockstepPage;       // 0.81.0
    ColossusPage colossusPage;       // 0.82.0
    CanyonPage canyonPage;           // 0.84.0
    JackInPage jackInPage;           // 0.85.0
    NaviComputerPage naviComputerPage;   // 0.86.0
    SweepPage sweepPage;                 // 0.87.0
    RedlinePage redlinePage;             // 0.88.0
    FreezeTagPage freezeTagPage;         // 0.89.0
    EqPage eqPage;
    DominionPage dominionPage;
    CompPage compPage;
    LimiterPage limiterPage;
};
