#include "RackSpecs.h"

#include "ui/DownpourPage.h"

#include "dsp/effects/Wah.h"
#include "dsp/effects/Echoes.h"

#include "EqDisplay.h"
#include "LedDisplay.h"
#include "Parameters.h"
#include "ParagonLookAndFeel.h"
#include "dsp/eq/EqDesign.h"
#include "dsp/effects/Delay.h"
#include "dsp/effects/Drive.h"
#include "dsp/effects/Glitch.h"
#include "dsp/effects/LoFi.h"
#include "dsp/effects/Modulation.h"
#include "dsp/effects/Reverb.h"

namespace
{
    const auto orange = LedDisplay::ledOrange;

    juce::RangedAudioParameter& parameterOf (ParagonX1AudioProcessor& p, const juce::String& id)
    {
        return *p.getParameters().getParameter (id);
    }

    float valueOf (ParagonX1AudioProcessor& p, const juce::String& id)
    {
        return p.getParameters().getRawParameterValue (id)->load();
    }

    juce::String textOf (ParagonX1AudioProcessor& p, const juce::String& id)
    {
        return parameterOf (p, id).getCurrentValueAsText();
    }

    // Sets a parameter as one gesture (one undo step).
    void setValue (ParagonX1AudioProcessor& p, const juce::String& id, float plain)
    {
        auto& parameter = parameterOf (p, id);
        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost (parameter.convertTo0to1 (plain));
        parameter.endChangeGesture();
    }

    void toggle (ParagonX1AudioProcessor& p, const juce::String& id)
    {
        setValue (p, id, valueOf (p, id) > 0.5f ? 0.0f : 1.0f);
    }

    // A number in the screen's seven-segment digits ("-4.2", "5.0"), `h`
    // high, from `topLeft`; returns where it ends.
    float drawNumber (juce::Graphics& g, const juce::String& text, juce::Point<float> topLeft, float h)
    {
        auto x = topLeft.x;
        const auto w = h * 0.62f;
        for (auto c : text)
        {
            if (c == '.')
            {
                g.setColour (orange);
                g.fillRect (juce::Rectangle<float> (h * 0.12f, h * 0.12f).withPosition (x - h * 0.02f, topLeft.y + h * 0.86f));
                x += h * 0.16f;
                continue;
            }
            LedDisplay::drawDigit (g, c, { x, topLeft.y, w, h }, orange);
            x += w + h * 0.08f;
        }
        return x;
    }

    void line (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float size)
    {
        g.setColour (orange);
        g.setFont (juce::FontOptions (size));
        g.drawText (text, area, juce::Justification::centredLeft, false);
    }

    // The right-hand column of a readout: lines of text from the top.
    void column (juce::Graphics& g, juce::Rectangle<float> area, const juce::StringArray& lines, float size)
    {
        for (const auto& text : lines)
            line (g, text, area.removeFromTop (size * 1.55f), size);
    }

    // A bar across the bottom of a readout, filled in the menus' gradient to `fraction`.
    void bar (juce::Graphics& g, juce::Rectangle<float> area, float fraction)
    {
        g.setColour (orange.withAlpha (0.15f));
        g.fillRect (area);
        const auto filled = area.withWidth (area.getWidth() * juce::jlimit (0.0f, 1.0f, fraction));
        g.setGradientFill (ParagonLookAndFeel::menuHighlight (area));
        g.fillRect (filled);
    }

    float xForFrequency (juce::Rectangle<float> area, double hz)
    {
        return area.getX() + (float) (std::log (juce::jlimit (20.0, 20000.0, hz) / 20.0) / std::log (1000.0)) * area.getWidth();
    }
}

