// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "UserTable.h"

#include "../dsp/Fft.h"

#include <algorithm>
#include <cmath>

namespace ohpe::usertable
{
FormantFrame sanitise (FormantFrame f) noexcept
{
    for (int k = 0; k < numFormants; ++k)
    {
        auto fin = [] (float v, float def) { return std::isfinite (v) ? v : def; };
        f.freq[(size_t) k] = std::clamp (fin (f.freq[(size_t) k], 500.0f * (float) (k + 1)), 50.0f, 8000.0f);
        f.gainDb[(size_t) k] = std::clamp (fin (f.gainDb[(size_t) k], -30.0f), -60.0f, 0.0f);
        f.bandwidth[(size_t) k] = std::clamp (fin (f.bandwidth[(size_t) k], 100.0f), 30.0f, 600.0f);
    }
    // rising order (keeps level / bandwidth with their frequency)
    for (int i = 1; i < numFormants; ++i)
        for (int j = i; j > 0 && f.freq[(size_t) j] < f.freq[(size_t) j - 1]; --j)
        {
            std::swap (f.freq[(size_t) j], f.freq[(size_t) j - 1]);
            std::swap (f.gainDb[(size_t) j], f.gainDb[(size_t) j - 1]);
            std::swap (f.bandwidth[(size_t) j], f.bandwidth[(size_t) j - 1]);
        }
    for (int k = 1; k < numFormants; ++k) // at least 40 Hz apart
        f.freq[(size_t) k] = std::min (8000.0f + 40.0f * (float) k, std::max (f.freq[(size_t) k], f.freq[(size_t) k - 1] + 40.0f));
    return f;
}

std::shared_ptr<const UserTable> UserTable::create (std::string name, const std::vector<float>& input, std::vector<FormantFrame> formants)
{
    auto t = std::shared_ptr<UserTable> (new UserTable());
    t->tableName = std::move (name);
    const int n = std::min (maxFrames, (int) (input.size() / (size_t) frameSize));
    t->frames = n;
    if (formants.size() > (size_t) maxFormantFrames) formants.resize ((size_t) maxFormantFrames);
    for (auto& f : formants) f = sanitise (f);
    t->formantFrames = std::move (formants);
    if (n <= 0 && t->formantFrames.empty()) return nullptr;

    t->mips.assign ((size_t) n * (size_t) numLevels * (size_t) stride, 0.0f);
    std::vector<std::complex<double>> spec ((size_t) frameSize), buf ((size_t) frameSize);
    std::vector<bool> silent ((size_t) n, false);
    for (int f = 0; f < n; ++f)
    {
        const float* src = input.data() + (size_t) f * (size_t) frameSize;
        for (int i = 0; i < frameSize; ++i)
            spec[(size_t) i] = std::complex<double> (std::isfinite (src[i]) ? (double) src[i] : 0.0, 0.0);
        dsp::fft (spec, -1);
        double energy = 0.0;
        for (int h = 1; h <= maxHarmonics; ++h) energy += std::norm (spec[(size_t) h]);
        if (energy < 2.5e-9 * (double) frameSize * (double) frameSize) { silent[(size_t) f] = true; continue; } // below about -80 dB
        double norm = 1.0;
        for (int lv = 0; lv < numLevels; ++lv)
        {
            const int harmonics = maxHarmonics >> lv;
            std::fill (buf.begin(), buf.end(), std::complex<double> (0.0, 0.0));
            // one-sided synthesis: x[n] = sum_h Re (2 X[h] / N e^(i 2 pi h n / N)), DC (h = 0) left out
            for (int h = 1; h <= harmonics; ++h) buf[(size_t) h] = spec[(size_t) h] * (2.0 / (double) frameSize);
            dsp::fft (buf, +1);
            if (lv == 0)
            {
                double pk = 0.0;
                for (int i = 0; i < frameSize; ++i) pk = std::max (pk, std::abs (buf[(size_t) i].real()));
                norm = pk > 1.0e-9 ? 1.0 / pk : 1.0; // one scale for all levels: no jumps between them
            }
            auto* dst = t->mips.data() + ((size_t) (f * numLevels + lv)) * (size_t) stride;
            for (int i = 0; i < frameSize; ++i) dst[i] = (float) (buf[(size_t) i].real() * norm);
            dst[frameSize] = dst[0];
        }
    }
    // silent frames take the nearest sounding frame (a sine when the whole table is silent)
    int firstSound = -1;
    for (int f = 0; f < n && firstSound < 0; ++f)
        if (! silent[(size_t) f]) firstSound = f;
    const size_t frameFloats = (size_t) numLevels * (size_t) stride;
    if (firstSound < 0 && n > 0)
    {
        for (int lv = 0; lv < numLevels; ++lv)
        {
            auto* dst = t->mips.data() + (size_t) lv * (size_t) stride;
            for (int i = 0; i <= frameSize; ++i) dst[i] = (float) std::sin (2.0 * 3.14159265358979323846 * (double) i / (double) frameSize);
        }
        silent[0] = false;
        firstSound = 0;
    }
    for (int f = 0; f < n; ++f)
        if (silent[(size_t) f])
        {
            int src = f > 0 ? f - 1 : firstSound; // previous frames are filled already
            std::copy_n (t->mips.data() + (size_t) src * frameFloats, frameFloats, t->mips.data() + (size_t) f * frameFloats);
        }
    return t;
}

std::vector<float> UserTable::levelZero() const
{
    std::vector<float> out ((size_t) frames * (size_t) frameSize);
    for (int f = 0; f < frames; ++f) std::copy_n (frame (f, 0), frameSize, out.data() + (size_t) f * (size_t) frameSize);
    return out;
}

FormantFrame UserTable::formantsAt (float position) const noexcept
{
    if (formantFrames.empty()) return {};
    const int n = (int) formantFrames.size();
    if (n == 1) return formantFrames.front();
    const float p = std::clamp (std::isfinite (position) ? position : 0.0f, 0.0f, 1.0f) * (float) (n - 1);
    const int a = std::min ((int) p, n - 2);
    const float t = p - (float) a;
    const auto& fa = formantFrames[(size_t) a];
    const auto& fb = formantFrames[(size_t) a + 1];
    FormantFrame out;
    for (int k = 0; k < numFormants; ++k)
    {
        const auto i = (size_t) k;
        out.freq[i] = fa.freq[i] * std::pow (fb.freq[i] / fa.freq[i], t);
        out.gainDb[i] = fa.gainDb[i] + (fb.gainDb[i] - fa.gainDb[i]) * t;
        out.bandwidth[i] = fa.bandwidth[i] + (fb.bandwidth[i] - fa.bandwidth[i]) * t;
    }
    return out;
}
} // namespace ohpe::usertable
