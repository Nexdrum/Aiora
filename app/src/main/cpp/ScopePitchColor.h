#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

namespace aiora {

// Split each sounding voice's instantaneous *musical* pitch across the two
// neighboring chromatic colors. The synth publishes fractional MIDI pitch
// after bend/LFO, rather than the note-on key. Wrap B -> C across octaves.
// Keeping the 12-class light buffer also preserves existing additive mixing.
inline void addScopePitchLevel(std::array<float,12>& levels,
                              float midiPitch,float level) noexcept {
    if(!std::isfinite(midiPitch)||!std::isfinite(level)||level<=0.0f)return;
    const int lower=static_cast<int>(std::floor(midiPitch));
    const float fraction=midiPitch-static_cast<float>(lower);
    const int first=((lower%12)+12)%12;
    const int second=(first+1)%12;
    levels[static_cast<std::size_t>(first)]+=level*(1.0f-fraction);
    levels[static_cast<std::size_t>(second)]+=level*fraction;
}

// Add light from each currently sounding pitch class in RGB, clipping each
// channel at full brightness. Unlike averaging, simultaneous pitches retain
// their additive Spectrachord-style color mixture.
template <typename Rgb>
Rgb additiveScopePitchColor(const std::array<Rgb,12>& palette,
                            const std::array<float,12>& levels) noexcept {
    Rgb out{0.0f,0.0f,0.0f};
    for(std::size_t i=0;i<levels.size();++i){
        const float intensity=std::clamp(levels[i],0.0f,1.0f);
        out.r=std::min(1.0f,out.r+palette[i].r*intensity);
        out.g=std::min(1.0f,out.g+palette[i].g*intensity);
        out.b=std::min(1.0f,out.b+palette[i].b*intensity);
    }
    return out;
}

} // namespace aiora
