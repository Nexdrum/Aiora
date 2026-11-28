#pragma once
// Experimental whole-instrument physical pitch search, v7.
// This searches every energy-connected universal vibrating medium and each
// supported physical property; link controls are explicitly examined but
// cannot be used to counterfeit tuning if current reduced modal physics
// contains no link-to-frequency coupling. Candidates are verified by
// generating actual output audio at multiple keyboard notes.
#include "PhysicalFrequencyAnalyzer.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace aiora::physical {

constexpr int kScanResults=8;
struct PhysicalScanCandidate {
    PitchCalibration pitch{};
    std::array<float,12> spectrum{};
    int playableNotes{0};  // contiguous playable notes at reference
    int modeledNotes{0},verifiedNotes{0},renderChecks{0};
    bool acousticAttempted{false};
    int firstMidi{-1},lastMidi{-1};
    int audibleChecks{0},audibleMatches{0};
    float meanAbsCents{0.f};
    float confidence{0.f};
    float meanRms{0.f};
    float score{0.f};
    bool audibleVerified{false};
};
struct PhysicalScanResults {
    std::vector<PhysicalScanCandidate> best{};
    int count{0};
    int examinedNodes{0},examinedLinks{0},testedProperties{0};
    int modelCandidates{0},auditionedCandidates{0};
    uint32_t graphStamp{0};
    int referenceMidi{60};
    float a4Hz{440.f};
};
struct PhysicalScanJob {
    std::atomic<bool> ready{false};
    PhysicalScanResults result{};
};
inline bool scanLinkInEnergyPath(const FlowGraph& g,int id) noexcept {
    if(id<0||id>=g.linkCount)return false;
    const auto& e=g.links[id];
    return e.from<g.nodeCount&&e.to<g.nodeCount&&
        pitchHasCompleteEnergyPath(g,e.from)&&
        pitchHasCompleteEnergyPath(g,e.to);
}
inline int contiguousPitchSpan(const PitchCalibration& pitch,
                              int& first,int& last) noexcept {
    const int reference=std::clamp<int>(pitch.referenceMidi,0,127);
    first=last=-1;
    if(!pitch.playable[reference])return 0;
    first=last=reference;
    while(first>0&&pitch.playable[first-1])--first;
    while(last<127&&pitch.playable[last+1])++last;
    return last-first+1;
}
inline void scanRankInsert(PhysicalScanResults& result,
                           const PhysicalScanCandidate& entry) {
    if(!(entry.score>0.f))return;
    int at=0;
    while(at<result.count&&result.best[at].score>=entry.score)++at;
    if(at>=kScanResults)return;
    result.best.insert(result.best.begin()+at,entry);
    if(result.best.size()>kScanResults)result.best.pop_back();
    result.count=static_cast<int>(result.best.size());
}
// The quick model test is performed on EACH accessible physical component;
// it avoids 128-note+FFT analysis of every property that cannot affect that
// resonator's frequency. Every link on a complete energy path is inspected
// but v6 has no acoustic eigenfrequency law for link position/aperture.
inline bool scanParameterResponse(const FlowGraph& graph,int tracked,
                                  const PitchAssignment& assignment) noexcept {
    if(!pitchTargetValid(graph,assignment))return false;
    const float base=trackedFundamental(graph,tracked);
    if(base<=0.f)return false;
    const auto bounds=pitchPropertyBounds(assignment.property);
    FlowGraph tmp=graph;
    pitchWrite(tmp,assignment,bounds[0]);
    const float lo=trackedFundamental(tmp,tracked);
    pitchWrite(tmp,assignment,bounds[1]);
    const float hi=trackedFundamental(tmp,tracked);
    if(lo<=0.f||hi<=0.f)return false;
    return std::fabs(1200.f*std::log2(hi/lo))>80.f;
}
inline PhysicalScanCandidate evaluateScanPitch(
    const Blueprint& blueprint,PitchCalibration pitch) {
    PhysicalScanCandidate result{};
    if(!buildPhysicalKeyboard(blueprint.graph,pitch))return result;
    result.pitch=pitch;
    result.playableNotes=contiguousPitchSpan(pitch,result.firstMidi,result.lastMidi);
    if(result.playableNotes<2)return result;
    // Probe lower, central and upper notes across the ACTUALLY reachable
    // register. Never extrapolate measured audibility to all 128 keys.
    const int reference=std::clamp<int>(pitch.referenceMidi,0,127);
    const int lower=std::max(result.firstMidi,reference-12);
    const int upper=std::min(result.lastMidi,reference+12);
    std::array<int,3> probes{lower,reference,upper};
    const float referenceRms=0.0002f;
    float confidenceSum=0.f,errorSum=0.f,rmsSum=0.f;
    int measured=0;
    for(int note:probes){
        if(!pitch.playable[note])continue;
        Blueprint test=blueprint;
        test.pitch=pitch;
        const auto spectrum=analyzePhysicalSpectrum(test,note,true);
        ++result.audibleChecks;
        if(note==reference){
            result.spectrum=spectrum.bands;
            result.pitch.measuredHz=spectrum.fundamentalHz;
            result.pitch.confidence=spectrum.confidence;
            result.pitch.centsError=spectrum.cents;
        }
        if(!spectrum.detected||!std::isfinite(spectrum.cents))continue;
        // Penalize wrong overtones, weak spectral evidence and poor loudness.
        // No candidate is "audible matched" just because the analytic
        // physical-frequency equation predicts a mathematically exact note.
        if(std::fabs(spectrum.cents)>35.f||spectrum.rms<1.e-6f||
           spectrum.confidence<.10f)continue;
        ++result.audibleMatches;++measured;
        confidenceSum+=spectrum.confidence;
        errorSum+=std::fabs(spectrum.cents);
        rmsSum+=spectrum.rms;
    }
    if(measured==0)return result;
    result.meanAbsCents=errorSum/measured;
    result.confidence=confidenceSum/measured;
    result.meanRms=rmsSum/measured;
    const float audibleFraction=static_cast<float>(result.audibleMatches)/
                                std::max(1,result.audibleChecks);
    const float pitchAccuracy=std::clamp(1.f-result.meanAbsCents/35.f,0.f,1.f);
    const float level=std::clamp(result.meanRms/referenceRms,0.f,1.f);
    const float quality=audibleFraction*
        (.60f*result.confidence+.24f*pitchAccuracy+.16f*level);
    result.audibleVerified=measured==result.audibleChecks&&measured>=2;
    // Real playable semitone span carries most weight. A broad model range
    // with weak/out-of-tune sound falls behind a better audible mechanism.
    result.score=result.playableNotes*quality;
    return result;
}
// Build a real keyboard table: every marked playable note has been rendered
// and matched, rather than extrapolating three probes across a modeled span.
inline PhysicalScanCandidate evaluateAcousticScanPitch(const Blueprint& source,PitchCalibration pitch) {
    PhysicalScanCandidate result{};result.acousticAttempted=true;
    (void)buildPhysicalKeyboard(source.graph,pitch);
    result.pitch=pitch;
    const auto modelPlayable=pitch.playable;
    for(auto x:modelPlayable)result.modeledNotes+=x!=0;
    if(result.modeledNotes<2)return result;
    pitch.playable.fill(0);pitch.valid=false;pitch.enabled=false;pitch.acousticRefined=true;
    const int ref=std::clamp<int>(pitch.referenceMidi,0,127);
    float correction=0.f;PitchSpectrum refSpectrum{};
    auto tryNote=[&](int note,float& seed,PitchSpectrum& z){
        ++result.audibleChecks;
        float updated=seed;
        bool ok=refineAcousticNote(source,pitch,note,seed,z,updated,result.renderChecks);
        if(!ok&&std::abs(seed)>10.f)ok=refineAcousticNote(source,pitch,note,0.f,z,updated,result.renderChecks);
        if(!ok&&std::abs(seed)>10.f)for(float shift:{-45.f,45.f,-90.f,90.f}){
            if(refineAcousticNote(source,pitch,note,seed+shift,z,updated,result.renderChecks)){ok=true;break;}
        }
        if(!ok)return false;
        // A fixed body resonance can coincide with a key, yet not follow
        // the controlled object. Reject reversals or near-stationary
        // parameter solutions relative to the nearest confirmed note.
        for(int distance=1;distance<128;++distance){
            int other=-1;for(int n:{note-distance,note+distance})if(n>=0&&n<128&&pitch.playable[n]){other=n;break;}
            if(other<0)continue;
            const float step=100.f*(note-other);
            FlowGraph a=source.graph,b=source.graph;
            for(int k=0;k<pitch.count;++k){pitchWrite(a,pitch.assignments[k],pitch.values[note][k]);pitchWrite(b,pitch.assignments[k],pitch.values[other][k]);}
            const float movement=1200.f*std::log2(trackedFundamental(a,pitch.trackedNode)/trackedFundamental(b,pitch.trackedNode));
            const float slope=movement/step;
            if(slope<.15f||slope>3.f){pitch.playable[note]=0;return false;}
            break;
        }
        seed=updated;++result.audibleMatches;++result.verifiedNotes;
        result.meanAbsCents+=std::abs(z.cents);result.confidence+=z.confidence;result.meanRms+=z.rms;
        return true;
    };
    // Use a stable middle-C anchor when both it and the chosen reference
    // lie in the modeled range. Changing the reference should not change
    // branch continuation throughout an otherwise identical keyboard.
    int anchor=modelPlayable[60]&&modelPlayable[ref]?60:ref;
    PitchSpectrum anchorSpectrum{};
    if(!tryNote(anchor,correction,anchorSpectrum)){
        if(anchor==ref||!tryNote(ref,correction,anchorSpectrum)){result.pitch=pitch;return result;}
        anchor=ref;
    }
    if(anchor==ref)refSpectrum=anchorSpectrum;
    // Seek in both directions, using the preceding measured correction as
    // a seed. A failed note remains a hole; it does not conceal later notes.
    for(int direction:{-1,1}){
        float seed=correction;
        for(int note=anchor+direction;note>=0&&note<128;note+=direction){
            const float hz=noteFrequency12TET(note,pitch.a4Hz);
            if(hz<25.f||hz>3500.f)break;
            if(!modelPlayable[note]){
                // A coupled shift can extend the isolated boundary; try
                // within three semitones of any model-reachable note.
                bool near=false;for(int j=std::max(0,note-3);j<=std::min(127,note+3);++j)near|=modelPlayable[j]!=0;
                if(!near)continue;
            }
            PitchSpectrum z{};if(tryNote(note,seed,z)&&note==ref)refSpectrum=z;
        }
    }
    pitch.valid=pitch.playable[ref]&&result.verifiedNotes>=2;pitch.enabled=pitch.valid;
    if(pitch.valid){FlowGraph atRef=source.graph;for(int k=0;k<pitch.count;++k)pitchWrite(atRef,pitch.assignments[k],pitch.values[ref][k]);pitch.modelHz=trackedFundamental(atRef,pitch.trackedNode);}
    pitch.measuredHz=refSpectrum.fundamentalHz;pitch.centsError=refSpectrum.cents;pitch.confidence=refSpectrum.confidence;
    result.spectrum=refSpectrum.bands;result.pitch=pitch;
    result.playableNotes=contiguousPitchSpan(pitch,result.firstMidi,result.lastMidi);
    if(!pitch.valid)return result;
    result.meanAbsCents/=result.verifiedNotes;result.confidence/=result.verifiedNotes;result.meanRms/=result.verifiedNotes;
    result.audibleVerified=true; // every playable key was individually verified
    const float fraction=float(result.verifiedNotes)/std::max(1,result.audibleChecks);
    const float quality=.45f*result.confidence+.35f*(1.f-std::min(1.f,result.meanAbsCents/10.f))+.20f*std::min(1.f,result.meanRms/.0002f);
    result.score=(.65f*result.verifiedNotes+.35f*result.playableNotes)*quality*fraction;
    return result;
}
inline PhysicalScanResults scanAcousticPhysicalPitch(const Blueprint& blueprint){
    PhysicalScanResults result{};const auto& g=blueprint.graph;
    result.graphStamp=physicalGraphStamp(g);result.referenceMidi=std::clamp<int>(blueprint.pitch.referenceMidi,0,127);result.a4Hz=blueprint.pitch.a4Hz;
    for(int node=0;node<g.nodeCount;++node){
        if(g.nodes[node].kind!=FlowKind::Universal||!physicalVibrates(g.nodes[node])||!pitchHasCompleteEnergyPath(g,node))continue;
        ++result.examinedNodes;
        for(int k=0;k<=int(PitchProperty::Coupling);++k){
            ++result.testedProperties;const auto prop=static_cast<PitchProperty>(k);const auto bounds=pitchPropertyBounds(prop);
            const PitchAssignment a{static_cast<int8_t>(node),prop,bounds[0],bounds[1],1.f};
            if(!scanParameterResponse(g,node,a))continue;
            PitchCalibration p{};p.trackedNode=static_cast<int8_t>(node);p.referenceMidi=result.referenceMidi;p.a4Hz=result.a4Hz;p.count=1;p.assignments[0]=a;
            auto c=evaluateAcousticScanPitch(blueprint,p);
            if(c.renderChecks>0)++result.auditionedCandidates;
            if(c.modeledNotes>=2){++result.modelCandidates;result.best.push_back(std::move(c));}
        }
    }
    for(int j=0;j<g.linkCount;++j)if(scanLinkInEnergyPath(g,j)){++result.examinedLinks;result.testedProperties+=2;}
    std::stable_sort(result.best.begin(),result.best.end(),[](const auto& a,const auto& b){return a.score>b.score;});
    result.count=static_cast<int>(result.best.size());return result;
}

