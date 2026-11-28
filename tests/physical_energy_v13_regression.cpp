#include "MaterialTimeVoice.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
using namespace aiora::physical;
static void wav(const std::string& path,const std::vector<float>& data){
    std::ofstream f(path,std::ios::binary);
    auto u16=[&](uint16_t v){char b[2]={char(v),char(v>>8)};f.write(b,2);};
    auto u32=[&](uint32_t v){u16(v&65535);u16(v>>16);};
    f.write("RIFF",4);u32(36+2*data.size());f.write("WAVEfmt ",8);u32(16);
    u16(1);u16(1);u32(48000);u32(96000);u16(2);u16(16);
    f.write("data",4);u32(2*data.size());
    for(float v:data)u16(static_cast<uint16_t>(static_cast<int16_t>(std::lround(std::tanh(v)*32767.))));
}
static FlowGraph struck(bool drum){
    auto g=constructionStarterGraph();g.spanMs=6.f;
    auto& e=g.nodes[0];e.sourceAcceleration=100.f;e.sourcePotential=.95f;e.sourceTimeMs=6.f;
    auto& mallet=g.nodes[1];mallet.form.lengthMeters=.035f;mallet.form.widthMeters=.02f;mallet.form.depthMeters=.012f;mallet.form.rounding=1.f;
    mallet.tensionNewtons=0.f;mallet.substance.value={.74f,.4f,.85f,.60f,.30f,.6f,.2f,.3f,.2f,.2f,0.f,.4f,.12f};
    auto& n=g.nodes[2];n.form.lengthMeters=drum?.28f:.18f;n.form.widthMeters=drum?.28f:.16f;
    n.form.depthMeters=drum?.0008f:.008f;n.form.rounding=1.f;n.form.edges=32;
    n.form.curvature=drum?0.f:.6f;n.form.wallFill=drum?1.f:.4f;
    n.form.wallMm=2.f;n.form.fixing=.7f;n.tensionNewtons=drum?160.f:0.f;
    n.damping=drum?.7f:.25f;
    n.substance.value=drum?std::array<float,MaterialControlCount>{.74f,.35f,.85f,.70f,.24f,.40f,.2f,.35f,.2f,.3f,0.f,.3f,.12f}:
       std::array<float,MaterialControlCount>{.94f,.9f,.99f,.89f,.12f,.08f,0.f,.9f,.2f,.2f,0.f,.15f,.12f};
    g.links[1].motion=.8f;g.links[1].angleDeg=0.f;g.links[1].engagement=1.f;g.links[1].aperture=.9f;
    g.links[1].elasticity=.7f;g.links[1].restitution=.75f;
    g.links[2].position=.32f;g.links[2].aperture=.7f;
    return energyConstruction(g);
}
static FlowGraph flute(){
    auto g=constructionStarterGraph();g.spanMs=6.f;
    auto& e=g.nodes[0];e.sourceAcceleration=80.f;e.sourcePotential=.85f;e.sourceFollowNote=true;
    auto& air=g.nodes[1];air.form.lengthMeters=.04f;air.form.widthMeters=.008f;air.form.depthMeters=.004f;
    air.form.rounding=1.f;air.form.confinement=.2f;air.form.spread=.25f;
    air.substance.value={.0846f,.02f,.99f,0.f,.856f,.2f,.0256f,.05f,0.f,.1f,0.f,0.f,0.f};
    auto& tube=g.nodes[2];tube.form.lengthMeters=.45f;tube.form.widthMeters=.02f;tube.form.depthMeters=.02f;
    tube.form.rounding=1.f;tube.form.confinement=1.f;tube.form.opening=1.f;
    tube.substance=air.substance;tube.damping=.2f;
    g.links[1].motion=1.f;g.links[1].angleDeg=20.f;g.links[1].engagement=1.f;
    g.links[1].aperture=1.f;g.links[1].adhesion=0.f;g.links[1].restitution=.8f;
    g.links[2].position=.35f;g.links[2].aperture=.8f;
    return energyConstruction(g);
}
static FlowGraph violin(){
    auto g=constructionGuitarGraph();g.disconnect(2,3);g.removeNode(3);
    auto& e=g.nodes[0];e.sourceAcceleration=60.f;e.sourcePotential=.75f;e.sourceFollowNote=true;
    auto& bow=g.nodes[1];bow.form.lengthMeters=.28f;bow.form.widthMeters=.009f;bow.form.depthMeters=.001f;
    bow.form.rounding=.7f;bow.substance.value[Roughness]=.9f;bow.substance.value[Hardness]=.45f;
    auto& s=g.nodes[2];s.form.lengthMeters=.33f;s.form.widthMeters=.0005f;s.form.depthMeters=.0005f;
    s.tensionNewtons=50.f;
    auto& c=g.links[1];c.motion=1.f;c.angleDeg=89.f;c.friction=1.f;c.aperture=.8f;c.engagement=1.f;
    c.elasticity=.5f;c.restitution=.3f;c.adhesion=0.f;
    return energyConstruction(g);
}
struct Result {double peak{},early{},post{},late{},last{},maxError{},maxPassiveRise{},source{},control{},loss{},radiation{},elapsed{};};
static Result simulate(const std::string& folder,const std::string& name,FlowGraph graph,float initialM,bool muteGesture,bool sustained,int sr=48000){
    Blueprint b;b.graph=graph;EnergyMechanicalVoice v;v.prepare(float(sr));v.start(b,50,.8f,initialM);
    Result r{};std::vector<float> audio;audio.reserve(4*sr);
    std::ofstream csv(folder+"/"+name+".csv");csv<<"seconds,pressure_Pa,string_or_body_displacement_m,source_work_J,control_work_J,stored_J,material_loss_J,contact_loss_J,radiated_J,balance_error_J\n";
    const auto start=std::chrono::steady_clock::now();
    EnergyLedger previous{};
    for(int i=0;i<4*sr;++i){
        if(muteGesture&&i==sr/4)v.setExpression(.8f,1.f);
        if(i==(sustained?sr:sr/4))v.release();
        const float sample=v.render();assert(std::isfinite(sample));
        const auto l=v.energyLedger();
        assert(std::isfinite(l.stored)&&l.stored>=0.);
        r.maxError=std::max(r.maxError,std::abs(l.balanceError()));
        if(i>0&&std::abs(l.sourceWork-previous.sourceWork)<1.e-20&&std::abs(l.controlWork-previous.controlWork)<1.e-20)
            r.maxPassiveRise=std::max(r.maxPassiveRise,l.stored-previous.stored);
        previous=l;
        const double x=sample;r.peak=std::max(r.peak,std::abs(x));audio.push_back(sample);
        if(i>=sr/20&&i<sr/4)r.early+=x*x/sr;
        if(i>=sr/4&&i<sr/2)r.post+=x*x/sr;
        if(i>=3*sr)r.late+=x*x/sr;
        if(i%(sr/200)==0)csv<<double(i)/sr<<","<<v.pressurePa()<<","<<v.displacement(2)<<","<<l.sourceWork<<","<<l.controlWork<<","<<l.stored<<","<<l.materialLoss<<","<<l.contactLoss<<","<<l.radiated<<","<<l.balanceError()<<"\n";
    }
    r.elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    auto l=v.energyLedger();r.last=l.stored;r.source=l.sourcePositiveWork;r.control=l.controlWork;
    r.loss=l.materialLoss+l.contactLoss;r.radiation=l.radiated;
    if(sr==48000)wav(folder+"/"+name+".wav",audio);
    std::printf("%s sr %d peak %.9g early %.9g post %.9g late %.9g source_J %.9g control_J %.9g stored_J %.9g loss_J %.9g radiation_J %.9g max_balance_J %.9g passive_rise_J %.9g render_s %.4f\n",
        name.c_str(),sr,r.peak,r.early,r.post,r.late,r.source,r.control,r.last,r.loss,r.radiation,r.maxError,r.maxPassiveRise,r.elapsed);
    assert(r.source>0.&&r.peak>1.e-8);
    assert(r.maxError<std::max(1.e-12,r.source*1.e-7));
    assert(r.maxPassiveRise<std::max(1.e-13,r.source*1.e-8));
    if(!sustained)assert(r.source<=22.e-6*.95*.8+1.e-12);
    assert(r.last<=r.source+std::max(0.,r.control)+1.e-12);
    return r;
}
int main(int argc,char**argv){
    const std::string folder=argc>1?argv[1]:".";
    const auto guitar=energyGuitarGraph();
    const auto open=simulate(folder,"guitar_open",guitar,0.f,false,false);
    const auto muted=simulate(folder,"guitar_muted",guitar,1.f,false,false);
    const auto gesture=simulate(folder,"guitar_M_gesture",guitar,0.f,true,false);
    assert(muted.early<open.early&&gesture.post<open.post);
    simulate(folder,"bell_like",struck(false),0.f,false,false);
    simulate(folder,"drum_like",struck(true),0.f,false,false);
    simulate(folder,"flute_pressure",flute(),0.f,false,true);
    simulate(folder,"violin_friction",violin(),0.f,false,true);
    simulate(folder,"guitar_44100",guitar,0.f,false,false,44100);
    simulate(folder,"guitar_96000",guitar,0.f,false,false,96000);
    // Confirm native wrapper routes to SI path, and zero potential is silent.
    Blueprint b;b.graph=guitar;MaterialTimeVoice wrapper;wrapper.prepare(48000);wrapper.start(b,50,.8f);
    assert(wrapper.usesEnergySolver());double out=0.;for(int i=0;i<24000;++i){const float s=wrapper.render();out+=s*s;}assert(out>0.);
    b.graph.nodes[0].sourcePotential=0.f;wrapper.start(b,50,.8f);
    for(int i=0;i<24000;++i)assert(wrapper.render()==0.f);
    // B changes actual physical parameters; changes in energy are recorded
    // as external work rather than mistaken for passive feedback.
    b.graph=guitar;b.pitch.count=1;b.pitch.trackedNode=2;b.pitch.referenceMidi=50;
    b.pitch.assignments[0]={2,PitchProperty::Tension,1.f,400.f,1.f};
    assert(buildPhysicalKeyboard(b.graph,b.pitch));assert(preparePhysicalBendMap(b));
    EnergyMechanicalVoice bend;bend.prepare(48000);bend.start(b,50,.8f);
    for(int i=0;i<10000;++i)bend.render();
    const double old=bend.pitchHz();bend.setPitchBend(2.f);
    assert(bend.pitchHz()>old);
    for(int i=0;i<10000;++i)bend.render();
    const auto ledger=bend.energyLedger();
    assert(std::abs(ledger.balanceError())<std::max(1.e-12,ledger.sourcePositiveWork*1.e-7));
    std::printf("PASS SI energy ledger, five constructions, M, B, zero drive, multi-rate\n");
}
