/* GuitarMidi-LV2 Library
 * String fingerprint calibration module
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include <stringfingerprint.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>

namespace GuitarMidi {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

float midiToHz(int midi)
{
    return 440.0f * std::pow(2.0f, (static_cast<float>(midi) - 69.0f) / 12.0f);
}

float cosineSimilarity(const std::array<float, kFingerprintSize>& a,
                       const std::array<float, kFingerprintSize>& b)
{
    double dot = 0.0;
    double aa = 0.0;
    double bb = 0.0;
    for (std::size_t i = 0; i < kFingerprintSize; ++i) {
        dot += static_cast<double>(a[i]) * b[i];
        aa += static_cast<double>(a[i]) * a[i];
        bb += static_cast<double>(b[i]) * b[i];
    }
    if (aa <= 1.0e-16 || bb <= 1.0e-16) {
        return -1.0f;
    }
    return static_cast<float>(dot / std::sqrt(aa * bb));
}

std::size_t nearestAnchor(float detectedFrequencyHz, int openMidi)
{
    const std::array<float, 3> frequencies{{
        midiToHz(openMidi),
        midiToHz(openMidi + 5),
        midiToHz(openMidi + 12)
    }};

    std::size_t best = 0;
    float bestDistance = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < frequencies.size(); ++i) {
        const float distance = std::fabs(std::log2(std::max(1.0f, detectedFrequencyHz) / frequencies[i]));
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

bool writeFingerprint(std::ostream& out, const StringFingerprint& fp)
{
    out << fp.referenceFrequencyHz << ' '
        << fp.referenceLevelDb << ' '
        << fp.sampleCount;
    for (float value : fp.values) {
        out << ' ' << value;
    }
    out << '\n';
    return static_cast<bool>(out);
}

bool readFingerprint(std::istream& in, StringFingerprint& fp)
{
    if (!(in >> fp.referenceFrequencyHz >> fp.referenceLevelDb >> fp.sampleCount)) {
        return false;
    }
    for (float& value : fp.values) {
        if (!(in >> value)) {
            return false;
        }
    }
    return true;
}

} // namespace

FingerprintExtractor::FingerprintExtractor(float sampleRate)
    : m_sampleRate(sampleRate)
{
}

StringFingerprint FingerprintExtractor::extract(const float* samples,
                                                std::size_t sampleCount,
                                                float expectedFrequencyHz) const
{
    StringFingerprint result;
    result.referenceFrequencyHz = expectedFrequencyHz;
    result.sampleCount = static_cast<std::uint32_t>(sampleCount);

    if (samples == nullptr || sampleCount < 64 || m_sampleRate <= 0.0f) {
        return result;
    }

    double energy = 0.0;
    double attackEnergy = 0.0;
    double sustainEnergy = 0.0;
    const std::size_t attackEnd = std::max<std::size_t>(1, sampleCount / 8);
    const std::size_t sustainStart = sampleCount / 3;

    for (std::size_t i = 0; i < sampleCount; ++i) {
        const double x = samples[i];
        energy += x * x;
        if (i < attackEnd) {
            attackEnergy += x * x;
        }
        if (i >= sustainStart) {
            sustainEnergy += x * x;
        }
    }

    const double rms = std::sqrt(energy / static_cast<double>(sampleCount));
    result.referenceLevelDb = static_cast<float>(20.0 * std::log10(std::max(rms, 1.0e-9)));

    std::array<double, kFingerprintBands> bandEnergy{};
    const float minFrequency = 55.0f;
    const float maxFrequency = std::min(8000.0f, 0.45f * m_sampleRate);

    for (std::size_t band = 0; band < kFingerprintBands; ++band) {
        const float ratio = static_cast<float>(band) / static_cast<float>(kFingerprintBands - 1);
        const float frequency = minFrequency * std::pow(maxFrequency / minFrequency, ratio);
        const float omega = 2.0f * kPi * frequency / m_sampleRate;
        const float coeff = 2.0f * std::cos(omega);
        double s0 = 0.0;
        double s1 = 0.0;
        double s2 = 0.0;
        for (std::size_t i = 0; i < sampleCount; ++i) {
            const float window = 0.5f - 0.5f * std::cos(2.0f * kPi * static_cast<float>(i)
                                                       / static_cast<float>(sampleCount - 1));
            s0 = static_cast<double>(samples[i] * window) + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        bandEnergy[band] = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    }

    double spectralTotal = 0.0;
    for (double value : bandEnergy) {
        spectralTotal += value;
    }
    spectralTotal = std::max(spectralTotal, 1.0e-18);

    for (std::size_t i = 0; i < kFingerprintBands; ++i) {
        result.values[i] = static_cast<float>(std::log1p(1000.0 * bandEnergy[i] / spectralTotal));
    }

    const double attackRms = std::sqrt(attackEnergy / static_cast<double>(attackEnd));
    const std::size_t sustainCount = std::max<std::size_t>(1, sampleCount - sustainStart);
    const double sustainRms = std::sqrt(sustainEnergy / static_cast<double>(sustainCount));

    double centroidNumerator = 0.0;
    double centroidDenominator = 0.0;
    double highEnergy = 0.0;
    for (std::size_t band = 0; band < kFingerprintBands; ++band) {
        const float ratio = static_cast<float>(band) / static_cast<float>(kFingerprintBands - 1);
        const float frequency = minFrequency * std::pow(maxFrequency / minFrequency, ratio);
        centroidNumerator += frequency * bandEnergy[band];
        centroidDenominator += bandEnergy[band];
        if (frequency >= 2000.0f) {
            highEnergy += bandEnergy[band];
        }
    }

    const float centroid = static_cast<float>(centroidNumerator / std::max(centroidDenominator, 1.0e-18));
    result.values[kFingerprintBands + 0] = static_cast<float>(std::log1p(attackRms / std::max(sustainRms, 1.0e-9)));
    result.values[kFingerprintBands + 1] = clamp01(centroid / maxFrequency);
    result.values[kFingerprintBands + 2] = clamp01(static_cast<float>(highEnergy / spectralTotal));
    result.values[kFingerprintBands + 3] = clamp01(expectedFrequencyHz / 1400.0f);
    result.values[kFingerprintBands + 4] = clamp01(static_cast<float>(attackEnergy / std::max(energy, 1.0e-18)));
    result.values[kFingerprintBands + 5] = clamp01(static_cast<float>(sustainEnergy / std::max(energy, 1.0e-18)));

    double norm = 0.0;
    for (float value : result.values) {
        norm += static_cast<double>(value) * value;
    }
    norm = std::sqrt(std::max(norm, 1.0e-18));
    for (float& value : result.values) {
        value = static_cast<float>(value / norm);
    }

    return result;
}

void StringFingerprintClassifier::setProfile(const GuitarProfile& profile)
{
    m_profile = profile;
    m_hasProfile = true;
}

const GuitarProfile& StringFingerprintClassifier::profile() const noexcept
{
    return m_profile;
}

StringMatch StringFingerprintClassifier::classify(const StringFingerprint& observation,
                                                   float detectedFrequencyHz) const
{
    StringMatch result;
    if (!m_hasProfile || detectedFrequencyHz <= 0.0f) {
        return result;
    }

    float bestScore = -1.0f;
    float secondScore = -1.0f;

    for (std::size_t stringIndex = 0; stringIndex < kGuitarStringCount; ++stringIndex) {
        const int openMidi = m_profile.tuningMidi[stringIndex];
        const float openHz = midiToHz(openMidi);
        const float maxHz = midiToHz(openMidi + 24);
        if (detectedFrequencyHz < openHz * 0.94f || detectedFrequencyHz > maxHz * 1.06f) {
            continue;
        }

        const std::size_t anchorIndex = nearestAnchor(detectedFrequencyHz, openMidi);
        const StringFingerprint& prototype = m_profile.strings[stringIndex].anchors[anchorIndex];
        if (prototype.sampleCount == 0) {
            continue;
        }

        float score = cosineSimilarity(observation.values, prototype.values);
        const float cents = 1200.0f * std::fabs(std::log2(detectedFrequencyHz
                                                          / std::max(1.0f, prototype.referenceFrequencyHz)));
        score -= std::min(0.15f, cents / 12000.0f);

        if (score > bestScore) {
            secondScore = bestScore;
            bestScore = score;
            result.stringId = static_cast<GuitarString>(stringIndex);
        } else if (score > secondScore) {
            secondScore = score;
        }
    }

    result.score = bestScore;
    if (bestScore > -1.0f) {
        const float margin = bestScore - std::max(-1.0f, secondScore);
        result.confidence = clamp01(0.5f * (bestScore + 1.0f) * clamp01(margin * 4.0f));
    }
    return result;
}

bool saveGuitarProfile(const GuitarProfile& profile,
                       const std::string& path,
                       std::string* error)
{
    std::ofstream out(path.c_str(), std::ios::trunc);
    if (!out) {
        if (error) *error = "cannot open profile for writing";
        return false;
    }

    out << "GUITARMIDI_STRING_PROFILE 1\n";
    out << profile.name << '\n';
    for (int midi : profile.tuningMidi) {
        out << midi << ' ';
    }
    out << '\n';

    for (std::size_t i = 0; i < kGuitarStringCount; ++i) {
        out << i << '\n';
        for (const StringFingerprint& fp : profile.strings[i].anchors) {
            if (!writeFingerprint(out, fp)) {
                if (error) *error = "cannot write profile data";
                return false;
            }
        }
    }
    return true;
}

bool loadGuitarProfile(const std::string& path,
                       GuitarProfile& profile,
                       std::string* error)
{
    std::ifstream in(path.c_str());
    if (!in) {
        if (error) *error = "cannot open profile";
        return false;
    }

    std::string magic;
    int version = 0;
    if (!(in >> magic >> version) || magic != "GUITARMIDI_STRING_PROFILE" || version != 1) {
        if (error) *error = "unsupported profile format";
        return false;
    }
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::getline(in, profile.name);

    for (int& midi : profile.tuningMidi) {
        if (!(in >> midi)) {
            if (error) *error = "invalid tuning data";
            return false;
        }
    }

    for (std::size_t i = 0; i < kGuitarStringCount; ++i) {
        std::size_t storedIndex = 0;
        if (!(in >> storedIndex) || storedIndex != i) {
            if (error) *error = "invalid string order";
            return false;
        }
        profile.strings[i].stringId = static_cast<GuitarString>(i);
        for (StringFingerprint& fp : profile.strings[i].anchors) {
            if (!readFingerprint(in, fp)) {
                if (error) *error = "invalid fingerprint data";
                return false;
            }
        }
    }
    return true;
}

const char* guitarStringName(GuitarString value) noexcept
{
    switch (value) {
    case GuitarString::LowE: return "E2";
    case GuitarString::A: return "A2";
    case GuitarString::D: return "D3";
    case GuitarString::G: return "G3";
    case GuitarString::B: return "B3";
    case GuitarString::HighE: return "E4";
    default: return "unknown";
    }
}

} // namespace GuitarMidi
