#define main v13_regression_main
#include "physical_energy_v13_regression.cpp"
#undef main
#include <complex>
struct JetResult {double early{},held{},tail{},peak{},hz{},tonality{},work{},jetWork{},radiated{},err{},rise{},speed{},delay{};};
static JetResult testJet(const std::string& folder,const std::string& name,FlowGraph graph,float M=.5f,int sr=48000,bool change=false){
    Blueprint b;b.graph=graph;EnergyMechanicalVoice v;v.prepare(sr);v.start(b,62,.8f,M);
    JetResult r{};std::vector<float> audio;audio.reserve(3*sr);
    std::ofstream csv(folder+"/"+name+".csv");csv<<"seconds,pressure_Pa,jet_pressure_Pa,jet_speed_mps,delay_s,source_work_J,jet_work_J,jet_exhaust_J,stored_J,material_loss_J,contact_loss_J,radiated_J,balance_error_J\n";
    EnergyLedger prev{};int active=0;
    for(int i=0;i<3*sr;++i){
        if(change&&i==sr)v.setExpression(.8f,1.f);
        if(i==2*sr)v.release();
        const float x=v.render();assert(std::isfinite(x));
        const auto e=v.energyLedger();assert(std::isfinite(e.stored)&&e.stored>=0.);
        r.err=std::max(r.err,std::abs(e.balanceError()));
        if(i&&std::abs(e.sourceWork-prev.sourceWork)<1.e-20&&std::abs(e.controlWork-prev.controlWork)<1.e-20)
            r.rise=std::max(r.rise,e.stored-prev.stored);
        prev=e;audio.push_back(x);r.peak=std::max(r.peak,double(std::abs(x)));
        if(i>=sr/2&&i<sr)r.early+=double(x)*x/sr;
        if(i>=3*sr/2&&i<2*sr){r.held+=double(x)*x/sr;r.speed+=v.jetSpeed();r.delay+=v.jetDelay();++active;}
        if(i>=5*sr/2)r.tail+=double(x)*x/sr;
        if(i%(sr/200)==0)csv<<double(i)/sr<<","<<v.pressurePa()<<","<<v.jetPressure()<<","<<v.jetSpeed()<<","<<v.jetDelay()<<","<<e.sourceWork<<","<<e.jetWork<<","<<e.jetExhaust<<","<<e.stored<<","<<e.materialLoss<<","<<e.contactLoss<<","<<e.radiated<<","<<e.balanceError()<<"\n";
    }
    auto e=v.energyLedger();r.work=e.sourcePositiveWork;r.jetWork=e.jetWork;r.radiated=e.radiated;
    r.speed/=std::max(1,active);r.delay/=std::max(1,active);
    // Spectral analysis of held window: coarse DFT, no oscillator in solver.
    const int N=4096,start=sr*3/2,dec=2;
    double best=0.,sum=0.;
    for(int k=1;k<512;++k){
        double re=0.,im=0.;for(int n=0;n<N;++n){
            const double window=.5-.5*std::cos(2.*kSiPi*n/(N-1));
            const double a=2.*kSiPi*k*n/N;
            const double s=audio[start+n*dec]*window;
            re+=s*std::cos(a);im-=s*std::sin(a);
        }
        const double power=re*re+im*im;sum+=power;
        if(power>best){best=power;r.hz=double(k)*sr/(N*dec);}
    }
    r.tonality=best/std::max(1.e-30,sum);
    if(sr==48000)wav(folder+"/"+name+".wav",audio);
    std::printf("JET %s sr %d early %.9g held %.9g tail %.9g peak %.9g Hz %.6g tonality %.6g speed %.6g delay %.9g source %.9g jet_work %.9g radiated %.9g error %.9g passive_rise %.9g\n",name.c_str(),sr,r.early,r.held,r.tail,r.peak,r.hz,r.tonality,r.speed,r.delay,r.work,r.jetWork,r.radiated,r.err,r.rise);
    assert(r.err<std::max(1.e-12,r.work*1.e-7));
    assert(r.rise<std::max(1.e-13,r.work*1.e-8));
    assert(r.work<=2.*aptCapacity(graph.nodes[0].sourcePotential,true)*.8+1.e-8);
    return r;
}
int main(int argc,char**argv){
    const std::string folder=argc>1?argv[1]:".";
    const auto graph=jetFluteGraph();
    assert(graph.nodeCount==5&&graph.linkCount==4&&jetEdgeCandidate(graph,graph.links[1]));
    const auto normal=testJet(folder,"jet_centered",graph);
    auto angled=graph;angled.links[1].angleDeg=35.f;
    const auto angle=testJet(folder,"jet_angle_35",angled);
    auto rounded=graph;rounded.nodes[2].form.rounding=.95f;
    testJet(folder,"jet_rounded_edge",rounded);
    auto offset=graph;offset.links[1].modEnabled=false;offset.links[1].position=.99f;
    const auto off=testJet(folder,"jet_offset",offset);
    auto longJet=graph;longJet.links[1].jetDistanceMeters=.009f;
    testJet(folder,"jet_long",longJet);
    auto thick=graph;thick.nodes[1].form.depthMeters=.0024f;
    testJet(folder,"jet_thick",thick);
    auto noPower=graph;noPower.nodes[0].sourcePotential=0.f;
    const auto silent=testJet(folder,"jet_zero_power",noPower);assert(silent.peak==0.&&silent.work==0.);
    auto blocked=graph;blocked.links[1].engagement=0.f;
    const auto block=testJet(folder,"jet_blocked",blocked);assert(block.peak==0.);
    testJet(folder,"jet_M_motion",graph,.5f,48000,true);
    testJet(folder,"jet_44100",graph,.5f,44100);
    testJet(folder,"jet_96000",graph,.5f,96000);
    // Separate a numerical stability result from acoustic usefulness.
    assert(normal.work>0.&&normal.jetWork>0.&&normal.held>1.e-12);
    assert(normal.tail<normal.held*.25+1.e-12);
    std::printf("center/angle held ratio %.9g center/offset held ratio %.9g modeled fundamental %.8g Hz\n",normal.held/std::max(1.e-30,angle.held),normal.held/std::max(1.e-30,off.held),physicalFundamental(graph.nodes[3]));
    std::printf("PASS jet work, zero source, zero engagement, release and multi-rate energy balance; assess pitch and spectral quality from measured results\n");
}
