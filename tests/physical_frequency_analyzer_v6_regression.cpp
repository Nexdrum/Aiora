#include "PhysicalFrequencyAnalyzer.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace aiora::physical;
int main(){
    Blueprint b;
    b.graph=universalEmptyGraph();
    int wire=addPhysicalRole(b.graph,PhysicalRole::Resonator);
    int out=addPhysicalRole(b.graph,PhysicalRole::Output);
    auto& n=b.graph.nodes[wire];
    n.form.continuum=Continuum::Filament;
    n.material=Material::Nylon;
    n.form.lengthMeters=.65f;
    n.form.widthMeters=.001f;n.form.depthMeters=.001f;
    n.tensionNewtons=80.f;
    n.y=.40f;b.graph.nodes[out].y=.91f;
    assert(b.graph.connect(0,wire));
    assert(b.graph.connect(wire,out));
    b.pitch.referenceMidi=60;
    b.pitch.trackedNode=wire;
    b.pitch.count=1;
    b.pitch.assignments[0]={
        static_cast<int8_t>(wire),PitchProperty::Tension,1.f,400.f,1.f};
    auto before=analyzePhysicalSpectrum(b,60,false);
    std::printf("before: model=%.6f detected=%.6f, dominant=%.6f, confidence=%.4f, rms=%.6g\n",
        before.expectedHz,before.fundamentalHz,before.strongestHz,
        before.confidence,before.rms);
    bool calibrated=findPhysicalFrequency(b);
    std::printf("after: calibrated=%d, model=%.6f, heard=%.6f, errCents=%.3f, conf=%.4f, audible=%d\n",
        calibrated,b.pitch.modelHz,b.pitch.measuredHz,b.pitch.centsError,
        b.pitch.confidence,physicalGraphStamp(b.graph)==b.pitch.graphStamp);
    assert(calibrated);
    assert(pitchMapCurrent(b));
    assert(b.pitch.playable[60]&&b.pitch.playable[69]);
    const auto after=analyzePhysicalSpectrum(b,60,true);
    assert(after.rms>0.f);
    assert(std::isfinite(after.cents));
    std::printf("PASS spectrum + Find Frequency from modelled instrument\n");
}
