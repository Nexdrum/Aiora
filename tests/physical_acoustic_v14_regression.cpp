// Reuse the exact v13 simulation harness, fixed pressure-to-PCM mapping.
#define main v13_regression_main
#define assert_unused_marker 1
#include "physical_energy_v13_regression.cpp"
#undef main
static Result v14run(const std::string& folder,const std::string& name,FlowGraph graph,float M=0.f,bool gesture=false,bool sustained=false){
    Blueprint b;b.graph=graph;EnergyMechanicalVoice v;v.prepare(48000);v.start(b,50,.8f,M);
    Result r{};std::vector<float> audio;audio.reserve(192000);
    std::ofstream csv(folder+"/"+name+".csv");
    csv<<"seconds,pressure_Pa,string_or_body_displacement_m,source_work_J,control_work_J,stored_J,material_loss_J,contact_loss_J,radiated_J,balance_error_J\n";
    EnergyLedger previous{};const auto start=std::chrono::steady_clock::now();
    for(int i=0;i<192000;++i){
        if(gesture&&i==12000)v.setExpression(.8f,1.f);
        if(i==(sustained?48000:12000))v.release();
        const float sample=v.render();assert(std::isfinite(sample));const auto l=v.energyLedger();
        r.maxError=std::max(r.maxError,std::abs(l.balanceError()));
        if(i>0&&std::abs(l.sourceWork-previous.sourceWork)<1.e-20&&std::abs(l.controlWork-previous.controlWork)<1.e-20)
            r.maxPassiveRise=std::max(r.maxPassiveRise,l.stored-previous.stored);
        previous=l;r.peak=std::max(r.peak,double(std::abs(sample)));audio.push_back(sample);
        const double sq=sample*double(sample)/48000.;
        if(i>=2400&&i<12000)r.early+=sq;if(i>=12000&&i<24000)r.post+=sq;if(i>=144000)r.late+=sq;
        if(i%240==0)csv<<double(i)/48000<<","<<v.pressurePa()<<","<<v.displacement(2)<<","<<l.sourceWork<<","<<l.controlWork<<","<<l.stored<<","<<l.materialLoss<<","<<l.contactLoss<<","<<l.radiated<<","<<l.balanceError()<<"\n";
    }
    const auto l=v.energyLedger();r.source=l.sourcePositiveWork;r.control=l.controlWork;r.last=l.stored;r.radiation=l.radiated;r.loss=l.contactLoss+l.materialLoss;
    r.elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    wav(folder+"/"+name+".wav",audio);
    std::printf("v14 %s peak %.9g early %.9g post %.9g late %.9g source_J %.9g control_J %.9g stored_J %.9g loss_J %.9g radiation_J %.9g max_balance_J %.9g passive_rise_J %.9g render_s %.4f\n",name.c_str(),r.peak,r.early,r.post,r.late,r.source,r.control,r.last,r.loss,r.radiation,r.maxError,r.maxPassiveRise,r.elapsed);
    assert(r.source>0.&&r.peak>1.e-12);
    assert(r.maxError<std::max(1.e-12,r.source*1.e-7));
    assert(r.maxPassiveRise<std::max(1.e-13,r.source*1.e-8));
    const auto& source=graph.nodes[0];
    const double cap=aptCapacity(source.sourcePotential,sustained)*.8;
    if(graph.acousticSolver)assert(r.source<=cap+1.e-9);
    return r;
}
int main(int argc,char**argv){
    const std::string folder=argc>1?argv[1]:".";
    const auto bare=acousticBareGuitarGraph();
    auto body=acousticStringBodyGraph();
    auto matched=body;matched.nodes[0]=bare.nodes[0];matched.nodes[1]=bare.nodes[1];matched.nodes[1].y=.20f;
    // Identical source and string, changed radiating construction.
    const auto bareR=v14run(folder,"guitar_bare_matched",bare);
    const auto bodyR=v14run(folder,"guitar_body_matched",matched);
    const auto full=v14run(folder,"guitar_body",body);
    const auto muted=v14run(folder,"guitar_body_muted",body,1.f);
    v14run(folder,"guitar_body_M",body,0.f,true);
    auto bell=acousticBellDrumGraph(),drum=acousticBellDrumGraph(true);
    assert(acousticShell(bell.nodes[2])&&!acousticShell(drum.nodes[2]));
    assert(exposedRadiatingArea(bell.nodes[2])>.04&&exposedRadiatingArea(drum.nodes[2])>.07);
    std::printf("geometry bell mass %.8g kg area %.8g m2 f %.8g Hz; drum area %.8g m2 f %.8g Hz\n",
      occupiedVolume(bell.nodes[2])*objectMaterial(bell.nodes[2]).densityKgM3,exposedRadiatingArea(bell.nodes[2]),physicalFundamental(bell.nodes[2]),exposedRadiatingArea(drum.nodes[2]),physicalFundamental(drum.nodes[2]));
    v14run(folder,"bell_shell",bell);
    auto soft=bell;soft.nodes[0].sourcePotential=static_cast<float>(aptCapacitySlider(22.e-6*.95,false));
    v14run(folder,"bell_shell_low_work",soft);
    v14run(folder,"drum_membrane",drum);
    v14run(folder,"violin_body",acousticStringBodyGraph(true),0.f,false,true);
    auto air=acousticConstruction(flute());air.nodes[0].sourcePotential=static_cast<float>(aptCapacitySlider(.02,true));
    v14run(folder,"flute_pressure",air,0.f,false,true);
    // Distance changes only pressure, not stored energy/radiation.
    auto far=bare;far.links[far.linkCount-1].clearance=1.f;
    const auto farR=v14run(folder,"guitar_bare_far",far);
    assert(std::abs(farR.radiation-bareR.radiation)<1.e-13&&std::abs(farR.last-bareR.last)<1.e-13);
    assert(farR.peak<bareR.peak);
    assert(aptCapacity(0.,false)==0.&&aptCapacity(0.,true)==0.);
    std::printf("matched radiated ratio %.9g; muted early ratio %.9g\n",bodyR.radiation/std::max(1.e-30,bareR.radiation),muted.early/std::max(1.e-30,full.early));
    std::printf("PASS v14 SI conservation, geometry and distance invariance; acoustic quality metrics reported separately\n");
}
