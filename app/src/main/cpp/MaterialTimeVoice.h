#pragma once
// Aiora Material-Time Flow research synthesizer, v3.
// Material-dependent resonant objects with strictly causal, downward-only
// links. Vertical space is time. No mandatory string or bridge.
// Uses reduced modal models, not a full 3D finite-element simulation.
// Fixed buffers; render() has no dynamic allocation or graph edits.
#include "PhysicalInstrumentVoice.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace aiora::physical {

constexpr int kMaterialModes=7;
constexpr int kFlowDelayFrames=4096; // 85.3 ms at 48 kHz per connection

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
        if(graph_.nodeCount==0){
            legacyMode_=true;
            legacy_.start(blueprint,midi,velocity,mod);
            active_=true;
            return;
        }
        legacyMode_=false;
        active_=true;released_=false;tick_=0;releasedAt_=0;head_=0;
        e1_=safe01(velocity);e2_=safe01(mod);e2Smooth_=e2_;
        nodeCount_=std::min<int>(graph_.nodeCount,kMaxFlowNodes);
        edgeCount_=0;
        for(int i=0;i<kMaxFlowNodes;++i){
            nodes_[i]={};
            nodeOrder_[i]=i;
        }
        for(int i=0;i<nodeCount_;++i){
            nodes_[i].node=graph_.nodes[static_cast<size_t>(i)];
            nodes_[i].randomSeed=0x713f9a35u+static_cast<uint32_t>(i)*0x3c6ef35fu;
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
            const auto& from=nodes_[raw.from].node;
            const auto& to=nodes_[raw.to].node;
            // Vertical position represents time elapsed between interactions.
            // A small transit term accounts for sound velocity in the source.
            const float speed=materialWaveSpeed(from.material);
            const float physicalMs=1000.f*.12f/std::max(40.f,speed);
            const float delayMs=(to.y-from.y)*
                std::clamp(graph_.spanMs,3.f,82.f)+physicalMs;
            const int delay=static_cast<int>(std::lround(delayMs*sampleRate_*.001f));
            edge.delay=std::clamp(delay,1,kFlowDelayFrames-1);
            edge.filter=0.f;
            std::fill(edge.buffer.begin(),edge.buffer.end(),0.f);
            const float z0=materialImpedance(from.material);
            const float z1=materialImpedance(to.material);
            const float match=2.f*std::sqrt(z0*z1)/(z0+z1);
            const float contact=std::sqrt(std::clamp(to.coupling,0.f,1.f));
            edge.gain=std::clamp(.80f*match*contact,.03f,.83f);
            edge.smoothing=std::clamp(.83f-.58f*materialProperties(to.material).contactSoftness,
                                       .14f,.93f);
            nodes_[raw.from].outgoing[nodes_[raw.from].outCount++]=edgeCount_;
            nodes_[raw.to].incoming[nodes_[raw.to].inCount++]=edgeCount_;
            ++edgeCount_;
        }
        // Normalize every fan-out to prevent volume multiplication across
        // arbitrary parallel branches. No feedback loops are permitted.
        for(int i=0;i<edgeCount_;++i){
            auto& edge=edges_[static_cast<size_t>(i)];
            edge.gain/=std::sqrt(static_cast<float>(
                std::max(1,nodes_[edge.from].outCount)));
        }
        const float hz=std::clamp(
            440.f*std::pow(2.f,(midi-69)/12.f),25.f,sampleRate_*.16f);
        primaryPitchAssigned_=false;
        for(int j=0;j<nodeCount_;++j){
            const int id=nodeOrder_[j];
            setupNode(nodes_[static_cast<size_t>(id)],hz);
        }
    }
    void setExpression(float v,float m) noexcept {
        if(legacyMode_){legacy_.setExpression(v,m);return;}
        e1_=safe01(v);e2_=safe01(m);
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
            // Multiple inputs are real mixed mechanical contributions, but
            // bounded at the contact to avoid unrealistic energy insertion.
            input=std::clamp(input,-1.f,1.f);
            float output=0.f;
            switch(n.node.kind) {
            case FlowKind::Exciter:output=excite(n);break;
            case FlowKind::Output:
                output=input;
                result+=output;
                break;
            case FlowKind::Damper:{
                const float softness=materialProperties(n.node.material).contactSoftness;
                const float contact=std::clamp(e2Smooth_*n.node.coupling*
                    (.18f+.70f*softness),0.f,.98f);
                output=input*(1.f-contact);
                break;
            }
            case FlowKind::Bridge:{
                const float stiffness=materialWaveSpeed(n.node.material);
                const float alpha=std::clamp(stiffness/6800.f,.08f,.95f);
                n.lowpass+=alpha*(input-n.lowpass);
                output=(1.f-materialProperties(n.node.material).lossFactor)*
                    n.lowpass;
                break;
            }
            default:output=ring(n,input);break;
            }
            output=std::clamp(output,-.98f,.98f);
            for(int k=0;k<n.outCount;++k){
                auto& e=edges_[static_cast<size_t>(n.outgoing[k])];
                const float x=output*e.gain;
                e.filter+=e.smoothing*(x-e.filter);
                e.buffer[head_]=e.filter;
            }
        }
        head_=(head_+1)&(kFlowDelayFrames-1);
        ++tick_;
        if(released_&&tick_-releasedAt_>
           static_cast<uint64_t>(sampleRate_*3.f))active_=false;
        if(!std::isfinite(result)){stop();return 0.f;}
        return std::clamp(result,-1.f,1.f);
    }