std::array<RackUnit::Spec, 4> makeRackSpecs (ParagonX1AudioProcessor& p, RackActions actions)
{
    using namespace ParamIDs;
    std::array<RackUnit::Spec, 4> specs;

    //==========================================================================
    // ECHELON
    {
        auto& s = specs[0];
        s.number = 1;
        s.name = "ECHELON";
        s.kind = "PARAMETRIC EQUALIZER";
        s.series = "PR7-K SERIES  24 BANDS";
        s.onParameterIds = { eqOn };
        s.powerTooltip = "ECHELON on or off. Click POWER (the word) or EDIT for its page";
        s.keys[0] = { "AUTO GAIN", "Keeps the level the same as you EQ, so louder doesn't fool you",
                      [&p] { return valueOf (p, eqAutoGain) > 0.5f; }, [&p] { toggle (p, eqAutoGain); } };
        s.keys[1] = { "ANALYSER", "The spectrum in and out, on ECHELON's page",
                      [actions] { return actions.analyserOn && actions.analyserOn(); },
                      [actions] { if (actions.setAnalyser) actions.setAnalyser (! actions.analyserOn()); } };
        s.keys[2] = { "MATCH", "Opens MATCH on ECHELON's page: make your sound's tone like a reference's",
                      [] { return false; }, [actions] { if (actions.openMatch) actions.openMatch(); } };
        s.knobs = { RackUnit::Knob { eqOutput, "OUTPUT" }, RackUnit::Knob { eqGainScale, "GAIN SCALE" } };
        s.ladderTitle = "DYN";
        s.ladderMarks = { 12.0f, 6.0f, 3.0f, 1.0f, 0.5f };
        s.ladderLabels = { "12", "6", "3", "1", ".5" };
        s.ladderReading = [&p]
        {
            // How far the dynamic bands are moving now.
            const auto settings = p.getEqSettings();
            float most = -100.0f;
            for (int band = 0; band < eq::ParametricEq::numBands; ++band)
                if (settings.bands[(size_t) band].isActive() && settings.bands[(size_t) band].isDynamic())
                    most = juce::jmax (most, std::abs (p.getEqDynamics (band).changeDb));
            return most;
        };
        s.paintReadout = [&p] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.035f, glass.getHeight() * 0.12f);
            auto graph = content.removeFromLeft (content.getWidth() * 0.62f);
            const auto settings = EqBands (p).live();
            const auto rate = p.getSampleRate() > 0.0 ? p.getSampleRate() : 48000.0;
            std::array<eq::Design, eq::ParametricEq::numBands> designs;
            for (size_t i = 0; i < designs.size(); ++i)
                designs[i] = eq::design (settings.bands[i], rate, settings.gainScale);
            const auto yFor = [&graph] (double db) { return graph.getCentreY() - (float) juce::jlimit (-13.0, 13.0, db) / 12.0f * graph.getHeight() * 0.46f; };

            g.setColour (orange.withAlpha (0.14f));
            g.drawHorizontalLine (juce::roundToInt (graph.getCentreY()), graph.getX(), graph.getRight());
            for (const auto hz : { 100.0, 1000.0, 10000.0 })
                g.drawVerticalLine (juce::roundToInt (xForFrequency (graph, hz)), graph.getY(), graph.getBottom());

            juce::Path curve;
            for (auto x = graph.getX(); x <= graph.getRight(); x += 1.5f)
            {
                const auto hz = 20.0 * std::pow (1000.0, (double) ((x - graph.getX()) / graph.getWidth()));
                double db = 0.0;
                for (size_t i = 0; i < designs.size(); ++i)
                    if (settings.bands[i].isActive() && settings.bands[i].placement == eq::Placement::stereo)
                        db += eq::responseDb (designs[i], hz, rate);
                if (x <= graph.getX())
                    curve.startNewSubPath (x, yFor (db));
                else
                    curve.lineTo (x, yFor (db));
            }
            g.setColour (juce::Colours::white.withAlpha (settings.on ? 0.95f : 0.4f));
            g.strokePath (curve, juce::PathStrokeType (1.6f));
            int count = 0;
            for (const auto& b : settings.bands)
                if (b.used)
                {
                    ++count;
                    g.setColour (b.on ? EqBands::colourFor (b) : orange.withAlpha (0.3f));
                    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ xForFrequency (graph, b.frequency),
                                                                                   yFor (eq::usesGain (b.shape) ? b.gainDb : 0.0) }));
                }

            content.removeFromLeft (content.getWidth() * 0.08f);
            const auto h = content.getHeight() * 0.36f;
            const auto end = drawNumber (g, juce::String (count), content.getTopLeft(), h);
            line (g, "BANDS", { end + 4.0f, content.getY() + h * 0.45f, content.getRight() - end, h * 0.55f }, h * 0.32f);
            column (g, content.withTrimmedTop (h + h * 0.25f),
                    { "OUT " + textOf (p, eqOutput), "SCALE " + textOf (p, eqGainScale) }, h * 0.33f);
        };
        s.footTitle = "AUTO GAIN";
        s.footText = [&p]
        {
            if (valueOf (p, eqAutoGain) < 0.5f)
                return juce::String ("OFF");
            const auto db = p.getEqAutoGainDb();
            return (db > 0.05 ? "+" : "") + juce::String (db, 1) + " dB";
        };
        s.onEdit = [actions] { actions.openPage (0); };
    }

    //==========================================================================
    // DOMINION
    {
        auto& s = specs[1];
        s.number = 2;
        s.name = "DOMINION";
        s.kind = "RESONANCE SUPPRESSOR";
        s.series = "PR7-K SERIES  SOFT / HARD";
        s.onParameterIds = { domOn };
        s.powerTooltip = "DOMINION on or off. Click POWER (the word) or EDIT for its page";
        s.keys[0] = { "HARD", "HARD: turns down what's loud, at any frequency; off (SOFT): what sticks out of the sound around it",
                      [&p] { return valueOf (p, domMode) > 0.5f; }, [&p] { toggle (p, domMode); } };
        s.keys[1] = { "M/S", "Works on the middle and sides (lit) or on left and right",
                      [&p] { return valueOf (p, domStereo) > 0.5f; }, [&p] { toggle (p, domStereo); } };
        s.keys[2] = { "DELTA", "Hear only what it takes away",
                      [&p] { return valueOf (p, domDelta) > 0.5f; }, [&p] { toggle (p, domDelta); } };
        s.knobs = { RackUnit::Knob { domDepth, "DEPTH" }, RackUnit::Knob { domDetail, "DETAIL" } };
        s.ladderTitle = "CUT";
        s.ladderMarks = { 12.0f, 6.0f, 3.0f, 1.0f, 0.5f };
        s.ladderLabels = { "12", "6", "3", "1", ".5" };
        const auto deepest = [&p]
        {
            const auto& d = p.getDominion();
            float most = 0.0f;
            for (int f = 0; f < d.getNumFilters(); ++f)
                most = juce::jmax (most, d.getReductionDb (0, f), d.getReductionDb (1, f));
            return most;
        };
        s.ladderReading = [&p, deepest] { return valueOf (p, domOn) > 0.5f ? deepest() : -100.0f; };
        s.paintReadout = [&p, deepest] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.035f, glass.getHeight() * 0.12f);
            auto graph = content.removeFromLeft (content.getWidth() * 0.62f);
            const auto& d = p.getDominion();
            const auto top = graph.getY() + graph.getHeight() * 0.25f;
            const auto depthFor = [&] (float db) { return top + juce::jlimit (0.0f, 12.0f, db) / 12.0f * (graph.getBottom() - top); };
            juce::Path cut;
            cut.startNewSubPath (graph.getX(), top);
            for (int f = 0; f < d.getNumFilters(); ++f)
                cut.lineTo (xForFrequency (graph, d.getFrequency (f)), depthFor (juce::jmax (d.getReductionDb (0, f), d.getReductionDb (1, f))));
            cut.lineTo (graph.getRight(), top);
            cut.closeSubPath();
            g.setGradientFill (ParagonLookAndFeel::menuHighlight (graph));
            g.fillPath (cut);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawHorizontalLine (juce::roundToInt (top), graph.getX(), graph.getRight());

            content.removeFromLeft (content.getWidth() * 0.08f);
            const auto h = content.getHeight() * 0.36f;
            drawNumber (g, juce::String (valueOf (p, domDepth), 1), content.getTopLeft(), h);
            column (g, content.withTrimmedTop (h + h * 0.25f),
                    { "DEPTH   " + textOf (p, domMode).toUpperCase() + " MODE", "CUT -" + juce::String (deepest(), 1) + " dB" }, h * 0.33f);
        };
        s.footTitle = "PRESET";
        s.footText = [&p] { return p.getDominionPresetName() + (p.isDominionPresetModified() ? " *" : ""); };   // * as on its page: changed since
        s.onEdit = [actions] { actions.openPage (1); };
    }

    //==========================================================================
    // CONSTRICTOR
    {
        auto& s = specs[2];
        s.number = 3;
        s.name = "CONSTRICTOR";
        s.kind = "COMPRESSOR  6 BANDS";
        s.series = "PR7-K SERIES  VCA OPTO FET";
        s.onParameterIds = { compOn };
        s.powerTooltip = "CONSTRICTOR on or off. Click POWER (the word) or EDIT for its page";
        const auto character = [&p] (int which) { return [&p, which] { return juce::roundToInt (valueOf (p, compCharacter)) == which; }; };
        const auto choose = [&p] (int which) { return [&p, which] { setValue (p, compCharacter, (float) which); }; };
        s.keys[0] = { "VCA", "The full band's character: clean and precise", character (0), choose (0) };
        s.keys[1] = { "OPTO", "The full band's character: slower and smooth", character (1), choose (1) };
        s.keys[2] = { "FET", "The full band's character: fast, with a little grit", character (2), choose (2) };
        s.knobs = { RackUnit::Knob { compThreshold, "THRESHOLD" }, RackUnit::Knob { compRatio, "RATIO" } };
        s.ladderTitle = "GR";
        s.ladderMarks = { 20.0f, 10.0f, 6.0f, 3.0f, 1.0f };
        s.ladderLabels = { "20", "10", "6", "3", "1" };
        const auto reduction = [&p]
        {
            // The full band's gain reduction, or the deepest a band is turning its part down.
            auto most = juce::jmax (p.getCompressorReductionDb (0), p.getCompressorReductionDb (1));
            const auto bands = p.getMultibandSettings();
            for (int band = 0; band < effects::multiband::maxBands; ++band)
                if (bands.bands[(size_t) band].used)
                    most = juce::jmax (most, -p.getMultibandReading (band).changeDb);
            return most;
        };
        s.ladderReading = [&p, reduction] { return valueOf (p, compOn) > 0.5f ? reduction() : -100.0f; };
        s.paintReadout = [&p, reduction] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.55f);
            const auto gr = valueOf (p, compOn) > 0.5f ? reduction() : 0.0f;
            const auto h = left.getHeight() * 0.56f;
            const auto end = drawNumber (g, "-" + juce::String (gr, 1), left.getTopLeft(), h);
            line (g, "dB GR", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            bar (g, left.withTop (left.getBottom() - left.getHeight() * 0.14f).withTrimmedRight (left.getWidth() * 0.08f), gr / 12.0f);

            int bands = 0;
            for (const auto& b : p.getMultibandSettings().bands)
                bands += b.used ? 1 : 0;
            column (g, content, { "THRESH " + textOf (p, compThreshold), "RATIO " + textOf (p, compRatio),
                                  textOf (p, compCharacter).toUpperCase() + "  " + juce::String (bands) + (bands == 1 ? " BAND" : " BANDS"),
                                  juce::String ("LOOK-AHEAD ") + (valueOf (p, mbLookahead) > 0.5f ? "ON" : "OFF") },
                    content.getHeight() * 0.16f);
        };
        s.footTitle = "PRESET";
        s.footText = [&p] { return p.getCompPresetName() + (p.isCompPresetModified() ? " *" : ""); };
        s.onEdit = [actions] { actions.openPage (2); };
    }

    //==========================================================================
    // LIMITER
    {
        auto& s = specs[3];
        s.number = 4;
        s.name = "LIMITER";
        s.kind = "TRUE PEAK LIMITER";
        s.series = "PR7-K SERIES  LOUDNESS";
        s.onParameterIds = { limitOn };
        s.powerTooltip = "The LIMITER on or off. Click POWER (the word) or EDIT for its page";
        s.keys[0] = { "TRUE PEAK", "Keeps the peaks between the samples under the ceiling too",
                      [&p] { return valueOf (p, limitTruePeak) > 0.5f; }, [&p] { toggle (p, limitTruePeak); } };
        s.keys[1] = { "UNITY", "Hear it at the level it came in",
                      [&p] { return valueOf (p, limitUnity) > 0.5f; }, [&p] { toggle (p, limitUnity); } };
        s.keys[2] = { "DELTA", "Hear only what it takes away",
                      [&p] { return valueOf (p, limitDelta) > 0.5f; }, [&p] { toggle (p, limitDelta); } };
        s.knobs = { RackUnit::Knob { limitGain, "GAIN" }, RackUnit::Knob { limitCeiling, "OUTPUT" } };
        s.ladderTitle = "OUT";
        s.ladderMarks = { 0.0f, -3.0f, -6.0f, -12.0f, -24.0f };
        s.ladderLabels = { "0", "-3", "-6", "-12", "-24" };
        s.ladderReading = [&p] { return valueOf (p, limitOn) > 0.5f ? p.getLimiter().getOutputPeakDb() : -100.0f; };
        auto heldPeak = std::make_shared<std::pair<float, double>> (-100.0f, 0.0);   // the true peak shown, and when it was set
        s.paintReadout = [&p, heldPeak] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.12f);
            auto& limiter = p.getLimiter();
            const auto on = valueOf (p, limitOn) > 0.5f;
            // The true peak, held for two seconds so it can be read.
            const auto now = juce::Time::getMillisecondCounterHiRes();
            if (const auto peak = on ? limiter.getOutputTruePeakDb() : -100.0f; peak >= heldPeak->first || now - heldPeak->second > 2000.0)
                *heldPeak = { peak, now };
            const auto integrated = limiter.getLoudness().getIntegrated();

            const auto h = content.getHeight() * 0.4f;
            const auto label = content.getHeight() * 0.13f;
            auto first = content.removeFromLeft (content.getWidth() * 0.36f);
            line (g, "TRUE PEAK", first.withHeight (label * 1.4f), label);
            drawNumber (g, heldPeak->first > -99.0f ? juce::String (heldPeak->first, 1) : juce::String ("--"), first.getTopLeft().translated (0.0f, label * 1.5f), h);
            auto second = content.removeFromLeft (content.getWidth() * 0.6f);
            line (g, "INTEGRATED", second.withHeight (label * 1.4f), label);
            drawNumber (g, integrated > effects::LoudnessMeter::silent + 1.0f ? juce::String (integrated, 1) : juce::String ("--"),
                        second.getTopLeft().translated (0.0f, label * 1.5f), h);
            column (g, content, { textOf (p, limitMode).toUpperCase(), "LOOK " + textOf (p, limitLookahead), "OVERSAMPLE " + textOf (p, limitOversampling).toUpperCase() },
                    label);
            const auto reduction = on ? juce::jmax (limiter.getReductionDb (0), limiter.getReductionDb (1)) : 0.0f;
            bar (g, glass.reduced (glass.getWidth() * 0.045f, 0.0f).withTop (glass.getBottom() - glass.getHeight() * 0.18f).withHeight (glass.getHeight() * 0.06f),
                 reduction / 12.0f);
        };
        s.footTitle = "STYLE";
        s.footText = [&p] { return textOf (p, limitMode); };
        s.onEdit = [actions] { actions.openPage (3); };
    }
    return specs;
}

