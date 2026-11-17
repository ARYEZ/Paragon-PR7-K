#include "Aftermath.h"

namespace effects
{
    namespace
    {
        // The room's four lines (ms at ROOM 0.4's scale of 1), apart enough not to share echoes. Our own choices.
        constexpr float lineMs[Aftermath::numLines] = { 7.9f, 11.3f, 15.7f, 21.1f };
        constexpr int chunk = 32;                 // the darkening worked out every 32 samples
        constexpr float sealSeconds = 0.03f;      // SEAL's smoothing
        constexpr float wetLevel = 0.85f;         // the room at full SEAL, against the sound
        constexpr float dryDip = 0.25f;           // the sound itself a little lower inside
        // The level kept (as LEGION's): the gain follows sqrt (power in / power out) over 0.3 s, held to -6 to +4 dB.
        constexpr float levelSeconds = 0.3f, gainLow = 0.5f, gainHigh = 1.6f;
        // The machinery: the hum's harmonics and the shaft's air (noise through 120-600 Hz, made up to sit with it).
        constexpr float hum2 = 0.35f, hum3 = 0.2f, humNorm = 1.0f / 1.55f;
        constexpr float airLowHz = 120.0f, airHighHz = 600.0f, airMakeUp = 4.0f;
        // Only while there's music: a follower 10 ms up, 0.6 s down; presence = follower / 0.03, none under 0.02.
        constexpr float followUpSeconds = 0.01f, followDownSeconds = 0.6f, presenceFull = 0.03f, presenceGone = 0.02f;
    }

    float Aftermath::ringSeconds (float room, float ring) noexcept
    {
        float mean = 0.0f;
        for (auto ms : lineMs)
            mean += ms;
        mean = mean / (float) numLines * roomScale (room) * 0.001f;
        return -3.0f * mean / std::log10 (ringGain (ring));
    }

    void Aftermath::prepare (double newSampleRate, int)
    {
        sampleRate = newSampleRate;
        auto size = 1;
        while (size < (int) (0.045 * sampleRate))   // the longest line at the biggest room, with room to spare
            size *= 2;
        lineSize = size;
        for (auto& l : lines)
            l.assign ((size_t) size, 0.0f);
        sealCoef = 1.0f - std::exp (-1.0f / (sealSeconds * (float) sampleRate));
        levelCoef = 1.0f - std::exp (-1.0f / (levelSeconds * (float) sampleRate));
        followUp = 1.0f - std::exp (-1.0f / (followUpSeconds * (float) sampleRate));
        followDown = 1.0f - std::exp (-1.0f / (followDownSeconds * (float) sampleRate));
        reset();
    }

    void Aftermath::reset() noexcept
    {
        for (auto& l : lines)
            std::fill (l.begin(), l.end(), 0.0f);
        writes = {};
        lengthsFor = -1.0f;
        loopTone = {};
        roomEnergy = 0.0f;
        quietSamples = 0;
        darkA = darkB = {};
        humPhase = 0.0;
        airLow = airHigh = {};
        follower = 0.0f;
        powerIn = powerOut = 0.0f;
        gainNow = 1.0f;
        sealNow = 0.0f;
        resting = true;
        sealShown.store (0.0f);
        roomShown.store (0.0f);
        humShown.store (0.0f);
        gainShown.store (0.0f);
    }

    void Aftermath::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const auto n = (int) block.getNumSamples();
        const auto channels = (int) block.getNumChannels();
        if (lineSize == 0 || n == 0 || channels == 0)
            return;
        const auto target = ! s.on || s.seal < 0.001f ? 0.0f : juce::jlimit (0.0f, 1.0f, s.seal);
        if (resting && target <= 0.0f)
            return;   // untouched, bit for bit
        resting = false;

        auto* left = block.getChannelPointer (0);
        auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;
        const auto rate = (float) sampleRate;