private:
    struct Mode {
        float real{},imag{},cosStep{},sinStep{},r{},inputGain{},weight{},maxAmp{};
    };
    struct Node {
        FlowNode node{};
        std::array<Mode,kMaterialModes> modes{};
        std::array<int,kMaxFlowLinks> incoming{},outgoing{};
        int inCount{},outCount{},modeCount{};
        float lowpass{};
        uint32_t randomSeed{};
    };
    struct Edge {
        int from{},to{},delay{1};
        float gain{},filter{},smoothing{};
        std::array<float,kFlowDelayFrames> buffer{};
    };
    static float safe01(float v) noexcept {
        return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;
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
        const float force=e1_;
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
    void setupNode(Node& n,float pitch) noexcept {
        if(!vibrates(n.node.kind))return;
        const auto& p=n.node;
        const auto material=materialProperties(p.material);
        const float speed=materialWaveSpeed(p.material);
        const float refSpeed=std::sqrt(11.e9f/650.f);
        const float speedRatio=std::clamp(speed/refSpeed,.2f,6.f);
        const float thickness=std::clamp(p.thicknessMm,.3f,30.f)/3.f;
        const float size=std::clamp(p.size,.25f,4.f);
        const float stiffnessFactor=std::clamp(std::sqrt(speedRatio),.5f,2.2f);
        const float loss=std::max(.0008f,material.lossFactor);
        const bool anchored=!primaryPitchAssigned_;
        if(anchored)primaryPitchAssigned_=true;
        const float bodyBase=(p.kind==FlowKind::Body)?
            std::clamp(110.f*speedRatio*std::pow(thickness,.32f)/size,
                       36.f,sampleRate_*.15f):pitch;
        const float base=anchored?pitch:bodyBase;
        n.modeCount=kMaterialModes;
        constexpr std::array<float,kMaterialModes> membrane{
            1.f,1.59f,2.14f,2.30f,2.65f,2.92f,3.16f};
        constexpr std::array<float,kMaterialModes> plate{
            1.f,2.31f,3.86f,5.59f,7.76f,10.5f,13.5f};
        constexpr std::array<float,kMaterialModes> bar{
            1.f,2.756f,5.404f,8.933f,13.34f,18.65f,24.86f};
        constexpr std::array<float,kMaterialModes> body{
            1.f,1.72f,2.82f,4.22f,6.22f,8.32f,11.9f};
        const float b=std::clamp(.00022f*(material.youngModulusPa/1.e9f)*
            std::pow(std::clamp(p.diameterMm,.1f,3.f)/.5f,4.f)*
            (80.f/std::max(2.f,p.tensionNewtons)),0.00001f,.11f);
        for(int i=0;i<n.modeCount;++i){
            const float index=static_cast<float>(i);
            float ratio=index+1.f;
            switch(p.kind){
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
                ratio=body[i];break;
            default:break;
            }
            float f=base*ratio;
            const float massStiffness=std::pow(speedRatio,.18f);
            if(p.kind==FlowKind::Plate||p.kind==FlowKind::Bar)
                f*=std::pow(thickness,.25f)*std::pow(size,-.28f)*
                    std::pow(massStiffness,index*.12f);
            else if(p.kind==FlowKind::Membrane)
                f*=std::pow(size,-.12f)*
                    std::pow(std::clamp(p.tensionNewtons/80.f,.1f,8.f),.06f);
            else if(p.kind==FlowKind::Body&&!anchored)
                f*=std::pow(massStiffness,index*.09f);
            else if(p.kind==FlowKind::String)
                f*=std::pow(size,-.12f);
            f=std::clamp(f,28.f,sampleRate_*.44f);
            const float omega=2.f*kPi*f/sampleRate_;
            const float damping=std::clamp(p.damping,.12f,8.f);
            // Material loss, surface friction and higher-frequency damping.
            const float modalQ=std::clamp(
                .90f/(loss*damping*(1.f+.23f*index*index)),1.8f,160.f);
            const float r=std::exp(-kPi*f/(modalQ*sampleRate_));
            // Stiff objects retain higher partials; soft materials absorb them.
            const float bright=std::clamp(.35f+speed/4600.f,.4f,1.45f);
            const float weight=std::pow(index+1.f,-std::clamp(1.40f/bright,.8f,3.2f));
            auto& m=n.modes[static_cast<size_t>(i)];
            m.cosStep=std::cos(omega);
            m.sinStep=std::sin(omega);
            m.r=r;
            m.inputGain=.08f*weight*(.7f+.3f*p.coupling);
            m.weight=weight;
            m.maxAmp=.45f*weight; // Modal energy bound: cannot swell arbitrarily
        }
    }
    static float ring(Node& n,float input) noexcept {
        float sum=0.f;
        for(int i=0;i<n.modeCount;++i){
            auto& m=n.modes[static_cast<size_t>(i)];
            const float real=m.r*(m.real*m.cosStep-m.imag*m.sinStep)+
                m.inputGain*input;
            const float imag=m.r*(m.real*m.sinStep+m.imag*m.cosStep);
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
    std::array<Node,kMaxFlowNodes> nodes_{};
    std::array<Edge,kMaxFlowLinks> edges_{};
    std::array<int,kMaxFlowNodes> nodeOrder_{};
    int nodeCount_{0},edgeCount_{0},head_{0};
    uint64_t tick_{0},releasedAt_{0};
    bool active_{false},released_{false},legacyMode_{false};
    bool primaryPitchAssigned_{false};
};

} // namespace aiora::physical
