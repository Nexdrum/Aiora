#pragma once
// Aiora universal material / energy-arrival research voice, v9.
// APT accelerates the transfer mass, with Potential limiting mechanical work
// (finite) or power (note-locked). V modulates Potential continuously and
// modestly scales INITIAL acceleration; M moves assigned physical objects.
// Nodes retain energy, Y is first arrival, link X is local entry position.
// Reduced modal + wind models, NOT a full fluid / finite-element solver.
// Fixed buffers; render() has no dynamic allocation or graph edits.
#include "PhysicalInstrumentVoice.h"
#include "PhysicalPitchTuning.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace aiora::physical {

constexpr int kMaterialModes=7;
constexpr int kFlowDelayFrames=4096; // 85.3 ms at 48 kHz per connection

// Fixed instrument-level calibration for constructed graphs, not automatic
// normalization. Zero physical energy remains silent; relative changes remain.
inline constexpr float kConstructionOutputGain=16.f;
inline bool usesConstructedMaterial(const FlowGraph& graph) noexcept {
    if(!graph.objectFlow)return false;
    for(int i=0;i<graph.nodeCount;++i)if(graph.nodes[i].substance.enabled)return true;
    return false;
}
class MaterialTimeVoice {
public:
    void prepare(float sr) noexcept {
        sampleRate_=std::clamp(sr,8000.f,192000.f);
        legacy_.prepare(sr);
        stop();
    }
    void start(const Blueprint& blueprint,int midi,float velocity,float mod=0.f) noexcept {
        blueprint_=blueprint;
        graph_=blueprint.graph;
        baseMidi_=std::clamp(midi,0,127);
        currentBendSemitones_=0.f;
        pitchBendReady_=pitchMapCurrent(blueprint)&&
            blueprint.pitch.playable[baseMidi_]!=0;
        // No audio pitch shift: each keyboard note applies its previously
        // solved physical settings before any modal/edge state is built.
        // Stale or unreachable notes retain their unaltered physical shape.
        if(pitchMapCurrent(blueprint))
            applyMappedPitch(blueprint.pitch,graph_,midi);
        if(graph_.nodeCount==0){
            legacyMode_=true;
            legacy_.start(blueprint,midi,velocity,mod);
            active_=true;
            return;
        }
        legacyMode_=false;
        active_=true;released_=false;tick_=0;releasedAt_=0;head_=0;
        e1_=safe01(velocity);e2_=safe01(mod);e2Smooth_=e2_;
        noteOnEffort_=e1_;reactionForceN_=0.f;nextReactionForceN_=0.f;
        pitchHz_=std::clamp(440.f*std::pow(2.f,(midi-69)/12.f),25.f,sampleRate_*.16f);
        quietFrames_=0;activity_=0.f;
        nodeCount_=std::min<int>(graph_.nodeCount,kMaxFlowNodes);
        edgeCount_=0;
        contactCount_=0;
        for(int i=0;i<kMaxFlowNodes;++i){
            nodes_[i]={};
            nodeOrder_[i]=i;
        }
        for(int i=0;i<nodeCount_;++i){
            nodes_[i].node=graph_.nodes[static_cast<size_t>(i)];
            nodes_[i].physicalMaterial=objectMaterial(nodes_[i].node);
            nodes_[i].fluid=objectIsFluid(nodes_[i].node);
            nodes_[i].viscosity=nodes_[i].node.substance.enabled?constructedViscosity(nodes_[i].node.substance):0.f;
            nodes_[i].randomSeed=0x713f9a35u+static_cast<uint32_t>(i)*0x3c6ef35fu;
            if(isUniversalSource(nodes_[i].node)){
                // Source acceleration is applied to actual connected medium
                // mass, rather than prescribing an output velocity or waveform.
                float mass=0.f;
                for(int l=0;l<graph_.linkCount;++l){
                    const auto& link=graph_.links[l];
                    if(link.from==i&&link.to<nodeCount_)
                        mass+=physicalMediumMass(graph_.nodes[link.to]);
                }
                nodes_[i].driveMassKg=std::clamp(mass,1.e-6f,40.f);
            }
        }
        // Insertion sort avoids allocation on the real-time note-on path.
        for(int i=1;i<nodeCount_;++i){
            const int key=nodeOrder_[i];int j=i;
            while(j>0&&nodes_[nodeOrder_[j-1]].node.y>nodes_[key].node.y){
                nodeOrder_[j]=nodeOrder_[j-1];--j;
            }
            nodeOrder_[j]=key;
        }
        for(int i=0;i<graph_.linkCount&&edgeCount_<kMaxFlowLinks;++i){
            const auto raw=graph_.links[static_cast<size_t>(i)];
            if(raw.from>=nodeCount_||raw.to>=nodeCount_||
               !graph_.compatible(raw.from,raw.to))continue;
            auto& edge=edges_[static_cast<size_t>(edgeCount_)];
            edge.from=raw.from;edge.to=raw.to;
            edge.interaction=raw;edge.elastic=0.f;edge.slip=0.f;edge.previousFlux=0.f;
            edge.position=std::clamp(raw.position,0.f,1.f);
            edge.modTravel=std::clamp(raw.modTravel,-1.f,1.f);
            edge.modEnabled=raw.modEnabled;
            edge.modMin=std::clamp(raw.modMin,0.f,1.f);
            edge.modMax=std::clamp(raw.modMax,0.f,1.f);
            edge.law=raw.law;
            edge.aperture=std::clamp(raw.aperture,
                graph_.nodes[raw.from].substance.enabled||graph_.nodes[raw.to].substance.enabled?0.f:.035f,1.f);
            const auto& from=nodes_[raw.from].node;
            const auto& to=nodes_[raw.to].node;
            // Vertical position represents time elapsed between interactions.
            // A small transit term accounts for sound velocity in the source.
            const float speed=materialWaveSpeed(
                isUniversalSource(from)?to:from);
            const float physicalMs=1000.f*.12f/std::max(40.f,speed);
            const float delayMs=(to.y-from.y)*
                std::clamp(graph_.spanMs,3.f,82.f)+physicalMs;
            const int delay=static_cast<int>(std::lround(delayMs*sampleRate_*.001f));
            edge.delay=std::clamp(delay,1,kFlowDelayFrames-1);
            edge.filter=0.f;
            std::fill(edge.buffer.begin(),edge.buffer.end(),0.f);
            const float z0=materialImpedance(from);
            const float z1=materialImpedance(to);
            const bool airCoupled=from.kind==FlowKind::Router||
                (from.kind==FlowKind::Universal&&
                 objectIsFluid(from))||
                (to.kind==FlowKind::Universal&&
                 objectIsFluid(to))||
                from.kind==FlowKind::Reed||from.kind==FlowKind::Edge||
                to.kind==FlowKind::Reed||to.kind==FlowKind::Edge||
                (from.kind==FlowKind::Exciter&&
                 from.excitation==Excitation::Blow);
            const float match=isUniversalSource(from)?.92f:
                airCoupled?.92f:2.f*std::sqrt(z0*z1)/(z0+z1);
            const float contact=std::sqrt(std::clamp(to.coupling,0.f,1.f));
            edge.gain=std::clamp(.80f*match*contact,.03f,.83f);
            edge.smoothing=std::clamp(.83f-.58f*objectMaterial(to).contactSoftness,
                                       .14f,.93f);
            nodes_[raw.from].outgoing[nodes_[raw.from].outCount++]=edgeCount_;
            nodes_[raw.to].incoming[nodes_[raw.to].inCount++]=edgeCount_;
            ++edgeCount_;
        }
        // Normalize every fan-out to prevent volume multiplication across
        // arbitrary parallel branches. No feedback loops are permitted.
        for(int i=0;i<edgeCount_;++i){
            auto& edge=edges_[static_cast<size_t>(i)];
            if(nodes_[edge.from].node.kind!=FlowKind::Router)
                edge.gain/=std::sqrt(static_cast<float>(
                    std::max(1,nodes_[edge.from].outCount)));
        }
        // Radiation into downstream objects is an energy sink for a stored
        // resonator. This is a reduced admittance approximation, not a
        // bidirectional finite-element energy solver.
        for(int i=0;i<nodeCount_;++i){
            auto& n=nodes_[static_cast<size_t>(i)];
            float transmitted=0.f;
            for(int j=0;j<n.outCount;++j){
                const auto& edge=edges_[static_cast<size_t>(n.outgoing[j])];
                transmitted+=edge.gain*edge.gain;
            }
            n.transferRetention=std::exp(
                -std::clamp(5.f*transmitted,0.f,15.f)/sampleRate_);
        }
        const float hz=pitchHz_;
        primaryPitchAssigned_=false;
        for(int j=0;j<nodeCount_;++j){
            const int id=nodeOrder_[j];
            setupNode(nodes_[static_cast<size_t>(id)],hz);
        }
        // A CONTACT is a physical attachment to a vibrating object, not
        // merely an audio level control inserted after the object.
        // Unlike an ordinary downward audio link, its interaction drains
        // energy stored in the source object's modal state.
        for(int i=0;i<edgeCount_;++i){
            const auto& edge=edges_[static_cast<size_t>(i)];
            const auto& target=nodes_[static_cast<size_t>(edge.to)].node;
            const auto& source=nodes_[static_cast<size_t>(edge.from)].node;
            const bool universalContact=target.kind==FlowKind::Universal&&
                target.role==PhysicalRole::Interaction&&
                (target.law==ContactLaw::Contact||edge.law==ContactLaw::Contact);
            if(!(target.kind==FlowKind::Damper||universalContact)||
               !physicalVibrates(source)||contactCount_>=kMaxFlowLinks)continue;
            auto& touch=contacts_[static_cast<size_t>(contactCount_++)];
            touch.source=edge.from;
            touch.pressure=std::clamp(target.restPressure,0.f,1.f);
            // M moves the linked damper into the source; contact stiffness
            // converts displacement (penetration) to mechanical dissipation.
            touch.mDepth=std::clamp(edge.modTravel,-1.f,1.f);
            touch.modEnabled=edge.modEnabled||target.movingEnabled;
            touch.modMin=target.movingEnabled?target.movingMin:edge.modMin;
            touch.modMax=target.movingEnabled?target.movingMax:edge.modMax;
            touch.stiffness=std::clamp(target.coupling,0.f,1.f);
            touch.activateAt=static_cast<uint64_t>(std::max(0.f,
                target.y*std::clamp(graph_.spanMs,3.f,82.f)*
                sampleRate_*.001f));
            const auto mat=materialProperties(target.material);
            const float softness=std::clamp(mat.contactSoftness,0.f,1.f);
            const float location=std::clamp(edge.position,.005f,.995f);
            for(int mode=0;mode<kMaterialModes;++mode){
                // A contact near a mode's node dissipates less vibration.
                const float shape=std::sin(kPi*(mode+1.f)*location);
                const float overlap=.10f+.90f*shape*shape;
                touch.lossPerSecond[static_cast<size_t>(mode)]=
                    (25.f+290.f*softness)*overlap;
            }
        }
    }
    void setExpression(float v,float m) noexcept {
        if(legacyMode_){legacy_.setExpression(v,m);return;}
        e1_=safe01(v);e2_=safe01(m);
    }
    // The piano-roll B curve is in fractional semitones. Retune the
    // ASSIGNED physical properties, not the audio output. The modal states
    // (real/imaginary oscillation, delayed forces, source gesture and energy)
    // are deliberately preserved, so an already-ringing note can glide.
    // Call on the audio thread at the same control cadence as V and M.
    void setPitchBend(float semitones) noexcept {
        if(legacyMode_||!pitchBendReady_||!std::isfinite(semitones))return;
        const float bend=std::clamp(semitones,-127.f,127.f);
        if(std::fabs(bend-currentBendSemitones_)<.00002f)return;
        std::array<float,kPitchSlots> physicalValues{};
        if(!physicalPitchValuesAt(blueprint_,baseMidi_+bend,physicalValues))
            return;
        currentBendSemitones_=bend;
        std::array<bool,kMaxFlowNodes> dirty{};
        const auto& tuning=blueprint_.pitch;
        for(int i=0;i<tuning.count&&i<kPitchSlots;++i){
            const auto& assignment=tuning.assignments[i];
            if(!pitchTargetValid(graph_,assignment))continue;
            const float oldValue=pitchRead(graph_,assignment);
            const float newValue=physicalValues[i];
            if(std::fabs(newValue-oldValue)<1.e-8f)continue;
            if(!pitchWrite(graph_,assignment,newValue))continue;
            if(pitchIsLink(assignment.property)){
                const auto& link=graph_.links[
                    static_cast<size_t>(assignment.objectIndex)];
                for(int j=0;j<edgeCount_;++j){
                    auto& edge=edges_[static_cast<size_t>(j)];
                    if(edge.from!=link.from||edge.to!=link.to)continue;
                    if(assignment.property==PitchProperty::LinkPosition)
                        edge.position=link.position;
                    else edge.aperture=link.aperture;
                    break;
                }
            }else dirty[static_cast<size_t>(assignment.objectIndex)]=true;
        }
        for(int i=0;i<nodeCount_;++i){
            if(!dirty[static_cast<size_t>(i)])continue;
            auto& n=nodes_[static_cast<size_t>(i)];
            n.node=graph_.nodes[static_cast<size_t>(i)];
            // setupNode updates modal frequency/partial/damping and
            // mechanical input gains but DOES NOT reset stored real/imag
            // oscillation or node arrival buffers.
            setupNode(n,pitchHz_);
        }
    }
    [[nodiscard]] float currentPhysicalPitchHz() const noexcept {
        return trackedFundamental(graph_,blueprint_.pitch.trackedNode);
    }
    [[nodiscard]] float currentPitchPropertyValue(int slot=0) const noexcept {
        if(slot<0||slot>=blueprint_.pitch.count)return 0.f;
        return pitchRead(graph_,blueprint_.pitch.assignments[slot]);
    }
    [[nodiscard]] float sourceMotionVelocity(int sourceIndex=0) const noexcept {
        return sourceIndex>=0&&sourceIndex<nodeCount_?
            nodes_[sourceIndex].driveVelocity:0.f;
    }
    [[nodiscard]] float sourceMechanicalWork(int sourceIndex=0) const noexcept {
        return sourceIndex>=0&&sourceIndex<nodeCount_?
            nodes_[sourceIndex].driveWorkJ:0.f;
    }
    [[nodiscard]] float sourceDrivenMass(int sourceIndex=0) const noexcept {
        return sourceIndex>=0&&sourceIndex<nodeCount_?
            nodes_[sourceIndex].driveMassKg:0.f;
    }
    [[nodiscard]] float interactionElasticState(int nodeIndex) const noexcept {
        return nodeIndex>=0&&nodeIndex<nodeCount_?
            nodes_[nodeIndex].contactElastic:0.f;
    }
    [[nodiscard]] float storedModalEnergy() const noexcept {
        float energy=0.f;
        for(int n=0;n<nodeCount_;++n)
            for(int i=0;i<nodes_[n].modeCount;++i){
                const auto& mode=nodes_[n].modes[i];
                energy+=mode.real*mode.real+mode.imag*mode.imag;
            }
        return energy;
    }
    void release() noexcept {
        if(legacyMode_){legacy_.release();return;}
        if(!released_){released_=true;releasedAt_=tick_;}
    }
    void stop() noexcept {
        legacy_.stop();
        active_=false;
    }
    [[nodiscard]] bool active() const noexcept {
        return legacyMode_?legacy_.active():active_;
    }
    float render() noexcept {
        if(legacyMode_)return legacy_.render();
        if(!active_)return 0.f;
        e2Smooth_+=.002f*(e2_-e2Smooth_);
        nextReactionForceN_=0.f;
        // Recompute passive loss from ongoing physical contacts each
        // sample so an M gesture can silence an *already ringing* string.
        // The contact acts on every outgoing branch of its source object.
        for(int i=0;i<nodeCount_;++i)nodes_[i].touchLoss.fill(0.f);
        for(int i=0;i<contactCount_;++i){
            const auto& touch=contacts_[static_cast<size_t>(i)];
            if(tick_<touch.activateAt)continue; // Y coordinate is time
            const float position=touch.modEnabled?
                touch.modMin+(touch.modMax-touch.modMin)*e2Smooth_:
                touch.pressure+touch.mDepth*e2Smooth_;
            // Moving contact travels toward its interacting surface; only
            // geometric overlap causes dissipation (no direct amplitude knob).
            const float pressure=std::clamp(position,0.f,1.f);
            if(pressure<.0001f)continue;
            const float amount=pressure*pressure*touch.stiffness;
            auto& source=nodes_[static_cast<size_t>(touch.source)];
            for(int m=0;m<source.modeCount;++m)
                source.touchLoss[static_cast<size_t>(m)]+=
                    amount*touch.lossPerSecond[static_cast<size_t>(m)];
        }
        float result=0.f;
        for(int j=0;j<nodeCount_;++j){
            const int id=nodeOrder_[static_cast<size_t>(j)];
            auto& n=nodes_[static_cast<size_t>(id)];
            float input=0.f;
            for(int k=0;k<n.inCount;++k){
                const auto& e=edges_[static_cast<size_t>(n.incoming[k])];
                input+=e.buffer[(head_-e.delay+kFlowDelayFrames)&
                    (kFlowDelayFrames-1)];
            }
            // Inputs are delayed wave/force arrivals, not instant audio gains.
            // Modal objects retain energy after the source ceases.
            input=std::clamp(input,-1.f,1.f);
            float output=0.f;
            switch(n.node.kind) {
            case FlowKind::Exciter:output=excite(n);break;
            case FlowKind::Universal:
                if(n.node.role==PhysicalRole::Source)
                    output=humanSource(n);
                else if(n.node.role==PhysicalRole::Output){
                    output=input;result+=output;
                }else if(n.node.role==PhysicalRole::Interaction)
                    output=interact(n,input,id);
                else {
                    output=ring(n,input,id);
                    // The first object carries imposed motion AND its own vibration.
                    if(graph_.objectFlow&&n.node.role==PhysicalRole::Transfer)
                        output=std::clamp(output+input,-.98f,.98f);
                }
                break;
            case FlowKind::Output:
                output=input;
                result+=output;
                break;
            case FlowKind::Router:
                // The router is an energy-conserving moving nozzle: only
                // outgoing port weights change when M moves its position.
                output=input;
                break;
            case FlowKind::Reed:
                output=windExcite(n,input,true);
                break;
            case FlowKind::Edge:
                output=windExcite(n,input,false);
                break;
            case FlowKind::Damper:
                // An attached contact acts on the upstream object's
                // vibration (above), not a post-string volume control.
                // The contact's audio-through path remains available to
                // drive a later resonating object in the timeline.
                output=input;
                break;
            case FlowKind::Bridge:{
                const float stiffness=materialWaveSpeed(n.node.material);
                const float alpha=std::clamp(stiffness/6800.f,.08f,.95f);
                n.lowpass+=alpha*(input-n.lowpass);
                output=(1.f-materialProperties(n.node.material).lossFactor)*
                    n.lowpass;
                break;
            }
            default:output=ring(n,input,id);break;
            }
            output=std::clamp(output,-.98f,.98f);
            // The router shares one finite upstream flow across receivers.
            // A moving outlet is a physical position, not a crossfade gain.
            float routeTotal=0.f;
            if(n.node.kind==FlowKind::Router){
                const float nozzle=std::clamp(
                    n.node.position+n.node.motion*e2Smooth_,0.f,1.f);
                for(int k=0;k<n.outCount;++k){
                    const auto& e=edges_[static_cast<size_t>(n.outgoing[k])];
                    const float delta=(nozzle-e.position)/e.aperture;
                    routeTotal+=1.f/(1.f+delta*delta*delta*delta);
                }
            }
            for(int k=0;k<n.outCount;++k){
                auto& e=edges_[static_cast<size_t>(n.outgoing[k])];
                float portion=1.f;
                if(n.node.kind==FlowKind::Router){
                    const float nozzle=std::clamp(
                        n.node.position+n.node.motion*e2Smooth_,0.f,1.f);
                    const float delta=(nozzle-e.position)/e.aperture;
                    const float weight=1.f/(1.f+delta*delta*delta*delta);
                    portion=weight/std::max(routeTotal,1.e-8f);
                }
                float flux=output*e.gain*portion;
                // Different interaction laws propagate different mechanical
                // disturbances, without directly changing the final audio.
                if(graph_.objectFlow&&physicalVibrates(n.node)&&
                   physicalVibrates(nodes_[e.to].node)){
                    flux=objectInteraction(e,n,flux);
                }else if(graph_.objectFlow&&n.node.substance.enabled&&isUniversalOutput(nodes_[e.to].node)){
                    const float x=std::clamp(e.modEnabled?e.modMin+(e.modMax-e.modMin)*e2Smooth_:e.position,.005f,.995f);
                    const float width=e.aperture*.5f;
                    float pickup=0.f;
                    for(int m=0;m<n.modeCount;++m){
                        const float k=kPi*(m+1.f);
                        const float span=k*width;
                        const float avg=span>.00001f?std::sin(span)/span:1.f;
                        pickup+=n.modes[m].real*std::sin(k*x)*avg;
                    }
                    const auto& port=e.interaction;
                    const float orientation=.12f+.88f*std::fabs(std::cos(port.angleDeg*kPi/180.f));
                    const float location=(.4f+.6f*(1.f-.5f*(std::fabs(port.receiverY-.5f)+std::fabs(port.receiverZ-.5f))))/(1.f+4.f*port.clearance);
                    flux=e.gain*e.aperture*orientation*location*pickup*.85f;
                }else if(!graph_.objectFlow&&e.law==ContactLaw::Friction){
                    flux=.72f*flux+.28f*(flux-e.previousFlux);
                }else if(!graph_.objectFlow&&e.law==ContactLaw::Contact){
                    flux*=.72f+.28f*std::clamp(e.aperture,0.f,1.f);
                }else if(!graph_.objectFlow&&e.law==ContactLaw::Flow){
                    flux*=.5f+.5f*std::sqrt(std::max(0.f,e.aperture));
                }
                e.previousFlux=output*e.gain*portion;
                e.filter+=e.smoothing*(flux-e.filter);
                e.buffer[head_]=e.filter;
            }
        }
        head_=(head_+1)&(kFlowDelayFrames-1);
        // Contacts react on the driving transfer mass on the NEXT sample:
        // graph travel remains forward-only, physical force feedback does not.
        reactionForceN_=std::clamp(
            .96f*reactionForceN_+.04f*nextReactionForceN_,0.f,30.f);
        ++tick_;
        // Never kill a physical voice at a fixed note-off duration. The
        // simulator runs until both stored and traveling energy have decayed.
        const auto& source=graph_.nodes[0];
        bool continuous=false;
        for(int i=0;i<nodeCount_;++i){
            const auto& n=nodes_[i].node;
            const bool sustained=isUniversalSource(n)?
                sourceDriveActive(n):
                n.kind==FlowKind::Exciter&&
                (n.excitation==Excitation::Blow||
                 n.excitation==Excitation::Scrape)&&!released_;
            if(sustained&&e1_>.0001f){continuous=true;break;}
        }
        float stored=0.f;
        for(int i=0;i<nodeCount_;++i){
            for(int j=0;j<nodes_[i].modeCount;++j){
                const auto& mode=nodes_[i].modes[j];
                stored+=mode.real*mode.real+mode.imag*mode.imag;
            }
            stored+=nodes_[i].lowpass*nodes_[i].lowpass;
        }
        // Delay lines can contain energy after nodes go quiet. Account for
        // at least a full maximum transit before quiescence detection.
        const float level=std::max(result*result,stored*.002f);
        activity_+=.001f*(level-activity_);
        const uint64_t earliest=static_cast<uint64_t>(
            sampleRate_*(.28f+graph_.spanMs*.002f));
        if(!continuous&&tick_>earliest&&activity_<1.e-10f){
            if(++quietFrames_>static_cast<uint32_t>(sampleRate_*.18f))
                active_=false;
        }else quietFrames_=0;
        if(!std::isfinite(result)){stop();return 0.f;}
        return std::clamp(result,-1.f,1.f);
    }
private:
    struct Mode {
        float real{},imag{},cosStep{},sinStep{},r{},inputGain{},weight{},maxAmp{};
    };
    struct Node {
        FlowNode node{};
        MaterialProperties physicalMaterial{};
        bool fluid{};
        float viscosity{};
        std::array<Mode,kMaterialModes> modes{};
        std::array<float,kMaterialModes> touchLoss{};
        std::array<int,kMaxFlowLinks> incoming{},outgoing{};
        int inCount{},outCount{},modeCount{};
        float lowpass{},phase{},jetMemory{},transferRetention{1.f};
        // Voice-local mechanical motion and contact storage; never shared
        // across notes and never reinitialized by B pitch recalibration.
        float driveMassKg{1.e-3f},driveVelocity{},driveWorkJ{};
        float contactElastic{},contactSlip{};
        uint32_t randomSeed{};
    };
    struct Edge {
        int from{},to{},delay{1};
        float gain{},filter{},smoothing{};
        float position{.5f},modTravel{},aperture{.22f};
        float modMin{.5f},modMax{.5f},previousFlux{};
        bool modEnabled{false};
        ContactLaw law{ContactLaw::Coupled};
        FlowLink interaction{};
        float elastic{},slip{};
        std::array<float,kFlowDelayFrames> buffer{};
    };
    struct Contact {
        int source{-1};
        float pressure{},mDepth{},stiffness{1.f},modMin{},modMax{};
        bool modEnabled{false};
        uint64_t activateAt{};
        std::array<float,kMaterialModes> lossPerSecond{};
    };
    static float safe01(float v) noexcept {
        return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;
    }
    static float materialWaveSpeed(const FlowNode& n) noexcept {
        const auto m=objectMaterial(n);
        return std::sqrt(m.youngModulusPa/std::max(n.substance.enabled?.05f:1.f,m.densityKgM3));
    }
    static float materialImpedance(const FlowNode& n) noexcept {
        const auto m=objectMaterial(n);return std::sqrt(m.youngModulusPa*m.densityKgM3);
    }
    static float materialWaveSpeed(Material material) noexcept {
        const auto m=materialProperties(material);
        return std::sqrt(m.youngModulusPa/std::max(m.densityKgM3,1.f));
    }
    static float materialImpedance(Material material) noexcept {
        const auto m=materialProperties(material);
        // Impedance proxy for an equally sized contact area.
        return std::sqrt(m.youngModulusPa*m.densityKgM3);
    }
    static float randomSigned(Node& n) noexcept {
        n.randomSeed^=n.randomSeed<<13;
        n.randomSeed^=n.randomSeed>>17;
        n.randomSeed^=n.randomSeed<<5;
        return (static_cast<float>(n.randomSeed&65535u)/32767.5f)-1.f;
    }
    float excite(Node& n) noexcept {
        const auto mat=materialProperties(n.node.material);
        const float softness=mat.contactSoftness;
        const float hardness=safe01(n.node.hardness);
        // V is normalized source energy; force amplitudes scale as sqrt(V).
        const float force=std::sqrt(e1_);
        const float timeMs=std::clamp(
            .18f+4.0f*(1.f-hardness)*(.25f+softness),.15f,6.0f);
        const int duration=std::max(2,static_cast<int>(
            sampleRate_*timeMs*.001f));
        switch(n.node.excitation) {
        case Excitation::Pluck:
            if(tick_>=static_cast<uint64_t>(duration))return 0.f;
            return .055f*force*std::sin(
                kPi*static_cast<float>(tick_+1)/static_cast<float>(duration));
        case Excitation::Hammer:
            if(tick_>=static_cast<uint64_t>(duration))return 0.f;
            return .065f*force*(1.f-std::cos(
                2.f*kPi*static_cast<float>(tick_)/static_cast<float>(duration)));
        case Excitation::Blow:
            if(released_)return 0.f;
            return (.025f+.010f*randomSigned(n))*force;
        case Excitation::Scrape:
            if(released_)return 0.f;
            return .038f*force*randomSigned(n)*
                (softness*.4f+.6f);
        }
        return 0.f;
    }
    static float physicalMediumMass(const FlowNode& n) noexcept {
        // Real material density times geometric volume. Filaments use their
        // cross section, sheets use thickness, gas uses cavity volume.
        if(n.substance.enabled)return std::clamp(occupiedVolume(n)*objectMaterial(n).densityKgM3,1.e-6f,40.f);
        const auto& g=n.form;
        const float L=std::clamp(g.lengthMeters,.001f,4.f);
        const float W=std::clamp(g.widthMeters,1.e-5f,2.f);
        const float D=std::clamp(g.depthMeters,1.e-5f,2.f);
        const float profile=std::clamp(normalizedAverageProfile(g),.1f,4.f);
        const float section=objectContinuum(n)==Continuum::Filament?
            kPi*.25f*W*D:W*D;
        const float volume=L*section*profile;
        return std::clamp(volume*objectMaterial(n).densityKgM3,
                          1.e-6f,40.f);
    }
    bool sourceDriveActive(const FlowNode& p) const noexcept {
        if(p.sourceFollowNote)return !released_;
        return tick_<static_cast<uint64_t>(
            std::max(1.f,std::clamp(p.sourceTimeMs,.4f,2000.f)*
                sampleRate_*.001f));
    }
    // APT: acceleration sets free driving force F=m*a against the connected
    // transfer mass. Potential limits delivered work (finite) or power
    // (sustained). Reaction from contact opposes the source on the next
    // sample, but NEVER modifies the user's stored Potential setting.
    float humanSource(Node& n) noexcept {
        const auto& p=n.node;
        const float mass=n.driveMassKg;
        const float dt=1.f/sampleRate_;
        const float demand=std::clamp(p.sourceAcceleration,.1f,120.f)*
            (.88f+.12f*noteOnEffort_); // V changes initial acceleration gently
        const float potential=safe01(p.sourcePotential)*e1_;
        const bool drive=sourceDriveActive(p)&&potential>0.f;
        float force=drive?mass*demand:0.f;
        if(drive){
            const float forwardSpeed=std::max(.004f,n.driveVelocity);
            if(p.sourceFollowNote){
                // Normalized Potential becomes continuous mechanical power.
                const float maxPower=.006f*potential;
                force=std::min(force,maxPower/forwardSpeed);
            }else{
                // Potential is a FINITE total mechanical work budget.
                const float budget=.000022f*potential;
                const float workLeft=std::max(0.f,budget-n.driveWorkJ);
                force=std::min(force,workLeft/(forwardSpeed*dt));
            }
        }
        // Viscous drag ensures bounded motion for an indefinitely held note.
        // Contact reaction may reverse moving mass briefly (rebound).
        const float drag=mass*42.f*n.driveVelocity;
        // Resistive contact force opposes CURRENT velocity. Once a mass
        // rebounds it must not keep being accelerated backwards by the
        // same positive scalar reaction (a source of numerical energy).
        const float oppose=std::copysign(
            std::min(reactionForceN_,mass*120.f),n.driveVelocity);
        const float acceleration=(force-drag-oppose)/mass;
        n.driveVelocity=std::clamp(
            n.driveVelocity+acceleration*dt,-2.f,2.f);
        if(force>0.f){
            const float work=force*std::max(0.f,n.driveVelocity)*dt;
            n.driveWorkJ+=std::max(0.f,work);
        }
        // Transfer carries bounded mechanical movement; it is NOT a
        // predefined pluck/bow/breath waveform or a final-output envelope.
        // No new acoustic input is emitted after the finite source stops;
        // modal and delay-line energy downstream continues to ring.
        if(!drive)return 0.f;
        return .060f*std::tanh(3.8f*n.driveVelocity);
    }
    // Contact state belongs to this edge; only the two endpoints resonate.
    // Reduced normalized mechanics, not an energy-exact distributed solver.
    float objectInteraction(Edge& e,Node& sender,float input) noexcept {
        const auto& p=e.interaction;
        const auto& receiver=nodes_[e.to];
        float outgoing=0.f,returning=0.f;
        const float targetX=std::clamp(e.modEnabled?
            e.modMin+(e.modMax-e.modMin)*e2Smooth_:e.position,.005f,.995f);
        const float sourceX=std::clamp(p.fromPosition,.005f,.995f);
        for(int m=0;m<sender.modeCount;++m)
            outgoing+=sender.modes[m].imag*sender.modes[m].weight*
                std::sin(kPi*(m+1.f)*sourceX);
        for(int m=0;m<receiver.modeCount;++m)
            returning+=receiver.modes[m].imag*receiver.modes[m].weight*
                std::sin(kPi*(m+1.f)*targetX);
        const bool constructed=sender.node.substance.enabled||receiver.node.substance.enabled;
        // A receiver must not be reinjected into its own delayed force input.
        // The forward path is driven only by the sender; passive back reaction
        // is applied separately below. Preserve older experiment behavior.
        const float rel=std::clamp(constructed?input:input+.12f*outgoing-.18f*returning,-.8f,.8f);
        const float softness=.5f*(sender.physicalMaterial.contactSoftness+
            receiver.physicalMaterial.contactSoftness);
        // M moves the contact across the receiver's finite axial surface.
        // Endpoints have zero overlap, midpoint full overlap in this reduced
        // contact profile. This is geometry, not an output volume automation.
        const float mx=e.modEnabled?e.modMin+(e.modMax-e.modMin)*e2Smooth_:e.position;
        const float overlap=constructed&&e.modEnabled?std::pow(std::sin(kPi*safe01(mx)),2.f):1.f;
        const float area=std::clamp(e.aperture,0.f,1.f)*safe01(p.engagement)*overlap;
        const float adhesion=safe01(p.adhesion),motion=safe01(p.motion);
        const float elasticity=.15f+1.7f*safe01(p.elasticity);
        const float roughness=.5f*((sender.node.substance.enabled?sender.node.substance.value[Roughness]:.5f)+
            (receiver.node.substance.enabled?receiver.node.substance.value[Roughness]:.5f));
        const float friction=2.f*safe01(p.friction)*(sender.node.substance.enabled||receiver.node.substance.enabled?.35f+1.3f*roughness:1.f);
        const float recovery=.5f*((sender.node.substance.enabled?sender.node.substance.value[ElasticRecovery]:1.f)+
            (receiver.node.substance.enabled?receiver.node.substance.value[ElasticRecovery]:1.f));
        const float restitution=safe01(p.restitution)*recovery;
        const float gap=safe01(p.clearance)*.10f;
        const float gate=gap>0.f?std::clamp((std::fabs(rel)-gap)/.025f,0.f,1.f):1.f;
        const float angle=std::clamp(p.angleDeg,-90.f,90.f)*kPi/180.f;
        const float elevation=p.elevationDeg*kPi/180.f;
        const float spatial=.35f+.65f*std::clamp(1.f-.5f*(std::fabs(p.senderY-p.receiverY)+std::fabs(p.senderZ-p.receiverZ)),0.f,1.f);
        const float previous=e.elastic;
        float flux=0.f,resistance=0.f;
        // Stationary adhesive coupling transfers signed force, no invented motion.
        const float attached=adhesion*(1.f-motion);
        const float normal=std::max(0.f,std::cos(angle)*std::cos(elevation)*rel);
        const float tangential=std::clamp(std::sin(angle)+.5f*std::sin(elevation),-1.f,1.f)*rel*motion;
        if(sender.fluid||receiver.fluid){
            const float viscous=1.f/(1.f+.15f*(sender.viscosity+receiver.viscosity));
            e.elastic+=.055f*elasticity*viscous*(rel-e.elastic);
            float coherence=1.f;
            const Node* fluid=sender.fluid?&sender:&receiver;
            if(fluid->node.substance.enabled){
                const auto& g=fluid->node.form;
                coherence=(.2f+.8f*g.fillLevel)*(.4f+.6f*g.confinement)*
                    (1.f-.6f*g.spread*(1.f-g.confinement))*
                    (.25f+.75f*std::fabs(std::cos((g.flowAngle-p.angleDeg)*kPi/180.f)));
            }
            flux=.28f*viscous*coherence*(constructed?e.elastic:
                std::copysign(std::sqrt(std::fabs(e.elastic)),e.elastic));
            resistance=.3f*std::fabs(rel-e.elastic);
        }else{
            const float compression=(1.f-attached)*normal;
            const float shear=friction*tangential;
            e.elastic=std::clamp((.86f+.10f*restitution)*e.elastic+
                .12f*elasticity*(attached*rel+compression+shear),-.3f,.3f);
            const float threshold=.005f+.025f*softness+.02f*adhesion;
            e.slip=std::copysign(std::max(0.f,std::fabs(e.elastic)-threshold),e.elastic);
            e.elastic-=.45f*motion*e.slip;
            flux=attached*.75f*rel+.75f*(e.elastic-previous)+
                .22f*restitution*e.elastic+1.8f*motion*e.slip;
            resistance=(1.f-attached)*normal+friction*std::fabs(tangential)+
                std::fabs(e.slip)*motion+attached*.2f*std::fabs(rel-e.elastic);
        }
        flux*=area*gate*spatial;
        resistance*=area*gate*spatial;
        if(constructed){
            // Stable forward transmission: the elastic state may shape an
            // input but cannot emit a larger force than the incoming wave
            // plus its decaying stored displacement. No feedback gain.
            const float bound=area*gate*spatial*(std::fabs(input)+.25f*std::fabs(previous));
            flux=std::clamp(flux,-bound,bound);
        }
        // Local passive loss opposes sender vibration; only directly driven
        // transfer contacts feed the APT drive reaction.
        const float retain=1.f-std::clamp(resistance*.08f,0.f,.025f);
        for(int m=0;m<sender.modeCount;++m){
            float localRetain=retain;
            if(constructed){
                // Passive contact loading: a stationary lossy receiver absorbs
                // vibration at the sender contact point even after drive stops.
                // No receiver state is copied into the sender or vice versa.
                const float shape=std::sin(kPi*(m+1.f)*sourceX);
                const float loss=receiver.physicalMaterial.lossFactor;
                const float absorption=area*gate*spatial*(1.f-motion)*
                    (1.f-safe01(p.restitution))*(.15f+.85f*receiver.physicalMaterial.contactSoftness)*
                    std::clamp(loss*4.f,0.f,1.f);
                const float rate=2200.f*absorption*shape*shape;
                localRetain*=std::exp(-rate/sampleRate_);
            }
            sender.modes[m].real*=localRetain;sender.modes[m].imag*=localRetain;
        }
        if(sender.node.role==PhysicalRole::Transfer)
            nextReactionForceN_+=std::clamp(resistance*
                nodes_[0].driveMassKg*240.f,0.f,8.f);
        return std::clamp(flux,-.8f,.8f);
    }
    // Reduced two-material contact solver. The moving material and receiving
    // resonator act on each other: previous resonant state changes relative
    // velocity and contact stores, releases or dissipates mechanical energy.
    // It is a stable local approximation, not a full finite-element solver.
    float interact(Node& n,float input,int id) noexcept {
        const FlowNode& p=n.node;
        float incomingSoftness=materialProperties(p.material).contactSoftness;
        float area=1.f;
        for(int j=0;j<n.inCount;++j){
            const auto& edge=edges_[static_cast<size_t>(n.incoming[j])];
            incomingSoftness=materialProperties(
                nodes_[static_cast<size_t>(edge.from)].node.material).contactSoftness;
            area=std::clamp(edge.aperture,.035f,1.f);
            break;
        }
        float returnMotion=0.f;
        float receivingSoftness=incomingSoftness;
        for(int j=0;j<n.outCount;++j){
            const auto& edge=edges_[static_cast<size_t>(n.outgoing[j])];
            const Node& receiver=nodes_[static_cast<size_t>(edge.to)];
            receivingSoftness=materialProperties(receiver.node.material).contactSoftness;
            // Quadrature components approximate local returning velocity.
            for(int m=0;m<receiver.modeCount;++m)
                returnMotion+=receiver.modes[m].imag*
                    receiver.modes[m].weight;
        }
        returnMotion=std::clamp(returnMotion,-.25f,.25f);
        const float rel=std::clamp(input-.20f*returnMotion,-.8f,.8f);
        const float angle=std::clamp(p.interactionAngleDeg,-90.f,90.f)*
            (kPi/180.f);
        const float normal=std::max(0.f,std::cos(angle));
        const float tangent=std::sin(angle);
        const float engagement=safe01(p.interactionEngagement)*
            std::clamp(p.coupling,0.f,1.f);
        const float softness=std::clamp(
            (incomingSoftness+materialProperties(p.material).contactSoftness+
             receivingSoftness)/3.f,0.f,1.f);
        const float friction=2.f*safe01(p.interactionFriction);
        const float elasticity=2.f*safe01(p.interactionElasticity);
        const float restitution=2.f*safe01(p.interactionRestitution);
        const float adhesion=safe01(p.interactionAdhesion);
        const float clearance=safe01(p.interactionClearance)*.12f;
        const float gapFactor=clearance>0.f?
            std::clamp((std::fabs(rel)-clearance)/.025f,0.f,1.f):1.f;
        area=std::clamp(area*2.f*safe01(p.interactionArea),0.f,1.f);
        const float stiffness=1.f-softness;
        const float previousElastic=n.contactElastic;
        float excitation=0.f,reaction=0.f;
        switch(p.law){
        case ContactLaw::Coupled:
            // Compliant coupling transmits the relative force both ways.
            n.contactElastic+=.09f*elasticity*(rel-n.contactElastic);
            excitation=engagement*(.74f*rel+.26f*n.contactElastic);
            reaction=engagement*std::fabs(rel-n.contactElastic);
            break;
        case ContactLaw::Friction:{
            // A tangential stick-slip approximation: sticking stores
            // deformation, threshold crossing releases it as a pulse.
            // Receiver motion shifts this crossing; no lock frequency exists.
            const float speed=tangent*rel;
            n.contactElastic+=.24f*speed*friction;
            const float threshold=(.002f+.026f*softness)*
                (.35f+.65f*engagement)*(1.f+adhesion);
            const bool slips=std::fabs(n.contactElastic)>threshold;
            if(slips){
                const float excess=n.contactElastic-
                    std::copysign(threshold*.40f,n.contactElastic);
                n.contactElastic-=std::clamp(.82f*restitution,.05f,.98f)*excess;
                n.contactSlip=excess;
            }else n.contactSlip*=.73f;
            const float restoring=n.contactElastic-previousElastic;
            excitation=engagement*area*(.65f*restoring+
                (slips?2.8f*n.contactSlip:0.f));
            reaction=engagement*(friction*std::fabs(speed)*(.3f+.7f*stiffness)+
                2.f*std::fabs(n.contactSlip));
            break;
        }
        case ContactLaw::Contact:{
            // Normal compression and elastic rebound; tangential energy
            // can slip away rather than being forced into the resonator.
            const float compression=std::max(0.f,normal*rel);
            n.contactElastic=std::clamp(
                std::clamp(.90f+.07f*(restitution-1.f),.75f,.98f)*n.contactElastic+.32f*elasticity*compression,0.f,.3f);
            const float delta=n.contactElastic-previousElastic;
            excitation=engagement*(.8f*delta+.18f*n.contactElastic);
            reaction=engagement*std::fabs(compression-delta);
            break;
        }
        case ContactLaw::Flow:{
            // Pressure difference through a finite opening produces a
            // nonlinear signed flow. Gas properties live in the medium.
            n.contactElastic+=.055f*(rel-n.contactElastic);
            const float pressureDifference=n.contactElastic;
            excitation=engagement*area*std::copysign(
                std::sqrt(std::fabs(pressureDifference))*.28f,
                pressureDifference);
            reaction=engagement*.3f*std::fabs(rel-pressureDifference);
            break;
        }
        }
        // A continuous solid-contact option combines normal and tangential motion.
        // Flow retains its separate pressure/flow law.
        const float mix=p.law==ContactLaw::Flow?0.f:safe01(p.interactionBlend);
        if(mix>0.f){
            const float compression=std::max(0.f,normal*rel);
            const float sliding=tangent*rel;
            const float target=std::clamp(elasticity*compression+
                friction*sliding,-.3f,.3f);
            const float elastic=std::clamp(previousElastic+
                .12f*(target-previousElastic),-.3f,.3f);
            const float delta=elastic-previousElastic;
            const float threshold=.006f+.025f*softness+.02f*adhesion;
            const float slip=std::copysign(std::max(0.f,
                std::fabs(elastic)-threshold),elastic);
            const float unified=engagement*area*(.65f*delta+
                restitution*.18f*elastic+friction*1.2f*slip);
            const float resistance=engagement*area*(compression+
                friction*std::fabs(sliding)+std::fabs(slip));
            n.contactElastic=std::clamp((1.f-mix)*n.contactElastic+
                mix*(elastic-.4f*slip),-.3f,.3f);
            excitation=(1.f-mix)*excitation+mix*unified;
            reaction=(1.f-mix)*reaction+mix*resistance;
        }
        excitation*=gapFactor*(p.law==ContactLaw::Friction||
            p.law==ContactLaw::Flow?1.f:std::clamp(2.f*safe01(p.interactionArea),0.f,2.f));
        reaction*=gapFactor*(1.f+.4f*adhesion);
        // Port reaction is capped and smoothed by the mechanical source.
        // The source is not reprogrammed: its actual transfer motion changes.
        nextReactionForceN_+=std::clamp(
            reaction*std::max(1.e-6f,nodes_[0].driveMassKg)*240.f,
            0.f,8.f);
        // Keep role's own material modes; interactions are not an audio
        // gain stage. Physical state survives pitch bend and note-off.
        return ring(n,std::clamp(excitation,-.8f,.8f),id);
    }

