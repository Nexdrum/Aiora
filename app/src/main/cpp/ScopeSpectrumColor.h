#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <utility>

namespace aiora {

struct ScopeSpectrumRgb {
    float r{},g{},b{};
    float opacity{}; // Absolute post-FX audio amplitude, never a color curve.
};

struct ScopeBandPoint {float nm{},level{};};

// Shape-preserving monotone cubic interpolation of the four independent
// wavelength-response bands. This is the approved Aiora perceptual DESIGN
// curve, fitted to the original pitch palette; the fourth IR-red band is
// an inferred octave-folding response, not measured infrared vision.
template<size_t N>
inline float scopeBandAt(
    const std::array<ScopeBandPoint,N>& points,float nm) noexcept {
    static_assert(N>=2);
    if(!std::isfinite(nm)||nm<points.front().nm||
       nm>points.back().nm)return 0.0f;
    const auto upper=std::upper_bound(
        points.begin(),points.end(),nm,
        [](float value,const ScopeBandPoint& p){return value<p.nm;});
    const size_t i=upper==points.begin()?0:
        std::min(static_cast<size_t>(upper-points.begin()-1),N-2);
    const float h=points[i+1].nm-points[i].nm;
    if(h<=0.0f)return 0.0f;
    const auto derivative=[&](size_t j)->float {
        if(j==0||j+1>=N)return 0.0f;
        const float h0=points[j].nm-points[j-1].nm;
        const float h1=points[j+1].nm-points[j].nm;
        const float d0=(points[j].level-points[j-1].level)/h0;
        const float d1=(points[j+1].level-points[j].level)/h1;
        if(d0*d1<=0.0f)return 0.0f;
        const float w0=2.0f*h1+h0,w1=h1+2.0f*h0;
        return (w0+w1)/(w0/d0+w1/d1);
    };
    const float t=std::clamp((nm-points[i].nm)/h,0.0f,1.0f);
    const float t2=t*t,t3=t2*t;
    return std::clamp(
        (2*t3-3*t2+1)*points[i].level+
        (t3-2*t2+t)*h*derivative(i)+
        (-2*t3+3*t2)*points[i+1].level+
        (t3-t2)*h*derivative(i+1),0.0f,1.0f);
}

inline float scopeSmoothstep(float x) noexcept {
    const float t=std::clamp(x,0.0f,1.0f);
    return t*t*(3.0f-2.0f*t);
}

// Each band has its own width and continuous falloff. Values reflect the
// previously approved C4-C5 design strip. Wavelengths are nanometers.
// Red/green/blue are sRGB display components (not literal cone responses).
inline constexpr std::array<ScopeBandPoint,18> kScopeRedBand{{
    {350,0},{490,0},{510,0},{521.1f,.2275f},
    {552.1f,.6667f},{580,1},{620,1},{645,1},
    {656.5f,.9529f},{695.6f,.7843f},
    {737,.56f},{780,.30f},{789,.24f},
    {820,.13f},{860,.04f},{900,0},{1000,0},{1100,0}
}};
inline constexpr std::array<ScopeBandPoint,15> kScopeGreenBand{{
    {350,0},{430,0},{440,0},{464.2f,.5608f},
    {491.8f,1},{510,1},{521.1f,1},{552.1f,1},{580,1},
    {584.9f,.9373f},{619.7f,.4706f},{645,0},
    {750,0},{900,0},{1100,0}
}};
inline constexpr std::array<ScopeBandPoint,15> kScopeBlueBand{{
    {340,0},{360,0},{375,0},{380,.13f},
    {390.4f,.5020f},{413.6f,.7451f},{420,.83f},
    {438.2f,.9843f},{445,1},{464.2f,1},
    {490,1},{491.8f,.9255f},{510,0},{530,0},{1100,0}
}};
inline constexpr std::array<ScopeBandPoint,17> kScopeInfraredRedBand{{
    {350,0},{490,0},{560,0},{590,.70f},{620,1},
    {645,1},{656.5f,.9529f},{695.6f,.7843f},
    {736.9f,.5961f},{780.7f,.4314f},
    {827.2f,.3882f},{876.4f,.0588f},
    {910,.007f},{940,0},{980,0},{1040,0},{1100,0}
}};

// Fold audio into C4..C5 with the exact continuous 2^x relationship.
// Double the mapped wavelength to obtain its adjacent infrared/red octave.
// Visible red and IR red merge by weighted AVERAGING. The inferred IR red
// contributes additively to the BLUE channel's independent light; it never
// raises the visible-red channel simply by stacking two reds.
inline ScopeSpectrumRgb scopeFourBandFrequencyRgb(float audioHz) noexcept {
    if(!std::isfinite(audioHz)||audioHz<=0.0f)return {};
    constexpr float kC4=261.6255653f,kC5=kC4*2.0f;
    float folded=audioHz;
    while(folded<kC4)folded*=2.0f;
    while(folded>=kC5)folded*=0.5f;
    // 2^41 is the calibrated audio-to-light octave used in the design
    // strip. Folding into C4..C5 keeps all musical octaves equivalent.
    constexpr float kLightSpeed=299792458.0f;
    const float violetNm=(kLightSpeed/std::ldexp(folded,41))*1.0e9f;
    const float redNm=violetNm*2.0f;
    const float irWeight=scopeSmoothstep((redNm-650.0f)/160.0f);

    const float redViolet=scopeBandAt(kScopeRedBand,violetNm);
    const float redVisible=scopeBandAt(kScopeRedBand,redNm);
    const float redInfrared=scopeBandAt(kScopeInfraredRedBand,redNm);
    const float red=redViolet+
        (1.0f-irWeight)*redVisible+irWeight*redInfrared;
    const float green=scopeBandAt(kScopeGreenBand,violetNm)+
        scopeBandAt(kScopeGreenBand,redNm);
    const float blue=scopeBandAt(kScopeBlueBand,violetNm)+
        scopeBandAt(kScopeBlueBand,redNm);
    return {
        std::clamp(red,0.0f,1.0f),
        std::clamp(green,0.0f,1.0f),
        std::clamp(blue,0.0f,1.0f),0.0f};
}

// Audio RMS controls opacity, never the individual color channels.
// A quiet note remains faintly visible; full opacity starts at RMS 0.16.
inline float scopeOpacityFromRms(float rms) noexcept {
    if(!std::isfinite(rms)||rms<=0.0f)return 0.0f;
    return std::pow(std::clamp(rms/0.16f,0.0f,1.0f),0.6f);
}

// The FFT observes actual final stereo samples; out-of-phase channels
// retain their individual spectral contributions. No audio-thread FFT.
class ScopeSpectrumAnalyzer {
public:
    static constexpr size_t kSamples=8192;
    ScopeSpectrumAnalyzer() noexcept {
        constexpr float pi=3.14159265358979323846f;
        for(size_t i=0;i<kSamples;++i)
            window_[i]=.5f-.5f*std::cos(
                2*pi*static_cast<float>(i)/static_cast<float>(kSamples-1));
    }
    std::array<float,kSamples>& leftSamples() noexcept {return left_;}
    std::array<float,kSamples>& rightSamples() noexcept {return right_;}
    [[nodiscard]] ScopeSpectrumRgb analyze(int sampleRate) noexcept {
        sampleRate=std::max(1,sampleRate);
        if(sampleRate!=mappedRate_){
            mappedRate_=sampleRate;
            for(size_t k=0;k<=kSamples/2;++k){
                const float hz=static_cast<float>(k)*sampleRate/
                               static_cast<float>(kSamples);
                colors_[k]=(hz>=20.0f&&hz<=20000.0f)
                    ?scopeFourBandFrequencyRgb(hz):ScopeSpectrumRgb{};
            }
        }
        for(size_t i=0;i<kSamples;++i)
            frequency_[i]={left_[i]*window_[i],right_[i]*window_[i]};
        fft();
        // Hann-window power normalization: sum of positive-frequency
        // bins approximates the mean stereo RMS^2. No per-channel exposure
        // curve or frequency-specific normalization is applied.
        constexpr float hannMeanSquare=.375f;
        constexpr float invPower=1.0f/
            (2.0f*static_cast<float>(kSamples)*
             static_cast<float>(kSamples)*hannMeanSquare);
        float totalPower=0.0f;
        ScopeSpectrumRgb weighted{};
        for(size_t k=1;k<kSamples/2;++k){
            const auto& color=colors_[k];
            const float power=(std::norm(frequency_[k])+
                std::norm(frequency_[kSamples-k]))*invPower;
            if(power<=0.0f)continue;
            totalPower+=power;
            weighted.r+=power*color.r;
            weighted.g+=power*color.g;
            weighted.b+=power*color.b;
        }
        if(totalPower<=1.0e-12f)return {}; // Pure silence = no trace.
        // One sensitive, compressed amplitude-to-opacity curve shared by
        // all RGB channels. Never change the intrinsic four-band color.
        const float alpha=scopeOpacityFromRms(std::sqrt(2.0f*totalPower));
        return {
            std::clamp(weighted.r/totalPower,0.0f,1.0f),
            std::clamp(weighted.g/totalPower,0.0f,1.0f),
            std::clamp(weighted.b/totalPower,0.0f,1.0f),
            alpha};
    }

private:
    void fft() noexcept {
        constexpr float pi=3.14159265358979323846f;
        size_t j=0;
        for(size_t i=1;i<kSamples;++i){
            size_t bit=kSamples>>1;
            while(j&bit){j^=bit;bit>>=1;}
            j^=bit;
            if(i<j)std::swap(frequency_[i],frequency_[j]);
        }
        for(size_t span=2;span<=kSamples;span<<=1){
            const float angle=-2*pi/static_cast<float>(span);
            const std::complex<float> turn(std::cos(angle),std::sin(angle));
            for(size_t start=0;start<kSamples;start+=span){
                std::complex<float> phase(1,0);
                for(size_t offset=0;offset<span/2;++offset){
                    const auto a=frequency_[start+offset];
                    const auto b=frequency_[start+offset+span/2]*phase;
                    frequency_[start+offset]=a+b;
                    frequency_[start+offset+span/2]=a-b;
                    phase*=turn;
                }
            }
        }
    }
    int mappedRate_{0};
    std::array<float,kSamples> left_{},right_{},window_{};
    std::array<std::complex<float>,kSamples> frequency_{};
    std::array<ScopeSpectrumRgb,kSamples/2+1> colors_{};
};

} // namespace aiora
