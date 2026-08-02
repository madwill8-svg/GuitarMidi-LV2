#include <stringfingerprint.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

std::vector<float> makeString(float sampleRate,
                              float frequency,
                              float harmonicTilt,
                              std::size_t sampleCount)
{
    constexpr float pi = 3.14159265358979323846f;
    std::vector<float> result(sampleCount, 0.0f);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const float t = static_cast<float>(i) / sampleRate;
        const float envelope = std::exp(-3.0f * t);
        float value = 0.0f;
        for (int harmonic = 1; harmonic <= 8; ++harmonic) {
            const float amplitude = std::pow(harmonicTilt, static_cast<float>(harmonic - 1))
                                    / static_cast<float>(harmonic);
            value += amplitude * std::sin(2.0f * pi * frequency * harmonic * t);
        }
        result[i] = 0.2f * envelope * value;
    }
    return result;
}

} // namespace

int main()
{
    constexpr float sampleRate = 48000.0f;
    GuitarMidi::FingerprintExtractor extractor(sampleRate);
    GuitarMidi::GuitarProfile profile;
    profile.name = "Test bridge profile";

    for (std::size_t stringIndex = 0; stringIndex < GuitarMidi::kGuitarStringCount; ++stringIndex) {
        profile.strings[stringIndex].stringId = static_cast<GuitarMidi::GuitarString>(stringIndex);
        const int openMidi = profile.tuningMidi[stringIndex];
        const float openHz = 440.0f * std::pow(2.0f, (openMidi - 69.0f) / 12.0f);
        const float tilt = 0.84f - 0.055f * static_cast<float>(stringIndex);
        const int offsets[3] = {0, 5, 12};
        for (int anchor = 0; anchor < 3; ++anchor) {
            const float frequency = openHz * std::pow(2.0f, offsets[anchor] / 12.0f);
            const std::vector<float> audio = makeString(sampleRate, frequency, tilt, 4096);
            profile.strings[stringIndex].anchors[anchor] =
                extractor.extract(audio.data(), audio.size(), frequency);
        }
    }

    const char* path = "stringfingerprint-test.profile";
    std::string error;
    assert(GuitarMidi::saveGuitarProfile(profile, path, &error));

    GuitarMidi::GuitarProfile loaded;
    assert(GuitarMidi::loadGuitarProfile(path, loaded, &error));
    assert(loaded.name == profile.name);
    std::remove(path);

    GuitarMidi::StringFingerprintClassifier classifier;
    classifier.setProfile(loaded);

    const float highE = 329.62756f;
    const std::vector<float> observationAudio = makeString(sampleRate, highE, 0.565f, 4096);
    const GuitarMidi::StringFingerprint observation =
        extractor.extract(observationAudio.data(), observationAudio.size(), highE);
    const GuitarMidi::StringMatch match = classifier.classify(observation, highE);

    assert(match.stringId == GuitarMidi::GuitarString::HighE);
    assert(match.score > 0.85f);
    assert(match.confidence > 0.0f);
    return 0;
}
