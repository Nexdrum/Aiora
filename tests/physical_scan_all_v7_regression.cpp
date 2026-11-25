#include "PhysicalPitchScanner.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace aiora::physical;

static Blueprint graphWithTwoResonators(){
    Blueprint b;
    b.graph=universalEmptyGraph();
    const int wire=addPhysicalRole(b.graph,PhysicalRole::Transfer);
    const int chamber=addPhysicalRole(b.graph,PhysicalRole::Resonator);
    const int output=addPhysicalRole(b.graph,PhysicalRole::Output);
    assert(wire==1&&chamber==2&&output==3);
    auto& w=b.graph.nodes[wire];
    w.form.continuum=Continuum::Filament;
    w.material=Material::Nylon;
    w.form.lengthMeters=.65f;
    w.form.widthMeters=.001f;
    w.form.depthMeters=.001f;
    w.tensionNewtons=80.f;
    w.y=.29f;
    auto& c=b.graph.nodes[chamber];
    c.form.continuum=Continuum::Gas;
    c.material=Material::Air;
    c.form.lengthMeters=.64f;
    c.form.widthMeters=.035f;
    c.form.depthMeters=.035f;
    c.form.opening=.9f;
    c.y=.64f;
    b.graph.nodes[output].y=.93f;
    assert(b.graph.connect(0,wire));
    assert(b.graph.connect(wire,chamber));
    assert(b.graph.connect(chamber,output));
    b.pitch.trackedNode=wire;  // Scan All must NOT honor this restriction
    b.pitch.referenceMidi=60;
    b.pitch.count=1;
    b.pitch.assignments[0]={
        1,PitchProperty::Tension,1.f,400.f,1.f};
    return b;
}
int main(){
    auto source=graphWithTwoResonators();
    const auto savedStamp=physicalGraphStamp(source.graph);
    const auto savedPitch=source.pitch;
    auto scan=scanAllPhysicalPitch(source);
    std::printf("Whole graph scan: nodes=%d links=%d properties=%d modeled=%d auditioned=%d results=%d\n",
        scan.examinedNodes,scan.examinedLinks,scan.testedProperties,
        scan.modelCandidates,scan.auditionedCandidates,scan.count);
    assert(scan.graphStamp==savedStamp);
    assert(scan.examinedNodes==2);
    assert(scan.examinedLinks==3);
    assert(scan.testedProperties>=34);
    assert(scan.modelCandidates>=2);
    assert(scan.auditionedCandidates>=2);
    assert(scan.count>0);
    assert(physicalGraphStamp(source.graph)==savedStamp);
    assert(source.pitch.trackedNode==savedPitch.trackedNode);
    assert(source.pitch.count==savedPitch.count);
    assert(source.pitch.assignments[0].property==
           savedPitch.assignments[0].property);
    // Scan results are only suggestions. They don't apply until selected.
    bool sawFilament=false,sawChamber=false;
    for(int i=0;i<scan.count;++i){
        const auto& entry=scan.best[i];
        assert(entry.pitch.valid);
        assert(entry.pitch.graphStamp==savedStamp);
        assert(entry.playableNotes>=2);
        assert(entry.audibleMatches>=1);
        assert(entry.score>0.f);
        assert(entry.pitch.trackedNode!=0);
        if(entry.pitch.trackedNode==1)sawFilament=true;
        if(entry.pitch.trackedNode==2)sawChamber=true;
        // Applying a selected result changes only its assigned component,
        // not the rest of the constructed physical graph.
        Blueprint applied=source;
        applied.pitch=entry.pitch;
        assert(pitchMapCurrent(applied));
        FlowGraph adjusted=applied.graph;
        assert(applyMappedPitch(applied.pitch,adjusted,60));
        assert(adjusted.nodes[0].driver==applied.graph.nodes[0].driver);
        assert(adjusted.nodes[0].sourceDurationMs==
               applied.graph.nodes[0].sourceDurationMs);
    }
    assert(sawFilament||sawChamber);
    std::printf("Best: tracked=%d %s, %d semitones, audio %d/%d, %.2f cents, score %.2f\n",
        int(scan.best[0].pitch.trackedNode),
        pitchPropertyName(scan.best[0].pitch.assignments[0].property),
        scan.best[0].playableNotes,scan.best[0].audibleMatches,
        scan.best[0].audibleChecks,scan.best[0].meanAbsCents,
        scan.best[0].score);
    // Disconnect the chamber and it must be excluded from the search.
    auto isolated=source;
    assert(isolated.graph.disconnect(1,2));
    assert(isolated.graph.connect(1,3));
    auto second=scanAllPhysicalPitch(isolated);
    assert(second.examinedNodes==1);
    assert(second.graphStamp==physicalGraphStamp(isolated.graph));
    std::puts("PASS physical scan across connected components, acoustic ranking, no changes until Apply");
}