//==============================================================================
std::array<RackUnit::Spec, 2> makeSendSpecs (ParagonX1AudioProcessor& p, std::function<void (int)> openPage)
{
    using namespace ParamIDs;
    std::array<RackUnit::Spec, 2> specs;

    // DELAY (PR7-G's): the echo time in big digits, its type, feedback and PING-PONG.
    {
        auto& s = specs[0];
        s.number = 1;
        s.name = "DELAY";
        s.kind = "STEREO DELAY  8 TYPES";
        s.series = "PR7-K SERIES  SEND 1";
        s.onParameterIds = { delayOn };
        s.powerTooltip = "The DELAY on or off (each oscillator's DELAY send feeds it). Click POWER (the word) or EDIT for its page";
        auto lastSync = std::make_shared<float> (3.0f);   // SYNC's note when it's switched back on (1/4 to start)
        s.keys[0] = { "PING-PONG", "The echoes bounce left, right, left...",
                      [&p] { return valueOf (p, delayPingPong) > 0.5f; }, [&p] { toggle (p, delayPingPong); } };
        s.keys[1] = { "SYNC", "The echo time in notes at the song's tempo (lit), or in milliseconds",
                      [&p] { return valueOf (p, delaySync) > 0.5f; },
                      [&p, lastSync]
                      {
                          if (const auto now = valueOf (p, delaySync); now > 0.5f)
                          {
                              *lastSync = now;
                              setValue (p, delaySync, 0.0f);
                          }
                          else
                              setValue (p, delaySync, *lastSync);
                      } };
        s.keys[2] = { "DUCK", "Tucks the echoes under what you play, swelling back in the gaps (half way; set it on the page)",
                      [&p] { return valueOf (p, delayDuck) > 0.01f; }, [&p] { setValue (p, delayDuck, valueOf (p, delayDuck) > 0.01f ? 0.0f : 0.5f); } };
        s.knobs = { RackUnit::Knob { delayFeedback, "FEEDBACK" }, RackUnit::Knob { delayMix, "MIX" } };
        s.ladderTitle = "RET";
        s.ladderMarks = { -3.0f, -9.0f, -18.0f, -30.0f, -48.0f };
        s.ladderLabels = { "-3", "-9", "-18", "-30", "-48" };
        s.ladderReading = [&p] { return p.getReturnPeakDb (0); };
        s.paintReadout = [&p] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.55f);
            const auto synced = valueOf (p, delaySync) > 0.5f;
            const auto ms = effects::Delay::delaySeconds (p.getDelaySettings(), juce::jmax (1.0, p.getHostBpm())) * 1000.0f;
            const auto h = left.getHeight() * 0.56f;
            const auto end = drawNumber (g, juce::String (juce::roundToInt (ms)), left.getTopLeft(), h);
            line (g, "ms", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, synced ? textOf (p, delaySync) + " AT " + juce::String (p.getHostBpm(), 0) + " BPM" : juce::String ("FREE"),
                  left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            column (g, content, { textOf (p, delayType).toUpperCase(), "FEEDBACK " + textOf (p, delayFeedback),
                                  juce::String (valueOf (p, delayPingPong) > 0.5f ? "PING-PONG" : "STEREO"),
                                  "> REVERB " + textOf (p, delayToReverb) },
                    content.getHeight() * 0.16f);
        };
        s.footTitle = "TYPE";
        s.footText = [&p] { return textOf (p, delayType); };
        s.onEdit = [openPage] { openPage (0); };
    }

    // REVERB (PR7-G's): how long its tail lasts in big digits, its type, size and pre-delay.
    {
        auto& s = specs[1];
        s.number = 2;
        s.name = "REVERB";
        s.kind = "STEREO REVERB  8 TYPES";
        s.series = "PR7-K SERIES  SEND 2";
        s.onParameterIds = { reverbOn };
        s.powerTooltip = "The REVERB on or off (each oscillator's REVERB send feeds it). Click POWER (the word) or EDIT for its page";
        const auto typeKey = [&p] (effects::Reverb::Type type, const char* tip)
        {
            return RackUnit::Key { effects::Reverb::typeNames[(int) type].toUpperCase(), tip,
                                   [&p, type] { return juce::roundToInt (valueOf (p, reverbType)) == (int) type; },
                                   [&p, type] { setValue (p, reverbType, (float) (int) type); } };
        };
        s.keys[0] = typeKey (effects::Reverb::Type::room, "A small room: close reflections, a short tail");
        s.keys[1] = typeKey (effects::Reverb::Type::hall, "A concert hall: a gap, then a long, smooth tail");
        s.keys[2] = typeKey (effects::Reverb::Type::plate, "A studio plate: dense and bright from the start");
        s.knobs = { RackUnit::Knob { reverbSize, "SIZE" }, RackUnit::Knob { reverbMix, "MIX" } };
        s.ladderTitle = "RET";
        s.ladderMarks = { -3.0f, -9.0f, -18.0f, -30.0f, -48.0f };
        s.ladderLabels = { "-3", "-9", "-18", "-30", "-48" };
        s.ladderReading = [&p] { return p.getReturnPeakDb (1); };
        s.paintReadout = [&p] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.55f);
            const auto settings = p.getReverbSettings();
            const auto seconds = effects::Reverb::decaySeconds (settings.type, settings.size);
            const auto h = left.getHeight() * 0.56f;
            const auto end = drawNumber (g, juce::String (seconds, seconds < 10.0f ? 1 : 0), left.getTopLeft(), h);
            line (g, "s", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, "TAIL TO -60 dB", left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            column (g, content, { textOf (p, reverbType).toUpperCase(), "SIZE " + textOf (p, reverbSize),
                                  "PRE-DELAY " + textOf (p, reverbPreDelay), "TONE " + textOf (p, reverbTone) },
                    content.getHeight() * 0.16f);
        };
        s.footTitle = "TYPE";
        s.footText = [&p] { return textOf (p, reverbType); };
        s.onEdit = [openPage] { openPage (1); };
    }
    return specs;
}

//==============================================================================
namespace
{
    // A MOD block's unit (stage 7c; a bus's MOD, 7d): the LFO's rate in big digits, the type's own settings beside
    // it, and the ladder following the sweep.
    RackUnit::Spec makeModSpec (ParagonX1AudioProcessor& p, const ParamIDs::ModIds& ids, int number, const juce::String& name,
                                const juce::String& series, const juce::String& powerTooltip,
                                std::function<effects::Modulation::Settings()> settingsNow, std::function<float()> phaseNow,
                                std::function<void()> onEdit)
    {
        using Type = effects::Modulation::Type;
        RackUnit::Spec s;
        s.number = number;
        s.name = name;
        s.kind = "STEREO MODULATION  12 TYPES";
        s.series = series;
        s.onParameterIds = { ids.on };
        s.powerTooltip = powerTooltip;
        const juce::String typeId (ids.type), syncId (ids.sync);
        const auto step = [&p, typeId] (int by)
        {
            const auto count = effects::Modulation::typeNames.size();
            setValue (p, typeId, (float) ((juce::roundToInt (valueOf (p, typeId)) + by + count) % count));
        };
        s.keys[0] = { "< TYPE", "The type before this one", [] { return false; }, [step] { step (-1); } };
        s.keys[1] = { "TYPE >", "The next type", [] { return false; }, [step] { step (1); } };
        auto lastSync = std::make_shared<float> (3.0f);   // SYNC's note when it's switched back on (1/4 to start)
        s.keys[2] = { "SYNC", "The LFO in notes at the song's tempo (lit), or at RATE",
                      [&p, syncId] { return valueOf (p, syncId) > 0.5f; },
                      [&p, syncId, lastSync]
                      {
                          if (const auto now = valueOf (p, syncId); now > 0.5f)
                          {
                              *lastSync = now;
                              setValue (p, syncId, 0.0f);
                          }
                          else
                              setValue (p, syncId, *lastSync);
                      } };
        s.knobs = { RackUnit::Knob { ids.rate, "RATE" }, RackUnit::Knob { ids.depth, "DEPTH" } };
        s.ladderTitle = "LFO";
        s.ladderMarks = { 0.8f, 0.4f, 0.0f, -0.4f, -0.8f };
        s.ladderLabels = { "+", "", "0", "", "-" };
        s.ladderReading = [settingsNow, phaseNow]
        {
            const auto settings = settingsNow();
            return settings.on ? effects::Modulation::sweepShape (settings.type, phaseNow(), settings) : -100.0f;
        };
        const ParamIDs::ModIds idsCopy = ids;
        s.paintReadout = [&p, idsCopy, settingsNow] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            const auto& ids = idsCopy;
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto settings = settingsNow();
            const auto h = left.getHeight() * 0.56f;
            const auto end = drawNumber (g, juce::String (settings.rateHz, settings.rateHz < 1.0f ? 2 : 1), left.getTopLeft(), h);
            line (g, "Hz", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, valueOf (p, ids.sync) > 0.5f ? textOf (p, ids.sync) + " AT " + juce::String (p.getHostBpm(), 0) + " BPM" : juce::String ("FREE"),
                  left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            juce::StringArray rows { textOf (p, ids.type).toUpperCase() };
            switch (settings.type)
            {
                case Type::chorus:   rows.add ("DEPTH " + textOf (p, ids.depth)); rows.add ("E.LEVEL " + textOf (p, ids.mix));
                                     rows.add ("EQ " + textOf (p, ids.eq).toUpperCase() + (settings.stereoOutputs ? "  STEREO" : "  MONO")); break;
                case Type::flanger:  rows.add ("DEPTH " + textOf (p, ids.depth)); rows.add ("MANUAL " + textOf (p, ids.manual));
                                     rows.add ("RES " + textOf (p, ids.res)); break;
                case Type::phaser:   rows.add ("CIRCUIT " + textOf (p, ids.hadron).toUpperCase()); break;
                case Type::tremolo:  rows.add ("DEPTH " + textOf (p, ids.depth)); rows.add ("WAVE " + textOf (p, ids.wave)); break;
                case Type::carbonite:
                case Type::vibrato:
                case Type::harmonicTremolo:
                case Type::autoPan:  rows.add ("DEPTH " + textOf (p, ids.depth)); break;
                case Type::dimension:
                case Type::uniVibe:
                case Type::rotary:
                case Type::doubler:  rows.add ("DEPTH " + textOf (p, ids.depth)); rows.add ("MIX " + textOf (p, ids.mix)); break;
            }
            column (g, content, rows, content.getHeight() * 0.16f);
        };
        s.footTitle = "TYPE";
        s.footText = [&p, typeId] { return textOf (p, typeId); };
        s.onEdit = std::move (onEdit);
        return s;
    }

