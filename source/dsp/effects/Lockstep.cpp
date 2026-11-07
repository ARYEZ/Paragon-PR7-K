#include "Lockstep.h"

namespace effects
{
    namespace
    {
        constexpr float amountSeconds = 0.03f;                       // AMOUNT's smoothing
        constexpr float levelSeconds = 0.3f, gainLow = 0.25f, gainHigh = 4.0f;  // SQUEEZE's level kept
        constexpr float squeezeRef = 0.3f;                           // a level SQUEEZE leaves where it was
        constexpr float maxDrive = 7.0f;                             // SQUEEZE at full: x 8
        constexpr double attackSeconds = 0.0015, releaseSeconds = 0.006;
        constexpr float maxMakeUp = 2.0f;                            // the march's make-up: +6 dB at most
        // The boots: a 6 ms crack (noise through 900 Hz-5 kHz) and a 70 Hz thud over 45 ms.
        constexpr float crackSeconds = 0.006f, thudSeconds = 0.045f, thudHz = 70.0f;
        constexpr float crackLevel = 1.4f, thudLevel = 1.0f, stampScale = 0.9f;
        constexpr float followUpSeconds = 0.01f, followDownSeconds = 0.4f, presenceGone = 0.002f;
    }

    void Lockstep::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        amountCoef = 1.0f - std::exp (-1.0f / (amountSeconds * (float) sampleRate));
        levelCoef = 1.0f - std::exp (-1.0f / (levelSeconds * (float) sampleRate));
        followUp = 1.0f - std::exp (-1.0f / (followUpSeconds * (float) sampleRate));
        followDown = 1.0f - std::exp (-1.0f / (followDownSeconds * (float) sampleRate));
        reset();
    }

    void Lockstep::reset() noexcept
    {
        amountNow = 0.0f;
        ownBeats = 0.0;
        lastStep = -1.0;
        powerIn = powerOut = 0.0f;
        squeezeGain = 1.0f;
        makeUpNow = 1.0f;
        stampAge = 1.0e9f;
        stampLevel = 0.0f;
        thudPhase = 0.0;
        crackLow = crackHigh = 0.0f;
        follower = 0.0f;
        quietSamples = 0;
        resting = true;
        amountShown.store (0.0f);
        stampShown.store (0.0f);
        makeUpShown.store (0.0f);
    }

    float Lockstep::marchLevel (double steps, float a, double stepSeconds, float gate, float offBeat, double beatsPerStep) const noexcept
    {
        const auto whole = std::floor (steps);
        const auto phase = (steps - whole) * stepSeconds;   // seconds into this step
        const auto open = gate * stepSeconds;
        double g;
        if (phase < attackSeconds)
            g = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * phase / attackSeconds);
        else if (phase < open)
            g = 1.0;
        else if (phase < open + releaseSeconds)
            g = 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * (phase - open) / releaseSeconds);
        else
            g = 0.0;
        // On the beat: the step starts on a whole beat.
        const auto startBeats = whole * beatsPerStep;
        const auto onBeat = std::abs (startBeats - std::round (startBeats)) < 1.0e-6;
        const auto level = (float) g * (onBeat ? 1.0f : offBeat);
        return 1.0f - a * (1.0f - level);
    }

    void Lockstep::process (juce::dsp::AudioBlock<float> block, const Settings& s, double bpm, double ppq) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = (int) block.getNumChannels();
        if (n == 0 || channels == 0)
            return;
        const auto target = ! s.on || s.amount < 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, s.amount);
        const auto beatsPerSample = juce::jmax (20.0, bpm) / 60.0 / sampleRate;
        if (resting && target <= 0.0f)
        {
            ownBeats += beatsPerSample * n;   // keep counting, so the march wakes in time
            return;                           // untouched, bit for bit
        }
        resting = false;

        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto beatsPerStep = divisionBeats[(size_t) juce::jlimit (0, (int) divisionBeats.size() - 1, s.division)];
        const auto stepSeconds = beatsPerStep * 60.0 / juce::jmax (20.0, bpm);
        const auto gate = gateShare (s.gate);
        const auto offBeat = offBeatLevel (s.accent);
        const auto squeeze = juce::jlimit (0.0f, 1.0f, s.squeeze);
        const auto stampAmount = juce::jlimit (0.0f, 1.0f, s.stamp);
        const auto rate = (float) sampleRate;
        const auto crackLowCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 900.0f / rate);
        const auto crackHighCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 5000.0f / rate);

        // The march's make-up at this block's AMOUNT: its average power over two steps (one on the beat, one off where
        // the division has them), worked out from the pattern itself.
        const auto makeUpFor = [&] (float a)
        {
            if (a <= 0.0f)
                return 1.0f;
            double sum = 0.0;
            constexpr int points = 512;
            for (int k = 0; k < points; ++k)
            {
                const auto v = marchLevel (2.0 * (k + 0.5) / points, a, stepSeconds, gate, offBeat, beatsPerStep);
                sum += (double) v * v;
            }
            return juce::jlimit (1.0f, maxMakeUp, (float) (1.0 / std::sqrt (juce::jmax (1.0e-6, sum / points))));
        };
        const auto makeUpTarget = makeUpFor (target);
        const auto start = ppq >= 0.0 ? ppq : ownBeats;

        float stampPeak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            amountNow += (target - amountNow) * amountCoef;
            if (target <= 0.0f && amountNow < 1.0e-5f)
                amountNow = 0.0f;
            const auto steps = (start + beatsPerSample * i) / beatsPerStep;
            const auto inL = left[i], inR = right != nullptr ? right[i] : inL;

            // SQUEEZE: driven into a tanh and brought back to its level.
            const auto drive = 1.0f + maxDrive * amountNow * squeeze;
            auto sqL = inL, sqR = inR;
            if (drive > 1.0f)
            {
                const auto norm = squeezeRef / std::tanh (squeezeRef * drive);
                sqL = std::tanh (inL * drive) * norm;
                sqR = std::tanh (inR * drive) * norm;
            }
            powerIn += (inL * inL + inR * inR - powerIn) * levelCoef;
            powerOut += (sqL * sqL + sqR * sqR - powerOut) * levelCoef;
            const auto wanted = drive <= 1.0f ? 1.0f
                              : powerOut > 1.0e-12f ? juce::jlimit (gainLow, gainHigh, std::sqrt (powerIn / powerOut)) : squeezeGain;
            squeezeGain += (wanted - squeezeGain) * amountCoef;

            // The march, its loss made up.
            makeUpNow += (makeUpTarget - makeUpNow) * amountCoef;
            const auto march = marchLevel (steps, amountNow, stepSeconds, gate, offBeat, beatsPerStep) * makeUpNow;

            // The boots: a new step starts one.
            const auto level = juce::jmax (std::abs (inL), std::abs (inR));
            follower += (level - follower) * (level > follower ? followUp : followDown);
            const auto stepNow = std::floor (steps);
            if (stepNow != lastStep)
            {
                if (lastStep >= 0.0 && stampAmount > 0.0f && amountNow > 0.0f && follower > presenceGone)
                {
                    const auto startBeats = stepNow * beatsPerStep;
                    const auto onBeat = std::abs (startBeats - std::round (startBeats)) < 1.0e-6;
                    stampLevel = stampScale * stampAmount * amountNow * follower * (onBeat ? 1.0f : offBeat);
                    stampAge = 0.0f;
                    thudPhase = 0.0;
                    stamps.fetch_add (1, std::memory_order_relaxed);
                }
                lastStep = stepNow;
            }
            auto boot = 0.0f;
            if (stampAge < thudSeconds * 6.0f)
            {
                const auto w = noise();
                crackHigh += (w - crackHigh) * crackHighCoef;
                crackLow += (crackHigh - crackLow) * crackLowCoef;
                const auto crack = (crackHigh - crackLow) * crackLevel * std::exp (-stampAge / (crackSeconds / 6.9f));
                thudPhase += thudHz / sampleRate;
                const auto thud = thudLevel * (float) std::sin (juce::MathConstants<double>::twoPi * thudPhase)
                                * std::exp (-stampAge / (thudSeconds / 6.9f));
                boot = stampLevel * (crack + thud);
                stampAge += 1.0f / rate;
                stampPeak = juce::jmax (stampPeak, std::abs (boot));
            }

            left[i] = sqL * squeezeGain * march + boot;
            if (right != nullptr)
                right[i] = sqR * squeezeGain * march + boot;
        }
        ownBeats += beatsPerSample * n;

        amountShown.store (amountNow);
        stepsShown.store ((start + beatsPerSample * n) / beatsPerStep);
        stampShown.store (juce::jlimit (0.0f, 1.0f, stampPeak * 4.0f));
        makeUpShown.store (juce::Decibels::gainToDecibels (makeUpNow * squeezeGain));

        // Back at rest once AMOUNT is 0, the boots silent and the gains back to 1: cleared, so it wakes clean.
        if (target <= 0.0f && amountNow <= 0.0f && stampAge >= thudSeconds * 6.0f)
        {
            quietSamples = std::abs (makeUpNow - 1.0f) < 1.0e-3f && std::abs (squeezeGain - 1.0f) < 1.0e-3f ? quietSamples + n : 0;
            if (quietSamples > (int) (0.3 * sampleRate))
            {
                const auto beats = ownBeats;
                reset();
                ownBeats = beats;
            }
        }
        else
            quietSamples = 0;
    }
}
