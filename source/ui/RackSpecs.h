#pragma once

#include "PluginProcessor.h"
#include "ui/RackUnit.h"

// The four rack units' specs (0.18.0): ECHELON, DOMINION, CONSTRICTOR and
// the LIMITER, in the signal chain's order: their keys, knobs, ladders and
// what their screens show.
struct RackActions
{
    std::function<void (int unit)> openPage;      // 0 ECHELON ... 3 LIMITER
    std::function<void()> openMatch;              // ECHELON's page with MATCH open
    std::function<bool()> analyserOn;             // ECHELON's analyser
    std::function<void (bool)> setAnalyser;
};

std::array<RackUnit::Spec, 4> makeRackSpecs (ParagonX1AudioProcessor&, RackActions);

// The SENDS' two rack units (stage 7b): DELAY and REVERB. `openPage` 0 opens DELAY's page, 1 REVERB's.
std::array<RackUnit::Spec, 2> makeSendSpecs (ParagonX1AudioProcessor&, std::function<void (int send)> openPage);

// MOD, MOD 2 and LO-FI's rack units (stage 7c). `openPage` 0 opens MOD's page, 1 MOD 2's, 2 LO-FI's.
// MOD, MOD 2, LO-FI, then the SIGNATURE slot's effects (0.58.0: ECHOES, CRACKLE, DOWNPOUR, SIROCCO; 0.59.0 INFERNAL: one shown at a time
// in the fourth place, picked on the small screen under EDIT, which calls `chooseSignature`).
std::array<RackUnit::Spec, 35> makeModFxSpecs (ParagonX1AudioProcessor&, std::function<void (int unit)> openPage, std::function<void()> chooseSignature);
// The SIGNATURE slot's effects, in its order (never reorder: the page's choice follows it), and whether each is on.
const juce::StringArray& signatureNames();
bool signatureOn (ParagonX1AudioProcessor&, int signature);

// An insert bus's (0 to 3: A to D) four rack units (stage 7d): DRIVE, GLITCH, MOD, LO-FI. `openPage` 0 to 3 opens each one's page.
std::array<RackUnit::Spec, 5> makeInsertSpecs (ParagonX1AudioProcessor&, int bus, std::function<void (int unit)> openPage);
