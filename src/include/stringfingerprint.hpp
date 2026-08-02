/* GuitarMidi-LV2 Library
 * String fingerprint calibration module
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace GuitarMidi {

static constexpr std::size_t kGuitarStringCount = 6;
static constexpr std::size_t kFingerprintBands = 24;
static constexpr std::size_t kFingerprintSize = kFingerprintBands + 6;

enum class GuitarString : std::uint8_t {
    LowE = 0,
    A,
    D,
    G,
    B,
    HighE,
    Unknown = 255
};

struct StringFingerprint {
    std::array<float, kFingerprintSize> values{};
    float referenceFrequencyHz = 0.0f;
    float referenceLevelDb = -120.0f;
    std::uint32_t sampleCount = 0;
};

struct StringProfile {
    GuitarString stringId = GuitarString::Unknown;
    std::array<StringFingerprint, 3> anchors{}; // frets 0, 5 and 12
};

struct GuitarProfile {
    std::string name = "Bridge pickup profile";
    std::array<int, kGuitarStringCount> tuningMidi{{40, 45, 50, 55, 59, 64}};
    std::array<StringProfile, kGuitarStringCount> strings{};
};

struct StringMatch {
    GuitarString stringId = GuitarString::Unknown;
    float confidence = 0.0f;
    float score = -1.0f;
};

class FingerprintExtractor {
public:
    explicit FingerprintExtractor(float sampleRate);

    StringFingerprint extract(const float* samples,
                              std::size_t sampleCount,
                              float expectedFrequencyHz) const;

private:
    float m_sampleRate;
};

class StringFingerprintClassifier {
public:
    void setProfile(const GuitarProfile& profile);
    const GuitarProfile& profile() const noexcept;

    StringMatch classify(const StringFingerprint& observation,
                         float detectedFrequencyHz) const;

private:
    GuitarProfile m_profile{};
    bool m_hasProfile = false;
};

bool saveGuitarProfile(const GuitarProfile& profile,
                       const std::string& path,
                       std::string* error = nullptr);

bool loadGuitarProfile(const std::string& path,
                       GuitarProfile& profile,
                       std::string* error = nullptr);

const char* guitarStringName(GuitarString value) noexcept;

} // namespace GuitarMidi