    // Reduced differentiable reed/edge exciters; they share the physical
    // incoming breath source and send independently generated disturbances
    // into a downstream air chamber. Full bore-to-valve feedback is future
    // work and must not be implied by this approximation.
    float windExcite(Node& n,float input,bool reed) noexcept {
        const float pressure=std::clamp(input*8.f,0.f,1.f);
        const float bodySoftness=materialProperties(n.node.material).contactSoftness;
        const float stiffness=materialWaveSpeed(n.node.material);
        const float compliance=reed?
            (.6f+.8f*(1.f-n.node.hardness)):(.7f+.3f*bodySoftness);
        const float inertia=reed?
            std::clamp(.008f*compliance,.002f,.04f):
            std::clamp(.035f/(.5f+n.node.thicknessMm*.07f),.002f,.05f);
        n.jetMemory+=inertia*(pressure-n.jetMemory);
        const float flow=n.jetMemory;
        if(flow<.00001f)return 0.f;
        const float relative=std::clamp(stiffness/3000.f,.55f,1.65f);
        const float geometric=reed?
            std::pow(std::clamp(n.node.size,.25f,4.f),-.14f):
            (1.f+.06f*(n.node.position-.5f));
        const float frequency=pitchHz_*(reed?1.f:1.018f)*
            (1.f+.03f*flow)*std::pow(relative,reed?.09f:.02f)*geometric;
        n.phase+=2.f*kPi*frequency/sampleRate_;
        if(n.phase>2.f*kPi)n.phase-=2.f*kPi;
        const float cycle=std::sin(n.phase);
        if(reed){
            // A periodically closing valve makes a harmonically rich
            // pressure disturbance, modulated by compliance.
            const float softness=materialProperties(n.node.material).contactSoftness;
            return .40f*flow*n.node.coupling*std::tanh(
                (1.2f+1.3f*n.node.hardness+2.f*(1.f-softness))*cycle);
        }
        // An air jet interacting with an edge has smoother oscillation and
        // weak turbulence. No separate energy source is introduced.
        const float turbulence=.04f*randomSigned(n);
        return .34f*flow*n.node.coupling*(cycle+.15f*std::sin(2.f*n.phase)+turbulence);
    }
    void setupNode(Node& n,float pitch) noexcept {
        if(!physicalVibrates(n.node))return;
        const auto& p=n.node;
        const auto material=objectMaterial(p);
        const float speed=materialWaveSpeed(p);
        const float refSpeed=std::sqrt(11.e9f/650.f);
        const float speedRatio=std::clamp(speed/refSpeed,.2f,6.f);
        const float inertia=std::clamp(std::sqrt(650.f/material.densityKgM3),.22f,2.8f);
        const float thickness=std::clamp(p.thicknessMm,.3f,30.f)/3.f;
        const float size=std::clamp(p.size,.25f,4.f);
        const float stiffnessFactor=std::clamp(std::sqrt(speedRatio),.5f,2.2f);
        const float loss=std::max(.0008f,material.lossFactor);
        const bool anchored=!primaryPitchAssigned_;
        if(anchored)primaryPitchAssigned_=true;
        const float bodyBase=(p.kind==FlowKind::Body)?
            std::clamp(110.f*speedRatio*std::pow(thickness,.32f)/size,
                       36.f,sampleRate_*.15f):pitch;
        // Unlike the legacy v4 instrument objects, a universal object's
        // fundamental frequency is derived from geometry and material.
        const float geometryBase=p.kind==FlowKind::Universal?
            physicalFundamental(p):0.f;
        const float base=p.kind==FlowKind::Universal?
            geometryBase:(anchored?pitch:bodyBase);
        n.modeCount=kMaterialModes;
        constexpr std::array<float,kMaterialModes> membrane{
            1.f,1.59f,2.14f,2.30f,2.65f,2.92f,3.16f};
        constexpr std::array<float,kMaterialModes> plate{
            1.f,2.31f,3.86f,5.59f,7.76f,10.5f,13.5f};
        constexpr std::array<float,kMaterialModes> bar{
            1.f,2.756f,5.404f,8.933f,13.34f,18.65f,24.86f};
        constexpr std::array<float,kMaterialModes> bodyBox{
            1.f,1.72f,2.82f,4.22f,6.22f,8.32f,11.9f};
        constexpr std::array<float,kMaterialModes> bodyBell{
            1.f,1.53f,2.12f,2.81f,3.68f,4.79f,6.20f};
        constexpr std::array<float,kMaterialModes> bodyCylinder{
            1.f,2.05f,2.79f,3.95f,4.97f,6.11f,8.10f};
        constexpr std::array<float,kMaterialModes> bodySphere{
            1.f,1.92f,3.11f,4.37f,5.81f,7.42f,9.36f};
        constexpr std::array<float,kMaterialModes> bodyBowl{
            1.f,1.64f,2.70f,4.16f,5.78f,7.99f,10.60f};
        const float b=std::clamp(.00022f*(material.youngModulusPa/1.e9f)*
            std::pow(std::clamp(p.diameterMm,.1f,3.f)/.5f,4.f)*
            (80.f/std::max(2.f,p.tensionNewtons)),0.00001f,.11f);
        for(int i=0;i<n.modeCount;++i){
            const float index=static_cast<float>(i);
            float ratio=index+1.f;
            switch(p.kind){
            case FlowKind::Universal:{
                const auto c=objectContinuum(p);
                if(c==Continuum::Filament){
                    const float h=index+1.f;
                    const float L=std::max(.025f,p.form.lengthMeters);
                    const float diameter=std::sqrt(std::max(.00000001f,
                        p.form.widthMeters*p.form.depthMeters));
                    const float area=.785398f*diameter*diameter;
                    const float EI=material.youngModulusPa*area*diameter*diameter*.0625f;
                    const float stiff=std::clamp(
                        kPi*kPi*EI/std::max(1.e-6f,p.tensionNewtons*L*L),0.f,.25f);
                    ratio=h*std::sqrt((1.f+stiff*h*h)/(1.f+stiff));
                    if(p.substance.enabled){
                        const float tension=std::max(0.f,p.tensionNewtons);
                        const float bend=EI/std::max(1.e-8f,L*L);
                        const float mix=bend/std::max(1.e-12f,bend+tension);
                        ratio=(1.f-mix)*ratio+mix*h*h;
                    }
                }else if(c==Continuum::Sheet){
                    // Bending surface with tension-dependent lower modes.
                    ratio=(p.tensionNewtons>120.f)?
                        membrane[i]:plate[i];
                }else if(fluidContinuum(c)){
                    // An open/open duct has integer harmonics; a mostly
                    // closed end supports the odd quarter-wave family.
                    ratio=(p.substance.enabled?fluidLeak(p.form):p.form.opening)<.5f?2.f*index+1.f:index+1.f;
                }else ratio=(p.form.depthMeters<p.form.widthMeters*.24f)?
                    plate[i]:bar[i];
                break;
            }
            case FlowKind::String:{
                const float h=index+1.f;
                ratio=h*std::sqrt((1.f+b*h*h)/(1.f+b));break;
            }
            case FlowKind::Membrane:ratio=membrane[i];break;
            case FlowKind::Plate:ratio=plate[i];break;
            case FlowKind::Bar:ratio=bar[i];break;
            case FlowKind::AirColumn:
                ratio=p.geometry==Geometry::HollowCylinder?index*2.f+1.f:
                    index+1.f;
                break;
            case FlowKind::Body:
                if(p.geometry==Geometry::Bell)ratio=bodyBell[i];
                else if(p.geometry==Geometry::HollowCylinder||
                        p.geometry==Geometry::Tube)ratio=bodyCylinder[i];
                else if(p.geometry==Geometry::Sphere)ratio=bodySphere[i];
                else if(p.geometry==Geometry::Bowl)ratio=bodyBowl[i];
                else if(p.geometry==Geometry::Plate)ratio=plate[i];
                else ratio=bodyBox[i];
                break;
            default:break;
            }
            float f=base*ratio;
            if(p.kind==FlowKind::Universal){
                const auto& form=p.form;
                const float avg=std::clamp(normalizedAverageProfile(form),.18f,3.f);
                const float neck=(form.stations[0]+form.stations[3])*.5f;
                const float bulge=(form.stations[1]+form.stations[2])*.5f;
                const float profileContrast=(bulge-neck)/(bulge+neck+.01f);
                const float curvature=std::clamp(form.curvature,-1.f,1.f);
                const float taper=std::clamp(form.taper,-1.f,1.f);
                // Geometry affects mode spacing, not just a common
                // gain; cross-section stations yield a nonuniform profile.
                const float dispersion=1.f+
                    index*(.028f*curvature+.052f*taper+
                           .08f*profileContrast)+
                    index*index*.002f*(avg-1.f);
                f*=std::clamp(dispersion,.65f,1.7f);
            }
            const float massStiffness=std::pow(speedRatio,.18f);
            if(p.kind==FlowKind::Plate||p.kind==FlowKind::Bar)
                f*=std::pow(thickness,.25f)*std::pow(size,-.28f)*
                    std::pow(massStiffness,index*.12f);
            else if(p.kind==FlowKind::Membrane)
                f*=std::pow(size,-.12f)*
                    std::sqrt(std::clamp(p.tensionNewtons/80.f,.1f,8.f));
            else if(p.kind==FlowKind::Body&&!anchored)
                f*=std::pow(massStiffness,index*.09f);
            else if(p.kind==FlowKind::String)
                f*=std::pow(size,-.12f);
            if(p.substance.enabled&&!objectIsFluid(p)&&i>=n.modeCount-2){
                const float cavity=cavityHz(p.form,i-(n.modeCount-2));
                if(cavity>0.f)f=cavity;
            }
            f=std::clamp(f,28.f,sampleRate_*.44f);
            const float omega=2.f*kPi*f/sampleRate_;
            const float damping=std::clamp(p.damping,.12f,8.f);
            // Material loss, surface friction and higher-frequency damping.
            float structuralLoss=1.f;
            if(p.substance.enabled){
                const auto& v=p.substance.value;
                // Empirical audible-band scattering proxy; not atomic simulation.
                structuralLoss+=.12f*(1.f-v[StructureOrder])*v[StructureScale]*index*index+
                    .25f*v[Porosity]*index;
                structuralLoss+=.08f*std::log1p(constructedViscosity(p.substance))*(1.f+index);
                if(objectIsFluid(p))structuralLoss+=4.f*(1.f-p.form.confinement)*(1.f+p.form.spread)+2.f*fluidLeak(p.form);
                else structuralLoss+=voidFraction(p.form)*.3f+fluidLeak(p.form)*voidFraction(p.form)*index*.25f;
                for(const auto& hole:p.form.openings)structuralLoss+=.08f*hole.size*hole.size*
                    (1.f+std::fabs(std::sin(kPi*(index+1.f)*hole.position+hole.azimuth*2.f*kPi)));
            }
            const float modalQ=std::clamp(
                .90f/(loss*damping*(1.f+.23f*index*index)*structuralLoss),1.8f,160.f);
            const float r=std::exp(-kPi*f/(modalQ*sampleRate_));
            // Stiff objects retain higher partials; soft materials absorb them.
            const float bright=std::clamp(.35f+speed/4600.f,.4f,1.45f);
            const float weight=std::pow(index+1.f,-std::clamp(1.40f/bright,.8f,3.2f));
            auto& m=n.modes[static_cast<size_t>(i)];
            m.cosStep=std::cos(omega);
            m.sinStep=std::sin(omega);
            m.r=r;
            // For equal impact force, higher mass means lower acceleration.
            const float thicknessMass=std::pow(thickness,-.20f);
            const int exciterId=graph_.first(FlowKind::Exciter);
            const float attackPosition=exciterId>=0?
                graph_.nodes[static_cast<size_t>(exciterId)].position:.22f;
            // The initial contact location excites different modal shapes.
            const float shape=std::fabs(std::sin(kPi*(index+1.f)*
                std::clamp(attackPosition,.01f,.99f)));
            m.inputGain=.19f*weight*inertia*thicknessMass*
                (.32f+.68f*shape)*(.7f+.3f*p.coupling);
            if(p.kind==FlowKind::Universal){
                const auto& form=p.form;
                const float dimensionMass=std::sqrt(
                    std::max(.0000001f,p.substance.enabled?occupiedVolume(p):form.lengthMeters*form.widthMeters*
                        form.depthMeters*normalizedAverageProfile(form)));
                // Integrate incoming mechanical force into a normalized
                // modal state. The old v4 full-scale per-sample coefficient
                // was too large for cascaded generic resonators; successive
                // stages saturated and inverted the V/energy relationship.
                // This factor is a reduced solver calibration, not a user
                // timbre control or a final audio gain adjustment.
                m.inputGain*=.30f*
                    std::clamp(.040f/dimensionMass,.22f,1.8f);
            }
            m.weight=weight;
            m.maxAmp=.45f*weight; // Modal energy bound: cannot swell arbitrarily
        }
    }
    float ring(Node& n,float input,int nodeId) noexcept {
        float sum=0.f;
        for(int i=0;i<n.modeCount;++i){
            auto& m=n.modes[static_cast<size_t>(i)];
            // Passive mechanical dissipation: r is always <= 1.
            // M varies the decay of energy already in the vibrator,
            // rather than merely scaling its output afterward.
            const float extra=std::clamp(
                1.f-n.touchLoss[static_cast<size_t>(i)]/sampleRate_,
                0.f,1.f);
            const float retention=m.r*extra*(n.node.kind==FlowKind::Universal?
                n.transferRetention:1.f);
            float localForce=0.f;
            for(int k=0;k<n.inCount;++k){
                const auto& e=edges_[static_cast<size_t>(n.incoming[k])];
                const float wave=e.buffer[(head_-e.delay+kFlowDelayFrames)&
                    (kFlowDelayFrames-1)];
                // Local position changes mode excitation, distinct from the
                // node's physical size or its drawing position.
                float physicalX=e.modEnabled?
                    e.modMin+(e.modMax-e.modMin)*e2Smooth_:
                    e.position+e.modTravel*e2Smooth_;
                const auto& movingSource=nodes_[static_cast<size_t>(e.from)].node;
                if(movingSource.kind==FlowKind::Universal&&
                   movingSource.movingEnabled)
                    physicalX+=movingSource.movingMin+
                        (movingSource.movingMax-movingSource.movingMin)*
                        e2Smooth_-.5f;
                if(n.node.kind==FlowKind::Universal&&n.node.movingEnabled)
                    physicalX+=n.node.movingMin+
                        (n.node.movingMax-n.node.movingMin)*e2Smooth_-.5f;
                const float x=std::clamp(physicalX,.005f,.995f);
                const float shape=std::sin(kPi*(i+1.f)*x);
                const float cross=n.node.substance.enabled?(.5f+.5f*std::fabs(std::cos(kPi*(e.interaction.receiverY-.5f))*std::cos(kPi*(e.interaction.receiverZ-.5f)))):1.f;
                localForce+=wave*(.15f+.85f*shape)*cross;
            }
            const float real=retention*(m.real*m.cosStep-m.imag*m.sinStep)+
                m.inputGain*localForce;
            const float imag=retention*(m.real*m.sinStep+m.imag*m.cosStep);
            const float amplitude=std::sqrt(real*real+imag*imag);
            const float limited=amplitude>m.maxAmp?
                m.maxAmp/std::max(amplitude,1.e-7f):1.f;
            m.real=real*limited;
            m.imag=imag*limited;
            sum+=m.real;
        }
        return sum*.85f;
    }
    Blueprint blueprint_{};
    FlowGraph graph_{};
    PhysicalInstrumentVoice legacy_{};
    float sampleRate_{48000.f},e1_{.8f},e2_{0.f},e2Smooth_{0.f};
    float noteOnEffort_{.8f},reactionForceN_{0.f},nextReactionForceN_{0.f};
    float pitchHz_{220.f},activity_{0.f};
    float currentBendSemitones_{0.f};
    int baseMidi_{60};
    bool pitchBendReady_{false};
    uint32_t quietFrames_{0};
    std::array<Node,kMaxFlowNodes> nodes_{};
    std::array<Edge,kMaxFlowLinks> edges_{};
    std::array<Contact,kMaxFlowLinks> contacts_{};
    int contactCount_{0};
    std::array<int,kMaxFlowNodes> nodeOrder_{};
    int nodeCount_{0},edgeCount_{0},head_{0};
    uint64_t tick_{0},releasedAt_{0};
    bool active_{false},released_{false},legacyMode_{false};
    bool primaryPitchAssigned_{false};
};

} // namespace aiora::physical
