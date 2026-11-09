#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <utility>

namespace aiora {

// The native oscilloscope's RGB comes from final *audio*, not MIDI notes,
// operator pitches, or piano-roll coordinates. The spectral light follows
// Nexdrum/Visualizer's octave-folded audio frequency -> visible wavelength
// mapping, including its red/violet magenta bridge.
struct ScopeSpectrumRgb { float r{},g{},b{}; };

inline ScopeSpectrumRgb visualizerFrequencyRgb(float audioHz) noexcept {
    if(!std::isfinite(audioHz)||audioHz<=0.0f)return {};
    constexpr float kLightSpeed=299792458.0f;
    constexpr float kVisibleMin=380.0e12f;
    constexpr float kVisibleMax=750.0e12f;
    float lightHz=audioHz;
    while(lightHz<kVisibleMin)lightHz*=2.0f;
    while(lightHz>kVisibleMax)lightHz*=0.5f;
    const float wl=(kLightSpeed/lightHz)*1.0e9f;
    float r=0,g=0,b=0;
    if(wl>=380&&wl<440){r=(440-wl)/60;b=1;}
    else if(wl>=440&&wl<490){g=(wl-440)/50;b=1;}
    else if(wl>=490&&wl<510){g=1;b=(510-wl)/20;}
    else if(wl>=510&&wl<580){r=(wl-510)/70;g=1;}
    else if(wl>=580&&wl<645){r=1;g=(645-wl)/65;}
    else if(wl>=645&&wl<=789){r=1;}

    float factor=1;
    if(wl>=380&&wl<420)factor=.55f+.45f*(wl-380)/40;
    else if(wl>700&&wl<=789)factor=.55f+.45f*(789-wl)/89;
    const auto gamma=[&](float x){
        return std::pow(std::clamp(x*factor,0.0f,1.0f),.82f);
    };
    r=gamma(r);g=gamma(g);b=gamma(b);
    // Match Visualizer's additive circular overlap and 8-bit RGB result.
    if(wl>=380&&wl<=430){
        const float amount=.85f*(1.0f-std::clamp((wl-380)/50,0.0f,1.0f));
        r=std::min(1.0f,r+amount);
    }
    if(wl>=700&&wl<=789){
        const float amount=.85f*std::clamp((wl-700)/89,0.0f,1.0f);
        r=std::min(1.0f,r+.55f*amount);
        b=std::min(1.0f,b+amount);
    }
    return {std::round(r*255)/255.0f,
            std::round(g*255)/255.0f,
            std::round(b*255)/255.0f};
}

// One 8192-point transform analyzes both stereo channels simultaneously,
// using left as real and right as imaginary components. Sum the conjugate
// FFT powers to recover independent stereo energy: out-of-phase content
// cannot disappear as it would in a mono (L+R) spectrum.
// All FFT work stays on the drawing/UI thread, never the audio callback.
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
                    ?visualizerFrequencyRgb(hz):ScopeSpectrumRgb{};
            }
        }
        for(size_t i=0;i<kSamples;++i)
            frequency_[i]={
                left_[i]*window_[i],
                right_[i]*window_[i]};
        fft();
        constexpr float hannMeanSquare=.375f;
        constexpr float invPower=1.0f/
            (2.0f*static_cast<float>(kSamples)*static_cast<float>(kSamples)*hannMeanSquare);
        ScopeSpectrumRgb energy{};
        for(size_t k=1;k<kSamples/2;++k){
            const auto& color=colors_[k];
            if(color.r==0&&color.g==0&&color.b==0)continue;
            const float power=(std::norm(frequency_[k])+
                std::norm(frequency_[kSamples-k]))*invPower;
            energy.r+=power*color.r;
            energy.g+=power*color.g;
            energy.b+=power*color.b;
        }
        // Sum linear spectral powers first. Compress only after addition,
        // allowing individual overtones to contribute without clipping an
        // entire chord or noisy timbre to white at ordinary signal levels.
        const auto illuminate=[](float p){
            return 1.0f-std::exp(-14.0f*std::sqrt(std::max(0.0f,p)));
        };
        return {illuminate(energy.r),illuminate(energy.g),illuminate(energy.b)};
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
