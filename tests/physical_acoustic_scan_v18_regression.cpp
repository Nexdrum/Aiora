#include "PhysicalPitchScanner.h"
#include <cassert>
#include <cstdio>
#include <chrono>
using namespace aiora::physical;
static PhysicalScanCandidate lengthCandidate(const Blueprint& b){
    const auto stamp=physicalGraphStamp(b.graph);const auto r=scanAllPhysicalPitch(b);
    assert(r.graphStamp==stamp&&physicalGraphStamp(b.graph)==stamp);
    printf("SCAN ref=%d candidates=%d modeled=%d auditioned=%d\n",b.pitch.referenceMidi,r.count,r.modelCandidates,r.auditionedCandidates);
    assert(r.count>3&&r.best[0].pitch.valid);
    for(int i=0;i<r.count;++i){const auto& c=r.best[i];if(c.pitch.trackedNode!=2||c.pitch.assignments[0].property!=PitchProperty::Length)continue;
        printf("LENGTH rank=%d model=%d verified=%d continuous=%d [%d,%d] mean_cents=%.5f renders=%d\n",i+1,c.modeledNotes,c.verifiedNotes,c.playableNotes,c.firstMidi,c.lastMidi,c.meanAbsCents,c.renderChecks);
        assert(i<3&&c.pitch.valid&&c.pitch.acousticRefined&&c.verifiedNotes>=60&&c.playableNotes>=12&&c.meanAbsCents<5.f);return c;
    }assert(false);return {};
}
int main(){Blueprint b;b.graph=integratedConstruction(acousticStringBodyGraph());const auto c=lengthCandidate(b);b.pitch=c.pitch;
    assert(preparePhysicalBendMap(b)&&physicalBendMapCurrent(b));
    float maxError=0.;int count=0;float last=100.f;
    for(int n=0;n<128;++n){if(!b.pitch.playable[n])continue;const auto z=analyzePhysicalSpectrum(b,n,true,noteFrequency12TET(n,b.pitch.a4Hz));
        assert(z.detected&&std::abs(z.cents)<=10.01f);assert(b.pitch.values[n][0]<last);last=b.pitch.values[n][0];maxError=std::max(maxError,std::abs(z.cents));++count;}
    assert(count==c.verifiedNotes);printf("INDEPENDENT keys=%d max_cents=%.5f\n",count,maxError);
    int good=0;float bendMax=0;for(float note:{50.5f,55.5f,60.5f,64.5f,68.5f,72.5f,83.5f,96.5f,100.5f}){
        const int n=int(note);assert(b.pitch.playable[n]&&b.pitch.playable[n+1]);std::array<float,kPitchSlots> v{};
        assert(physicalPitchValuesAt(b,note,v));assert(v[0]<b.pitch.values[n][0]&&v[0]>b.pitch.values[n+1][0]);
        Blueprint t=b;t.pitch.enabled=false;t.pitch.valid=false;for(int i=0;i<b.pitch.count;++i)assert(pitchWrite(t.graph,b.pitch.assignments[i],v[i]));
        const float hz=440.f*std::exp2((note-69.f)/12.f);const auto z=analyzePhysicalSpectrum(t,n,false,hz);const float error=1200.f*std::log2(z.fundamentalHz/hz);
        printf("BEND %.2f length=%.7f target=%.4f actual=%.4f cents=%.4f detected=%d\n",note,v[0],hz,z.fundamentalHz,error,z.detected);
        assert(z.detected&&std::abs(error)<15.f);bendMax=std::max(bendMax,std::abs(error));++good;}
    printf("BEND verified samples %d max %.5f\n",good,bendMax);
    EnergyMechanicalVoice voice;voice.prepare(48000);voice.start(b,60,.9f,0.f);for(int i=0;i<10000;++i)voice.render();
    std::array<float,kPitchSlots> v{};assert(physicalPitchValuesAt(b,60.5f,v));voice.setPitchBend(.5f);assert(std::abs(voice.pitchValue(0)-v[0])<1.e-6f);
    for(int i=0;i<10000;++i)assert(std::isfinite(voice.render()));auto l=voice.energyLedger();assert(std::abs(l.balanceError())<std::max(1.e-12,l.sourcePositiveWork*1.e-7));
    auto changed=b;changed.pitch.values[60][0]*=.99f;assert(!physicalBendMapCurrent(changed));
    b.pitch={};b.pitch.referenceMidi=50;lengthCandidate(b);puts("PASS full acoustic guitar scan, independent per-key render, fractional B and energy ledger");}