    // A LO-FI's unit: AMOUNT in big digits, the band it lets through (or the bitcrusher's bits), a level on the
    // ladder. `id` gives its parameter IDs from the whole sound's names ("lofiOn"...).
    RackUnit::Spec makeLoFiSpec (ParagonX1AudioProcessor& p, std::function<juce::String (const char*)> id, int number,
                                 const juce::String& series, std::function<effects::LoFi::Settings()> settingsNow,
                                 juce::String ladderTitle, std::function<float()> ladderDb, std::function<void()> onEdit)
    {
        using LoFiType = effects::LoFi::Type;
        RackUnit::Spec s;
        s.number = number;
        s.name = "LO-FI";
        s.kind = "STEREO LO-FI  5 TYPES";
        s.series = series;
        s.onParameterIds = { id ("lofiOn") };
        s.powerTooltip = "LO-FI on or off: through a phone, a radio, a megaphone, a cassette or a bitcrusher. Click POWER (the word) or EDIT for its page";
        const auto typeId = id ("lofiType");
        const auto typeKey = [&p, typeId] (LoFiType type, const char* tip)
        {
            return RackUnit::Key { effects::LoFi::typeNames[(int) type].toUpperCase(), tip,
                                   [&p, typeId, type] { return juce::roundToInt (valueOf (p, typeId)) == (int) type; },
                                   [&p, typeId, type] { setValue (p, typeId, (float) (int) type); } };
        };
        s.keys[0] = typeKey (LoFiType::telephone, "A phone line: only the voice band gets through");
        s.keys[1] = typeKey (LoFiType::cassette, "A worn tape: wobbling pitch, a dull top end, saturation and hiss");
        s.keys[2] = typeKey (LoFiType::bitcrusher, "Fewer bits and a lower sample rate: early samplers and games");
        s.knobs = { RackUnit::Knob { id ("lofiAmount"), "AMOUNT" }, RackUnit::Knob { id ("lofiMix"), "MIX" } };
        s.ladderTitle = std::move (ladderTitle);
        s.ladderMarks = { -3.0f, -9.0f, -18.0f, -30.0f, -48.0f };
        s.ladderLabels = { "-3", "-9", "-18", "-30", "-48" };
        s.ladderReading = std::move (ladderDb);
        s.paintReadout = [&p, id, settingsNow] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto settings = settingsNow();
            const auto h = left.getHeight() * 0.56f;
            drawNumber (g, juce::String (settings.amount, 1), left.getTopLeft(), h);
            line (g, "AMOUNT", left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            juce::StringArray rows { textOf (p, id ("lofiType")).toUpperCase() };
            const auto sampleRate = p.getSampleRate() > 0.0 ? p.getSampleRate() : 48000.0;
            if (settings.type == LoFiType::bitcrusher)
                rows.add (juce::String (effects::LoFi::crusherBits (settings.amount), 1) + " BITS  1/"
                          + juce::String (effects::LoFi::crusherHold (settings.amount, sampleRate)) + " RATE");
            else
            {
                const auto band = effects::LoFi::bandFor (settings.type, settings.amount, sampleRate);
                const auto hz = [] (float f) { return f < 1000.0f ? juce::String (juce::roundToInt (f)) : juce::String (f / 1000.0f, 1) + "k"; };
                rows.add (hz (band.lowHz) + " - " + hz (band.highHz) + " Hz");
            }
            rows.add ("NOISE " + textOf (p, id ("lofiNoise")));
            rows.add ("MIX " + textOf (p, id ("lofiMix")) + "  " + textOf (p, id ("lofiLevel")));
            column (g, content, rows, content.getHeight() * 0.16f);
        };
        s.footTitle = "TYPE";
        s.footText = [&p, typeId] { return textOf (p, typeId); };
        s.onEdit = std::move (onEdit);
        return s;
    }

    // "lofiOn" -> "ins1LofiOn"...: a bus's name for one of the whole sound's settings ("drive..." and "glitch..." too).
    juce::String busId (int bus, const char* wholeName)
    {
        juce::String name (wholeName);
        return ParamIDs::ins (bus, (name.substring (0, 1).toUpperCase() + name.substring (1)).toRawUTF8());
    }
}

const juce::StringArray& signatureNames()
{
    static const juce::StringArray names { "ECHOES", "CRACKLE", "DOWNPOUR", "SIROCCO", "INFERNAL", "AFTERGLOW", "LIGHTTRAIL", "BLACKHOLE", "DILATION", "REMNANT", "PROXIMITY", "HAZE" };   // never reorder
    return names;
}

bool signatureOn (ParagonX1AudioProcessor& p, int signature)
{
    const char* ids[] = { ParamIDs::echoOn, ParamIDs::crackleOn, ParamIDs::downpourOn, ParamIDs::siroccoOn, ParamIDs::infernalOn, ParamIDs::afterglowOn, ParamIDs::trailOn, ParamIDs::blackholeOn, ParamIDs::dilationOn, ParamIDs::remnantOn, ParamIDs::proximityOn, ParamIDs::hazeOn };
    return signature >= 0 && signature < (int) std::size (ids) && valueOf (p, ids[signature]) > 0.5f;
}

