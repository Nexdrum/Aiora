#pragma once
// Physical-keyboard tuning v6: the played note changes ONLY assigned
// physical properties. No output-pitch resampling / oscillator retuning.
// All note-table construction runs off the real-time audio thread.
#include "PhysicalFlowGraph.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace aiora::physical {

inline float noteFrequency12TET(int midi,float a4=440.f) noexcept {
    return std::clamp(a4,400.f,480.f)*
        std::pow(2.f,(std::clamp(midi,0,127)-69.f)/12.f);
}
// Reduced ring and plate bases, derived only from editable dimensions.
inline double acousticShellHz(const FlowNode& p,int circumferential,int axial) noexcept {
    const auto& g=p.form;const auto mat=objectMaterial(p);
    const double R=std::max(.0001,.25*double(g.widthMeters+g.depthMeters)*normalizedAverageProfile(g));
    const double t=std::max(.00005,g.wallMm*.001);
    const double n=std::max(2,circumferential);
    const double ring=n*(n*n-1.)/std::sqrt(n*n+1.)/(R*R);
    const double longitudinal=3.141592653589793*axial/std::max(.025,double(g.lengthMeters));
    return std::sqrt(mat.youngModulusPa/std::max(.05f,mat.densityKgM3))*t/std::sqrt(12.)*
        std::sqrt(ring*ring+std::pow(longitudinal,4.))/(2.*3.141592653589793)*
        (1.+.12*std::abs(g.curvature)+.15*materialUnit(g.fixing));
}
inline double acousticSheetHz(const FlowNode& p,int nx,int ny) noexcept {
    const auto& g=p.form;const auto mat=objectMaterial(p);
    const double L=std::max(.025,double(g.lengthMeters)),W=std::max(.001,double(g.widthMeters));
    const double t=std::max(.0001,double(g.depthMeters));
    const double sigma=std::max(1.e-8,double(occupiedVolume(p))*mat.densityKgM3/(L*W));
    const double D=mat.youngModulusPa*t*t*t/(12.*(1.-.3*.3));
    const double tension=std::max(0.f,p.tensionNewtons)/std::min(L,W);
    const double k2=std::pow(3.141592653589793*nx/L,2)+std::pow(3.141592653589793*ny/W,2);
    return std::sqrt((D*k2*k2+tension*k2)/sigma)/(2.*3.141592653589793)*
        (.75+.25*materialUnit(g.fixing));
}
inline double acousticHelmholtzHz(const FlowNode& p) noexcept {
    const auto& g=p.form;
    if(!p.siAcoustics||!objectIsFluid(p)||g.confinement<.7f)return 0.;
    double sum=0.;for(const auto& o:g.openings){
        const double A=openingArea(g,o);
        if(A>0.)sum+=A/std::max(.0001,g.wallMm*.001+o.neck*g.depthMeters+1.7*std::sqrt(A/3.141592653589793));
    }
    if(sum<=0.)return 0.;
    const auto mat=objectMaterial(p);
    return std::sqrt(mat.youngModulusPa/std::max(.05f,mat.densityKgM3))/
        (2.*3.141592653589793)*std::sqrt(sum/std::max(1.e-12,double(occupiedVolume(p))));
}
inline float physicalFundamental(const FlowNode& p) noexcept {
    if(p.kind!=FlowKind::Universal||!physicalVibrates(p))return 0.f;
    const auto material=objectMaterial(p);
    const auto& g=p.form;
    const float rho=std::max(p.substance.enabled?.05f:100.f,material.densityKgM3);
    const float E=std::max(p.substance.enabled?1.f:1.e6f,material.youngModulusPa);
    const float L=std::clamp(g.lengthMeters,.025f,5.f);
    const float w=std::clamp(g.widthMeters,.0001f,2.f);
    const float d=std::clamp(g.depthMeters,.0001f,1.f);
    const float profile=std::clamp(normalizedAverageProfile(g),.1f,3.f);
    const float rootE=std::sqrt(E/rho);
    if(acousticShell(p))return std::clamp(static_cast<float>(acousticShellHz(p,2,0)),25.f,12000.f);
    if(p.siAcoustics&&objectContinuum(p)==Continuum::Sheet)return std::clamp(static_cast<float>(acousticSheetHz(p,1,1)),25.f,12000.f);
    const double helmholtz=acousticHelmholtzHz(p);
    if(helmholtz>0.)return std::clamp(static_cast<float>(helmholtz),25.f,12000.f);
    if(p.siMechanics&&objectContinuum(p)==Continuum::Filament){
        const float mu=std::max(1.e-12f,rho*occupiedVolume(p)/L);
        const float area=occupiedVolume(p)/L;
        const float I=area*d*d/16.f; // equivalent elliptical section
        const float k=3.14159265358979323846f/L;
        const float omega=std::sqrt((std::max(0.f,p.tensionNewtons)*k*k+E*I*k*k*k*k)/mu);
        return std::clamp(omega/6.28318530718f,25.f,12000.f);
    }
    float f=110.f;
    switch(objectContinuum(p)){
    case Continuum::Filament:{
        const float mu=p.substance.enabled?rho*occupiedVolume(p)/L:rho*w*d*profile;
        f=.5f/L*std::sqrt(std::max(p.substance.enabled?0.f:1.f,p.tensionNewtons)/std::max(1.e-12f,mu));
        if(p.substance.enabled){
            const float bend=1.875104f*1.875104f/(6.2831853f*L*L)*
                std::sqrt(E*d*d/(12.f*rho))*(1.f+1.35f*materialUnit(g.fixing));
            f=std::sqrt(f*f+bend*bend);
        }
        break;
    }
    case Continuum::Sheet:{
        const float slender=std::max(w,L);
        const float bending=.13f*d/(slender*slender)*rootE;
        const float tensionWave=.25f/slender*std::sqrt(
            std::max(0.f,p.tensionNewtons)/(rho*d*std::max(w,.004f)));
        f=std::sqrt(bending*bending+tensionWave*tensionWave);
        break;
    }
    case Continuum::Solid:{
        const float IOverA=d*d/12.f*(p.substance.enabled?1.f+.65f*voidFraction(g):1.f);
        f=(1.875104f*1.875104f/(2.f*3.14159265358979323846f*L*L))*
            std::sqrt(E*IOverA/rho);
        f*=1.f+1.35f*std::clamp(g.fixing,0.f,1.f);
        break;
    }
    case Continuum::Gas:
    case Continuum::Liquid:{
        const float soundSpeed=p.substance.enabled||g.continuum==Continuum::Liquid?
            std::sqrt(material.youngModulusPa/std::max(p.substance.enabled?.05f:1.f,material.densityKgM3)):343.f;
        const float open=p.substance.enabled?fluidLeak(g):g.opening;
        const float closed=open<.5f?4.f:2.f;
        const float endCorrection=.61f*std::sqrt(w*d);
        const float effective=L+endCorrection*(open+.25f);
        const float wallCompliance=1.f+.025f*
            std::clamp(3.f/std::max(.2f,g.wallMm),.1f,6.f);
        f=soundSpeed/(closed*effective*wallCompliance);
        if(p.substance.enabled){
            // Free fluid is not a rigid pipe: finite jet length, spread and
            // confinement reduce coherent pressure resonance. Gravity and
            // capillary restoring terms act on a partially free liquid surface.
            const float conf=materialUnit(g.confinement);
            f*=.45f+.55f*conf;
            const float k=3.14159265f/L;
            const float surface=std::sqrt(kGravityMps2*k+constructedSurfaceTension(p.substance)*k*k*k/rho)/6.2831853f;
            const float blend=(1.f-conf)*(1.f-materialUnit(g.fillLevel))*(rho/(rho+20.f));
            f=(1.f-blend)*f+blend*surface;
        }
        break;
    }
    }
    if(p.substance.enabled&&!p.siMechanics&&!objectIsFluid(p))f*=std::sqrt(sectionFactor(g))*(1.f-.12f*voidFraction(g));
    f*=std::sqrt(profile)*
        (1.f+.08f*std::clamp(g.curvature,-1.f,1.f));
    return std::clamp(f,25.f,12000.f);
}
inline bool pitchIsLink(PitchProperty p) noexcept {
    return p==PitchProperty::LinkPosition||
        p==PitchProperty::LinkAperture;
}
inline std::array<float,2> pitchPropertyBounds(PitchProperty p) noexcept {
    switch(p){
    case PitchProperty::Length:return {.025f,2.f};
    case PitchProperty::Width:return {.0005f,.50f};
    case PitchProperty::Depth:return {.0002f,.20f};
    case PitchProperty::Wall:return {.2f,20.f};
    case PitchProperty::Curvature:
    case PitchProperty::Taper:return {-1.f,1.f};
    case PitchProperty::Opening:
    case PitchProperty::Fixing:
    case PitchProperty::LinkPosition:return {0.f,1.f};
    case PitchProperty::Tension:return {1.f,400.f};
    case PitchProperty::Profile0:
    case PitchProperty::Profile1:
    case PitchProperty::Profile2:
    case PitchProperty::Profile3:return {.25f,3.f};
    case PitchProperty::Coupling:return {.02f,1.f};
    case PitchProperty::LinkAperture:return {.035f,1.f};
    case PitchProperty::Count:break;
    }
    return {0.f,1.f};
}
inline bool pitchTargetValid(const FlowGraph& g,
                            const PitchAssignment& a) noexcept {
    if(a.objectIndex<0)return false;
    if(pitchIsLink(a.property))
        return a.objectIndex<g.linkCount;
    if(a.objectIndex>=g.nodeCount)return false;
    const auto& n=g.nodes[static_cast<size_t>(a.objectIndex)];
    return n.kind==FlowKind::Universal&&physicalVibrates(n);
}
inline float pitchRead(const FlowGraph& g,
                       const PitchAssignment& a) noexcept {
    if(!pitchTargetValid(g,a))return 0.f;
    if(pitchIsLink(a.property)){
        const auto& link=g.links[static_cast<size_t>(a.objectIndex)];
        return a.property==PitchProperty::LinkPosition?
            link.position:link.aperture;
    }
    const auto& n=g.nodes[static_cast<size_t>(a.objectIndex)];
    switch(a.property){
    case PitchProperty::Length:return n.form.lengthMeters;
    case PitchProperty::Width:return n.form.widthMeters;
    case PitchProperty::Depth:return n.form.depthMeters;
    case PitchProperty::Wall:return n.form.wallMm;
    case PitchProperty::Curvature:return n.form.curvature;
    case PitchProperty::Taper:return n.form.taper;
    case PitchProperty::Opening:return n.form.opening;
    case PitchProperty::Fixing:return n.form.fixing;
    case PitchProperty::Tension:return n.tensionNewtons;
    case PitchProperty::Profile0:
    case PitchProperty::Profile1:
    case PitchProperty::Profile2:
    case PitchProperty::Profile3:
        return n.form.stations[static_cast<int>(a.property)-
            static_cast<int>(PitchProperty::Profile0)];
    case PitchProperty::Coupling:return n.coupling;
    default:return 0.f;
    }
}
inline bool pitchWrite(FlowGraph& g,const PitchAssignment& a,
                       float value) noexcept {
    if(!pitchTargetValid(g,a)||!std::isfinite(value))return false;
    const auto limits=pitchPropertyBounds(a.property);
    value=std::clamp(value,limits[0],limits[1]);
    if(pitchIsLink(a.property)){
        auto& link=g.links[static_cast<size_t>(a.objectIndex)];
        if(a.property==PitchProperty::LinkPosition)link.position=value;
        else link.aperture=value;
        return true;
    }
    auto& n=g.nodes[static_cast<size_t>(a.objectIndex)];
    switch(a.property){
    case PitchProperty::Length:n.form.lengthMeters=value;break;
    case PitchProperty::Width:n.form.widthMeters=value;break;
    case PitchProperty::Depth:n.form.depthMeters=value;break;
    case PitchProperty::Wall:n.form.wallMm=value;break;
    case PitchProperty::Curvature:n.form.curvature=value;break;
    case PitchProperty::Taper:n.form.taper=value;break;
    case PitchProperty::Opening:n.form.opening=value;break;
    case PitchProperty::Fixing:n.form.fixing=value;break;
    case PitchProperty::Tension:n.tensionNewtons=value;break;
    case PitchProperty::Profile0:
    case PitchProperty::Profile1:
    case PitchProperty::Profile2:
    case PitchProperty::Profile3:
        n.form.stations[static_cast<int>(a.property)-
            static_cast<int>(PitchProperty::Profile0)]=value;break;
    case PitchProperty::Coupling:n.coupling=value;break;
    default:return false;
    }
    return true;
}
inline uint32_t physicalGraphStamp(const FlowGraph& g) noexcept {
    uint32_t h=2166136261u;
    const auto add=[&](uint32_t v){h=(h^v)*16777619u;};
    const auto f=[&](float v){add(std::bit_cast<uint32_t>(v));};
    add(g.nodeCount);add(g.linkCount);f(g.spanMs);add(g.objectFlow);add(g.energySolver);add(g.acousticSolver);add(g.jetSolver);
    for(int i=0;i<g.nodeCount;++i){
        const auto& n=g.nodes[static_cast<size_t>(i)];
        add(static_cast<uint32_t>(n.kind));add(static_cast<uint32_t>(n.role));
        add(n.substance.enabled);add(n.siMechanics);add(n.siAcoustics);
        if(n.substance.enabled){
            for(float v:n.substance.value)f(v);
        }else{
            add(static_cast<uint32_t>(n.material));add(static_cast<uint32_t>(n.geometry));
            add(static_cast<uint32_t>(n.driver));add(static_cast<uint32_t>(n.gesture));
            add(static_cast<uint32_t>(n.form.continuum));
        }
        f(n.x);f(n.y);f(n.size);f(n.thicknessMm);
        f(n.damping);f(n.coupling);f(n.position);f(n.tensionNewtons);
        f(n.diameterMm);f(n.hardness);f(n.restPressure);f(n.motion);
        f(n.sourceDurationMs);f(n.sourceRiseMs);f(n.sourceDirection);
        f(n.sourceAcceleration);f(n.sourcePotential);f(n.sourceTimeMs);
        add(n.sourceFollowNote);f(n.interactionAngleDeg);
        f(n.interactionEngagement);
        f(n.interactionFriction);f(n.interactionElasticity);
        f(n.interactionRestitution);f(n.interactionArea);
        f(n.interactionClearance);f(n.interactionAdhesion);f(n.interactionBlend);
        f(n.form.lengthMeters);f(n.form.widthMeters);f(n.form.depthMeters);
        f(n.form.wallMm);f(n.form.curvature);f(n.form.taper);
        f(n.form.opening);f(n.form.fixing);
        for(float v:n.form.stations)f(v);
        if(n.substance.enabled){
            f(n.form.wallFill);f(n.form.rounding);add(n.form.edges);
            f(n.form.confinement);f(n.form.spread);f(n.form.flowAngle);f(n.form.fillLevel);
            for(const auto& c:n.form.cavities){f(c.length);f(c.width);f(c.depth);f(c.position);f(c.offset);}
            for(const auto& o:n.form.openings){f(o.size);f(o.position);f(o.azimuth);f(o.aspect);f(o.neck);}
        }
        add(n.movingEnabled);f(n.movingMin);f(n.movingMax);
        add(static_cast<uint32_t>(n.law));
    }
    for(int i=0;i<g.linkCount;++i){
        const auto& link=g.links[static_cast<size_t>(i)];
        add(link.from);add(link.to);f(link.position);
        f(link.modTravel);f(link.aperture);
        f(link.modMin);f(link.modMax);
        add(link.modEnabled);add(static_cast<uint32_t>(link.law));
        f(link.fromPosition);f(link.motion);f(link.angleDeg);f(link.engagement);
        f(link.friction);f(link.elasticity);f(link.restitution);f(link.clearance);f(link.adhesion);
        f(link.senderY);f(link.senderZ);f(link.receiverY);f(link.receiverZ);f(link.elevationDeg);f(link.jetDistanceMeters);f(link.edgeBevelDeg);
    }
    return h;
}
inline float trackedFundamental(const FlowGraph& g,
                                int trackedNode) noexcept {
    if(trackedNode<0||trackedNode>=g.nodeCount)return 0.f;
    return physicalFundamental(g.nodes[static_cast<size_t>(trackedNode)]);
}
inline bool pitchMapCurrent(const Blueprint& b) noexcept {
    const auto& t=b.pitch;
    return t.enabled&&t.valid&&t.count>0&&
        t.graphStamp==physicalGraphStamp(b.graph);
}
inline bool applyMappedPitch(const PitchCalibration& c,FlowGraph& graph,
                             int midi) noexcept {
    if(!c.valid||midi<0||midi>127||!c.playable[midi])return false;
    for(int i=0;i<c.count&&i<kPitchSlots;++i){
        if(!pitchWrite(graph,c.assignments[i],c.values[midi][i]))
            return false;
    }
    return true;
}
struct PitchSolveResult {
    bool reached{false};
    float modeledHz{0.f};
    float centsError{0.f};
    std::array<float,kPitchSlots> values{};
};
// Iterated bounded-coordinate searches allow multiple physical assignments;
// ineffective / unreachable assignments do NOT secretly adjust oscillator Hz.
// The selected tracked medium provides the modal fundamental of this reduced
// solver. Wider coupled-frequency analysis is a future numerical model.
inline PitchSolveResult solvePhysicalPitch(
    const FlowGraph& original,const PitchCalibration& pitch,
    float desiredHz) noexcept {
    PitchSolveResult result{};
    if(pitch.trackedNode<0||pitch.trackedNode>=original.nodeCount||
       pitch.count==0||!std::isfinite(desiredHz)||desiredHz<=0.f)
        return result;
    FlowGraph trial=original;
    for(int j=0;j<pitch.count&&j<kPitchSlots;++j){
        const auto& a=pitch.assignments[j];
        if(!pitchTargetValid(trial,a))return result;
        result.values[j]=std::clamp(pitchRead(trial,a),
            std::min(a.lower,a.upper),std::max(a.lower,a.upper));
        pitchWrite(trial,a,result.values[j]);
    }
    const auto err=[&](){
        const float hz=trackedFundamental(trial,pitch.trackedNode);
        return hz>0.f?1200.f*std::log2(hz/desiredHz):1.e9f;
    };
    float best=std::fabs(err());
    for(int iteration=0;iteration<6&&best>.15f;++iteration){
        bool moved=false;
        // Higher-priority assignments are adjusted first.
        for(int rank=0;rank<pitch.count;++rank){
            int slot=-1;
            for(int j=0;j<pitch.count;++j){
                int before=0;
                for(int k=0;k<pitch.count;++k)
                    if(pitch.assignments[k].priority>
                       pitch.assignments[j].priority||
                       (pitch.assignments[k].priority==
                        pitch.assignments[j].priority&&k<j))++before;
                if(before==rank){slot=j;break;}
            }
            if(slot<0)continue;
            const auto& a=pitch.assignments[slot];
            const float lo=std::min(a.lower,a.upper);
            const float hi=std::max(a.lower,a.upper);
            if(hi-lo<1.e-8f)continue;
            pitchWrite(trial,a,lo);
            const float eLo=err();
            pitchWrite(trial,a,hi);
            const float eHi=err();
            float candidate=lo,score=std::fabs(eLo);
            if(std::fabs(eHi)<score){candidate=hi;score=std::fabs(eHi);}
            // Only bracketed monotonic responses can be safely bisected.
            // Discontinuous opening/closed boundary changes are checked.
            if(std::isfinite(eLo)&&std::isfinite(eHi)&&eLo*eHi<=0.f){
                float left=lo,right=hi;
                for(int i=0;i<26;++i){
                    const float mid=(left+right)*.5f;
                    pitchWrite(trial,a,mid);
                    const float eMid=err();
                    if(std::fabs(eMid)<score){score=std::fabs(eMid);candidate=mid;}
                    if((eLo<0.f)==(eMid<0.f))left=mid;
                    else right=mid;
                }
            }
            pitchWrite(trial,a,candidate);
            // Prevent the optimizer from worsening pitch accidentally.
            if(score+1.e-4f<best){
                best=score;result.values[slot]=candidate;moved=true;
            }else pitchWrite(trial,a,result.values[slot]);
        }
        if(!moved)break;
    }
    result.modeledHz=trackedFundamental(trial,pitch.trackedNode);
    if(result.modeledHz>0.f&&std::isfinite(result.modeledHz)){
        result.centsError=1200.f*
            std::log2(result.modeledHz/desiredHz);
        result.reached=std::fabs(result.centsError)<1.f;
    }
    return result;
}
inline bool pitchHasCompleteEnergyPath(const FlowGraph& g,
                                        int tracked) noexcept {
    if(tracked<0||tracked>=g.nodeCount)return false;
    std::array<bool,kMaxFlowNodes> fromHuman{},toOutput{};
    for(int i=0;i<g.nodeCount;++i)
        fromHuman[i]=g.nodes[i].kind==FlowKind::Exciter||
            isUniversalSource(g.nodes[i]);
    for(int pass=0;pass<g.nodeCount;++pass)
        for(int j=0;j<g.linkCount;++j){
            const auto& edge=g.links[j];
            if(edge.from<g.nodeCount&&edge.to<g.nodeCount&&
               fromHuman[edge.from])fromHuman[edge.to]=true;
        }
    for(int i=0;i<g.nodeCount;++i)
        toOutput[i]=g.nodes[i].kind==FlowKind::Output||
            isUniversalOutput(g.nodes[i]);
    for(int pass=0;pass<g.nodeCount;++pass)
        for(int j=0;j<g.linkCount;++j){
            const auto& edge=g.links[j];
            if(edge.from<g.nodeCount&&edge.to<g.nodeCount&&
               toOutput[edge.to])toOutput[edge.from]=true;
        }
    return fromHuman[tracked]&&toOutput[tracked];
}
inline bool buildPhysicalKeyboard(const FlowGraph& graph,
                                  PitchCalibration& pitch) noexcept {
    pitch.valid=false;
    if(pitch.count==0||pitch.count>kPitchSlots||
       trackedFundamental(graph,pitch.trackedNode)<=0.f||
       !pitchHasCompleteEnergyPath(graph,pitch.trackedNode))
        return false;
    for(int i=0;i<pitch.count;++i){
        const auto& a=pitch.assignments[i];
        if(!pitchTargetValid(graph,a)||!std::isfinite(a.lower)||
           !std::isfinite(a.upper))return false;
        const auto bounds=pitchPropertyBounds(a.property);
        if(std::min(a.lower,a.upper)<bounds[0]||
           std::max(a.lower,a.upper)>bounds[1])return false;
        for(int j=0;j<i;++j)
            if(a.objectIndex==pitch.assignments[j].objectIndex&&
               a.property==pitch.assignments[j].property)return false;
    }
    pitch.playable.fill(0);
    const int ref=std::clamp<int>(pitch.referenceMidi,0,127);
    for(int midi=0;midi<128;++midi){
        const auto r=solvePhysicalPitch(graph,pitch,
            noteFrequency12TET(midi,pitch.a4Hz));
        pitch.values[midi]=r.values;
        pitch.playable[midi]=r.reached?1:0;
    }
    pitch.graphStamp=physicalGraphStamp(graph);
    pitch.enabled=true;
    pitch.valid=pitch.playable[ref]!=0;
    pitch.modelHz=trackedFundamental(graph,pitch.trackedNode);
    const auto r=solvePhysicalPitch(graph,pitch,
        noteFrequency12TET(ref,pitch.a4Hz));
    pitch.centsError=r.centsError;
    return pitch.valid;
}

