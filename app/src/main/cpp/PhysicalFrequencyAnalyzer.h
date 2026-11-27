#pragma once
// Offline physical-instrument frequency analyzer. Audio is actually rendered
// through MaterialTimeVoice, downsampled and transformed with a Hann window.
// The tracker searches around an explicitly selected physical resonator;
// arbitrary strongly inharmonic systems can have no unique fundamental.
#include "MaterialTimeVoice.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>

namespace aiora::physical {

struct PitchSpectrum {
    float fundamentalHz{0.f};
    float strongestHz{0.f};
    float expectedHz{0.f};
    float confidence{0.f};
    float cents{0.f};
    float rms{0.f};
    std::array<float,12> bands{};
    bool detected{false};
};
inline PitchSpectrum analyzePhysicalSpectrum(
    const Blueprint& blueprint,int midi,bool tuned=false) {
    PitchSpectrum out{};
    constexpr int kN=8192,kRecorded=4096,kDecimate=4,kSampleRate=32000;
    constexpr float kRate=static_cast<float>(kSampleRate)/kDecimate;
    Blueprint probe=blueprint;
    // When tuning=true, estimate the full sound with the assigned physical
    // note settings. Otherwise analyze the untouched constructed instrument.
    if(!tuned){probe.pitch.valid=false;probe.pitch.enabled=false;}
    FlowGraph effective=probe.graph;
    if(tuned&&pitchMapCurrent(probe))
        applyMappedPitch(probe.pitch,effective,midi);
    out.expectedHz=trackedFundamental(effective,probe.pitch.trackedNode);
    if(!(out.expectedHz>15.f&&out.expectedHz<3500.f))return out;
    MaterialTimeVoice voice;
    voice.prepare(static_cast<float>(kSampleRate));
    voice.start(probe,midi,.90f,.0f);
    std::array<std::complex<float>,kN> spectrum{};
    // Skip the startup/first transit; for impulses an early window captures
    // the decay, while sustained sources continue providing energy.
    for(int i=0;i<700;++i)(void)voice.render();
    double sumSquares=0.0;
    for(int i=0;i<kRecorded;++i){
        float sample=0.f;
        for(int j=0;j<kDecimate;++j)
            sample+=voice.render();
        sample*=1.f/kDecimate;
        sumSquares+=double(sample)*sample;
        const float hann=.5f-.5f*std::cos(
            6.2831853071795864769f*i/(kRecorded-1.f));
        spectrum[i]=std::complex<float>(sample*hann,0.f);
    }
    out.rms=static_cast<float>(std::sqrt(sumSquares/kRecorded));
    if(out.rms<1.e-8f)return out;

    // Allocation-free in-place Cooley-Tukey FFT (analysis is on the UI
    // thread only, NEVER the real-time audio callback).
    for(int i=1,j=0;i<kN;++i){
        int bit=kN>>1;
        for(;j&bit;bit>>=1)j^=bit;
        j^=bit;
        if(i<j)std::swap(spectrum[i],spectrum[j]);
    }
    for(int len=2;len<=kN;len<<=1){
        const float angle=-6.2831853071795864769f/len;
        const std::complex<float> step(std::cos(angle),std::sin(angle));
        for(int i=0;i<kN;i+=len){
            std::complex<float> turn(1.f,0.f);
            for(int j=0;j<len/2;++j){
                const auto a=spectrum[i+j];
                const auto b=spectrum[i+j+len/2]*turn;
                spectrum[i+j]=a+b;
                spectrum[i+j+len/2]=a-b;
                turn*=step;
            }
        }
    }
    constexpr float binHz=kRate/kN;
    std::array<float,kN/2> magnitudes{};
    float strongest=0.f;
    int strongestBin=0;
    for(int k=1;k<kN/2;++k){
        const float hz=k*binHz;
        const float magnitude=std::abs(spectrum[k]);
        magnitudes[k]=magnitude;
        if(hz>=35.f&&hz<=3500.f&&magnitude>strongest){
            strongest=magnitude;strongestBin=k;
        }
        if(hz>=35.f&&hz<3200.f){
            const int band=std::clamp(static_cast<int>(
                12.f*std::log2(hz/35.f)/std::log2(3200.f/35.f)),0,11);
            out.bands[static_cast<size_t>(band)]=
                std::max(out.bands[static_cast<size_t>(band)],magnitude);
        }
    }
    if(strongest<1.e-7f)return out;
    out.strongestHz=strongestBin*binHz;
    for(auto& band:out.bands)
        band=std::sqrt(std::clamp(band/strongest,0.f,1.f));

    // Prefer the calculated physical mode's neighborhood, NOT the highest
    // overtone. This estimate deliberately does not invent a fundamental
    // if the expected modal peak is absent from the resulting sound.
    const int lo=std::max(2,static_cast<int>(
        out.expectedHz*.82f/binHz));
    const int hi=std::min(kN/2-3,static_cast<int>(
        out.expectedHz*1.18f/binHz));
    int best=lo;
    for(int k=lo;k<=hi;++k)
        if(magnitudes[k]>magnitudes[best])best=k;
    // A parabolic interpolation of log spectral amplitude supplies a
    // sub-bin frequency estimate, with a bounded interpolated correction.
    const float l=std::log(magnitudes[best-1]+1.e-12f);
    const float c=std::log(magnitudes[best]+1.e-12f);
    const float r=std::log(magnitudes[best+1]+1.e-12f);
    const float denom=l-2.f*c+r;
    const float shift=std::fabs(denom)>1.e-6f?
        std::clamp(.5f*(l-r)/denom,-.5f,.5f):0.f;
    out.fundamentalHz=(best+shift)*binHz;
    const float relative=magnitudes[best]/strongest;
    const float expectedError=std::fabs(
        1200.f*std::log2(out.fundamentalHz/out.expectedHz));
    out.confidence=std::clamp(relative*
        (1.f-std::min(1.f,expectedError/390.f)),0.f,1.f);
    out.cents=1200.f*std::log2(out.fundamentalHz/
        noteFrequency12TET(midi,blueprint.pitch.a4Hz));
    out.detected=out.confidence>.08f&&
        std::isfinite(out.fundamentalHz);
    return out;
}
inline bool findPhysicalFrequency(Blueprint& blueprint) {
    auto& calibration=blueprint.pitch;
    calibration.valid=false;
    calibration.measuredHz=0.f;
    calibration.confidence=0.f;
    // The first measurement is the actual untouched instrument response.
    const auto before=analyzePhysicalSpectrum(blueprint,
        std::clamp<int>(calibration.referenceMidi,0,127),false);
    // An inharmonic or weakly radiating object may have an ambiguous heard
    // fundamental. The physical modal solver can still propose a mapping;
    // its acoustic verification is reported separately, never fabricated.
    if(before.detected){
        calibration.measuredHz=before.fundamentalHz;
        calibration.confidence=before.confidence;
    }

    if(!buildPhysicalKeyboard(blueprint.graph,calibration))
        return false;
    // Verify the keyboard-controlled physical settings by rendering the
    // instrument again. Retune the mechanism; never shift the final audio.
    const auto after=analyzePhysicalSpectrum(blueprint,
        calibration.referenceMidi,true);
    if(after.detected){
        calibration.measuredHz=after.fundamentalHz;
        calibration.confidence=after.confidence;
        calibration.centsError=after.cents;
    }else{
        calibration.measuredHz=0.f;
        calibration.confidence=0.f;
    }
    // The approximate modal solver may agree mathematically while the
    // complete system's output is dominated by a different resonance.
    // Mark this as unverified rather than falsely claiming acoustic accuracy.
    if(calibration.valid)
        preparePhysicalBendMap(blueprint); // UI/offline, not render()
    return calibration.valid;
}
} // namespace aiora::physical
