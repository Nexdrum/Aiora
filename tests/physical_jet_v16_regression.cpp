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
    std::setvbuf(stdout,nullptr,_IONBF,0);
    const std::string folder=argc>1?argv[1]:".";
    const auto old=jetFluteGraph();
    const auto graph=dispersiveJetFluteGraph();
    const auto baseline=testJet(folder,"v15_matched",old);
    const auto normal=testJet(folder,"v16_centered",graph);
    const double f=physicalFundamental(graph.nodes[3]);
    auto slow=graph;slow.nodes[0].sourceAcceleration=2250.f;
    testJet(folder,"v16_slow",slow);
    auto fast=graph;fast.nodes[1].form.lengthMeters=.18f;fast.nodes[0].sourceAcceleration=5000.f;
    testJet(folder,"v16_fast",fast);
    auto shortJet=graph;shortJet.links[1].jetDistanceMeters=.0028f;
    testJet(folder,"v16_short",shortJet);
    auto longTube=graph;longTube.nodes[3].form.lengthMeters=.45f;
    testJet(folder,"v16_long_tube",longTube);
    auto angled=graph;angled.links[1].angleDeg=15.f;
    testJet(folder,"v16_angle_15",angled);
    auto round=graph;round.nodes[2].form.rounding=.95f;
    testJet(folder,"v16_rounded",round);
    testJet(folder,"v16_M_motion",graph,.5f,48000,true);
    auto zero=graph;zero.nodes[0].sourcePotential=0.f;
    const auto silent=testJet(folder,"v16_zero",zero);assert(silent.peak==0.&&silent.work==0.);
    auto blocked=graph;blocked.links[1].engagement=0.f;
    const auto block=testJet(folder,"v16_blocked",blocked);assert(block.peak==0.);
    const auto r44=testJet(folder,"v16_44100",graph,.5f,44100);
    const auto r96=testJet(folder,"v16_96000",graph,.5f,96000);
    assert(normal.work>0.&&normal.jetWork>0.&&normal.held>1.e-12);
    assert(normal.tail<normal.held*.25+1.e-12);
    std::printf("REGISTER expected %.8g v15 %.8g v16 %.8g rate44 %.8g rate96 %.8g ratios %.8g %.8g %.8g\n",
      f,baseline.hz,normal.hz,r44.hz,r96.hz,normal.hz/f,r44.hz/f,r96.hz/f);
    // Do not equate numerical stability with acoustic accuracy; report the
    // register criterion separately, then fail if this specific example
    // still does not choose its lowest mode across sample rates.
    assert(std::abs(normal.hz/f-1.)<.08);
    assert(std::abs(r44.hz/f-1.)<.08&&std::abs(r96.hz/f-1.)<.08);
    std::printf("PASS v16 matched-source fundamental, multi-rate register, release, zero power and energy balance\n");
}