// The fine pitch map is generated on the editor thread after a completed
// calibration, NEVER in the note renderer or Android audio callback.
// A calibration key prevents stale fine maps when the tuning assignments
// or material/geometry change independently of the previous calibration.
inline uint64_t physicalBendCalibrationKey(const Blueprint& blueprint) noexcept {
    const auto& c=blueprint.pitch;
    uint64_t h=14695981039346656037ULL;
    const auto mix=[&](uint32_t x){h^=x;h*=1099511628211ULL;};
    mix(physicalGraphStamp(blueprint.graph));
    mix(static_cast<uint32_t>(c.count));
    mix(static_cast<uint32_t>(c.trackedNode));
    mix(static_cast<uint32_t>(c.referenceMidi));
    mix(std::bit_cast<uint32_t>(c.a4Hz));
    for(int i=0;i<c.count&&i<kPitchSlots;++i){
        const auto& a=c.assignments[i];
        mix(static_cast<uint32_t>(a.objectIndex));
        mix(static_cast<uint32_t>(a.property));
        mix(std::bit_cast<uint32_t>(a.lower));
        mix(std::bit_cast<uint32_t>(a.upper));
        mix(std::bit_cast<uint32_t>(a.priority));
    }
    return h?h:1ULL;
}
inline bool physicalBendMapCurrent(const Blueprint& b) noexcept {
    return b.bend.calibrationKey!=0&&pitchMapCurrent(b)&&
        b.bend.calibrationKey==physicalBendCalibrationKey(b);
}
inline float physicalPitchInterpolatedValue(
    PitchProperty property,float a,float b,float fraction) noexcept {
    fraction=std::clamp(fraction,0.f,1.f);
    // For strictly positive lengths, tensions and cross-sections, log
    // interpolation reduces curvature error between physically solved
    // semitone samples. Other properties move linearly.
    const bool positive=property==PitchProperty::Length||
        property==PitchProperty::Width||
        property==PitchProperty::Depth||
        property==PitchProperty::Wall||
        property==PitchProperty::Tension||
        property==PitchProperty::Profile0||
        property==PitchProperty::Profile1||
        property==PitchProperty::Profile2||
        property==PitchProperty::Profile3;
    if(positive&&a>0.f&&b>0.f)
        return a*std::pow(b/a,fraction);
    return a+(b-a)*fraction;
}
inline bool preparePhysicalBendMap(Blueprint& blueprint) noexcept {
    auto& bend=blueprint.bend;
    bend.calibrationKey=0;
    bend.playable.fill(0);
    if(!pitchMapCurrent(blueprint))return false;
    const auto& c=blueprint.pitch;
    constexpr int steps=kPhysicalBendSubsteps;
    for(int note=0;note<=127;++note){
        const int pos=note*steps;
        if(!c.playable[note])continue;
        bend.playable[pos]=1;
        bend.values[pos]=c.values[note];
    }
    // A physical transition is only considered achievable if both
    // bracketing notes and that sub-semitone actually solve. Holes and
    // instrument range limits are not bridged by pitch-shifting.
    for(int note=0;note<127;++note){
        if(!c.playable[note]||!c.playable[note+1])continue;
        for(int frac=1;frac<steps;++frac){
            const float targetMidi=note+static_cast<float>(frac)/steps;
            const float targetHz=std::clamp(c.a4Hz,400.f,480.f)*
                std::exp2((targetMidi-69.f)/12.f);
            const auto solved=solvePhysicalPitch(
                blueprint.graph,c,targetHz);
            if(!solved.reached)continue;
            const int pos=note*steps+frac;
            bend.playable[pos]=1;
            bend.values[pos]=solved.values;
        }
    }
    // Precompute nearest achievable settings for a bend outside the
    // constructed instrument's physical range, including interior holes.
    constexpr uint16_t none=UINT16_MAX;
    int previous=-1;
    for(int i=0;i<kPhysicalBendEntries;++i){
        if(bend.playable[i])previous=i;
        bend.nearest[i]=previous>=0?
            static_cast<uint16_t>(previous):none;
    }
    int following=-1;
    for(int i=kPhysicalBendEntries-1;i>=0;--i){
        if(bend.playable[i])following=i;
        const auto prev=bend.nearest[i];
        if(following>=0&&(prev==none||following-i<i-prev))
            bend.nearest[i]=static_cast<uint16_t>(following);
    }
    bend.calibrationKey=physicalBendCalibrationKey(blueprint);
    return true;
}
// Sample fractional MIDI position with no solver work, allocation, or audio
// pitch-shift. Return the values of EACH assigned physical component.
inline bool physicalPitchValuesAt(const Blueprint& blueprint,
                                  float midiPosition,
                                  std::array<float,kPitchSlots>& out) noexcept {
    const auto& c=blueprint.pitch;
    if(!pitchMapCurrent(blueprint)||!std::isfinite(midiPosition))
        return false;
    const float key=std::clamp(midiPosition,0.f,127.f);
    if(physicalBendMapCurrent(blueprint)){
        constexpr int steps=kPhysicalBendSubsteps;
        const auto& bend=blueprint.bend;
        const float scaled=key*steps;
        const int i0=std::clamp(static_cast<int>(std::floor(scaled)),
            0,kPhysicalBendEntries-1);
        const int i1=std::min(i0+1,kPhysicalBendEntries-1);
        const float fraction=scaled-i0;
        if(bend.playable[i0]&&bend.playable[i1]){
            for(int k=0;k<c.count&&k<kPitchSlots;++k){
                out[k]=physicalPitchInterpolatedValue(
                    c.assignments[k].property,
                    bend.values[i0][k],bend.values[i1][k],fraction);
            }
            return true;
        }
        const int nearest=std::clamp(static_cast<int>(std::lround(scaled)),
            0,kPhysicalBendEntries-1);
        const auto mapped=bend.nearest[nearest];
        if(mapped==UINT16_MAX)return false;
        out=bend.values[mapped];
        return true;
    }
    // Fallback for older integer-only calibrations: a continuous physical
    // interpolation is still preferable to stepping audio oscillator pitch.
    const int n0=std::clamp(static_cast<int>(std::floor(key)),0,127);
    const int n1=std::min(127,n0+1);
    if(c.playable[n0]&&c.playable[n1]){
        const float fraction=key-n0;
        for(int k=0;k<c.count&&k<kPitchSlots;++k)
            out[k]=physicalPitchInterpolatedValue(c.assignments[k].property,
                c.values[n0][k],c.values[n1][k],fraction);
        return true;
    }
    int nearest=-1;
    for(int distance=0;distance<=127;++distance){
        const int low=static_cast<int>(std::lround(key))-distance;
        const int high=static_cast<int>(std::lround(key))+distance;
        if(low>=0&&low<128&&c.playable[low]){nearest=low;break;}
        if(high>=0&&high<128&&c.playable[high]){nearest=high;break;}
    }
    if(nearest<0)return false;
    out=c.values[nearest];
    return true;
}

} // namespace aiora::physical
