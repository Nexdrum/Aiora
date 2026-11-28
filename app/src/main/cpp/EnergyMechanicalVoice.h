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
inline constexpr int kSiModes=8; // slot 0 rigid drive; 1..7 elastic modes
inline constexpr double kPressureFullScalePa=2.0;
struct EnergyLedger {
    double sourceWork{},sourcePositiveWork{},controlWork{};
    double materialLoss{},contactLoss{},radiated{},stored{};
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
        bool fluid{},vibrates{};
    };
    using Port=std::array<double,kSiModes>;
    struct Contact {
        int a{},b{};
        bool output{};
        uint64_t arrival{};
        double spring{},extension{},baseStiffness{},gap{},lastX{-1.};
        double radiationArea{},radiationDamping{};
        Port pa{},pb{};
    };
public:
    void prepare(float sr) noexcept {sr_=std::clamp<double>(sr,8000.,192000.);dt_=1./sr_;stop();}
    void start(const Blueprint& b,int midi,float velocity,float mod=0.f) noexcept {
        blueprint_=b;graph_=b.graph;note_=std::clamp(midi,0,127);
        if(pitchMapCurrent(b))applyMappedPitch(b.pitch,graph_,note_);
        v_=unit(velocity);initialV_=v_;m_=smoothM_=unit(mod);bend_=0.;
        tick_=0;released_=false;active_=true;ledger_={};pressure_=0.;quiet_=0;count_=0;
        objects_={};contacts_={};
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
    double displacement(int object,int mode=1) const noexcept {
        return object>=0&&object<graph_.nodeCount&&mode>=0&&mode<kSiModes?objects_[object].modes[mode].q:0.;
    }
    double storedEnergy() const noexcept {
        double e=0.;
        for(int i=0;i<graph_.nodeCount;++i)for(const auto& m:objects_[i].modes)e+=m.energy();
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
            for(auto& m:objects_[i].modes){
                if(m.mass<=0.)continue;
                const double old=m.energy();
                if(m.omega>0.){
                    const double q=m.q*m.cosStep+m.v*m.sinStep/m.omega;
                    const double vel=m.v*m.cosStep-m.q*m.omega*m.sinStep;
                    m.q=q*m.retain;m.v=vel*m.retain;
                }else{m.q+=m.v*dt_;m.v*=m.retain;}
                ledger_.materialLoss+=old*(1.-m.retain*m.retain);
            }
        }
        pressure_=0.;
        for(int i=0;i<count_;++i){
            auto& c=contacts_[i];
            if(tick_<c.arrival)continue;
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
        o.loss=mat.lossFactor;o.softness=mat.contactSoftness;
        o.roughness=p.substance.enabled?p.substance.value[Roughness]:.5;
        o.recovery=p.substance.enabled?p.substance.value[ElasticRecovery]:1.;
        o.viscosity=p.substance.enabled?constructedViscosity(p.substance):0.;
        for(int h=0;h<kSiModes;++h){
            auto& m=o.modes[h];const double momentum=m.mass*m.v;
            if(h==0){
                m.mass=p.role==PhysicalRole::Transfer?o.mass:0.;
                m.omega=0.;m.retain=std::exp(-42.*dt_); // reduced external drive drag
            }else{
                // Effective mass of a unit-peak sinusoidal generalized mode.
                // Orthogonal reduced basis; not a set of independent literal chunks.
                m.mass=.5*o.mass;
                const double f=std::clamp(modeHz(p,h),25.,sr_*.44);
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
        for(int h=1;h<kSiModes;++h){
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
            }else a[h]=std::sin(k*std::clamp(x,.001,.999))*cross*avg;
        }
        return a;
    }
    static double velocity(const Object& o,const Port& a) noexcept {
        double v=0.;for(int h=0;h<kSiModes;++h)if(o.modes[h].mass>0.)v+=a[h]*o.modes[h].v;return v;
    }
    static double invMass(const Object& o,const Port& a) noexcept {
        double z=0.;for(int h=0;h<kSiModes;++h)if(o.modes[h].mass>0.)z+=a[h]*a[h]/o.modes[h].mass;return z;
    }
    static void impulse(Object& o,const Port& a,double j) noexcept {
        for(int h=0;h<kSiModes;++h)if(o.modes[h].mass>0.)o.modes[h].v+=j*a[h]/o.modes[h].mass;
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
        for(int h=1;h<kSiModes;++h){
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
    std::array<int,kMaxFlowLinks> linkIndices_{};
    EnergyLedger ledger_{};
    double sr_{48000.},dt_{1./48000.},v_{.8},initialV_{.8},m_{},smoothM_{},bend_{},pressure_{};
    uint64_t tick_{};int count_{},note_{60},quiet_{};
    bool active_{},released_{};
};
} // namespace aiora::physical