std::array<RackUnit::Spec, 15> makeModFxSpecs (ParagonX1AudioProcessor& p, std::function<void (int)> openPage, std::function<void()> chooseSignature)
{
    std::array<RackUnit::Spec, 15> specs;
    for (int which = 0; which < 2; ++which)
        specs[(size_t) which] = makeModSpec (p, which == 0 ? ParamIDs::modIds : ParamIDs::mod2Ids, which + 1, which == 0 ? "MOD" : "MOD 2",
                                             "PR7-K SERIES  INSERT " + juce::String (which + 1),
                                             juce::String (which == 0 ? "MOD" : "MOD 2") + " on or off (on everything, after the sends come back"
                                                 + (which == 0 ? "" : " and after MOD") + "). Click POWER (the word) or EDIT for its page",
                                             [&p, which] { return p.getModSettings (which); },
                                             [&p, which] { return p.getModSweepPhase (which); },
                                             [openPage, which] { openPage (which); });
    specs[2] = makeLoFiSpec (p, [] (const char* name) { return juce::String (name); }, 3, "PR7-K SERIES  INSERT 3",
                             [&p] { return p.getLoFiSettings(); }, "OUT", [&p] { return p.getLoFiPeakDb(); }, [openPage] { openPage (2); });
    specs[2].powerTooltip = "LO-FI on or off: the whole sound, echoes and all, through a phone, a radio, a megaphone, a cassette or a bitcrusher. "
                            "Click POWER (the word) or EDIT for its page";

    // ECHOES (0.53.0): DISTANCE in big digits (beats), the mode and its settings beside it; the ladder what's coming back.
    {
        using Mode = effects::Echoes::Mode;
        auto& s = specs[3];
        s.number = 4;
        s.name = "ECHOES";
        s.kind = "THE PAST UNDER THE PRESENT  4 MODES";
        s.series = "PR7-K SERIES  INSERT 4";
        s.onParameterIds = { ParamIDs::echoOn };
        s.powerTooltip = "ECHOES on or off: what you played comes back from the past, in time with your song. Click POWER (the word) or EDIT for its page";
        // The keys pick REVERSE, TAPE or LOOP; the lit one again goes back to ECHO.
        const auto modeKey = [&p] (Mode mode, const char* tip)
        {
            return RackUnit::Key { effects::Echoes::modeNames[(int) mode].toUpperCase(), tip,
                                   [&p, mode] { return juce::roundToInt (valueOf (p, ParamIDs::echoMode)) == (int) mode; },
                                   [&p, mode]
                                   {
                                       const auto lit = juce::roundToInt (valueOf (p, ParamIDs::echoMode)) == (int) mode;
                                       setValue (p, ParamIDs::echoMode, (float) (int) (lit ? Mode::echo : mode));
                                   } };
        };
        s.keys[0] = modeKey (Mode::reverse, "REVERSE: each stretch comes back backwards while the next plays (again for ECHO)");
        s.keys[1] = modeKey (Mode::tape, "TAPE: each stretch comes back at half speed, an octave down (again for ECHO)");
        s.keys[2] = modeKey (Mode::loop, "LOOP: a stretch is caught and repeated while you play on (again for ECHO)");
        s.knobs = { RackUnit::Knob { ParamIDs::echoMix, "MIX" }, RackUnit::Knob { ParamIDs::echoFeedback, "FEEDBACK" } };
        s.ladderTitle = "PAST";
        s.ladderMarks = { -3.0f, -9.0f, -18.0f, -30.0f, -48.0f };
        s.ladderLabels = { "-3", "-9", "-18", "-30", "-48" };
        s.ladderReading = [&p] { return p.getEchoesPeakDb(); };
        s.paintReadout = [&p] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto settings = p.getEchoesSettings();
            const auto beats = effects::Echoes::distanceBeats[(size_t) juce::jlimit (0, (int) std::size (effects::Echoes::distanceBeats) - 1, settings.distance)];
            const auto h = left.getHeight() * 0.56f;
            const auto end = drawNumber (g, juce::String (juce::roundToInt (beats)), left.getTopLeft(), h);
            line (g, beats == 1.0f ? "BEAT" : "BEATS", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, "BACK AT " + juce::String (p.getHostBpm(), 0) + " BPM", left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            column (g, content, { textOf (p, ParamIDs::echoMode).toUpperCase(),
                                  (settings.mode == Mode::loop ? "REPEATS " : "FEEDBACK ") + textOf (p, ParamIDs::echoFeedback),
                                  "AGE " + textOf (p, ParamIDs::echoAge), "MIX " + textOf (p, ParamIDs::echoMix) },
                    content.getHeight() * 0.16f);
        };
        s.footTitle = "MODE";
        s.footText = [&p] { return textOf (p, ParamIDs::echoMode); };
        s.onEdit = [openPage] { openPage (3); };
    }

    // CRACKLE (0.56.0): the clicks counted in the last second in big digits, like a counter; the keys pick a density.
    {
        auto& s = specs[4];
        s.number = 5;
        s.name = "CRACKLE";
        s.kind = "RANDOM CLICKS  A COUNTER'S CRACKLE";
        s.series = "PR7-K SERIES  INSERT 5";
        s.onParameterIds = { ParamIDs::crackleOn };
        s.powerTooltip = "CRACKLE on or off: a radiation counter's random clicks, added to the sound. Click POWER (the word) or EDIT for its page";
        const auto rateKey = [&p] (const char* name, float rate, const char* tip)
        {
            return RackUnit::Key { name, tip,
                                   [&p, rate]
                                   {
                                       const auto now = valueOf (p, ParamIDs::crackleRate);
                                       return now > rate * 0.7f && now < rate * 1.4f;
                                   },
                                   [&p, rate] { setValue (p, ParamIDs::crackleRate, rate); } };
        };
        s.keys[0] = rateKey ("SPARSE", 4.0f, "SPARSE: about 4 clicks a second");
        s.keys[1] = rateKey ("STEADY", 40.0f, "STEADY: about 40 clicks a second");
        s.keys[2] = rateKey ("STORM", 600.0f, "STORM: about 600 clicks a second, a roar");
        s.knobs = { RackUnit::Knob { ParamIDs::crackleRate, "RATE" }, RackUnit::Knob { ParamIDs::crackleLevel, "LEVEL" } };
        // The ladder: how fast it's clicking (10 x log10 of the clicks a second: 1, 10, 30, 100, 1000).
        s.ladderTitle = "COUNT";
        s.ladderMarks = { 30.0f, 20.0f, 14.8f, 10.0f, 0.0f };
        s.ladderLabels = { "1K", "100", "30", "10", "1" };
        s.ladderReading = [&p]
        {
            const auto rate = p.getCrackle().getRecentRate();
            return rate > 0.0f ? 10.0f * std::log10 (rate) : -100.0f;
        };
        s.paintReadout = [&p] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto h = left.getHeight() * 0.56f;
            const auto end = drawNumber (g, juce::String (juce::roundToInt (p.getCrackle().getRecentRate())), left.getTopLeft(), h);
            line (g, "CPS", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, "CLICKS IN THE LAST SECOND", left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            column (g, content, { "RATE " + textOf (p, ParamIDs::crackleRate), "CLUSTER " + textOf (p, ParamIDs::crackleCluster),
                                  "TONE " + textOf (p, ParamIDs::crackleTone), "FOLLOW " + textOf (p, ParamIDs::crackleFollow) },
                    content.getHeight() * 0.16f);
        };
        s.footTitle = "LEVEL";
        s.footText = [&p] { return textOf (p, ParamIDs::crackleLevel); };
        s.onEdit = [openPage] { openPage (4); };
    }

    // DOWNPOUR (0.57.0): the drops a second in big digits, the last drop's note; the keys how hard it rains; the
    // ladder how thick the patter is.
    {
        auto& s = specs[5];
        s.number = 6;
        s.name = "DOWNPOUR";
        s.kind = "RAIN ON THE GLASS  DROPS ON YOUR NOTES";
        s.series = "PR7-K SERIES  INSERT 6";
        s.onParameterIds = { ParamIDs::downpourOn };
        s.powerTooltip = "DOWNPOUR on or off: rain on the glass, its drops ringing on the notes you play. Click POWER (the word) or EDIT for its page";
        const auto rainKey = [&p] (const char* name, float rain, const char* tip)
        {
            return RackUnit::Key { name, tip,
                                   [&p, rain] { return std::abs (valueOf (p, ParamIDs::downpourRain) - rain) < 0.08f; },
                                   [&p, rain] { setValue (p, ParamIDs::downpourRain, rain); } };
        };
        s.keys[0] = rainKey ("DRIZZLE", 0.25f, "DRIZZLE: a few spots on the glass");
        s.keys[1] = rainKey ("RAIN", 0.55f, "RAIN: steady rain");
        s.keys[2] = rainKey ("STORM", 0.9f, "STORM: a downpour");
        s.knobs = { RackUnit::Knob { ParamIDs::downpourRain, "RAIN" }, RackUnit::Knob { ParamIDs::downpourLevel, "LEVEL" } };
        // The ladder: the patter (10 x log10 of the taps a second: 40 to 4000).
        s.ladderTitle = "PATTER";
        s.ladderMarks = { 36.0f, 30.0f, 24.8f, 20.0f, 16.0f };
        s.ladderLabels = { "4K", "1K", "300", "100", "40" };
        s.ladderReading = [&p]
        {
            const auto rate = p.getDownpour().getCurrentPatterRate();
            return rate > 0.0f ? 10.0f * std::log10 (rate) : -100.0f;
        };
        s.paintReadout = [&p] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto h = left.getHeight() * 0.56f;
            const auto drops = p.getDownpour().getCurrentDropRate();
            const auto end = drawNumber (g, juce::String (drops, drops < 10.0f ? 1 : 0), left.getTopLeft(), h);
            line (g, "DPS", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            const auto last = p.getDownpour().getRecentDrops().back();
            line (g, last.sample >= 0 ? "LAST DROP " + DownpourDisplay::noteName (last.note) : juce::String ("DROPS ON YOUR NOTES"),
                  left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            column (g, content, { "RAIN " + textOf (p, ParamIDs::downpourRain), "DROPS " + textOf (p, ParamIDs::downpourDrops),
                                  "DISTANCE " + textOf (p, ParamIDs::downpourDistance), textOf (p, ParamIDs::downpourOctave) },
                    content.getHeight() * 0.16f);
        };
        s.footTitle = "LEVEL";
        s.footText = [&p] { return textOf (p, ParamIDs::downpourLevel); };
        s.onEdit = [openPage] { openPage (5); };
    }

    // SIROCCO (0.58.0): the answers sounding in big digits, the key and scale; the keys pick the interval.
    {
        auto& s = specs[6];
        s.name = "SIROCCO";
        s.kind = "ANSWERING HARMONY  IN THE SCALE";
        s.onParameterIds = { ParamIDs::siroccoOn };
        s.powerTooltip = "SIROCCO on or off: a harmony answering every note you play, kept to a scale. Click POWER (the word) or EDIT for its page";
        const auto intervalKey = [&p] (const char* name, int interval, const char* tip)
        {
            return RackUnit::Key { name, tip, [&p, interval] { return juce::roundToInt (valueOf (p, ParamIDs::siroccoInterval)) == interval; },
                                   [&p, interval] { setValue (p, ParamIDs::siroccoInterval, (float) interval); } };
        };
        s.keys[0] = intervalKey ("3RD", 0, "3RD ABOVE: each note answered a 3rd up in the scale");
        s.keys[1] = intervalKey ("5TH", 2, "5TH ABOVE: each note answered a 5th up in the scale");
        s.keys[2] = intervalKey ("OCTAVE", 4, "OCTAVE ABOVE");
        s.knobs = { RackUnit::Knob { ParamIDs::siroccoLevel, "LEVEL" }, RackUnit::Knob { ParamIDs::siroccoSpread, "SPREAD" } };
        const auto answersNow = [&p]
        {
            int count = 0;
            for (const auto f : p.getSoundingNoteFlags())
                count += (f & 2) != 0 ? 1 : 0;
            return count;
        };
        s.ladderTitle = "ANSWERS";
        s.ladderMarks = { 8.0f, 6.0f, 4.0f, 2.0f, 1.0f };
        s.ladderLabels = { "8", "6", "4", "2", "1" };
        s.ladderReading = [answersNow] { return (float) answersNow(); };
        s.paintReadout = [&p, answersNow] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto h = left.getHeight() * 0.56f;
            const auto end = drawNumber (g, juce::String (answersNow()), left.getTopLeft(), h);
            line (g, "ANS", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, textOf (p, ParamIDs::siroccoKey) + " " + textOf (p, ParamIDs::siroccoScale).toUpperCase(),
                  left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            column (g, content, { textOf (p, ParamIDs::siroccoInterval), "VOICES " + textOf (p, ParamIDs::siroccoVoices),
                                  "DELAY " + textOf (p, ParamIDs::siroccoDelay), "LEVEL " + textOf (p, ParamIDs::siroccoLevel) },
                    content.getHeight() * 0.16f);
        };
        s.onEdit = [openPage] { openPage (6); };
    }

    // INFERNAL (0.59.0): the drive in big digits, the split and the chains' blends; the keys set the character.
    {
        auto& s = specs[7];
        s.name = "INFERNAL";
        s.kind = "PARALLEL DESTRUCTION  FOUR CHAINS";
        s.onParameterIds = { ParamIDs::infernalOn };
        s.powerTooltip = "INFERNAL on or off: your sound through four chains of destruction at once. Click POWER (the word) or EDIT for its page";
        // The keys: three characters (FUZZ / AMP / NOISE to the fore), lit when that chain leads.
        const auto characterKey = [&p] (const char* name, const char* lead, const char* tip)
        {
            return RackUnit::Key { name, tip,
                                   [&p, lead]
                                   {
                                       const char* chains[] = { ParamIDs::infernalFuzz, ParamIDs::infernalAmp, ParamIDs::infernalNoise };
                                       for (const auto* c : chains)
                                           if (juce::String (c) != lead && valueOf (p, c) > valueOf (p, lead))
                                               return false;
                                       return valueOf (p, lead) > 0.0f;
                                   },
                                   [&p, lead]
                                   {
                                       for (const auto* c : { ParamIDs::infernalFuzz, ParamIDs::infernalAmp, ParamIDs::infernalNoise })
                                           setValue (p, c, juce::String (c) == lead ? 1.0f : 0.3f);
                                   } };
        };
        s.keys[0] = characterKey ("FUZZ", ParamIDs::infernalFuzz, "FUZZ to the fore: the buzzing riff");
        s.keys[1] = characterKey ("AMP", ParamIDs::infernalAmp, "AMP to the fore: the guitar-like lead");
        s.keys[2] = characterKey ("NOISE", ParamIDs::infernalNoise, "NOISE to the fore: the growl pitched to your notes");
        s.knobs = { RackUnit::Knob { ParamIDs::infernalDrive, "DRIVE" }, RackUnit::Knob { ParamIDs::infernalMix, "MIX" } };
        s.ladderTitle = "HEAT";
        s.ladderMarks = { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f };
        s.ladderLabels = { "0", "-6", "-12", "-24", "-48" };
        s.ladderReading = [&p]
        {
            auto loudest = -100.0f;
            for (int k = 0; k < 4; ++k)
                loudest = juce::jmax (loudest, p.getInfernal().getChainDb (k));
            return loudest;
        };
        s.paintReadout = [&p] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto h = left.getHeight() * 0.56f;
            const auto settings = p.getInfernalSettings();
            const auto end = drawNumber (g, juce::String (juce::roundToInt (42.0f * settings.drive)), left.getTopLeft(), h);
            line (g, "DB", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, "CLEAN UNDER " + textOf (p, ParamIDs::infernalTight), left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            column (g, content, { "FUZZ " + textOf (p, ParamIDs::infernalFuzz) + "  CRUSH " + textOf (p, ParamIDs::infernalCrush),
                                  "AMP " + textOf (p, ParamIDs::infernalAmp) + "  NOISE " + textOf (p, ParamIDs::infernalNoise),
                                  "FEEDBACK " + textOf (p, ParamIDs::infernalFeedback), "CAB " + textOf (p, ParamIDs::infernalCab) },
                    content.getHeight() * 0.16f);
        };
        s.onEdit = [openPage] { openPage (7); };
    }

    // AFTERGLOW (0.60.0): AGE in big digits as the cents it spreads the notes, DRIFT, ENSEMBLE and TAPE; the keys
    // pick a character: FRESH (a little), WORN (more), RELIC (all of it, the exaggerated end).
    {
        auto& s = specs[8];
        s.name = "AFTERGLOW";
        s.kind = "ANALOG VOICES  ENSEMBLE  TAPE";
        s.onParameterIds = { ParamIDs::afterglowOn };
        s.powerTooltip = "AFTERGLOW on or off: an old polysynth's glow, each note its own, then ensemble and tape. Click POWER (the word) or EDIT for its page";
        struct Character { const char* name; float age, drift, ensemble, tape; const char* tip; };
        static constexpr Character characters[] = {
            { "FRESH", 0.3f, 0.2f, 0.35f, 0.15f, "A young polysynth: the notes a little apart, a light ensemble" },
            { "WORN", 0.6f, 0.5f, 0.65f, 0.4f, "Well played: the notes clearly their own, a lush ensemble, some tape" },
            { "RELIC", 1.0f, 1.0f, 1.0f, 0.8f, "The exaggerated end: every note its own, swaying, a huge ensemble on worn tape" } };
        for (int k = 0; k < 3; ++k)
        {
            const auto& c = characters[k];
            s.keys[(size_t) k] = RackUnit::Key { c.name, c.tip,
                                                 [&p, c] { return std::abs (valueOf (p, ParamIDs::afterglowAge) - c.age) < 0.01f
                                                                  && std::abs (valueOf (p, ParamIDs::afterglowEnsemble) - c.ensemble) < 0.01f; },
                                                 [&p, c]
                                                 {
                                                     setValue (p, ParamIDs::afterglowAge, c.age);
                                                     setValue (p, ParamIDs::afterglowDrift, c.drift);
                                                     setValue (p, ParamIDs::afterglowEnsemble, c.ensemble);
                                                     setValue (p, ParamIDs::afterglowTape, c.tape);
                                                 } };
        }
        s.knobs = { RackUnit::Knob { ParamIDs::afterglowAge, "AGE" }, RackUnit::Knob { ParamIDs::afterglowEnsemble, "ENSEMBLE" } };
        s.ladderTitle = "GLOW";
        s.ladderMarks = { 100.0f, 75.0f, 50.0f, 25.0f, 0.0f };
        s.ladderLabels = { "100", "75", "50", "25", "0" };
        s.ladderReading = [&p]
        {
            const auto& fx = p.getAfterglow();
            return valueOf (p, ParamIDs::afterglowOn) > 0.5f
                       ? 100.0f * juce::jmax (valueOf (p, ParamIDs::afterglowAge), fx.getEnsembleNow(), fx.getTapeNow())
                       : 0.0f;
        };
        s.paintReadout = [&p] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto h = left.getHeight() * 0.56f;
            const auto cents = juce::roundToInt (valueOf (p, ParamIDs::afterglowAge) * synth::Settings::Afterglow::ageCents);
            const auto end = drawNumber (g, juce::String (cents), left.getTopLeft(), h);
            line (g, "CENTS", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, "EACH NOTE ITS OWN", left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            column (g, content, { "AGE " + textOf (p, ParamIDs::afterglowAge), "DRIFT " + textOf (p, ParamIDs::afterglowDrift),
                                  "ENSEMBLE " + textOf (p, ParamIDs::afterglowEnsemble), "TAPE " + textOf (p, ParamIDs::afterglowTape) },
                    content.getHeight() * 0.16f);
        };
        s.onEdit = [openPage] { openPage (8); };
    }

    // LIGHTTRAIL (0.61.0): the division in big digits, the rise and the length; the keys set the trail's way:
    // FLAT (on the note), CLIMB (a fifth up each repeat), FALL (an octave down).
    {
        auto& s = specs[9];
        s.name = "LIGHTTRAIL";
        s.kind = "SYNCED TRAIL  RISE  SPREAD";
        s.onParameterIds = { ParamIDs::trailOn };
        s.powerTooltip = "LIGHTTRAIL on or off: every note leaves a glowing trail. Click POWER (the word) or EDIT for its page";
        struct Way { const char* name; float rise; const char* tip; };
        static constexpr Way ways[] = { { "FLAT", 0.0f, "Each repeat on the note: a plain trail" },
                                        { "CLIMB", 7.0f, "Each repeat a fifth higher: the trail climbs" },
                                        { "FALL", -12.0f, "Each repeat an octave lower: the trail falls away" } };
        for (int k = 0; k < 3; ++k)
        {
            const auto& w = ways[k];
            s.keys[(size_t) k] = RackUnit::Key { w.name, w.tip,
                                                 [&p, w] { return std::abs (valueOf (p, ParamIDs::trailRise) - w.rise) < 0.05f; },
                                                 [&p, w] { setValue (p, ParamIDs::trailRise, w.rise); } };
        }
        s.knobs = { RackUnit::Knob { ParamIDs::trailLength, "LENGTH" }, RackUnit::Knob { ParamIDs::trailMix, "MIX" } };
        s.ladderTitle = "TRAIL";
        s.ladderMarks = { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f };
        s.ladderLabels = { "0", "-6", "-12", "-24", "-48" };
        s.ladderReading = [&p] { return p.getLightTrail().getTrailDb(); };
        s.paintReadout = [&p] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto h = left.getHeight() * 0.56f;
            const auto settings = p.getLightTrailSettings();
            const auto end = drawNumber (g, juce::String (settings.riseSemitones, 0), left.getTopLeft(), h);
            line (g, "ST", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, "EACH REPEAT", left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            column (g, content, { "EVERY " + textOf (p, ParamIDs::trailDivision), "LENGTH " + textOf (p, ParamIDs::trailLength),
                                  "TONE " + textOf (p, ParamIDs::trailTone), "MIX " + textOf (p, ParamIDs::trailMix) },
                    content.getHeight() * 0.16f);
        };
        s.onEdit = [openPage] { openPage (9); };
    }

    // BLACKHOLE (0.62.0): HOLD in big digits as seconds, the pull and the size; the keys set how long the haze hangs:
    // FAINT, DEEP, ENDLESS.
    {
        auto& s = specs[10];
        s.name = "BLACKHOLE";
        s.kind = "FROZEN HAZE  HOLD  PULL";
        s.onParameterIds = { ParamIDs::blackholeOn };
        s.powerTooltip = "BLACKHOLE on or off: the last moments of your sound stretched into a haze. Click POWER (the word) or EDIT for its page";
        struct Depth { const char* name; float hold; const char* tip; };
        static constexpr Depth depths[] = { { "FAINT", 0.25f, "A brief shimmer behind the notes" },
                                            { "DEEP", 0.6f, "A haze hanging a few seconds" },
                                            { "ENDLESS", 0.95f, "Clouds that hang for many seconds" } };
        for (int k = 0; k < 3; ++k)
        {
            const auto& d = depths[k];
            s.keys[(size_t) k] = RackUnit::Key { d.name, d.tip,
                                                 [&p, d] { return std::abs (valueOf (p, ParamIDs::blackholeHold) - d.hold) < 0.01f; },
                                                 [&p, d] { setValue (p, ParamIDs::blackholeHold, d.hold); } };
        }
        s.knobs = { RackUnit::Knob { ParamIDs::blackholeHold, "HOLD" }, RackUnit::Knob { ParamIDs::blackholeMix, "MIX" } };
        s.ladderTitle = "HAZE";
        s.ladderMarks = { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f };
        s.ladderLabels = { "0", "-6", "-12", "-24", "-48" };
        s.ladderReading = [&p] { return p.getBlackHole().getHazeDb(); };
        s.paintReadout = [&p] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto h = left.getHeight() * 0.56f;
            const auto settings = p.getBlackHoleSettings();
            const auto end = drawNumber (g, juce::String (effects::BlackHole::holdSeconds (settings.hold), 1), left.getTopLeft(), h);
            line (g, "S", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, "THE HAZE HANGS", left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            column (g, content, { "PULL " + textOf (p, ParamIDs::blackholePull), "SIZE " + textOf (p, ParamIDs::blackholeSize),
                                  "TONE " + textOf (p, ParamIDs::blackholeTone), "MIX " + textOf (p, ParamIDs::blackholeMix) },
                    content.getHeight() * 0.16f);
        };
        s.onEdit = [openPage] { openPage (10); };
    }

    // DILATION (0.63.0): the speed in big digits, how far behind and how it comes back; the keys set how slow:
    // DRIFT (a few semitones), SLOWMO (an octave), CRAWL (nearly two).
    {
        auto& s = specs[11];
        s.name = "DILATION";
        s.kind = "TIME SLOWING  SLOW  RAMP";
        s.onParameterIds = { ParamIDs::dilationOn };
        s.powerTooltip = "DILATION on or off: time slowing down, your sound dragged behind real time. Click POWER (the word) or EDIT for its page";
        struct Pace { const char* name; float slow; const char* tip; };
        static constexpr Pace paces[] = { { "DRIFT", 0.15f, "A little slower: a few semitones down" },
                                          { "SLOWMO", 0.5f, "Half speed: an octave down" },
                                          { "CRAWL", 0.9f, "Nearly a quarter speed: almost two octaves down" } };
        for (int k = 0; k < 3; ++k)
        {
            const auto& pace = paces[k];
            s.keys[(size_t) k] = RackUnit::Key { pace.name, pace.tip,
                                                 [&p, pace] { return std::abs (valueOf (p, ParamIDs::dilationSlow) - pace.slow) < 0.01f; },
                                                 [&p, pace] { setValue (p, ParamIDs::dilationSlow, pace.slow); } };
        }
        s.knobs = { RackUnit::Knob { ParamIDs::dilationSlow, "SLOW" }, RackUnit::Knob { ParamIDs::dilationRamp, "RAMP" } };
        s.ladderTitle = "DRAG";
        s.ladderMarks = { 24.0f, 18.0f, 12.0f, 6.0f, 0.5f };
        s.ladderLabels = { "-24", "-18", "-12", "-6", "0" };
        s.ladderReading = [&p] { return -12.0f * std::log2 (juce::jmax (0.01f, p.getDilation().getSpeed())); };
        s.paintReadout = [&p] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto h = left.getHeight() * 0.56f;
            const auto settings = p.getDilationSettings();
            const auto speed = settings.on ? p.getDilation().getSpeed() : 1.0f;
            const auto end = drawNumber (g, juce::String (speed, 2), left.getTopLeft(), h);
            line (g, "X", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, "REAL TIME", left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            column (g, content, { "BEHIND " + juce::String (settings.on ? p.getDilation().getBehindSeconds() : 0.0f, 1) + " s",
                                  "REACH " + textOf (p, ParamIDs::dilationReach), "RETURN " + textOf (p, ParamIDs::dilationReturn),
                                  "MIX " + textOf (p, ParamIDs::dilationMix) },
                    content.getHeight() * 0.16f);
        };
        s.onEdit = [openPage] { openPage (11); };
    }

    // REMNANT (0.64.0): how many layers in big digits, how far down each and how long; the keys set how many:
    // SHALLOW (1), DEEPER (2), DEEPEST (3).
    {
        auto& s = specs[12];
        s.name = "REMNANT";
        s.kind = "DREAM LAYERS  MIX  LENGTH";
        s.onParameterIds = { ParamIDs::remnantOn };
        s.powerTooltip = "REMNANT on or off: layers of what you play beneath it, each deeper one slower and lower. Click POWER (the word) or EDIT for its page";
        struct Depth { const char* name; int layers; const char* tip; };
        static constexpr Depth depths[] = { { "SHALLOW", 0, "One layer beneath your sound" },
                                            { "DEEPER", 1, "Two layers, the second a dream of the first" },
                                            { "DEEPEST", 2, "Three layers, each a dream of the one above" } };
        for (int k = 0; k < 3; ++k)
        {
            const auto& d = depths[k];
            s.keys[(size_t) k] = RackUnit::Key { d.name, d.tip,
                                                 [&p, d] { return juce::roundToInt (valueOf (p, ParamIDs::remnantLayers)) == d.layers; },
                                                 [&p, d] { setValue (p, ParamIDs::remnantLayers, (float) d.layers); } };
        }
        s.knobs = { RackUnit::Knob { ParamIDs::remnantMix, "MIX" }, RackUnit::Knob { ParamIDs::remnantLength, "LENGTH" } };
        s.ladderTitle = "DEPTH";
        s.ladderMarks = { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f };
        s.ladderLabels = { "0", "-6", "-12", "-24", "-48" };
        s.ladderReading = [&p] { return p.getRemnant().getTotalDb(); };
        s.paintReadout = [&p] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto h = left.getHeight() * 0.56f;
            const auto settings = p.getRemnantSettings();
            const auto end = drawNumber (g, juce::String (juce::jlimit (1, 3, settings.layers + 1)), left.getTopLeft(), h);
            line (g, settings.layers == 0 ? "LAYER" : "LAYERS", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, "BENEATH YOUR SOUND", left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            column (g, content, { "STEP " + textOf (p, ParamIDs::remnantStep), "LENGTH " + textOf (p, ParamIDs::remnantLength),
                                  "TAPER " + juce::String (effects::Remnant::taperDb (settings.taper), 1) + " DB", "MIX " + textOf (p, ParamIDs::remnantMix) },
                    content.getHeight() * 0.16f);
        };
        s.onEdit = [openPage] { openPage (12); };
    }

    // PROXIMITY (0.65.0): CLOSE's boost in big digits, the touches and the room; the keys set how close the
    // microphone is: DISTANT, NEAR, INSIDE (NOISE, ROOM and CLOSE together).
    {
        auto& s = specs[13];
        s.name = "PROXIMITY";
        s.kind = "PLAYING UP CLOSE  NOISE  CLOSE";
        s.onParameterIds = { ParamIDs::proximityOn };
        s.powerTooltip = "PROXIMITY on or off: the sound of playing, heard up close. Click POWER (the word) or EDIT for its page";
        struct Place { const char* name; float noise, room, close; const char* tip; };
        static constexpr Place places[] = { { "DISTANT", 0.25f, 0.2f, 0.1f, "A step back: the touches faint, a little room" },
                                            { "NEAR", 0.5f, 0.35f, 0.4f, "Close: every touch heard, the room around it" },
                                            { "INSIDE", 0.8f, 0.5f, 0.8f, "Inside the instrument: the mechanism loud, the low end close" } };
        for (int k = 0; k < 3; ++k)
        {
            const auto& pl = places[k];
            s.keys[(size_t) k] = RackUnit::Key { pl.name, pl.tip,
                                                 [&p, pl] { return std::abs (valueOf (p, ParamIDs::proximityNoise) - pl.noise) < 0.01f
                                                                   && std::abs (valueOf (p, ParamIDs::proximityClose) - pl.close) < 0.01f; },
                                                 [&p, pl]
                                                 {
                                                     setValue (p, ParamIDs::proximityNoise, pl.noise);
                                                     setValue (p, ParamIDs::proximityRoom, pl.room);
                                                     setValue (p, ParamIDs::proximityClose, pl.close);
                                                 } };
        }
        s.knobs = { RackUnit::Knob { ParamIDs::proximityNoise, "NOISE" }, RackUnit::Knob { ParamIDs::proximityClose, "CLOSE" } };
        s.ladderTitle = "TOUCH";
        s.ladderMarks = { -12.0f, -24.0f, -36.0f, -48.0f, -60.0f };
        s.ladderLabels = { "-12", "-24", "-36", "-48", "-60" };
        s.ladderReading = [&p] { return p.getProximity().getNoiseDb(); };
        s.paintReadout = [&p] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto h = left.getHeight() * 0.56f;
            const auto settings = p.getProximitySettings();
            const auto end = drawNumber (g, "+" + juce::String (effects::Proximity::closeDb (settings.close), 1), left.getTopLeft(), h);
            line (g, "DB", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, "UP CLOSE", left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            column (g, content, { "NOISE " + textOf (p, ParamIDs::proximityNoise), "ROOM " + textOf (p, ParamIDs::proximityRoom),
                                  "HUM " + textOf (p, ParamIDs::proximityHum), "TONE " + textOf (p, ParamIDs::proximityTone) },
                    content.getHeight() * 0.16f);
        };
        s.onEdit = [openPage] { openPage (13); };
    }

    // HAZE (0.66.0): the pass in big digits, how far it bends and how often; the keys set how hard it passes:
    // CRUISE, PASSING, RUSH (DEPTH and DOPPLER together).
    {
        auto& s = specs[14];
        s.name = "HAZE";
        s.kind = "PASSING LIGHTS  DEPTH  DOPPLER";
        s.onParameterIds = { ParamIDs::hazeOn };
        s.powerTooltip = "HAZE on or off: your sound sweeping past like streetlights, in time with your song. Click POWER (the word) or EDIT for its page";
        struct Pace { const char* name; float depth, doppler; const char* tip; };
        static constexpr Pace paces[] = { { "CRUISE", 0.3f, 0.2f, "A gentle drift past, barely bending" },
                                          { "PASSING", 0.55f, 0.45f, "Lights sweeping by, the pitch leaning as they pass" },
                                          { "RUSH", 0.85f, 0.8f, "Rushing past, close and fast" } };
        for (int k = 0; k < 3; ++k)
        {
            const auto& pc = paces[k];
            s.keys[(size_t) k] = RackUnit::Key { pc.name, pc.tip,
                                                 [&p, pc] { return std::abs (valueOf (p, ParamIDs::hazeDepth) - pc.depth) < 0.01f
                                                                   && std::abs (valueOf (p, ParamIDs::hazeDoppler) - pc.doppler) < 0.01f; },
                                                 [&p, pc]
                                                 {
                                                     setValue (p, ParamIDs::hazeDepth, pc.depth);
                                                     setValue (p, ParamIDs::hazeDoppler, pc.doppler);
                                                 } };
        }
        s.knobs = { RackUnit::Knob { ParamIDs::hazeDepth, "DEPTH" }, RackUnit::Knob { ParamIDs::hazeDoppler, "DOPPLER" } };
        s.ladderTitle = "PASS";
        s.ladderMarks = { 12.0f, 9.0f, 6.0f, 3.0f, 1.0f };
        s.ladderLabels = { "-12", "-9", "-6", "-3", "-1" };
        s.ladderReading = [&p] { return -p.getHaze().getLevelDb(); };   // how far it has fallen: further away, more lit
        s.paintReadout = [&p] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto h = left.getHeight() * 0.56f;
            const auto settings = p.getHazeSettings();
            const auto shift = settings.on ? p.getHaze().getShiftCents() : 0.0f;
            const auto end = drawNumber (g, (shift >= 0.0f ? "+" : "") + juce::String (juce::roundToInt (shift)), left.getTopLeft(), h);
            line (g, "CENTS", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, "PASSING", left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            column (g, content, { "EVERY " + textOf (p, ParamIDs::hazeDivision), "DEPTH " + textOf (p, ParamIDs::hazeDepth),
                                  "DOPPLER " + textOf (p, ParamIDs::hazeDoppler), "WHOOSH " + textOf (p, ParamIDs::hazeWhoosh) },
                    content.getHeight() * 0.16f);
        };
        s.onEdit = [openPage] { openPage (14); };
    }

    // The SIGNATURE slot (0.58.0, the owner's design): ECHOES, CRACKLE, DOWNPOUR and SIROCCO take turns in the fourth
    // place; the small screen under EDIT names the one shown (and how many others are on) and picks another.
    for (int k = 0; k < signatureNames().size(); ++k)
    {
        auto& s = specs[(size_t) (3 + k)];
        s.number = 4;
        s.series = "PR7-K SERIES  SIGNATURE";
        s.footTitle = "SIGNATURE";
        s.footText = [&p, k]
        {
            int others = 0;
            for (int j = 0; j < signatureNames().size(); ++j)
                others += j != k && signatureOn (p, j) ? 1 : 0;
            return signatureNames()[k] + (others > 0 ? " +" + juce::String (others) : juce::String());
        };
        s.onFootClick = chooseSignature;
    }
    return specs;
}

//==============================================================================
std::array<RackUnit::Spec, 5> makeInsertSpecs (ParagonX1AudioProcessor& p, int bus, std::function<void (int)> openPage)
{
    std::array<RackUnit::Spec, 5> specs;
    const auto busName = ParamIDs::insertBusNames[bus + 1];
    const auto series = "PR7-K SERIES  BUS " + busName;
    const auto id = [bus] (const char* name) { return busId (bus, name); };

    // DRIVE (PR7-G's seven pedals): DRIVE in big digits, the model's own settings beside it, the bus's level on the ladder.
    {
        using Model = effects::Drive::Model;
        auto& s = specs[0];
        s.number = 2;
        s.name = "DRIVE";
        s.kind = "OVERDRIVE / DISTORTION / FUZZ  7 MODELS";
        s.series = series;
        s.onParameterIds = { id ("driveOn") };
        s.powerTooltip = "DRIVE on or off, on bus " + busName + ". Click POWER (the word) or EDIT for its page";
        const auto modelId = id ("driveModel");
        const auto modelKey = [&p, modelId] (Model model, const char* tip)
        {
            return RackUnit::Key { effects::Drive::modelNames[(int) model].toUpperCase(), tip,
                                   [&p, modelId, model] { return juce::roundToInt (valueOf (p, modelId)) == (int) model; },
                                   [&p, modelId, model] { setValue (p, modelId, (float) (int) model); } };
        };
        s.keys[0] = modelKey (Model::screamer, "BANSHEE: a TS808-style overdrive, mids pushed, bass clean");
        s.keys[1] = modelKey (Model::rabid, "RABID: a RAT-style distortion");
        s.keys[2] = modelKey (Model::muff, "COSMONAUT: a green Russian Big Muff-style fuzz (the other models on the page)");
        s.knobs = { RackUnit::Knob { id ("driveGain"), "DRIVE" }, RackUnit::Knob { id ("driveLevel"), "LEVEL" } };
        s.ladderTitle = "BUS";
        s.ladderMarks = { -3.0f, -9.0f, -18.0f, -30.0f, -48.0f };
        s.ladderLabels = { "-3", "-9", "-18", "-30", "-48" };
        s.ladderReading = [&p, bus] { return p.getInsertPeakDb (bus); };
        s.paintReadout = [&p, bus, id] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto settings = p.getInsertDriveSettings (bus);
            const auto h = left.getHeight() * 0.56f;
            drawNumber (g, juce::String (settings.drive, 1), left.getTopLeft(), h);
            line (g, "DRIVE", left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            juce::StringArray rows { textOf (p, id ("driveModel")).toUpperCase() };
            if (settings.model == Model::abyss)
                rows.add ("HIGH " + textOf (p, id ("driveHigh")) + "  LOW " + textOf (p, id ("driveLow")));
            else if (settings.model != Model::halo)
                rows.add ("TONE " + textOf (p, id ("driveTone")));
            if (settings.model == Model::skyline)
                rows.add ("ATTACK " + textOf (p, id ("driveAttack")) + "  GATE " + textOf (p, id ("driveGate")));
            rows.add ("LEVEL " + textOf (p, id ("driveLevel")));
            column (g, content, rows, content.getHeight() * 0.16f);
        };
        s.footTitle = "MODEL";
        s.footText = [&p, modelId] { return textOf (p, modelId); };
        s.onEdit = [openPage] { openPage (0); };
    }

    // GLITCH (PR7-G's): LENGTH in big digits (ms, or the note at the song's tempo), the mode and its settings.
    {
        using Mode = effects::Glitch::Mode;
        auto& s = specs[1];
        s.number = 3;
        s.name = "GLITCH";
        s.kind = "GLITCH / STUTTER  5 MODES";
        s.series = series;
        s.onParameterIds = { id ("glitchOn") };
        s.powerTooltip = "GLITCH on or off, on bus " + busName + ". Click POWER (the word) or EDIT for its page";
        const auto modeId = id ("glitchMode");
        const auto modeKey = [&p, modeId] (Mode mode, const char* tip)
        {
            return RackUnit::Key { effects::Glitch::modeNames[(int) mode].toUpperCase(), tip,
                                   [&p, modeId, mode] { return juce::roundToInt (valueOf (p, modeId)) == (int) mode; },
                                   [&p, modeId, mode] { setValue (p, modeId, (float) (int) mode); } };
        };
        s.keys[0] = modeKey (Mode::sequence, "Tempo-locked stutters: a slice repeated instead, like a beat-repeat edit");
        s.keys[1] = modeKey (Mode::interfere, "A failing signal: pieces dropped, reversed or caught in a buzzing loop");
        s.keys[2] = modeKey (Mode::killswitch, "A toggle switch flicked in time: the end of each step cut");
        s.knobs = { RackUnit::Knob { id ("glitchDensity"), "DENSITY" }, RackUnit::Knob { id ("glitchMix"), "MIX" } };
        s.ladderTitle = "BUS";
        s.ladderMarks = { -3.0f, -9.0f, -18.0f, -30.0f, -48.0f };
        s.ladderLabels = { "-3", "-9", "-18", "-30", "-48" };
        s.ladderReading = [&p, bus] { return p.getInsertPeakDb (bus); };
        s.paintReadout = [&p, bus, id] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto settings = p.getInsertGlitchSettings (bus);
            const auto bpm = juce::jmax (1.0, p.getHostBpm());
            const auto ms = settings.sync ? effects::Glitch::syncBeats[settings.division] * 60000.0 / bpm : (double) settings.lengthMs;
            const auto h = left.getHeight() * 0.56f;
            const auto end = drawNumber (g, juce::String (juce::roundToInt (ms)), left.getTopLeft(), h);
            line (g, "ms", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, settings.sync ? textOf (p, id ("glitchDivision")) + " AT " + juce::String (bpm, 0) + " BPM" : juce::String ("FREE"),
                  left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            column (g, content, { textOf (p, id ("glitchMode")).toUpperCase(), "DENSITY " + textOf (p, id ("glitchDensity")),
                                  "PITCH " + textOf (p, id ("glitchPitch")).toUpperCase(), "CHAOS " + textOf (p, id ("glitchChaos")) },
                    content.getHeight() * 0.16f);
        };
        s.footTitle = "MODE";
        s.footText = [&p, modeId] { return textOf (p, modeId); };
        s.onEdit = [openPage] { openPage (1); };
    }

    specs[2] = makeModSpec (p, ParamIDs::insModIds (bus), 4, "MOD", series, "MOD on or off, on bus " + busName + ". Click POWER (the word) or EDIT for its page",
                            [&p, bus] { return p.getInsertModSettings (bus); }, [&p, bus] { return p.getInsertModSweepPhase (bus); },
                            [openPage] { openPage (2); });
    specs[3] = makeLoFiSpec (p, id, 5, series, [&p, bus] { return p.getInsertLoFiSettings (bus); }, "BUS",
                             [&p, bus] { return p.getInsertPeakDb (bus); }, [openPage] { openPage (3); });
    specs[3].powerTooltip = "LO-FI on or off, on bus " + busName + ". Click POWER (the word) or EDIT for its page";

    // THE WAH (0.51.0): first on the bus (drawn at the top, numbered 1; last in this list so the others keep their
    // places). Where the filter is now in big digits (Hz), the mode and its settings beside it; the ladder is the pedal.
    {
        using Mode = effects::Wah::Mode;
        auto& s = specs[4];
        s.number = 1;
        s.name = "WAH";
        s.kind = "WAH / AUTO-WAH  3 MODES";
        s.series = series;
        s.onParameterIds = { id ("wahOn") };
        s.powerTooltip = "WAH on or off, on bus " + busName + " (before the DRIVE). Click POWER (the word) or EDIT for its page";
        const auto modeId = id ("wahMode");
        const auto modeKey = [&p, modeId] (Mode mode, const char* tip)
        {
            return RackUnit::Key { effects::Wah::modeNames[(int) mode].toUpperCase(), tip,
                                   [&p, modeId, mode] { return juce::roundToInt (valueOf (p, modeId)) == (int) mode; },
                                   [&p, modeId, mode] { setValue (p, modeId, (float) (int) mode); } };
        };
        s.keys[0] = modeKey (Mode::pedal, "PEDAL: the PEDAL knob rocks it, heel to toe. Route EXPRESSION to it (the EXPR wheel or your pedal) in the MOD matrix");
        s.keys[1] = modeKey (Mode::autoWah, "AUTO: it opens with how loud you play, up from the PEDAL by DEPTH");
        s.keys[2] = modeKey (Mode::lfo, "LFO: it sweeps by itself, free or in time with the song, up from the PEDAL by DEPTH");
        s.knobs = { RackUnit::Knob { id ("wahPosition"), "PEDAL" }, RackUnit::Knob { id ("wahResonance"), "RESONANCE" } };
        s.ladderTitle = "PEDAL";
        s.ladderMarks = { 0.9f, 0.7f, 0.5f, 0.3f, 0.1f };
        s.ladderLabels = { "TOE", "", "", "", "HEEL" };
        s.ladderReading = [&p, bus] { return p.getInsertWahSettings (bus).on ? p.getInsertWahSweep (bus) : -1.0f; };
        s.paintReadout = [&p, bus, id] (juce::Graphics& g, juce::Rectangle<float> glass)
        {
            auto content = glass.reduced (glass.getWidth() * 0.045f, glass.getHeight() * 0.14f);
            auto left = content.removeFromLeft (content.getWidth() * 0.5f);
            const auto settings = p.getInsertWahSettings (bus);
            const auto sweep = settings.on ? p.getInsertWahSweep (bus) : settings.position;
            const auto hz = effects::Wah::frequencyFor (settings.range, sweep);
            const auto h = left.getHeight() * 0.56f;
            const auto end = drawNumber (g, hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) : juce::String (juce::roundToInt (hz)), left.getTopLeft(), h);
            line (g, hz >= 1000.0f ? "kHz" : "Hz", { end + 6.0f, left.getY(), left.getRight() - end, h * 0.5f }, h * 0.24f);
            line (g, "PEDAL " + juce::String (juce::roundToInt (sweep * 100.0f)) + "%", left.withTop (left.getY() + h * 1.1f).withHeight (h * 0.4f), h * 0.22f);
            juce::StringArray rows { textOf (p, id ("wahMode")).toUpperCase() + "  " + textOf (p, id ("wahRange")).toUpperCase(),
                                     "RESONANCE " + textOf (p, id ("wahResonance")) };
            if (settings.mode == Mode::autoWah)
                rows.add ("SENS " + textOf (p, id ("wahSensitivity")) + "  DEPTH " + textOf (p, id ("wahDepth")));
            else if (settings.mode == Mode::lfo)
                rows.add ((settings.sync > 0 ? textOf (p, id ("wahSync")) : textOf (p, id ("wahRate"))) + "  DEPTH " + textOf (p, id ("wahDepth")));
            rows.add ("MIX " + textOf (p, id ("wahMix")) + "  " + textOf (p, id ("wahLevel")));
            column (g, content, rows, content.getHeight() * 0.16f);
        };
        s.footTitle = "RANGE";
        s.footText = [&p, rangeId = id ("wahRange")] { return textOf (p, rangeId); };
        s.onEdit = [openPage] { openPage (4); };
    }
    return specs;
}
