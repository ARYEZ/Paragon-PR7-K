#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

class ParagonX1AudioProcessor;

// THE GRID (0.98.0, the owner's design, mock-up D): the front panel's lower half, in PR7-D's "Tron" look. One glass
// screen: a dark red grid floor running to a glowing horizon, and the preset's two envelopes standing on it as walls
// of light, the FILTER envelope pink on the left, the AMP envelope orange on the right, each in its real shape
// (each stage as wide as the square root of its time, as the rack's envelope displays; sustain a fixed plateau).
// Readouts, not controls: the stages' values over each wall, the play mode and voices on the right (the controls
// are in the rack: SHOW RACK). While a note sounds, a spark runs along each wall where that envelope is now (the
// newest note's, from the engine: Engine::getAmpSpark), with a short trail; on the sustain it waits mid-plateau,
// breathing; let go, it slides down the release.
class EnvelopeGrid final : public juce::Component,
                           private juce::Timer
{
public:
    explicit EnvelopeGrid (ParagonX1AudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

    // An envelope's eight values (attack, hold, decay ms, sustain 0-1, release ms, the three curves) laid out as a
    // wall standing on `base` in `area` (area's bottom = the base).
    struct Wall
    {
        juce::Rectangle<float> area;
        std::array<float, 8> values {};
        // The x where each stage starts: attack, hold, decay, the sustain plateau, release, and the end.
        std::array<float, 6> xs {};
        static Wall make (juce::Rectangle<float> area, const std::array<float, 8>& values);
        float yFor (float level) const noexcept { return area.getBottom() - level * area.getHeight(); }
        juce::Path outline() const;   // the wall's top edge, from the start to the end
        // Where the spark goes for a stage (Envelope::Stage by number: 1 attack ... 5 release) and progress 0-1;
        // `breath` (0-1) moves it a little along the sustain plateau.
        juce::Point<float> pointAt (int stage, float progress, float breath = 0.5f) const;
    };

    // For tests: the walls as drawn now, and the sparks' places (none: a negative x).
    const Wall& getWall (int which) const { return walls[(size_t) which]; }
    juce::Point<float> getSpark (int which) const { return sparks[(size_t) which].now; }
    void updateForTest() { timerCallback(); }

private:
    void timerCallback() override;
    std::array<float, 8> envelopeValues (int which) const;
    juce::String readout (int which) const;
    juce::String playText (bool secondLine) const;
    void layoutWalls();
    void renderBackground (float scale);
    void paintSpark (juce::Graphics&, int which) const;

    ParagonX1AudioProcessor& processor;
    std::array<Wall, 2> walls;   // 0 filter, 1 amp
    juce::Rectangle<float> screen, floorArea;
    float horizon = 0.0f;

    struct Spark
    {
        juce::Point<float> now { -1.0f, -1.0f };
        std::array<juce::Point<float>, 14> trail {};   // the last places, newest first
        int trailLength = 0;
        float fade = 0.0f;   // 1 while sounding, falling to 0 after
        juce::Rectangle<float> drawnArea;
    };
    std::array<Spark, 2> sparks;
    float breathPhase = 0.0f;

    // The still picture (glass, floor, walls, readouts), redrawn only when what it shows changes.
    juce::Image background;
    float backgroundScale = 0.0f;
    std::vector<float> shownValues;
    juce::StringArray shownTexts;
    bool filterOn = true;
};
