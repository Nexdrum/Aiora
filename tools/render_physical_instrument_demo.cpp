// Standalone offline audition for the experimental PhysicalInstrumentVoice.
// Build: c++ -O2 -std=c++20 -Iapp/src/main/cpp tools/render_physical_instrument_demo.cpp -o /tmp/physical-demo
// Usage: /tmp/physical-demo output.wav
#include "PhysicalInstrumentVoice.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
void put16(std::ofstream& out, uint16_t x) { for (int n=0;n<2;++n) out.put(static_cast<char>((x >> (n*8)) & 255)); }
void put32(std::ofstream& out, uint32_t x) { for (int n=0;n<4;++n) out.put(static_cast<char>((x >> (n*8)) & 255)); }
void saveWav(const std::string& path, const std::vector<float>& samples, int sr) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Could not open WAV output");
    const auto dataBytes=static_cast<uint32_t>(samples.size()*2);
    out.write("RIFF",4);put32(out,36+dataBytes);out.write("WAVEfmt ",8);
    put32(out,16);put16(out,1);put16(out,1);put32(out,sr);put32(out,sr*2);put16(out,2);put16(out,16);
    out.write("data",4);put32(out,dataBytes);
    for (float sample:samples) {
        const float v=std::clamp(sample,-1.0f,1.0f);
        const auto q=static_cast<int16_t>(std::lround(v*32760.0f));
        put16(out,static_cast<uint16_t>(q));
    }
}
}
int main(int argc, char** argv) {
    if(argc<2){ std::cerr<<"Usage: physical-demo output.wav\n";return 2; }
    constexpr int sr=48000;
    std::vector<float> combined;
    auto render = [&](const char* label, aiora::physical::Blueprint bp, int midi,
                      float e1, float muteBefore, float muteAfter, float changeAt) {
        using namespace aiora::physical;
        PhysicalInstrumentVoice voice;
        voice.prepare(sr);
        voice.start(bp,midi,e1,muteBefore);
        constexpr int length=sr*3;
        std::vector<float> samples(length);
        for(int i=0;i<length;++i) {
            if(i==static_cast<int>(changeAt*sr))voice.setExpression(e1,muteAfter);
            samples[i]=voice.render();
        }
        // Equal fixed gain across passages, then master peak normalize only.
        combined.insert(combined.end(),samples.begin(),samples.end());
        combined.insert(combined.end(),sr/3,0.0f);
        double power=0;float maxAmp=0;
        for(float x:samples){power+=x*x;maxAmp=std::max(maxAmp,std::fabs(x));}
        std::cout<<label<<"  RMS="<<std::sqrt(power/length)<<"  peak="<<maxAmp
                 <<"  length="<<voice.effectiveLengthMeters()<<"m\n";
    };

    aiora::physical::Blueprint guitar;
    guitar.excitation=aiora::physical::Excitation::Pluck;
    guitar.stringMaterial=aiora::physical::Material::Steel;
    guitar.bodyMaterial=aiora::physical::Material::Wood;
    guitar.bodyGeometry=aiora::physical::Geometry::HollowBox;
    render("Steel pluck, open",guitar,57,0.85f,0.0f,0.0f,2.9f);
    render("Steel pluck, palm mute added at 0.6s",guitar,57,0.85f,0.0f,0.95f,0.6f);
    guitar.stringMaterial=aiora::physical::Material::Nylon;
    guitar.stringDiameterMm=0.75f;
    render("Nylon pluck",guitar,57,0.85f,0.0f,0.0f,2.9f);
    guitar.stringMaterial=aiora::physical::Material::Steel;
    guitar.stringDiameterMm=0.5f;
    guitar.excitation=aiora::physical::Excitation::Hammer;
    guitar.exciterMaterial=aiora::physical::Material::Wood;
    guitar.hammerHardness=0.9f;
    render("Hard wood hammer on steel",guitar,57,0.85f,0.0f,0.0f,2.9f);
    guitar.hammerHardness=0.15f;
    guitar.exciterMaterial=aiora::physical::Material::Felt;
    render("Soft felt hammer on steel",guitar,57,0.85f,0.0f,0.0f,2.9f);
    guitar.excitation=aiora::physical::Excitation::Pluck;
    guitar.bodyMaterial=aiora::physical::Material::Bronze;
    guitar.bodyGeometry=aiora::physical::Geometry::Bell;
    render("Steel string to bronze bell",guitar,57,0.85f,0.0f,0.0f,2.9f);

    float peak=0.0f;
    for(float x:combined)peak=std::max(peak,std::fabs(x));
    const float gain=peak>0.00001f?0.83f/peak:1.0f;
    for(float& x:combined)x*=gain;
    saveWav(argv[1],combined,sr);
    std::cout<<"Saved "<<argv[1]<<" with fixed gain="<<gain<<"\n";
}