        // The room's lengths follow ROOM (the lines only re-read, never cleared, so a change slides rather than clicks).
        const auto scale = roomScale (s.room);
        if (std::abs (scale - lengthsFor) > 1.0e-4f)
        {
            for (int k = 0; k < numLines; ++k)
                lengths[(size_t) k] = juce::jlimit (8, lineSize - 2, juce::roundToInt (lineMs[k] * 0.001f * scale * rate));
            lengthsFor = scale;
        }
        const auto feedback = ringGain (s.ring);
        const auto toneHz = 3500.0f + 5500.0f * (1.0f - juce::jlimit (0.0f, 1.0f, s.dark));
        const auto loopCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * toneHz / rate);
        const auto width = juce::jlimit (0.0f, 1.0f, s.width);
        const auto same = 0.5f * (1.0f + width), cross = 0.5f * (1.0f - width);
        const auto humGain = s.hum > 0.0f ? juce::Decibels::decibelsToGain (humDb (s.hum)) : 0.0f;
        const auto humStep = (s.humHz == 0 ? 50.0 : 60.0) / sampleRate;
        const auto airLowCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * airLowHz / rate);
        const auto airHighCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * airHighHz / rate);
        const auto sealedHz = darkHz (s.dark);

        float roomPeak = 0.0f, humPeak = 0.0f;
        for (int start = 0; start < n; start += chunk)
        {
            const auto m = juce::jmin (chunk, n - start);
            // The darkening at this chunk's SEAL: 20 kHz (open) down to DARK's cutoff (sealed), on a log scale.
            const auto hz = juce::jmin (0.45f * rate, 20000.0f * std::pow (sealedHz / 20000.0f, sealNow));
            const auto darkCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / rate);
            for (int i = 0; i < m; ++i)
            {
                const auto at = start + i;
                sealNow += (target - sealNow) * sealCoef;
                if (target <= 0.0f && sealNow < 1.0e-5f)
                    sealNow = 0.0f;
                const auto inL = left[at], inR = right != nullptr ? right[at] : inL;

                // The sound, darkened as it's sealed in.
                darkA[0] += (inL - darkA[0]) * darkCoef;
                darkB[0] += (darkA[0] - darkB[0]) * darkCoef;
                darkA[1] += (inR - darkA[1]) * darkCoef;
                darkB[1] += (darkA[1] - darkB[1]) * darkCoef;
                const auto dryL = inL + (darkB[0] - inL) * sealNow;
                const auto dryR = inR + (darkB[1] - inR) * sealNow;

                // The room: each line read, the four mixed (Householder), darkened in the loop, fed back with the sound.
                std::array<float, numLines> y {};
                float sum = 0.0f;
                for (int k = 0; k < numLines; ++k)
                {
                    const auto& l = lines[(size_t) k];
                    y[(size_t) k] = l[(size_t) ((writes[(size_t) k] - lengths[(size_t) k]) & (lineSize - 1))];
                    sum += y[(size_t) k];
                }
                const auto feedL = dryL * sealNow, feedR = dryR * sealNow;   // the room hears the darkened sound
                for (int k = 0; k < numLines; ++k)
                {
                    const auto mixed = y[(size_t) k] - 0.5f * sum;   // 2 / N with N = 4
                    loopTone[(size_t) k] += (mixed - loopTone[(size_t) k]) * loopCoef;
                    const auto in = (k < 2 ? feedL : feedR) * 0.5f;
                    auto& l = lines[(size_t) k];
                    l[(size_t) writes[(size_t) k]] = in + feedback * loopTone[(size_t) k];
                    writes[(size_t) k] = (writes[(size_t) k] + 1) & (lineSize - 1);
                }
                const auto roomL = 0.5f * (y[0] + y[1]), roomR = 0.5f * (y[2] + y[3]);
                const auto wetL = same * roomL + cross * roomR, wetR = same * roomR + cross * roomL;
                roomEnergy = wetL * wetL + wetR * wetR;
                roomPeak = juce::jmax (roomPeak, std::abs (wetL), std::abs (wetR));

                const auto inside = 1.0f - dryDip * sealNow, room = wetLevel * sealNow;
                const auto outL = dryL * inside + wetL * room, outR = dryR * inside + wetR * room;

                // The level kept.
                powerIn += (inL * inL + inR * inR - powerIn) * levelCoef;
                powerOut += (outL * outL + outR * outR - powerOut) * levelCoef;
                const auto wanted = sealNow <= 0.0f ? 1.0f
                                  : powerOut > 1.0e-12f ? juce::jlimit (gainLow, gainHigh, std::sqrt (powerIn / powerOut)) : gainNow;
                gainNow += (wanted - gainNow) * sealCoef;

                // The machinery, only while there's music.
                const auto level = juce::jmax (std::abs (inL), std::abs (inR));
                follower += (level - follower) * (level > follower ? followUp : followDown);
                auto presence = juce::jmin (1.0f, follower / presenceFull);
                if (presence < presenceGone)
                    presence = 0.0f;
                auto addL = 0.0f, addR = 0.0f;
                const auto machinery = humGain * sealNow * presence;
                if (machinery > 0.0f)
                {
                    humPhase += humStep;
                    if (humPhase >= 1.0)
                        humPhase -= 1.0;
                    const auto p = (float) (juce::MathConstants<double>::twoPi * humPhase);
                    const auto hum = (std::sin (p) + hum2 * std::sin (2.0f * p) + hum3 * std::sin (3.0f * p)) * humNorm;
                    for (int c = 0; c < 2; ++c)
                    {
                        const auto w = noise();
                        airHigh[(size_t) c] += (w - airHigh[(size_t) c]) * airHighCoef;
                        airLow[(size_t) c] += (airHigh[(size_t) c] - airLow[(size_t) c]) * airLowCoef;
                        const auto air = (airHigh[(size_t) c] - airLow[(size_t) c]) * airMakeUp;
                        (c == 0 ? addL : addR) = machinery * (hum + air);
                    }
                    humPeak = juce::jmax (humPeak, machinery);
                }

                left[at] = outL * gainNow + addL;
                if (right != nullptr)
                    right[at] = outR * gainNow + addR;
            }
        }

        sealShown.store (sealNow);
        roomShown.store (juce::jlimit (0.0f, 1.0f, roomPeak * sealNow * 2.0f));
        humShown.store (humGain > 0.0f ? juce::jlimit (0.0f, 1.0f, humPeak / humGain) : 0.0f);
        gainShown.store (juce::Decibels::gainToDecibels (gainNow));

        // Back at rest once SEAL is 0 and the gain has settled: everything cleared, so it wakes clean.
        if (target <= 0.0f && sealNow <= 0.0f)
        {
            quietSamples = std::abs (gainNow - 1.0f) < 1.0e-3f ? quietSamples + n : 0;
            if (quietSamples > (int) (0.3 * sampleRate))
                reset();
        }
        else
            quietSamples = 0;
    }
}
