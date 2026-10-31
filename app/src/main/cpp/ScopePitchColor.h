#pragma once
#include <algorithm>
#include <array>
#include <cstddef>

namespace aiora {

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