inline PhysicalScanResults scanAllPhysicalPitch(const Blueprint& blueprint) {
    if(blueprint.graph.energySolver)return scanAcousticPhysicalPitch(blueprint);
    PhysicalScanResults results{};
    const auto& graph=blueprint.graph;
    results.graphStamp=physicalGraphStamp(graph);
    results.referenceMidi=std::clamp<int>(blueprint.pitch.referenceMidi,0,127);
    results.a4Hz=blueprint.pitch.a4Hz;
    struct Feasible {
        PitchAssignment assignment{};
        int tracked{-1};
        float roughSpan{0.f};
    };
    // First stage: census of ALL connected components and interaction links,
    // without using the currently selected node as a restriction.
    std::array<Feasible,kMaxFlowNodes*14> feasible{};
    int feasibleCount=0;
    for(int tracked=0;tracked<graph.nodeCount;++tracked){
        const auto& node=graph.nodes[tracked];
        if(node.kind!=FlowKind::Universal||
           !physicalVibrates(node)||
           !pitchHasCompleteEnergyPath(graph,tracked))continue;
        ++results.examinedNodes;
        // Every universal physical property, including those that prove
        // unable to change the modal fundamental, is evaluated.
        for(int p=0;p<=int(PitchProperty::Coupling);++p){
            PitchAssignment a{};
            a.objectIndex=static_cast<int8_t>(tracked);
            a.property=static_cast<PitchProperty>(p);
            const auto bounds=pitchPropertyBounds(a.property);
            a.lower=bounds[0];a.upper=bounds[1];
            a.priority=1.f;
            ++results.testedProperties;
            if(!scanParameterResponse(graph,tracked,a))continue;
            if(feasibleCount>=static_cast<int>(feasible.size()))continue;
            auto& item=feasible[feasibleCount++];
            item.assignment=a;item.tracked=tracked;
            FlowGraph lo=graph,hi=graph;
            pitchWrite(lo,a,a.lower);
            pitchWrite(hi,a,a.upper);
            const float low=trackedFundamental(lo,tracked);
            const float high=trackedFundamental(hi,tracked);
            item.roughSpan=std::fabs(12.f*std::log2(high/low));
        }
    }
    for(int j=0;j<graph.linkCount;++j){
        if(!scanLinkInEnergyPath(graph,j))continue;
        ++results.examinedLinks;
        // These controls currently change coupling/level, not a mode's
        // eigenfrequency. Keep them in the census rather than falsely
        // reporting them as viable pitch tuners.
        results.testedProperties+=2;
    }
    // Rank EVERY eligible component/property combination, then verify its
    // actual audio. Unlike Find, there is no pre-selected tracked material.
    // Repeated FFT analyses run on a worker thread (never audio/UI).
    // Preserve the strongest options PER component. A very broad parameter
    // range on one resonator must not crowd out every other resonator before
    // the actual audible assessment.
    constexpr int kPerComponent=3;
    std::array<std::array<PhysicalScanCandidate,kPerComponent>,
               kMaxFlowNodes> staged{};
    std::array<int,kMaxFlowNodes> stageCounts{};
    for(int i=0;i<feasibleCount;++i){
        PitchCalibration pitch{};
        pitch.referenceMidi=results.referenceMidi;
        pitch.a4Hz=results.a4Hz;
        pitch.trackedNode=static_cast<int8_t>(feasible[i].tracked);
        pitch.count=1;
        pitch.assignments[0]=feasible[i].assignment;
        if(!buildPhysicalKeyboard(graph,pitch))continue;
        ++results.modelCandidates;
        PhysicalScanCandidate c{};
        c.pitch=pitch;
        c.playableNotes=contiguousPitchSpan(pitch,c.firstMidi,c.lastMidi);
        // Modeled range is only a shortlist. FFT confirmation follows.
        if(c.playableNotes<3)continue;
        const int node=feasible[i].tracked;
        int& n=stageCounts[node];
        int at=0;
        while(at<n&&staged[node][at].playableNotes>=c.playableNotes)++at;
        if(at>=kPerComponent)continue;
        const int last=std::min(kPerComponent-1,n);
        for(int j=last;j>at;--j)staged[node][j]=staged[node][j-1];
        staged[node][at]=c;
        n=std::min(kPerComponent,n+1);
    }
    // Each eligible object receives its own chance to be heard, rather than
    // doing an enormous spectral test on every low-impact parameter.
    for(int rank=0;rank<kPerComponent;++rank){
        for(int node=0;node<graph.nodeCount;++node){
            if(rank>=stageCounts[node])continue;
            ++results.auditionedCandidates;
            const auto scored=evaluateScanPitch(
                blueprint,staged[node][rank].pitch);
            scanRankInsert(results,scored);
        }
    }
    // Stage three: two cooperating physical controls on the same medium
    // can expand playable range beyond either one alone.
    const int singles=std::min(results.count,5);
    std::array<PhysicalScanCandidate,5> singleChoices{};
    for(int i=0;i<singles;++i)singleChoices[i]=results.best[i];
    for(int i=0;i<singles;++i){
        const auto& primary=singleChoices[i];
        if(primary.pitch.count!=1)continue;
        for(int j=0;j<feasibleCount;++j){
            const auto& option=feasible[j];
            if(option.tracked!=primary.pitch.trackedNode||
               option.assignment.property==
                   primary.pitch.assignments[0].property)continue;
            PitchCalibration pitch{};
            pitch.referenceMidi=results.referenceMidi;
            pitch.a4Hz=results.a4Hz;
            pitch.trackedNode=primary.pitch.trackedNode;
            pitch.count=2;
            pitch.assignments[0]=primary.pitch.assignments[0];
            pitch.assignments[0].priority=2.f;
            pitch.assignments[1]=option.assignment;
            pitch.assignments[1].priority=1.f;
            if(!buildPhysicalKeyboard(graph,pitch))continue;
            int first,last;
            const int extent=contiguousPitchSpan(pitch,first,last);
            if(extent<=primary.playableNotes+1)continue;
            ++results.modelCandidates;
            ++results.auditionedCandidates;
            const auto scored=evaluateScanPitch(blueprint,pitch);
            scanRankInsert(results,scored);
        }
    }
    return results;
}
} // namespace aiora::physical
