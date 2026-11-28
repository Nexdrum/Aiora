#pragma once
// v13: reduced SI modal mechanics. No audio signal travels between objects.
// Generalized coordinates q [m], velocity v [m/s], modal mass [kg],
// angular frequency [rad/s], contact stiffness [N/m], impulse [N s].
// A discrete midpoint contact solve applies equal/opposite generalized
// impulses; exact free rotations and passive loss operators are split steps.
// Fixed storage, no allocation in render. NOT FEM, CFD, or a calibrated mic.
#include "PhysicalPitchTuning.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace aiora::physical {
inline constexpr double kSiPi=3.14159265358979323846;
inline constexpr int kSiModes=33; // rigid + up to 32 filament partials; ordinary objects use 7
inline constexpr double kPressureFullScalePa=2.0;
struct EnergyLedger {
    double sourceWork{},sourcePositiveWork{},controlWork{};
    double materialLoss{},contactLoss{},radiated{},stored{};
    uint64_t stickSamples{},slipSamples{},stickSlipTransitions{};
    double frictionLoss{},valveWork{},valveExhaust{},valveSeatLoss{};
    double jetWork{},jetExhaust{}; // subsets of transfer/contact losses, not additional ledger terms
    double balanceError() const noexcept {
        return sourceWork+controlWork-materialLoss-contactLoss-radiated-stored;
    }
};
class EnergyMechanicalVoice {
    struct Mode {
        double mass{},q{},v{},omega{},cosStep{1.},sinStep{},retain{1.};
        double energy() const noexcept {return .5*mass*(v*v+omega*omega*q*q);}
    };
    struct Object {
        std::array<Mode,kSiModes> modes{};
        double mass{},area{},dimension{},frequency{},modulus{},loss{},softness{},roughness{},recovery{},viscosity{};
        int modeCount{8};
        bool fluid{},vibrates{};
    };
    using Port=std::array<double,kSiModes>;
    struct Contact {
        int a{},b{};
        bool output{},jetManaged{};
        uint64_t arrival{};
        double spring{},extension{},baseStiffness{},gap{},lastX{-1.};
        double radiationArea{},radiationDamping{};
        bool sticking{};double frictionState{1.};
        Port pa{},pb{};
    };
    // One driven stream can feed several windows; each keeps its own
    // causal history. The source remains the first transfer object.
    static constexpr int kJetHistory=4096;
    static constexpr int kMaxJets=4;
    struct Valve {
        int edgeContact{},windowContact{},tube{},solid{};uint64_t arrival{};
        double gap{},pressure{},flow{},meanFlow{},speed{},reedHz{};Port port{};
    };
    struct Jet {
        int edgeContact{},windowContact{},tube{};
        uint64_t arrival{};
        Port port{};
        std::array<float,kJetHistory> history{};
        int head{};
        double split{},noise{},lastPressure{},speed{},delaySeconds{},delaySpreadSeconds{};
        uint32_t random{0x2e3459abu};
        bool primed{};
    };
public:
    void prepare(float sr) noexcept {sr_=std::clamp<double>(sr,8000.,192000.);dt_=1./sr_;stop();}
    void start(const Blueprint& b,int midi,float velocity,float mod=0.f) noexcept {
        blueprint_=b;graph_=b.graph;note_=std::clamp(midi,0,127);
        if(pitchMapCurrent(b))applyMappedPitch(b.pitch,graph_,note_);
        v_=unit(velocity);initialV_=v_;m_=smoothM_=unit(mod);bend_=0.;
        tick_=0;released_=false;active_=true;ledger_={};pressure_=0.;quiet_=0;count_=0;
        objects_={};contacts_={};jets_={};valves_={};jetCount_=valveCount_=0;
        for(int i=0;i<graph_.nodeCount;++i)configureObject(i,false);
        for(int i=0;i<graph_.linkCount;++i){
            const auto& l=graph_.links[i];
            if(!graph_.compatible(l.from,l.to)||isUniversalSource(graph_.nodes[l.from]))continue;
            if(!objects_[l.from].vibrates)continue;
            auto& e=contacts_[count_++];
            e.a=l.from;e.b=l.to;e.output=isUniversalOutput(graph_.nodes[l.to]);
            linkIndices_[count_-1]=i;
            e.arrival=static_cast<uint64_t>(std::max(0.,double(graph_.nodes[l.to].y*graph_.spanMs*.001f)*sr_));
            configureContact(e,l,false);
        }
        if(graph_.jetSolver)discoverJets();
        if(graph_.integratedMechanics)discoverValves();
    }
    void stop() noexcept {active_=false;}
    void release() noexcept {released_=true;}
    bool active() const noexcept {return active_;}
    void setExpression(float v,float m) noexcept {v_=unit(v);m_=unit(m);}
    void setPitchBend(float semitones) noexcept {
        if(!active_||!std::isfinite(semitones)||std::abs(semitones-bend_)<.00002)return;
        std::array<float,kPitchSlots> values{};
        if(!physicalPitchValuesAt(blueprint_,note_+semitones,values))return;
        const double before=storedEnergy();
        std::array<bool,kMaxFlowNodes> dirty{};
        for(int i=0;i<blueprint_.pitch.count&&i<kPitchSlots;++i){
            const auto& a=blueprint_.pitch.assignments[i];
            if(!pitchTargetValid(graph_,a)||!pitchWrite(graph_,a,values[i]))continue;
            if(!pitchIsLink(a.property))dirty[a.objectIndex]=true;
        }
        for(int i=0;i<graph_.nodeCount;++i)if(dirty[i])configureObject(i,true);
        for(int i=0;i<count_;++i)configureContact(contacts_[i],graph_.links[linkIndices_[i]],true);
        for(int i=0;i<jetCount_;++i)configureJetPort(jets_[i]);
        for(int i=0;i<valveCount_;++i)configureValve(valves_[i]);
        ledger_.controlWork+=storedEnergy()-before; // external tuning work, not hidden energy
        bend_=semitones;
    }
    float pitchHz() const noexcept {return trackedFundamental(graph_,blueprint_.pitch.trackedNode);}
    float pitchValue(int slot) const noexcept {
        return slot>=0&&slot<blueprint_.pitch.count?pitchRead(graph_,blueprint_.pitch.assignments[slot]):0.f;
    }
    double sourceVelocity() const noexcept {return graph_.nodeCount>1?objects_[1].modes[0].v:0.;}
    double sourceMass() const noexcept {return graph_.nodeCount>1?objects_[1].mass:0.;}
    double pressurePa() const noexcept {return pressure_;}
    int jetCount() const noexcept {return jetCount_;}
    double jetSpeed() const noexcept {return jetCount_?jets_[0].speed:0.;}
    double jetDelay() const noexcept {return jetCount_?jets_[0].delaySeconds:0.;}
    double jetPressure() const noexcept {return jetCount_?jets_[0].lastPressure:0.;}
    double jetDelaySpread() const noexcept {return jetCount_?jets_[0].delaySpreadSeconds:0.;}
    int valveCount() const noexcept {return valveCount_;}
    double valveGap() const noexcept {return valveCount_?valves_[0].gap:0.;}
    double valvePressure() const noexcept {return valveCount_?valves_[0].pressure:0.;}
    double valveFlow() const noexcept {return valveCount_?valves_[0].flow:0.;}
    double valveSpeed() const noexcept {return valveCount_?valves_[0].speed:0.;}
    double valveFrequency() const noexcept {return valveCount_?valves_[0].reedHz:0.;}
    double localVelocity(int node,double x) const noexcept {return node>=0&&node<graph_.nodeCount?velocity(objects_[node],port(node,x,.5,.5,false,0.)):0.;}
    double displacement(int object,int mode=1) const noexcept {
        return object>=0&&object<graph_.nodeCount&&mode>=0&&mode<kSiModes?objects_[object].modes[mode].q:0.;
    }
    double storedEnergy() const noexcept {
        double e=0.;
        for(int i=0;i<graph_.nodeCount;++i)for(int h=0;h<objects_[i].modeCount;++h)e+=objects_[i].modes[h].energy();
        for(int i=0;i<count_;++i)e+=.5*contacts_[i].spring*contacts_[i].extension*contacts_[i].extension;
        return e;
    }
    EnergyLedger energyLedger() const noexcept {auto l=ledger_;l.stored=storedEnergy();return l;}
    float render() noexcept {
        if(!active_)return 0.f;
        smoothM_+=(m_-smoothM_)*std::min(1.,96.*dt_);
        drive();
        // Exact undamped harmonic rotations with uniform passive attenuation.
        for(int i=0;i<graph_.nodeCount;++i)if(objects_[i].vibrates){
            for(int h=0;h<objects_[i].modeCount;++h){
                auto& m=objects_[i].modes[h];if(m.mass<=0.)continue;
                const double old=m.energy();
                if(m.omega>0.){
                    const double q=m.q*m.cosStep+m.v*m.sinStep/m.omega;
                    const double vel=m.v*m.cosStep-m.q*m.omega*m.sinStep;
                    m.q=q*m.retain;m.v=vel*m.retain;
                }else{m.q+=m.v*dt_;m.v*=m.retain;}
                ledger_.materialLoss+=old*(1.-m.retain*m.retain);
            }
        }
        for(int i=0;i<jetCount_;++i)driveJet(jets_[i]);
        for(int i=0;i<valveCount_;++i)driveValve(valves_[i]);
        pressure_=0.;
        for(int i=0;i<count_;++i){
            auto& c=contacts_[i];
            if(tick_<c.arrival||c.jetManaged)continue;
            const auto& l=graph_.links[linkIndices_[i]];
            const double x=l.modEnabled?l.modMin+(l.modMax-l.modMin)*smoothM_:l.position;
            if(std::abs(x-c.lastX)>1.e-7){
                c.pb=port(c.b,x,l.receiverY,l.receiverZ,false,0.);
                if(c.output){c.pa=port(c.a,x,l.receiverY,l.receiverZ,true,l.aperture);if(graph_.acousticSolver)radiationPort(c,l);}
                c.lastX=x;
            }
            if(c.output)radiate(c,l);
            else interact(c,l,x);
        }
        ++tick_;
        // End only after residual mechanical energy has actually decayed.
        if((tick_&255u)==0&&!sourceActive()){
            if(storedEnergy()<1.e-15){if(++quiet_>=16)active_=false;}
            else quiet_=0;
        }
        if(!std::isfinite(pressure_)){active_=false;return 0.f;}
        return static_cast<float>(pressure_/kPressureFullScalePa);
    }
private:
    static double unit(double x) noexcept {return std::isfinite(x)?std::clamp(x,0.,1.):0.;}
    bool sourceActive() const noexcept {
        if(graph_.nodeCount<2)return false;
        const auto& p=graph_.nodes[0];
        return p.sourceFollowNote?!released_:tick_<static_cast<uint64_t>(std::max(1.,double(p.sourceTimeMs)*.001*sr_));
    }
    double modeHz(const FlowNode& p,int h) const noexcept {
        const double f=physicalFundamental(p);
        const auto family=objectContinuum(p);
        if(acousticShell(p)){
            constexpr int ns[]={2,2,3,3,4,4,5},ms[]={0,1,0,1,0,1,0};
            return acousticShellHz(p,ns[h-1],ms[h-1]);
        }
        if(p.siAcoustics&&family==Continuum::Sheet){
            constexpr int xs[]={1,2,1,2,3,1,3},ys[]={1,1,2,2,1,3,2};
            return acousticSheetHz(p,xs[h-1],ys[h-1]);
        }
        if(acousticHelmholtzHz(p)>0.&&h==1)return f;
        if(acousticHelmholtzHz(p)>0.)return std::sqrt(objectMaterial(p).youngModulusPa/std::max(.05f,objectMaterial(p).densityKgM3))*(h-1)/(2.*std::max(.025f,p.form.lengthMeters));
        if(family==Continuum::Filament){
            const double L=std::max(.025,double(p.form.lengthMeters));
            const auto mat=objectMaterial(p);
            const double area=occupiedVolume(p)/L,mu=std::max(1.e-12,area*mat.densityKgM3);
            const double I=area*p.form.depthMeters*p.form.depthMeters/16.;
            const double k=kSiPi*h/L;
            return std::sqrt((std::max(0.f,p.tensionNewtons)*k*k+mat.youngModulusPa*I*k*k*k*k)/mu)/(2.*kSiPi);
        }
        if(objectIsFluid(p))return f*(fluidLeak(p.form)<.5f?2*h-1:h);
        if(h>=6){const double cavity=cavityHz(p.form,h-6);if(cavity>0.)return cavity;}
        constexpr double plate[]={1.,2.31,3.86,5.59,7.76,10.5,13.5};
        constexpr double bar[]={1.,2.756,5.404,8.933,13.34,18.65,24.86};
        const double ratio=family==Continuum::Sheet?plate[h-1]:bar[h-1];
        return f*ratio*(1.+.025*(h-1)*std::abs(p.form.curvature)+.03*(h-1)*std::abs(p.form.taper));
    }
    void configureObject(int i,bool preserve) noexcept {
        auto& o=objects_[i];const auto& p=graph_.nodes[i];
        if(!physicalVibrates(p))return;
        const auto mat=objectMaterial(p);o.vibrates=true;o.fluid=objectIsFluid(p);
        o.mass=std::max(1.e-8,double(occupiedVolume(p))*mat.densityKgM3);
        o.area=std::max(1.e-10,double(p.form.widthMeters)*p.form.depthMeters*sectionFactor(p.form));
        o.dimension=std::max(1.e-5,double(std::min(p.form.widthMeters,p.form.depthMeters)));
        o.frequency=physicalFundamental(p);o.modulus=mat.youngModulusPa;
        o.modeCount=graph_.integratedMechanics&&objectContinuum(p)==Continuum::Filament?33:8;
        o.loss=mat.lossFactor;o.softness=mat.contactSoftness;
        o.roughness=p.substance.enabled?p.substance.value[Roughness]:.5;
        o.recovery=p.substance.enabled?p.substance.value[ElasticRecovery]:1.;
        o.viscosity=p.substance.enabled?constructedViscosity(p.substance):0.;
        for(int h=0;h<kSiModes;++h){
            auto& m=o.modes[h];const double momentum=m.mass*m.v;
            if(h>=o.modeCount){m={};continue;}
            if(h==0){
                m.mass=p.role==PhysicalRole::Transfer?o.mass:0.;
                m.omega=0.;m.retain=std::exp(-42.*dt_); // reduced external drive drag
            }else{
                // Effective mass of a unit-peak sinusoidal generalized mode.
                // Orthogonal reduced basis; not a set of independent literal chunks.
                m.mass=.5*o.mass;
                const double natural=modeHz(p,h);
                if(graph_.integratedMechanics&&h>=8&&natural>=sr_*.44){m={};continue;}
                const double f=std::clamp(natural,25.,sr_*.44);
                m.omega=2.*kSiPi*f;m.cosStep=std::cos(m.omega*dt_);m.sinStep=std::sin(m.omega*dt_);
                double loss=o.loss*std::max(.12,double(p.damping))*(1.+.10*(h-1)*(h-1));
                if(p.substance.enabled){
                    const auto& v=p.substance.value;
                    loss*=1.+.12*(1.-v[StructureOrder])*v[StructureScale]*(h-1)*(h-1)+
                        .25*v[Porosity]*(h-1)+.08*std::log1p(o.viscosity)*h;
                    if(o.fluid)loss*=1.+4.*(1.-p.form.confinement)*(1.+p.form.spread)+2.*fluidLeak(p.form);
                    else loss*=1.+voidFraction(p.form)*.3+fluidLeak(p.form)*voidFraction(p.form)*.25*(h-1);
                    for(const auto& hole:p.form.openings)loss*=1.+.08*hole.size*hole.size*
                        (1.+std::abs(std::sin(kSiPi*h*hole.position+hole.azimuth*2.*kSiPi)));
                }
                const double Q=std::clamp(1./std::max(1.e-8,loss),1.0,2000.);
                m.retain=std::exp(-m.omega*dt_/(2.*Q));
            }
            if(preserve&&m.mass>0.)m.v=momentum/m.mass;
            else if(!preserve){m.q=0.;m.v=0.;}
        }
    }
    Port port(int object,double x,double y,double z,bool pickup,double aperture) const noexcept {
        Port a{};
        if(object<0||object>=graph_.nodeCount||!objects_[object].vibrates)return a;
        a[0]=objects_[object].modes[0].mass>0.?1.:0.;
        const double cross=.5+.5*std::abs(std::cos(kSiPi*(y-.5))*std::cos(kSiPi*(z-.5)));
        for(int h=1;h<objects_[object].modeCount;++h){
            const double k=kSiPi*h;
            const double span=pickup?k*.08*std::sqrt(unit(aperture)):0.;
            const double avg=span>1.e-10?std::sin(span)/span:1.;
            const auto& n=graph_.nodes[object];
            if(acousticShell(n)){
                constexpr int ns[]={2,2,3,3,4,4,5},ms[]={0,1,0,1,0,1,0};
                a[h]=std::cos(2.*kSiPi*ns[h-1]*y)*std::cos(kSiPi*ms[h-1]*x)*avg;
            }else if(n.siAcoustics&&objectContinuum(n)==Continuum::Sheet){
                constexpr int xs[]={1,2,1,2,3,1,3},ys[]={1,1,2,2,1,3,2};
                a[h]=std::sin(kSiPi*xs[h-1]*std::clamp(x,.001,.999))*std::sin(kSiPi*ys[h-1]*std::clamp(y,.001,.999))*avg;
            }else if(graph_.jetSolver&&objects_[object].fluid&&n.form.opening>=.5f){
                // Open pressure boundaries have velocity antinodes at ends.
                a[h]=std::cos(k*std::clamp(x,0.,1.))*cross*avg;
            }else a[h]=std::sin(k*std::clamp(x,.001,.999))*cross*avg;
        }
        return a;
    }
    static double velocity(const Object& o,const Port& a) noexcept {
        double v=0.;for(int h=0;h<o.modeCount;++h)if(o.modes[h].mass>0.)v+=a[h]*o.modes[h].v;return v;
    }
    static double invMass(const Object& o,const Port& a) noexcept {
        double z=0.;for(int h=0;h<o.modeCount;++h)if(o.modes[h].mass>0.)z+=a[h]*a[h]/o.modes[h].mass;return z;
    }
    static void impulse(Object& o,const Port& a,double j) noexcept {
        for(int h=0;h<o.modeCount;++h)if(o.modes[h].mass>0.)o.modes[h].v+=j*a[h]/o.modes[h].mass;
    }
    void configureContact(Contact& c,const FlowLink& l,bool preserve) noexcept {
        auto& a=objects_[c.a];auto& b=objects_[c.b];
        c.pa=port(c.a,l.fromPosition,l.senderY,l.senderZ,false,0.);
        c.pb=port(c.b,l.position,l.receiverY,l.receiverZ,false,0.);
        c.lastX=-1.;
        if(c.output)return;
        // Contact spring K=effective modulus * contact area / deformation length.
        const double E=2.*a.modulus*b.modulus/std::max(1.,a.modulus+b.modulus);
        double A=std::min(a.area,b.area);
        double length=std::max(1.e-5,std::min(a.dimension,b.dimension));
        if(graph_.acousticSolver&&(a.fluid||b.fluid)){
            const auto& solid=a.fluid?graph_.nodes[c.b]:graph_.nodes[c.a];
            const auto& fluid=a.fluid?graph_.nodes[c.a]:graph_.nodes[c.b];
            if(!objectIsFluid(solid)&&objectContinuum(solid)==Continuum::Sheet&&fluid.form.confinement>.7f){
                A=std::min(exposedRadiatingArea(solid),double(fluid.form.lengthMeters)*fluid.form.widthMeters);
                length=std::max(.001,double(fluid.form.depthMeters));
            }
        }
        c.baseStiffness=std::max(1.e-8,E*A/length*(.05+.95*unit(l.elasticity)));
        c.gap=unit(l.clearance)*length;
        if(!preserve){c.extension=-c.gap;c.spring=0.;}
    }
    void configureJetPort(Jet& j) noexcept {
        const auto& l=graph_.links[linkIndices_[j.windowContact]];
        j.port=port(j.tube,l.position,l.receiverY,l.receiverZ,false,0.);
    }
    void discoverJets() noexcept {
        for(int i=0;i<count_&&jetCount_<kMaxJets;++i){
            const auto& l=graph_.links[linkIndices_[i]];
            if(!jetEdgeCandidate(graph_,l)||l.jetDistanceMeters<=0.||l.valveGapMeters>0.)continue;
            for(int k=0;k<count_&&jetCount_<kMaxJets;++k){
                const auto& w=graph_.links[linkIndices_[k]];
                if(w.from!=l.to||!objects_[w.to].fluid||graph_.nodes[w.to].form.confinement<.7f)continue;
                auto& j=jets_[jetCount_++];j.edgeContact=i;j.windowContact=k;j.tube=w.to;
                j.arrival=std::max(contacts_[i].arrival,contacts_[k].arrival);
                j.random+=static_cast<uint32_t>(k*127+i*31);
                configureJetPort(j);
                contacts_[i].jetManaged=true;contacts_[k].jetManaged=true;
            }
        }
    }
    // Jet-drive approximation (Auvray/Ernoult/Fabre/Lagré 2014;
    // equations summarized at euphonics.org/11-8-1-the-jet-drive-model-for-a-recorder-or-flute/).
    // Geometry defines the shear layer delay, split and pressure source.
    // All positive acoustic work is debited from the actual jet kinetic
    // reservoir, and unconverted through-flow energy is dissipated.
    void driveJet(Jet& j) noexcept {
        auto& jet=objects_[1];auto& rigid=jet.modes[0];auto& tube=objects_[j.tube];
        const auto& l=graph_.links[linkIndices_[j.edgeContact]];
        const auto& window=graph_.links[linkIndices_[j.windowContact]];
        const auto& g=graph_.nodes[1].form;const auto& edge=graph_.nodes[l.to].form;
        if(rigid.mass<=0.)return;
        const double U=std::max(0.,rigid.v);j.speed=U;
        const double rho=objectMaterial(graph_.nodes[1]).densityKgM3;
        const double h=std::max(.0001,double(g.depthMeters));
        const double width=std::max(.0001,double(g.widthMeters));
        const double W=std::max(.0005,double(l.jetDistanceMeters));
        const double A=width*h*sectionFactor(g)*unit(g.fillLevel);
        const double mouth=std::max(1.e-9,W*std::min(width,double(edge.widthMeters))*unit(window.aperture));
        const double bore=std::max(1.e-9,tube.area);
        const double inv=invMass(tube,j.port);
        const double acoustic=velocity(tube,j.port);
        const double u=acoustic*bore/mouth;
        j.history[j.head]=static_cast<float>(u);
        const double delay=W/(.4*std::max(.1,U));
        j.delaySeconds=delay;
        auto sampleDelay=[&](double seconds) noexcept {
            const double frames=std::clamp(seconds*sr_,1.,double(kJetHistory-2));
            const int whole=static_cast<int>(frames);
            const double frac=frames-whole;
            return (1.-frac)*j.history[(j.head-whole+kJetHistory)%kJetHistory]+
                frac*j.history[(j.head-whole-1+kJetHistory)%kJetHistory];
        };
        double past=sampleDelay(delay);
        if(graph_.dispersiveJet){
            // Finite-width receptivity approximation: different portions of
            // the sheared stream sample acoustic deflection at different
            // convective times. Viscous diffusion increases effective width.
            // A symmetric binomial quadrature approximates a distributed
            // positive causal transit-time kernel, not an audio EQ or
            // a selected resonant note. Coefficients sum exactly to one.
            const double nu=jet.viscosity/std::max(.05,rho);
            const double diffusion=std::sqrt(std::max(0.,2.*nu*delay));
            const double effectiveH=std::sqrt(h*h+4.*diffusion*diffusion);
            const double relativeSpread=std::clamp(.20+.32*effectiveH/W+.24*unit(g.spread),.20,.36);
            const double step=std::min(.30*delay,relativeSpread*delay/std::sqrt(1.5));
            j.delaySpreadSeconds=std::sqrt(1.5)*step;
            constexpr double weights[]={1.,6.,15.,20.,15.,6.,1.};
            past=0.;
            for(int k=0;k<7;++k)past+=weights[k]*sampleDelay(delay+(k-3)*step)/64.;
        }
        j.head=(j.head+1)%kJetHistory;
        // Kinetic energy flux of the moving stream; capped by reservoir.
        const double available=std::min(.5*rigid.mass*U*U,.5*rho*A*U*U*U*dt_/std::max(1,jetCount_));
        if(tick_<j.arrival||U<.1||inv<=0.||available<=0.){
            j.lastPressure=0.;j.primed=false;return;
        }
        const double pos=l.modEnabled?l.modMin+(l.modMax-l.modMin)*smoothM_:l.position;
        const double angle=(g.flowAngle-l.angleDeg)*kSiPi/180.;
        const double offset=(unit(pos)-.5)*4.*h+W*std::sin(angle)+(unit(l.receiverZ)-.5)*2.*h;
        const double spread=unit(g.spread);
        const double b=.4*h+W*.12*spread; // effective Bickley profile half-width
        const double growth=std::exp(std::min(5.,.3*W/h))/(1.+2.*spread);
        const double bevel=std::clamp(double(l.edgeBevelDeg),5.,90.)*kSiPi/180.;
        const double sharpness=(1.-.9*unit(edge.rounding))*
            (.2+.8*std::abs(std::cos(bevel*.5)))*
            (.35+.65*std::min(1.,W/(2.*h)));
        const double alignment=std::max(0.,std::cos(angle))*std::abs(std::cos(l.elevationDeg*kSiPi/180.));
        const double gate=unit(l.engagement)*unit(l.aperture)*unit(window.engagement)*unit(window.aperture)*
            sharpness*alignment*(1.-.8*unit(l.clearance));
        // Small broadband perturbation seeds the hydrodynamic instability,
        // not a pitch oscillator. Its energy is debited like all jet work.
        j.random^=j.random<<13;j.random^=j.random>>17;j.random^=j.random<<5;
        const double noise=double(j.random)/2147483648.-1.;
        j.noise+=std::min(1.,4000.*dt_)*(noise-j.noise);
        const double eta=h/std::max(.1,U)*growth*past+.0005*h*j.noise;
        const double split=std::tanh((eta-offset)/b);
        const double delta=4./kSiPi*std::sqrt(2.*h*W);
        const double pressure=j.primed?gate*rho*delta*U*b/W*(split-j.split)/dt_:0.;
        j.split=split;j.primed=true;j.lastPressure=pressure;
        double impulseJ=pressure*bore*dt_;
        // Exact acoustic work of generalized impulse: J*u + J²*A/2.
        // Limit positive work to through-flow kinetic energy, without
        // limiting the acoustic displacement or amplitude itself.
        const double proposed=impulseJ*acoustic+.5*inv*impulseJ*impulseJ;
        if(proposed>available){
            const double direction=std::copysign(1.,impulseJ),vel=direction*acoustic;
            const double root=std::sqrt(vel*vel+2.*inv*available);
            const double magnitude=vel>=0.?2.*available/(root+vel):(root-vel)/inv;
            impulseJ=direction*std::min(std::abs(impulseJ),magnitude);
        }
        const double work=impulseJ*acoustic+.5*inv*impulseJ*impulseJ;
        impulse(tube,j.port,impulseJ);
        rigid.v=std::sqrt(std::max(0.,U*U-2.*available/rigid.mass));
        const double exhaust=std::max(0.,available-work);
        ledger_.contactLoss+=exhaust;ledger_.jetExhaust+=exhaust;ledger_.jetWork+=work;
        // Bernoulli mouth loss: signed pressure drop opposes acoustic flow.
        const double current=velocity(tube,j.port);
        const double lossForce=.5*rho*std::pow(current*bore/mouth,2.)/(.6*.6)*bore;
        const double lossJ=std::copysign(std::min(std::abs(current)/inv,lossForce*dt_),current);
        impulse(tube,j.port,-lossJ);
        ledger_.contactLoss+=std::max(0.,lossJ*current-.5*inv*lossJ*lossJ);
    }
    void configureValve(Valve& valve) noexcept {
        auto& o=objects_[valve.solid];const auto& n=graph_.nodes[valve.solid];
        const double L=std::max(.001,double(n.form.lengthMeters)),t=std::max(.00005,double(n.form.depthMeters));
        const double rho=std::max(.05,double(objectMaterial(n).densityKgM3));
        // First cantilever bending mode, effective tip mass. Higher modes
        // of the same flexible object are retained but this port uses mode 1.
        const double omega=1.875104*1.875104/(L*L)*std::sqrt(o.modulus*t*t/(12.*rho));
        auto& m=o.modes[1];const double momentum=m.mass*m.v;
        m.mass=.236*o.mass;m.omega=std::clamp(omega,2.*kSiPi*25.,2.*kSiPi*sr_*.44);
        m.v=momentum/std::max(1.e-12,m.mass);
        m.cosStep=std::cos(m.omega*dt_);m.sinStep=std::sin(m.omega*dt_);
        m.retain=std::exp(-m.omega*dt_*std::clamp(o.loss*double(n.damping),.005,.8)/2.);
        valve.reedHz=m.omega/(2.*kSiPi);
        // Pressure modes at a closed inlet; normalized participation of
        // volume displacement, with acoustic compliance from modal energy.
        valve.port={};
        const auto& tube=objects_[valve.tube];
        const auto& fluid=graph_.nodes[valve.tube];
        const double K=objectMaterial(fluid).youngModulusPa;
        const double complianceStiffness=2.*K/std::max(1.e-12,double(occupiedVolume(fluid)));
        for(int h=1;h<tube.modeCount;++h){const auto& m=tube.modes[h];
            if(m.mass>0.)valve.port[h]=tube.area*std::sqrt(complianceStiffness/(m.mass*m.omega*m.omega));
        }
    }
    void discoverValves() noexcept {
        for(int i=0;i<count_&&valveCount_<kMaxJets;++i){
            const auto& l=graph_.links[linkIndices_[i]];
            if(!jetEdgeCandidate(graph_,l)||l.valveGapMeters<=0.)continue;
            for(int k=0;k<count_&&valveCount_<kMaxJets;++k){
                const auto& w=graph_.links[linkIndices_[k]];
                if(w.from!=l.to||!objects_[w.to].fluid||graph_.nodes[w.to].form.confinement<.7f)continue;
                auto& valve=valves_[valveCount_++];valve.edgeContact=i;valve.windowContact=k;
                valve.tube=w.to;valve.solid=l.to;valve.arrival=std::max(contacts_[i].arrival,contacts_[k].arrival);
                configureValve(valve);contacts_[i].jetManaged=true;contacts_[k].jetManaged=true;
            }
        }
    }
    static double boundedIncrement(double proposed,double linear,double quadratic,double budget) noexcept {
        // Exact energy increment linear*d + quadratic*d²/2.
        if(quadratic<=0.||linear*proposed+.5*quadratic*proposed*proposed<=budget)return proposed;
        const double sign=std::copysign(1.,proposed),v=sign*linear;
        const double root=std::sqrt(v*v+2.*quadratic*std::max(0.,budget));
        const double d=v>=0.?((root+v)>0.?2.*budget/(root+v):0.):(root-v)/quadratic;
        return sign*std::min(std::abs(proposed),d);
    }
    void driveValve(Valve& valve) noexcept {
        auto& stream=objects_[1];auto& rigid=stream.modes[0];auto& reed=objects_[valve.solid].modes[1];
        auto& tube=objects_[valve.tube];
        const auto& l=graph_.links[linkIndices_[valve.edgeContact]];
        const auto& w=graph_.links[linkIndices_[valve.windowContact]];
        const auto& form=graph_.nodes[valve.solid].form;
        const double U=std::max(0.,rigid.v),rho=std::max(.05,double(objectMaterial(graph_.nodes[1]).densityKgM3));
        valve.speed=U;
        const double gate=unit(l.engagement)*unit(l.aperture)*unit(w.engagement)*unit(w.aperture);
        if(tick_<valve.arrival||U<.1||gate<=0.||rigid.mass<=0.||reed.mass<=0.){valve.flow=0.;valve.pressure=0.;return;}
        const double bore=std::max(1.e-9,tube.area),supply=.5*rho*U*U;
        double p=0.,stiffness=0.;
        for(int h=1;h<tube.modeCount;++h){
            const auto& m=tube.modes[h];const double k=m.mass*m.omega*m.omega;
            p+=k*m.q*valve.port[h]/bore;stiffness+=k*valve.port[h]*valve.port[h];
        }
        valve.pressure=p;
        const double rest=l.valveGapMeters*(l.modEnabled?(.25+1.5*unit(l.modMin+(l.modMax-l.modMin)*smoothM_)):1.);
        const double polarity=std::clamp(double(l.valvePolarity),-1.,1.);
        const double diff=supply-p;
        const double flux=.5*rho*stream.area*U*U*U*dt_/std::max(1,jetCount_+valveCount_);
        const double available=std::min(.5*rigid.mass*U*U,flux);
        const double force=polarity*diff*form.lengthMeters*form.widthMeters*.4*gate;
        const double j=boundedIncrement(force*dt_,reed.v,1./reed.mass,available);
        const double reedWork=j*reed.v+.5*j*j/reed.mass;
        reed.v+=j/reed.mass;
        // Unilateral seat contact: reflect only inward velocity, and account
        // for discarded penetration energy as a loss. Never project outward
        // against the spring if the geometric rest gap was moved by M.
        if(reed.q < -rest&&reed.v<0.){
            const double before=reed.energy();
            reed.q=-rest;reed.v=0.;
            ledger_.contactLoss+=std::max(0.,before-reed.energy());
            ledger_.valveSeatLoss+=std::max(0.,before-reed.energy());
        }
        const double gap=std::max(0.,rest+reed.q);valve.gap=gap;
        // Bernoulli orifice with Reynolds-dependent viscous discharge.
        const double width=std::max(.0001,double(form.widthMeters));
        const double flowSpeed=std::sqrt(2.*std::abs(diff)/rho);
        const double Re=rho*flowSpeed*2.*gap/std::max(1.e-8,stream.viscosity);
        const double discharge=.62*std::sqrt(Re/(Re+40.));
        const double coeff=discharge*width*gap*gate*std::sqrt(2./rho);
        auto flowAt=[&](double pressure){const double d=supply-pressure;return std::copysign(coeff*std::sqrt(std::abs(d)),d);};
        // Implicit midpoint pressure makes the nonlinear orifice a monotone
        // scalar solve for each step, rather than a delayed explicit feedback.
        const double q0=flowAt(p);
        double lo=std::min(q0,valve.meanFlow),hi=std::max(q0,valve.meanFlow);
        for(int k=0;k<18;++k){const double Q=.5*(lo+hi);
            const double pm=p+.5*stiffness*(Q-valve.meanFlow)*dt_/(bore*bore);
            if(Q>flowAt(pm))hi=Q;else lo=Q;
        }
        const double Q=.5*(lo+hi);valve.flow=Q;
        const double dq=boundedIncrement((Q-valve.meanFlow)*dt_/bore,p*bore,stiffness,std::max(0.,available-reedWork));
        valve.meanFlow+=std::min(1.,2.*kSiPi*12.*dt_)*(Q-valve.meanFlow);
        const double tubeWork=p*bore*dq+.5*stiffness*dq*dq;
        for(int h=1;h<tube.modeCount;++h)tube.modes[h].q+=dq*valve.port[h];
        rigid.v=std::sqrt(std::max(0.,U*U-2.*available/rigid.mass));
        const double exhaust=std::max(0.,available-reedWork-tubeWork);
        ledger_.contactLoss+=exhaust;ledger_.valveExhaust+=exhaust;ledger_.valveWork+=tubeWork;
    }
    void frictionContact(Contact& c,const FlowLink& l,double scale,double tangent) noexcept {
        auto& a=objects_[c.a];auto& b=objects_[c.b];
        const double inv=invMass(a,c.pa)+invMass(b,c.pb);
        const double rel=velocity(a,c.pa)-velocity(b,c.pb);
        if(inv<=0.||scale<=0.||l.friction<=0.||l.normalLoadN<=0.){c.sticking=false;return;}
        const double normal=std::max(0.,double(l.normalLoadN))*scale;
        const double mu=unit(l.friction)*(.35+.65*(a.roughness+b.roughness)*.5);
        const double staticLimit=normal*mu*(.65+.35*c.frictionState);
        const double desired=rel/inv;
        const bool was=c.sticking;
        const double kinetic=normal*mu*unit(l.kineticRatio);
        const double recovery=std::max(.0001,l.frictionRecoveryMs*.001);
        // Static constraint plus a velocity-weakening sliding branch. The
        // recovery state heals during slow sliding as well as sticking.
        c.sticking=std::abs(desired)<=staticLimit*dt_;
        double j=desired;
        if(c.sticking){
            ++ledger_.stickSamples;
            c.frictionState+=std::min(1.,dt_/recovery)*(1.-c.frictionState);
        }else{
            ++ledger_.slipSamples;
            const double speed=std::abs(rel);
            const double rate=1./recovery+speed/.00008;
            const double equilibrium=1./(recovery*rate);
            c.frictionState=equilibrium+(c.frictionState-equilibrium)*std::exp(-rate*dt_);
            const double characteristic=.10+.10*(a.roughness+b.roughness)*.5;
            const double limit=kinetic+(staticLimit-kinetic)/(1.+std::pow(speed/characteristic,2.));
            j=std::copysign(std::min(std::abs(desired),std::max(0.,limit)*dt_),rel);
        }
        if(was!=c.sticking)++ledger_.stickSlipTransitions;
        impulse(a,c.pa,-j);impulse(b,c.pb,j);
        const double loss=std::max(0.,j*rel-.5*inv*j*j);
        ledger_.contactLoss+=loss;ledger_.frictionLoss+=loss;
    }
    void drive() noexcept {
        if(!sourceActive()||graph_.nodeCount<2)return;
        const auto& p=graph_.nodes[0];auto& m=objects_[1].modes[0];
        if(m.mass<=0.)return;
        const double potential=unit(p.sourcePotential)*v_;
        if(potential<=0.)return;
        const double capacity=graph_.acousticSolver?aptCapacity(p.sourcePotential,p.sourceFollowNote)*v_:
            (p.sourceFollowNote?.006:22.e-6)*potential;
        const double budget=p.sourceFollowNote?capacity*dt_:
            std::max(0.,capacity-ledger_.sourcePositiveWork);
        if(budget<=0.)return;
        const double force=m.mass*std::clamp(double(p.sourceAcceleration),.1,graph_.acousticSolver?5000.:120.)*(.88+.12*initialV_);
        double j=force*dt_;
        // Exact kinetic work of an impulse; solve the quadratic for the cap.
        const double work=j*m.v+j*j/(2.*m.mass);
        if(work>budget){
            const double root=std::sqrt(m.v*m.v+2.*budget/m.mass);
            j=m.v>=0.?2.*budget/(root+m.v):m.mass*(root-m.v);
        }
        const double w=j*m.v+j*j/(2.*m.mass);
        m.v+=j/m.mass;ledger_.sourceWork+=w;ledger_.sourcePositiveWork+=std::max(0.,w);
    }
    void setSpring(Contact& c,double k) noexcept {
        if(c.spring==k)return;
        const double delta=.5*(k-c.spring)*c.extension*c.extension;
        // Opening a contact releases unresolved stored energy as contact loss;
        // increasing stiffness while compressed is explicit external M work.
        if(delta>0.)ledger_.controlWork+=delta;
        else ledger_.contactLoss-=delta;
        c.spring=k;
        if(k==0.&&c.extension>0.)c.extension=0.;
    }
    void interact(Contact& c,const FlowLink& l,double x) noexcept {
        auto& a=objects_[c.a];auto& b=objects_[c.b];
        const double inv=invMass(a,c.pa)+invMass(b,c.pb);
        if(inv<=0.)return;
        const double u=velocity(a,c.pa)-velocity(b,c.pb);
        const double overlap=l.modEnabled?std::pow(std::sin(kSiPi*unit(x)),2.):1.;
        const double area=unit(l.aperture)*unit(l.engagement)*overlap;
        const double motion=unit(l.motion),adhesion=unit(l.adhesion);
        const double angle=l.angleDeg*kSiPi/180.,elevation=l.elevationDeg*kSiPi/180.;
        const double normal=std::abs(std::cos(angle)*std::cos(elevation));
        const double tangent=std::sqrt(std::max(0.,1.-normal*normal));
        const double spatial=.35+.65*unit(1.-.5*(std::abs(l.senderY-l.receiverY)+std::abs(l.senderZ-l.receiverZ)));
        const bool fluid=a.fluid||b.fluid;
        double coherence=1.;
        if(fluid){
            const auto& g=a.fluid?graph_.nodes[c.a].form:graph_.nodes[c.b].form;
            coherence=unit(g.fillLevel)*(.25+.75*unit(g.confinement))*
                (1.-.6*unit(g.spread)*(1.-unit(g.confinement)))*
                (.2+.8*std::abs(std::cos((g.flowAngle-l.angleDeg)*kSiPi/180.)));
        }
        const double scale=area*spatial*coherence;
        if(graph_.integratedMechanics&&!fluid&&l.normalLoadN>0.f&&motion*tangent>.5){
            setSpring(c,0.);frictionContact(c,l,scale,tangent);return;
        }
        const bool stationary=motion<.0001&&adhesion<.0001&&!fluid;
        const bool bilateral=adhesion>0.||fluid;
        // A stationary nonadhesive contact is a dissipative preloaded
        // interface (e.g. a palm); moving compression uses a unilateral spring.
        double k=stationary?0.:c.baseStiffness*scale*(fluid?1.:std::max(adhesion,normal*normal*motion));
        if(scale<=1.e-12){setSpring(c,0.);c.extension=-c.gap;return;}
        if(stationary&&c.gap>0.&&overlap<=unit(l.clearance)){setSpring(c,0.);return;}
        const double restitution=unit(l.restitution)*std::sqrt(unit(a.recovery)*unit(b.recovery));
        const double loss=std::clamp(a.loss+b.loss+.15*(a.softness+b.softness),0.,1.);
        // Effective critical damping in kg/s; loss and restitution set ratio.
        double damping=2.*std::sqrt(std::max(0.,c.baseStiffness*scale)/inv)*
            std::clamp((1.-restitution)*.3+loss,.001,1.5);
        if(fluid)damping+=std::min(1.e3,(a.viscosity+b.viscosity)*std::min(a.area,b.area)/std::min(a.dimension,b.dimension))*scale;
        double h=dt_;
        if(!bilateral&&!stationary){
            // Unilateral gap crossing: contact begins only for remaining time.
            if(c.extension<=0.){
                setSpring(c,0.);
                if(u<=0.||c.extension+u*dt_<=0.){
                    c.extension=std::max(-c.gap-std::min(a.dimension,b.dimension),c.extension+u*dt_);return;
                }
                h=std::clamp(dt_+c.extension/u,0.,dt_);c.extension=0.;
            }
        }else if(stationary)c.extension=0.;
        setSpring(c,k);
        const double s=c.extension;
        const double D=damping+.5*k*h;
        double j=h*(k*s+D*u)/(1.+.5*h*inv*D);
        double mid=u-.5*inv*j;
        double next=s+h*mid;
        double viscous=damping*h*mid*mid;
        if(!bilateral&&!stationary&&(next<0.||j<0.)){
            // Unilateral separation: discard unresolved spring energy,
            // use only a dissipative compression impulse, never pull.
            ledger_.contactLoss+=.5*k*s*s;
            c.spring=0.;k=0.;
            j=u>0.?h*damping*u/(1.+.5*h*inv*damping):0.;
            mid=u-.5*inv*j;next=std::min(0.,h*mid);
            viscous=damping>0.&&j!=0.?j*mid:0.;
        }
        if(stationary){next=0.;}
        impulse(a,c.pa,-j);impulse(b,c.pb,j);
        c.extension=next;
        ledger_.contactLoss+=std::max(0.,viscous);
        // Separate velocity-weakening friction impulse. The moving source
        // supplies work; friction always removes relative kinetic energy.
        if(!fluid&&motion*tangent>1.e-7&&l.friction>0.){
            const double rel=velocity(a,c.pa)-velocity(b,c.pb);
            const double mu=unit(l.friction)*(.35+.65*(a.roughness+b.roughness)*.5);
            const double preload=c.baseStiffness*std::min(a.dimension,b.dimension)*.001*scale;
            const double frictionForce=mu*preload*(.3+.7/(1.+std::abs(rel)/.05))*motion*tangent;
            const double jf=std::copysign(std::min(std::abs(rel)/inv,frictionForce*h),rel);
            impulse(a,c.pa,-jf);impulse(b,c.pb,jf);
            ledger_.contactLoss+=std::max(0.,jf*rel-.5*inv*jf*jf);
        }
    }
    void radiationPort(Contact& c,const FlowLink& l) noexcept {
        const auto& n=graph_.nodes[c.a];const auto& o=objects_[c.a];
        c.radiationArea=exposedRadiatingArea(n);
        const auto family=objectContinuum(n);
        // For a string the acoustic cancellation length is transverse, not
        // its full length. A broad plate radiates through its broad face.
        const double radius=family==Continuum::Filament?
            .5*std::max(.0001,double(n.form.widthMeters+n.form.depthMeters)*.5):
            std::sqrt(c.radiationArea/kSiPi);
        const double Z=kAmbientDensity*kAmbientSoundSpeed;
        c.radiationDamping=Z*c.radiationArea*unit(l.aperture);
        c.pa[0]=0.; // static transport flow is not sound
        for(int h=1;h<o.modeCount;++h){
            const double ka=o.modes[h].omega*radius/kAmbientSoundSpeed;
            // Compact dipole for a slender string; reduced baffled surface
            // radiation elsewhere. Shell cancellation grows with ring order.
            double efficiency=ka/std::sqrt(1.+ka*ka);
            if(family==Continuum::Filament)efficiency*=std::min(1.,ka);
            if(acousticShell(n))efficiency*=std::min(1.,ka/(2.+(h-1)/2));
            c.pa[h]*=efficiency;
        }
    }
    void radiate(Contact& c,const FlowLink& l) noexcept {
        auto& a=objects_[c.a];const auto& n=graph_.nodes[c.a];
        const double inv=invMass(a,c.pa);
        if(inv<=0.||l.aperture<=0.)return;
        if(graph_.acousticSolver){
            const double u=velocity(a,c.pa),b=c.radiationDamping;
            const double j=dt_*b*u/(1.+.5*dt_*inv*b),mid=u-.5*inv*j;
            impulse(a,c.pa,-j);ledger_.radiated+=dt_*b*mid*mid;
            // Far-field equivalent pressure from radiated power. Distance
            // affects reception only, never mechanical radiation damping.
            const double distance=.10+1.90*unit(l.clearance);
            const double orientation=std::abs(std::cos(l.angleDeg*kSiPi/180.));
            pressure_+=std::sqrt(kAmbientDensity*kAmbientSoundSpeed*b/(4.*kSiPi*distance*distance))*mid*orientation;
            return;
        }
        const double u=velocity(a,c.pa);
        const double surface=std::max(a.area,double(n.form.lengthMeters)*std::min(n.form.widthMeters,n.form.depthMeters));
        const double radius=std::sqrt(surface/kSiPi);
        const double ka=2.*kSiPi*a.frequency*radius/kAmbientSoundSpeed;
        const double efficiency=std::min(1.,std::max(.00001,ka));
        const double Z=kAmbientDensity*kAmbientSoundSpeed;
        const double damping=Z*surface*efficiency*efficiency*unit(l.aperture);
        const double j=dt_*damping*u/(1.+.5*dt_*inv*damping);
        const double mid=u-.5*inv*j;
        impulse(a,c.pa,-j);ledger_.radiated+=dt_*damping*mid*mid;
        // Receiver is a calibrated pressure proxy, not an oscillator.
        const double orientation=std::abs(std::cos(l.angleDeg*kSiPi/180.));
        pressure_+=Z*efficiency*mid*unit(l.aperture)*orientation/(1.+4.*unit(l.clearance));
    }
    Blueprint blueprint_{};FlowGraph graph_{};
    std::array<Object,kMaxFlowNodes> objects_{};
    std::array<Contact,kMaxFlowLinks> contacts_{};
    std::array<Jet,kMaxJets> jets_{};
    std::array<Valve,kMaxJets> valves_{};
    int valveCount_{};
    int jetCount_{};
    std::array<int,kMaxFlowLinks> linkIndices_{};
    EnergyLedger ledger_{};
    double sr_{48000.},dt_{1./48000.},v_{.8},initialV_{.8},m_{},smoothM_{},bend_{},pressure_{};
    uint64_t tick_{};int count_{},note_{60},quiet_{};
    bool active_{},released_{};
};
} // namespace aiora::physical
