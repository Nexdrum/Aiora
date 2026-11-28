#define main v13_regression_main
#include "physical_energy_v13_regression.cpp"
#undef main
struct Outcome {double peak{},held{},tail{},local{},hz{},localHz{},periodHz{},periodCorr{},fundRatio{},err{},rise{},work{},radiation{},friction{},valveWork{},gapMin{1.},gapMax{},speed{},secs{};uint64_t sticks{},slips{},transitions{};};
static double dominant(const std::vector<float>& data,int sr){
    int start=sr*3/2,N=4096,dec=2;double best=0.,hz=0.;
    for(int k=1;k<512;++k){double re=0.,im=0.;for(int n=0;n<N;++n){
        const double s=data[start+n*dec]*(.5-.5*std::cos(2.*kSiPi*n/(N-1))),a=2.*kSiPi*k*n/N;
        re+=s*std::cos(a);im+=s*std::sin(a);}
        const double e=re*re+im*im;if(e>best){best=e;hz=double(k)*sr/(N*dec);}
    }return hz;
}
static void period(const std::vector<float>& data,int sr,double expected,Outcome& r){
    if(expected<25.)return;
    const int start=sr*3/2,end=2*sr;double mean=0.;
    for(int i=start;i<end;++i)mean+=data[i];mean/=end-start;
    const int minLag=std::max(1,int(sr/(expected*1.08))),maxLag=int(sr/(expected*.92));
    double best=-1.;int lag=0;
    for(int k=minLag;k<=maxLag;++k){double ab=0.,aa=0.,bb=0.;
        for(int i=start+k;i<end;++i){const double a=data[i]-mean,b=data[i-k]-mean;ab+=a*b;aa+=a*a;bb+=b*b;}
        const double c=ab/std::max(1.e-30,std::sqrt(aa*bb));if(c>best){best=c;lag=k;}}
    r.periodCorr=best;r.periodHz=lag?double(sr)/lag:0.;
    // Fundamental line relative to total AC RMS; excludes a pure higher mode.
    double energy=0.,weight=0.;std::vector<double> windowed;windowed.reserve(end-start);
    for(int i=start;i<end;++i){const double w=.5-.5*std::cos(2.*kSiPi*(i-start)/(end-start-1));
        const double x=data[i]-mean;windowed.push_back(w*x);energy+=w*x*x;weight+=w;}
    // Integer-sample autocorrelation estimates are too coarse for a
    // half-second coherent DFT. Search a narrow physical frequency range.
    double bestLine=0.,bestHz=r.periodHz;
    for(double f=r.periodHz*.985;f<=r.periodHz*1.015;f+=.2){
        double re=0.,im=0.;const double w=2.*kSiPi*f/sr,cs=std::cos(w),sn=std::sin(w);
        double c=1.,s=0.;for(double x:windowed){re+=x*c;im+=x*s;const double nc=c*cs-s*sn;s=s*cs+c*sn;c=nc;}
        const double line=re*re+im*im;if(line>bestLine){bestLine=line;bestHz=f;}
    }
    r.fundRatio=2.*bestLine/std::max(1.e-30,weight*energy);
    if(r.fundRatio>.001)r.periodHz=bestHz;
}
static Outcome check(const std::string& folder,const std::string& name,FlowGraph g,int sr=48000,bool sustained=true){
    Blueprint b;b.graph=g;EnergyMechanicalVoice v;v.prepare(sr);v.start(b,62,.8f,.5f);
    Outcome r{};std::vector<float> audio,local;audio.reserve(3*sr);local.reserve(3*sr);
    std::ofstream csv(folder+"/"+name+".csv");
    csv<<"seconds,pressure_Pa,local_velocity_mps,source_J,stored_J,material_loss_J,contact_loss_J,radiation_J,control_J,balance_J,stick_samples,slip_samples,transitions,friction_loss_J,valve_work_J,gap_m,valve_pressure_Pa,valve_flow_m3s,source_velocity_mps\n";
    const auto start=std::chrono::steady_clock::now();EnergyLedger prev{};
    for(int i=0;i<3*sr;++i){
        if(i==(sustained?2*sr:sr/4))v.release();
        const float x=v.render(),u=v.localVelocity(2,.12);const auto e=v.energyLedger();
        assert(std::isfinite(x)&&std::isfinite(u)&&std::isfinite(e.stored));
        r.err=std::max(r.err,std::abs(e.balanceError()));
        if(i&&std::abs(e.sourceWork-prev.sourceWork)<1.e-20&&std::abs(e.controlWork-prev.controlWork)<1.e-20)r.rise=std::max(r.rise,e.stored-prev.stored);
        prev=e;audio.push_back(x);local.push_back(u);r.peak=std::max(r.peak,double(std::abs(x)));
        if(i>=sr*3/2&&i<2*sr){r.held+=double(x)*x/sr;r.local+=double(u)*u/sr;r.speed+=v.sourceVelocity()/(sr*.5);
            if(v.valveCount()){r.gapMin=std::min(r.gapMin,v.valveGap());r.gapMax=std::max(r.gapMax,v.valveGap());}}
        if(i>=sr*5/2)r.tail+=double(x)*x/sr;
        if(i%(sr/200)==0)csv<<double(i)/sr<<","<<v.pressurePa()<<","<<u<<","<<e.sourceWork<<","<<e.stored<<","<<e.materialLoss<<","<<e.contactLoss<<","<<e.radiated<<","<<e.controlWork<<","<<e.balanceError()<<","<<e.stickSamples<<","<<e.slipSamples<<","<<e.stickSlipTransitions<<","<<e.frictionLoss<<","<<e.valveWork<<","<<v.valveGap()<<","<<v.valvePressure()<<","<<v.valveFlow()<<","<<v.sourceVelocity()<<"\n";
    }
    r.secs=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    const auto e=v.energyLedger();r.work=e.sourcePositiveWork;r.radiation=e.radiated;r.friction=e.frictionLoss;r.valveWork=e.valveWork;
    r.sticks=e.stickSamples;r.slips=e.slipSamples;r.transitions=e.stickSlipTransitions;
    r.hz=dominant(audio,sr);r.localHz=dominant(local,sr);
    const bool isBow=g.links[1].normalLoadN>0.f;
    period(isBow?local:audio,sr,physicalFundamental(g.nodes[isBow?2:3]),r);
    std::printf("PERIOD %s sr %d Hz %.8g corr %.8g fundamental_fraction %.8g\n",name.c_str(),sr,r.periodHz,r.periodCorr,r.fundRatio);
    if(sr==48000)wav(folder+"/"+name+".wav",audio);
    std::printf("V17 %s sr %d peak %.9g held %.9g tail %.9g local %.9g Hz %.6g localHz %.6g source %.9g radiation %.9g balance %.9g rise %.9g stick %llu slip %llu transitions %llu friction %.9g valve_work %.9g gap %.9g %.9g speed %.6g render %.5g\n",name.c_str(),sr,r.peak,r.held,r.tail,r.local,r.hz,r.localHz,r.work,r.radiation,r.err,r.rise,(unsigned long long)r.sticks,(unsigned long long)r.slips,(unsigned long long)r.transitions,r.friction,r.valveWork,r.gapMin,r.gapMax,r.speed,r.secs);
    assert(r.err<std::max(1.e-12,r.work*1.e-7));assert(r.rise<std::max(1.e-13,r.work*1.e-8));
    return r;
}
static void stressAndExpression(){
    // Eight simultaneous voice-private graphs with independent APT, M and release.
    std::vector<EnergyMechanicalVoice> voices(8);
    const std::array<FlowGraph,8> graphs={integratedBowedGraph(),integratedBowedGraph(false),integratedValveGraph(),
        integratedConstruction(jetFluteGraph()),integratedConstruction(acousticStringBodyGraph()),
        integratedPercussionGraph(),integratedPercussionGraph(true),integratedValveGraph(1.f)};
    for(int k=0;k<8;++k){Blueprint b;b.graph=graphs[k];voices[k].prepare(48000);voices[k].start(b,50,.8f,0.f);}
    const auto started=std::chrono::steady_clock::now();double peak=0.;
    for(int i=0;i<48000;++i){double mix=0.;for(int k=0;k<8;++k){
        if(i==12000)voices[k].setExpression(.45f,.7f);
        if(i==24000)voices[k].release();
        mix+=voices[k].render();}
        assert(std::isfinite(mix));peak=std::max(peak,std::abs(mix));
    }
    for(auto& v:voices){const auto l=v.energyLedger();assert(std::abs(l.balanceError())<std::max(1.e-12,l.sourcePositiveWork*1.e-7));}
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
    std::printf("STRESS 8 voices 1s render %.6f s peak %.8g\n",seconds,peak);
    for(auto g:graphs){
        g.nodes[0].sourceAcceleration=5000.f;g.nodes[0].sourcePotential=1.f;
        g.nodes[0].sourceTimeMs=50.f;
        EnergyMechanicalVoice v;Blueprint b;b.graph=g;v.prepare(48000);v.start(b,62,1.f,.5f);
        double maxError=0.,maxRise=0.;EnergyLedger previous{};
        for(int i=0;i<48000;++i){if(i==12000)v.release();const float x=v.render();const auto l=v.energyLedger();
            assert(std::isfinite(x)&&std::isfinite(l.stored));maxError=std::max(maxError,std::abs(l.balanceError()));
            if(i&&std::abs(l.sourceWork-previous.sourceWork)<1.e-20&&std::abs(l.controlWork-previous.controlWork)<1.e-20)maxRise=std::max(maxRise,l.stored-previous.stored);previous=l;
        }
        const auto l=v.energyLedger();assert(maxError<std::max(1.e-12,l.sourcePositiveWork*1.e-7));assert(maxRise<std::max(1.e-13,l.sourcePositiveWork*1.e-8));
    }
    // Physical B mapping remains available in the new native wrapper.
    Blueprint b;b.graph=integratedConstruction(acousticBareGuitarGraph());b.pitch.count=1;b.pitch.trackedNode=2;b.pitch.referenceMidi=50;
    b.pitch.assignments[0]={2,PitchProperty::Tension,1.f,400.f,1.f};
    assert(buildPhysicalKeyboard(b.graph,b.pitch));assert(preparePhysicalBendMap(b));
    MaterialTimeVoice v;v.prepare(48000);v.start(b,50,.8f,0.f);assert(v.usesEnergySolver());
    for(int i=0;i<5000;++i)v.render();const double old=v.currentPhysicalPitchHz();v.setPitchBend(2.f);
    assert(v.currentPhysicalPitchHz()>old);v.setExpression(.8f,1.f);
    for(int i=0;i<10000;++i)assert(std::isfinite(v.render()));
    const auto l=v.energyLedger();assert(std::abs(l.balanceError())<std::max(1.e-12,l.sourcePositiveWork*1.e-7));
    // Compare M damping on the same bare string, using joules rather than gain.
    b.pitch={};EnergyMechanicalVoice open,muted;open.prepare(48000);muted.prepare(48000);open.start(b,50,.8f,0.f);muted.start(b,50,.8f,1.f);
    for(int i=0;i<12000;++i){open.render();muted.render();}
    std::printf("M stored open %.9g muted %.9g\n",open.storedEnergy(),muted.storedEnergy());
    assert(muted.storedEnergy()<open.storedEnergy());
    std::printf("PASS 8-voice independence, extreme APT, B mapping and M damping\n");
}
int main(int argc,char**argv){
    std::setvbuf(stdout,nullptr,_IONBF,0);
    const std::string folder=argc>1?argv[1]:".";
    const auto bow=integratedBowedGraph();const auto bare=integratedBowedGraph(false);
    const auto bowed=check(folder,"bow_body",bow);
    const auto bowedBare=check(folder,"bow_bare",bare);
    auto low=bow;low.links[1].normalLoadN=.12f;check(folder,"bow_light",low);
    auto high=bow;high.links[1].normalLoadN=1.2f;check(folder,"bow_heavy",high);
    auto zero=bow;zero.links[1].friction=0.f;const auto noFriction=check(folder,"bow_zero_friction",zero);assert(noFriction.peak==0.);
    const auto valve=integratedValveGraph();
    const auto closed=check(folder,"valve_closing",valve);
    auto wide=valve;wide.links[1].valveGapMeters=.0012f;check(folder,"valve_wide",wide);
    auto open=integratedValveGraph(1.f);check(folder,"valve_opening",open);
    auto stiff=valve;stiff.nodes[2].form.depthMeters=.0008f;check(folder,"valve_stiff",stiff);
    auto no=valve;no.nodes[0].sourcePotential=0.f;const auto silent=check(folder,"valve_zero",no);assert(silent.peak==0.);
    auto block=valve;block.links[1].engagement=0.f;const auto blocked=check(folder,"valve_blocked",block);assert(blocked.peak==0.);
    const auto valve44=check(folder,"valve_44100",valve,44100);const auto valve96=check(folder,"valve_96000",valve,96000);
    const auto bow44=check(folder,"bow_44100",bare,44100);const auto bow96=check(folder,"bow_96000",bare,96000);
    const auto body44=check(folder,"bow_body_44100",bow,44100);const auto body96=check(folder,"bow_body_96000",bow,96000);
    const auto jet=check(folder,"jet_integrated",integratedConstruction(jetFluteGraph()));
    check(folder,"guitar_integrated",integratedConstruction(acousticStringBodyGraph()),48000,false);
    const auto bell=check(folder,"bell_integrated",integratedPercussionGraph(),48000,false);
    const auto drum=check(folder,"drum_integrated",integratedPercussionGraph(true),48000,false);
    assert(bell.peak<1.&&drum.peak<1.&&bell.peak>1.e-5&&drum.peak>1.e-5);
    std::printf("FUNCTION bow expected %.6g actual %.6g bare %.6g valve expected %.6g actual %.6g jet %.6g\n",physicalFundamental(bow.nodes[2]),bowed.localHz,bowedBare.localHz,physicalFundamental(valve.nodes[3]),closed.hz,jet.hz);
    assert(bowed.sticks>100&&bowed.slips>100&&bowed.transitions>100&&bowed.held>1.e-12&&bowed.periodCorr>.85&&bowed.fundRatio>.015);
    assert(bowedBare.sticks>100&&bowedBare.slips>100&&bowedBare.transitions>100);
    for(auto r:{bowedBare,bow44,bow96,body44,body96})assert(r.transitions>100&&r.periodCorr>.85&&r.fundRatio>.015);
    assert(closed.held>1.e-12&&closed.gapMax-closed.gapMin>1.e-7&&closed.valveWork>0.);
    assert(closed.tail<closed.held*.3+1.e-12);
    for(auto r:{closed,valve44,valve96})assert(r.periodCorr>.85&&r.fundRatio>.005);
    assert(jet.held>1.e-12&&jet.tail<jet.held*.3+1.e-12);
    stressAndExpression();
    std::printf("PASS integrated balance, stick/slip events, valve modulation, silence, release and acoustic regression\n");
}
