// Reproducible offline render + timing harness for the approved v3 cymbal
// auditions. B, V and M all vary continuously during each render.
// This deliberately does not change the synthesis engine or its sample rate.
#include "ProjectCodec.h"
#include "SpectrachordVoice.h"
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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
    a.bend=automation({{0,0,.5f},{.24f,2.5f,.36f,1.25f,true},{.7f,-1.75f,.65f},
                       {1.18f,.8f,.5f},{2.25f,-2.25f,.45f},
                       {4.0f,.6f,.5f},{7.0f,0,.5f}});
    a.velocity=automation({{0,.15f,.5f},{.16f,.98f,.6f},
                           {.68f,.38f,.28f,.52f,true},{1.32f,.9f,.5f},
                           {2.3f,.55f,.5f},{4.0f,.75f,.5f},
                           {7.0f,.3f,.5f}});
    a.mod=automation({{0,0,.5f},{.30f,.8f,.43f},
                      {1.0f,.26f,.55f,.38f,true},{1.8f,1,.46f},
                      {3.1f,.36f,.5f},{5.4f,.72f,.5f},
                      {7.0f,.08f,.5f}});
    return a;
}

void renderCase(const char* name,const DspPatch& patch,
                int midi,int voiceCount,int frames,int releaseFrame=-1,
                bool automateExpression=true,bool liveUpdates=false) {
    const auto expression=automateExpression?expressiveAutomation():VoiceAutomation{};
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
    std::vector<float> pcm;
    if(std::getenv("DSP_BENCH_PCM_DIR"))pcm.reserve(static_cast<size_t>(frames));
    const auto begin=std::chrono::steady_clock::now();
    for(int frame=0;frame<frames;++frame) {
        if(frame==releaseFrame)for(auto& voice:voices)voice.release();
        // Exercise abrupt real-time B/V/M-independent live V and M changes.
        if(liveUpdates&&frame%251==0){
            const float v=.10f+.24f*static_cast<float>((frame/251)%4);
            const float m=.03f+.14f*static_cast<float>((frame/251)%7);
            for(auto& voice:voices)voice.setLiveExpression(v,m);
        }
        float out=0;
        for(auto& voice:voices) out+=voice.render();
        if(!std::isfinite(out)) throw std::runtime_error("Non-finite output");
        if(!pcm.empty()||std::getenv("DSP_BENCH_PCM_DIR"))pcm.push_back(out);
        hash^=std::bit_cast<uint32_t>(out);
        hash*=kPrime;
        energy+=static_cast<double>(out)*out;
    }
    const auto end=std::chrono::steady_clock::now();
    const double milliseconds=std::chrono::duration<double,std::milli>(end-begin).count();
    if(!(energy>0))throw std::runtime_error("Silent benchmark");
    if(const char* root=std::getenv("DSP_BENCH_PCM_DIR")){
        std::ofstream file(std::string(root)+"/"+name+".f32",std::ios::binary);
        if(!file)throw std::runtime_error("Cannot write PCM benchmark");
        file.write(reinterpret_cast<const char*>(pcm.data()),
                   static_cast<std::streamsize>(pcm.size()*sizeof(float)));
    }
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
        // A disabled evolving filter does not define a dry bypass for this
        // renderer: it produces silence. It is not a valid CPU comparison.
        // Keep audible reference cases for exact waveform matching.
        renderCase("ride-v3-static-vm",r,73,8,48000,-1,false);
        renderCase("ride-v3-live-expression",r,73,2,48000,-1,false,true);
        // Held input tests exact-bit V/M response cache reuse.
        DspPatch simple=toDspPatch(crash);
        renderCase("crash-v3-early-release",simple,80,1,48000,9000);
        // Exercise the dynamic (not cached) pitch-LFO and unison paths.
        Patch modulated=crash;
        modulated.lfo.target=LfoTarget::Pitch;
        modulated.lfo.amount=.65f;
        modulated.lfo.rate=6.8f;
        modulated.unison=.37f;
        modulated.unisonCurve=initialUnisonCurve(.37f);
        renderCase("crash-pitch-lfo-unison",toDspPatch(modulated),80,2,48000);
        // The shared played-note frequency is also used by keytrack and
        // the alternative pitched peak-EQ route.
        Patch keytracked=ride;
        keytracked.filter.evolving.keyTrack=true;
        renderCase("ride-keytracked-filter",toDspPatch(keytracked),73,2,48000);
        // Note-following peak EQ requires an audible main filter path in
        // this renderer. Avoid counting a completely silent signal as a valid
        // audio-fidelity test.
    } catch (const std::exception& e) {
        std::cerr << "DSP_BENCH error: "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
