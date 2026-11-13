// Reproducible offline render + timing harness for the approved v3 cymbal
// auditions. B, V and M all vary continuously during each render.
// This deliberately does not change the synthesis engine or its sample rate.
#include "ProjectCodec.h"
#include "SpectrachordVoice.h"
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace aiora;

namespace {
Patch importPatch(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error(std::string("Cannot read ")+path);
    const std::string json((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    Patch patch;
    std::string error;
    if (!deserializePatchJson(json, patch, &error))
        throw std::runtime_error(error);
    return patch;
}

DspCurve automation(std::initializer_list<DspCurvePoint> points) {
    DspCurve curve;
    curve.storage=std::make_shared<std::vector<DspCurvePoint>>(points);
    curve.points=curve.storage->data();
    curve.count=curve.storage->size();
    return curve;
}

VoiceAutomation expressiveAutomation() {
    VoiceAutomation a;
    // At 48k samples per step, one grid-step spans one second.
    a.bend=automation({{0,0,.5f},{.24f,2.5f,.36f},{.7f,-1.75f,.65f},
                       {1.18f,.8f,.5f},{2.25f,-2.25f,.45f},
                       {4.0f,.6f,.5f},{7.0f,0,.5f}});
    a.velocity=automation({{0,.15f,.5f},{.16f,.98f,.6f},
                           {.68f,.38f,.28f},{1.32f,.9f,.5f},
                           {2.3f,.55f,.5f},{4.0f,.75f,.5f},
                           {7.0f,.3f,.5f}});
    a.mod=automation({{0,0,.5f},{.30f,.8f,.43f},
                      {1.0f,.26f,.55f},{1.8f,1,.46f},
                      {3.1f,.36f,.5f},{5.4f,.72f,.5f},
                      {7.0f,.08f,.5f}});
    return a;
}

void renderCase(const char* name,const DspPatch& patch,
                int midi,int voiceCount,int frames) {
    const auto expression=expressiveAutomation();
    std::vector<SpectrachordVoice> voices(static_cast<size_t>(voiceCount));
    for(int i=0;i<voiceCount;++i) {
        auto& v=voices[static_cast<size_t>(i)];
        v.prepare(48000);
        // Different voice IDs exercise independent deterministic texture seeds.
        v.start(741+i,patch,midi+i%3,.8f,0u,48000.0f,expression);
    }
    constexpr uint64_t kOffset=14695981039346656037ull;
    constexpr uint64_t kPrime=1099511628211ull;
    uint64_t hash=kOffset;
    double energy=0.0;
    const auto begin=std::chrono::steady_clock::now();
    for(int frame=0;frame<frames;++frame) {
        float out=0;
        for(auto& voice:voices) out+=voice.render();
        if(!std::isfinite(out)) throw std::runtime_error("Non-finite output");
        hash^=std::bit_cast<uint32_t>(out);
        hash*=kPrime;
        energy+=static_cast<double>(out)*out;
    }
    const auto end=std::chrono::steady_clock::now();
    const double milliseconds=std::chrono::duration<double,std::milli>(end-begin).count();
    if(!(energy>0))throw std::runtime_error("Silent benchmark");
    std::cout << "DSP_BENCH " << name
              << " voices="<<voiceCount<<" frames="<<frames
              << " hash="<<std::hex<<std::setfill('0')<<std::setw(16)<<hash
              <<std::dec<<" ms="<<std::fixed<<std::setprecision(3)<<milliseconds
              <<" energy="<<std::setprecision(8)<<energy<<"\n";
}
}

int main() {
    try {
        const Patch crash=importPatch("auditions/nexdrum/Crash_Metallic_Bloom_v3.json");
        const Patch ride=importPatch("auditions/nexdrum/Ride_Heavy_Bow_v3.json");
        const DspPatch c=toDspPatch(crash),r=toDspPatch(ride);
        renderCase("crash-v3",c,80,1,96000);
        renderCase("ride-v3",r,73,1,96000);
        renderCase("ride-v3-long",r,73,1,340000);
        renderCase("ride-v3-four",r,73,4,48000);
        renderCase("ride-v3-eight",r,73,8,48000);
        DspPatch rideNoFilter=r;
        rideNoFilter.filter.evolving.configured=false;
        renderCase("ride-v3-no-filter",rideNoFilter,73,8,48000);
    } catch (const std::exception& e) {
        std::cerr << "DSP_BENCH error: "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
