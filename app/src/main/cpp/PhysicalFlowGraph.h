#pragma once
// Aiora universal physical roles + parametrically constructed geometry v5.
// Human source -> free repeatable physical roles -> output. Physical material,
// geometry, spatial interaction, and arrival time define behavior, not presets.
// All graph edits happen outside the real-time audio thread.

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>

namespace aiora::physical {

enum class Material : uint8_t {
    Steel, Nylon, Bronze, Wood, Glass, Ceramic, Rubber, Felt,
    Aluminum, Copper, Brass, Iron, Titanium, Silver, Gold, CarbonFiber,
    Bamboo, Hardwood, Plywood, Cork, Acrylic, ABS, Polycarbonate,
    Silicone, Leather, Wool, Stone, Concrete, Porcelain, Paper, Air, Water, Oil, SoftTissue, HardTissue,
    Count
};
inline constexpr int kMaterialCount=static_cast<int>(Material::Count);
inline constexpr std::array<const char*,kMaterialCount> kMaterialNames{
    "Steel","Nylon","Bronze","Wood","Glass","Ceramic","Rubber","Felt",
    "Aluminum","Copper","Brass","Iron","Titanium","Silver","Gold",
    "Carbon fiber","Bamboo","Hardwood","Plywood","Cork","Acrylic",
    "ABS plastic","Polycarbonate","Silicone","Leather","Wool",
    "Stone","Concrete","Porcelain","Paper","Air",
    "Water","Oil","Human soft tissue","Human hard tissue"
};
inline constexpr const char* materialName(Material m) noexcept {
    const int i=static_cast<int>(m);
    return i>=0&&i<kMaterialCount?kMaterialNames[static_cast<size_t>(i)]:"Unknown";
}
struct MaterialProperties {
    float densityKgM3;
    float youngModulusPa;
    float lossFactor;       // loss-factor approximation, not measured Q
    float contactSoftness;  // 0 = rigid, 1 = compressible
};
inline constexpr MaterialProperties materialProperties(Material material) noexcept {
    switch(material){
    case Material::Steel:return {7850.f,200.e9f,.0010f,.05f};
    case Material::Nylon:return {1140.f,2.5e9f,.008f,.30f};
    case Material::Bronze:return {8800.f,105.e9f,.002f,.08f};
    case Material::Wood:return {650.f,11.e9f,.025f,.28f};
    case Material::Glass:return {2500.f,70.e9f,.0025f,.02f};
    case Material::Ceramic:return {3000.f,100.e9f,.002f,.02f};
    case Material::Rubber:return {1100.f,0.01e9f,.18f,.85f};
    case Material::Felt:return {350.f,.003e9f,.30f,.95f};
    case Material::Aluminum:return {2700.f,69.e9f,.003f,.06f};
    case Material::Copper:return {8960.f,110.e9f,.003f,.07f};
    case Material::Brass:return {8500.f,100.e9f,.003f,.08f};
    case Material::Iron:return {7870.f,200.e9f,.004f,.06f};
    case Material::Titanium:return {4500.f,116.e9f,.002f,.05f};
    case Material::Silver:return {10490.f,83.e9f,.003f,.08f};
    case Material::Gold:return {19300.f,78.e9f,.005f,.09f};
    case Material::CarbonFiber:return {1600.f,90.e9f,.006f,.14f};
    case Material::Bamboo:return {700.f,16.e9f,.018f,.24f};
    case Material::Hardwood:return {850.f,14.e9f,.014f,.23f};
    case Material::Plywood:return {640.f,8.e9f,.035f,.30f};
    case Material::Cork:return {240.f,.025e9f,.15f,.82f};
    case Material::Acrylic:return {1180.f,3.2e9f,.022f,.26f};
    case Material::ABS:return {1050.f,2.2e9f,.050f,.35f};
    case Material::Polycarbonate:return {1200.f,2.4e9f,.030f,.32f};
    case Material::Silicone:return {1120.f,.005e9f,.22f,.94f};
    case Material::Leather:return {860.f,.25e9f,.10f,.68f};
    case Material::Wool:return {280.f,.002e9f,.31f,.94f};
    case Material::Stone:return {2700.f,60.e9f,.005f,.025f};
    case Material::Concrete:return {2400.f,30.e9f,.023f,.06f};
    case Material::Porcelain:return {2400.f,67.e9f,.003f,.02f};
    case Material::Paper:return {800.f,2.0e9f,.080f,.55f};
    // For a gas, the elastic parameter denotes bulk modulus, not Young's E.
    case Material::Air:return {1.225f,1.42e5f,.010f,.92f};
    case Material::Water:return {998.f,2.2e9f,.015f,.92f};
    case Material::Oil:return {870.f,1.5e9f,.08f,.96f};
    // Representative reduced-model tissue values, not anatomical simulations.
    case Material::SoftTissue:return {1050.f,0.15e6f,.25f,.94f};
    case Material::HardTissue:return {1850.f,15.e9f,.03f,.20f};
    case Material::Count:break;
    }
    return {7850.f,200.e9f,.001f,.05f};
}
enum class Geometry : uint8_t { HollowBox, HollowCylinder, Plate, Bell, Bowl, Sphere, Tube, Count };
inline constexpr std::array<const char*,static_cast<int>(Geometry::Count)> kGeometryNames{
    "Hollow box","Cylinder","Plate","Bell","Bowl","Sphere","Tube"
};
inline constexpr const char* geometryName(Geometry g) noexcept {
    const int i=static_cast<int>(g);
    return i>=0&&i<static_cast<int>(Geometry::Count)?kGeometryNames[static_cast<size_t>(i)]:"Body";
}

// Y defines FIRST energy arrival / delay only; a node continues vibrating
// independently after its first arrival and after the source has stopped.
enum class FlowKind : uint8_t {
    Exciter, String, Membrane, Plate, Bar, AirColumn, Body, Bridge,
    Damper, Output, Router, Reed, Edge, Universal
};
// Roles describe where a constructed physical object participates in energy
// transfer; they do not select an instrument, waveform or resonator algorithm.
enum class PhysicalRole : uint8_t { Source, Transfer, Interaction, Resonator, Output };
inline constexpr const char* roleName(PhysicalRole r) noexcept {
    switch(r){
    case PhysicalRole::Source:return "ENERGY";
    case PhysicalRole::Transfer:return "TRANSFER OBJECT";
    case PhysicalRole::Interaction:return "INTERACTION";
    case PhysicalRole::Resonator:return "OBJECT";
    case PhysicalRole::Output:return "OUTPUT";
    }
    return "ROLE";
}
enum class Continuum : uint8_t { Filament, Sheet, Solid, Gas, Liquid };
inline constexpr bool fluidContinuum(Continuum c) noexcept {
    return c==Continuum::Gas||c==Continuum::Liquid;
}
inline constexpr bool liquidMaterial(Material m) noexcept {
    return m==Material::Water||m==Material::Oil;
}
inline constexpr const char* continuumName(Continuum c) noexcept {
    switch(c){
    case Continuum::Filament:return "Filament";
    case Continuum::Sheet:return "Surface";
    case Continuum::Solid:return "Solid";
    case Continuum::Gas:return "Gas cavity";
    case Continuum::Liquid:return "Liquid volume";
    }
    return "Solid";
}
enum class Driver : uint8_t { Hand, Fingers, Breath, Foot };
inline constexpr const char* driverName(Driver d) noexcept {
    switch(d){
    case Driver::Hand:return "Hand / arm";
    case Driver::Fingers:return "Fingers";
    case Driver::Breath:return "Breath";
    case Driver::Foot:return "Foot / leg";
    }
    return "Hand";
}
enum class Gesture : uint8_t { Impulse, DeflectRelease, Continuous };
inline constexpr const char* gestureName(Gesture g) noexcept {
    switch(g){
    case Gesture::Impulse:return "Impact motion";
    case Gesture::DeflectRelease:return "Deflect / release";
    case Gesture::Continuous:return "Sustained motion";
    }
    return "Impulse";
}
enum class ContactLaw : uint8_t { Coupled, Friction, Contact, Flow };
inline constexpr const char* contactLawName(ContactLaw x) noexcept {
    switch(x){
    case ContactLaw::Coupled:return "Coupled";
    case ContactLaw::Friction:return "Friction";
    case ContactLaw::Contact:return "Contact";
    case ContactLaw::Flow:return "Airflow";
    }
    return "Coupled";
}
// Geometry is explicit, including local cross-section/width control points.
// Stations are at x=0,1/3,2/3,1. The rendering solver reduces the profile
// to a fixed set of modal coefficients; it is NOT a general FEM mesh.
// Bounded parametric features; zero size disables a feature. Coordinates are
// normalized to the object's envelope. No allocation in the audio thread.
struct CavityFeature {
    float length{},width{.6f},depth{.6f},position{.5f},offset{.5f};
};
struct OpeningFeature {
    float size{},position{.5f},azimuth{.25f},aspect{.5f},neck{.1f};
};
struct ParametricGeometry {
    Continuum continuum{Continuum::Solid};
    float lengthMeters{.65f};
    float widthMeters{.085f};
    float depthMeters{.016f};
    float wallMm{2.f};
    float curvature{0.f};        // normalized axial curvature
    float taper{0.f};            // signed axial taper
    float opening{.5f};          // open boundary fraction
    float fixing{.35f};          // mechanical end restraint
    std::array<float,4> stations{1.f,1.f,1.f,1.f};
    float wallFill{1.f},rounding{0.f};
    int edges{4}; // 32 approximates a circle
    std::array<CavityFeature,2> cavities{};
    std::array<OpeningFeature,3> openings{};
    float confinement{.8f},spread{.2f},flowAngle{0.f},fillLevel{1.f};
};
inline constexpr float normalizedAverageProfile(
    const ParametricGeometry& p) noexcept {
    return (p.stations[0]+p.stations[1]+p.stations[2]+p.stations[3])*.25f;
}
inline constexpr bool isUniversalSource(const struct FlowNode& n) noexcept;
inline constexpr const char* flowKindName(FlowKind k) noexcept {
    switch(k) {
    case FlowKind::Exciter:return "EXCITER";
    case FlowKind::String:return "STRING";
    case FlowKind::Membrane:return "MEMBRANE";
    case FlowKind::Plate:return "PLATE";
    case FlowKind::Bar:return "BAR";
    case FlowKind::AirColumn:return "AIR COLUMN";
    case FlowKind::Body:return "BODY";
    case FlowKind::Bridge:return "COUPLER";
    case FlowKind::Damper:return "CONTACT";
    case FlowKind::Output:return "OUTPUT";
    case FlowKind::Router:return "FLOW ROUTER";
    case FlowKind::Reed:return "REED VALVE";
    case FlowKind::Edge:return "JET EDGE";
    case FlowKind::Universal:return "PHYSICAL ROLE";
    }
    return "OBJECT";
}
inline constexpr bool vibrates(FlowKind kind) noexcept {
    return kind==FlowKind::String||kind==FlowKind::Membrane||
        kind==FlowKind::Plate||kind==FlowKind::Bar||
        kind==FlowKind::AirColumn||kind==FlowKind::Body;
}
enum class Excitation : uint8_t { Pluck, Hammer, Blow, Scrape };
enum class FlowTemplate : uint8_t { String, Drum, Plate, Bar, Air, Bell, Blank, HybridWind };
inline constexpr int kMaxFlowNodes=14;
inline constexpr int kMaxFlowLinks=22;
inline constexpr int kMaxFlowBodies=4; // compatibility with v2 offline voice
inline constexpr int kMaxFlowDampers=3;
// Fixed reference environment for universal construction.
inline constexpr float kAmbientPressurePa=101325.f;
inline constexpr float kAmbientTemperatureC=20.f;
inline constexpr float kGravityMps2=9.80665f;
inline constexpr float kAmbientDensity=1.204f;
inline constexpr float kAmbientSoundSpeed=343.f;
enum MaterialControl : int { Density, Hardness, ElasticRecovery, ShearStiffness,
    Compressibility, InternalLoss, Viscosity, StructureOrder, StructureAlignment,
    StructureScale, Porosity, Roughness, SurfaceTension, MaterialControlCount };
inline constexpr const char* kMaterialControlLabels[MaterialControlCount]={
    "DENSITY (kg/m3)","HARDNESS","ELASTIC RECOVERY","SHEAR STIFFNESS (Pa)",
    "COMPRESSIBILITY / BULK MODULUS","INTERNAL DAMPING","VISCOSITY (Pa s)",
    "STRUCTURE ORDER","STRUCTURE ALIGNMENT","STRUCTURE SCALE","POROSITY","ROUGHNESS","SURFACE TENSION (N/m)"};
struct ConstructedMaterial {
    bool enabled{false};
    std::array<float,MaterialControlCount> value{
        .70f,.72f,.88f,.78f,.23f,.18f,.18f,.65f,.30f,.30f,.05f,.25f,.12f};
};
inline float materialUnit(float x) noexcept {
    return std::isfinite(x)?std::clamp(x,0.f,1.f):0.f;
}
inline float constructedDensity(const ConstructedMaterial& m) noexcept {
    return std::pow(10.f,4.5f*materialUnit(m.value[Density])-.3f);
}
inline float constructedShear(const ConstructedMaterial& m) noexcept {
    const float x=materialUnit(m.value[ShearStiffness]);
    return x<=0.f?0.f:std::pow(10.f,12.f*x)-1.f;
}
inline float constructedBulk(const ConstructedMaterial& m) noexcept {
    return std::pow(10.f,12.f-8.f*materialUnit(m.value[Compressibility]));
}
inline float constructedViscosity(const ConstructedMaterial& m) noexcept {
    return std::pow(10.f,10.f*materialUnit(m.value[Viscosity])-5.f);
}
inline float constructedSurfaceTension(const ConstructedMaterial& m) noexcept {
    return .6f*materialUnit(m.value[SurfaceTension]);
}
inline float constructedLoss(const ConstructedMaterial& m) noexcept {
    return std::pow(10.f,-4.f+3.5f*materialUnit(m.value[InternalLoss]));
}
inline float fluidWeight(const ConstructedMaterial& m) noexcept {
    // A zero static shear modulus defines the fluid limit. A low G/K
    // ratio by itself does not make a nearly incompressible rubber a liquid.
    // Separate shear/bulk modes for viscoelastic transitions are future work.
    return constructedShear(m)<=1.f?1.f:0.f;
}
inline MaterialProperties constructedProperties(const ConstructedMaterial& m) noexcept {
    const float pores=materialUnit(m.value[Porosity])*.9f;
    const float density=constructedDensity(m)*(1.f-pores);
    const float G=constructedShear(m);
    const float K=constructedBulk(m);
    const float E=9.f*K*G/std::max(1.f,3.f*K+G);
    const float fluid=fluidWeight(m);
    const float alignment=materialUnit(m.value[StructureAlignment]);
    // Reduced axial anisotropy and void-volume weakening.
    const float stiffness=((1.f-fluid)*E*(1.f+.35f*alignment)+fluid*K)*
        (1.f-pores)*(1.f-pores);
    const float recover=materialUnit(m.value[ElasticRecovery]);
    const float loss=constructedLoss(m)+.03f*(1.f-recover)+.04f*pores;
    return {std::max(.05f,density),std::max(1.f,stiffness),
        std::clamp(loss,.0001f,.8f),1.f-materialUnit(m.value[Hardness])};
}
struct FlowNode {
    FlowKind kind{FlowKind::Plate};
    Material material{Material::Steel}; // legacy identity, ignored by constructed objects
    ConstructedMaterial substance{};
    bool siMechanics{false}; // v13 frequency model; not a user sound preset
    bool siAcoustics{false}; // v14 geometry and dimensional APT
    Geometry geometry{Geometry::Plate};
    Excitation excitation{Excitation::Hammer};
    float x{.5f},y{.5f}; // X = interaction location; Y = first energy arrival
    float size{1.0f};
    float thicknessMm{3.0f};
    float damping{1.0f};
    float coupling{.60f};
    float position{.22f}; // attack/contact location on object
    float tensionNewtons{80.f};
    float diameterMm{.50f};
    float hardness{.70f};
    float restPressure{0.f}; // fixed preload; M causes linked displacement
    float motion{0.f}; // legacy router outlet travel
    PhysicalRole role{PhysicalRole::Resonator};
    ParametricGeometry form{};
    Driver driver{Driver::Hand};
    Gesture gesture{Gesture::Impulse};
    float sourceDurationMs{4.f}; // mechanical motion duration, not sound envelope
    float sourceRiseMs{2.f};
    float sourceDirection{0.f};  // legacy direction, retained for saved graphs
    // Universal APT source: acceleration acts on connected transfer mass.
    // Finite input limits total work; note-locked input limits power over time.
    float sourceAcceleration{32.f}; // nominal free acceleration, m/s^2
    float sourcePotential{.65f};    // normalized energy / sustained power capacity
    float sourceTimeMs{4.f};
    bool sourceFollowNote{false};   // stop driving on note-off, no fixed deadline
    // Only INTERACTION nodes interpret these two settings. 0 degrees is
    // normal impact; +/-90 degrees is opposite tangential sliding directions.
    float interactionAngleDeg{65.f};
    float interactionEngagement{.55f};
    // Normalized experimental contact coefficients; .5 preserves the v9 response.
    float interactionFriction{.5f},interactionElasticity{.5f};
    float interactionRestitution{.5f},interactionArea{.5f};
    float interactionClearance{0.f},interactionAdhesion{0.f};
    // Blend from the original law to a shared normal/tangential contact model.
    float interactionBlend{0.f};
    float movingMin{.5f},movingMax{.5f};
    bool movingEnabled{false};   // M may reposition ANY physical object
    ContactLaw law{ContactLaw::Coupled}; // interaction with connected media (normalized position)
};
inline MaterialProperties objectMaterial(const FlowNode& n) noexcept {
    return n.substance.enabled?constructedProperties(n.substance):materialProperties(n.material);
}
inline Continuum objectContinuum(const FlowNode& n) noexcept {
    if(!n.substance.enabled)return n.form.continuum;
    // Geometry selects reduced modal approximation; material identity never does.
    if(fluidWeight(n.substance)>.5f)return Continuum::Liquid;
    const float l=n.form.lengthMeters,w=n.form.widthMeters,d=n.form.depthMeters;
    if(std::max(w,d)<.035f*l)return Continuum::Filament;
    if(d<.16f*std::min(l,w))return Continuum::Sheet;
    return Continuum::Solid;
}
inline bool objectIsFluid(const FlowNode& n) noexcept {
    return fluidContinuum(objectContinuum(n));
}
inline const char* objectMaterialLabel(const FlowNode& n) noexcept {
    return n.substance.enabled?"Editable material":materialName(n.material);
}
// Starting values are explicit editable slider positions, not a solver preset.
inline void initializeConstruction(FlowNode& n,bool transfer=false) noexcept {
    n.substance.enabled=true;
    if(transfer)n.substance.value={.75f,.65f,.90f,.75f,.30f,.25f,.18f,.65f,.25f,.25f,.02f,.20f,.12f};
    else n.substance.value={.70f,.72f,.88f,.78f,.23f,.32f,.18f,.65f,.30f,.30f,.05f,.25f,.12f};
}
// Geometry reduction shared by the editor, pitch analysis and voice.
inline float sectionFactor(const ParametricGeometry& g) noexcept {
    const float n=static_cast<float>(std::clamp(g.edges,3,32));
    // Area of a regular polygon inscribed in an ellipse, relative to its
    // bounding rectangle. Rounding interpolates toward the ellipse.
    const float polygon=n*std::sin(6.28318530718f/n)/8.f;
    return polygon+(.78539816f-polygon)*materialUnit(g.rounding);
}
inline float envelopeVolume(const ParametricGeometry& g) noexcept {
    const float taper=std::clamp(g.taper,-1.f,1.f);
    return std::max(1.e-12f,g.lengthMeters*g.widthMeters*g.depthMeters*
        std::clamp(normalizedAverageProfile(g),.1f,3.f)*sectionFactor(g)*(1.f+taper*taper/12.f));
}
inline float shellVoid(const ParametricGeometry& g) noexcept {
    const float t=std::max(0.f,g.wallMm*.001f);
    return (1.f-materialUnit(g.wallFill))*
        std::max(0.f,1.f-2.f*t/std::max(.00001f,g.lengthMeters))*
        std::max(0.f,1.f-2.f*t/std::max(.00001f,g.widthMeters))*
        std::max(0.f,1.f-2.f*t/std::max(.00001f,g.depthMeters));
}
inline float pocketFraction(const CavityFeature& c) noexcept {
    // Ellipsoidal pocket, clipped at the axial and transverse envelope.
    const float l=std::min(materialUnit(c.length),2.f*std::min(materialUnit(c.position),1.f-materialUnit(c.position)));
    const float w=std::min(materialUnit(c.width),2.f*std::min(materialUnit(c.offset),1.f-materialUnit(c.offset)));
    return .5235988f*l*w*materialUnit(c.depth);
}
inline float voidFraction(const ParametricGeometry& g) noexcept {
    float remain=1.f-shellVoid(g);
    for(const auto& c:g.cavities)remain*=1.f-pocketFraction(c);
    return std::clamp(1.f-remain,0.f,.98f);
}
inline float openingArea(const ParametricGeometry& g,const OpeningFeature& o) noexcept {
    const float side=std::max(.00001f,std::min(g.widthMeters,g.depthMeters));
    const float r=.5f*side*materialUnit(o.size);
    return 3.14159265f*r*r*(.15f+.85f*materialUnit(o.aspect));
}
// Geometry-derived shell, not a bell/wood/instrument identifier. A thin
// hollow envelope with comparable transverse dimensions admits ring modes.
inline bool acousticShell(const FlowNode& n) noexcept {
    const auto& g=n.form;
    return n.siAcoustics&&!objectIsFluid(n)&&g.wallFill<.5f&&
        g.wallMm*.001f<.18f*std::min(g.widthMeters,g.depthMeters)&&
        std::min(g.widthMeters,g.depthMeters)>.22f*std::max(g.widthMeters,g.depthMeters);
}
inline double shellPerimeter(const ParametricGeometry& g) noexcept {
    const double a=.5*g.widthMeters,b=.5*g.depthMeters;
    return 3.141592653589793*(3.*(a+b)-std::sqrt(std::max(0.,(3.*a+b)*(a+3.*b))));
}
inline double shellLateralArea(const ParametricGeometry& g) noexcept {
    const double r=.25*(g.widthMeters+g.depthMeters);
    const double slope=2.*r*g.taper/std::max(.00001,double(g.lengthMeters));
    return shellPerimeter(g)*g.lengthMeters*std::max(.1f,normalizedAverageProfile(g))*
        std::sqrt(1.+slope*slope)*(1.+.15*std::abs(g.curvature));
}
inline double exposedRadiatingArea(const FlowNode& n) noexcept {
    const auto& g=n.form;
    const auto family=objectContinuum(n);
    if(acousticShell(n))return shellLateralArea(g);
    if(family==Continuum::Sheet)return std::max(1.e-10,double(g.lengthMeters)*g.widthMeters*normalizedAverageProfile(g));
    if(family==Continuum::Filament)return std::max(1.e-10,shellPerimeter(g)*g.lengthMeters);
    if(objectIsFluid(n)){
        double area=double(g.widthMeters)*g.depthMeters*sectionFactor(g)*fluidContinuum(family);
        double holes=0.;for(const auto& h:g.openings)holes+=openingArea(g,h);
        return std::max(1.e-10,area*materialUnit(g.opening)+holes);
    }
    return std::max(1.e-10,double(g.lengthMeters)*std::max(g.widthMeters,g.depthMeters)*normalizedAverageProfile(g));
}
inline float occupiedVolume(const FlowNode& n) noexcept {
    const auto& g=n.form;
    if(acousticShell(n)){
        const double cap=double(g.widthMeters)*g.depthMeters*sectionFactor(g);
        const double t=g.wallMm*.001;
        double shell=(shellLateralArea(g)+cap*(2.-materialUnit(g.opening)))*t;
        for(const auto& o:g.openings)shell-=openingArea(g,o)*t;
        const double envelope=envelopeVolume(g);
        return static_cast<float>(std::clamp(shell+(envelope-shell)*materialUnit(g.wallFill),envelope*.005,envelope));
    }
    float fraction=1.f-voidFraction(g);
    for(const auto& o:g.openings){
        const float hole=openingArea(g,o)*std::min(g.depthMeters,
            std::max(.00001f,g.wallMm*.001f)+materialUnit(o.neck)*g.depthMeters);
        fraction-=hole/envelopeVolume(g);
    }
    if(objectIsFluid(n))fraction*=materialUnit(g.fillLevel);
    return envelopeVolume(g)*std::clamp(fraction,.005f,1.f);
}
inline float cavityVolume(const ParametricGeometry& g,int index) noexcept {
    return envelopeVolume(g)*(index==0?std::min(.98f,shellVoid(g)+pocketFraction(g.cavities[0])*(1.f-shellVoid(g))):
        pocketFraction(g.cavities[1])*(1.f-shellVoid(g)));
}
inline float cavityHz(const ParametricGeometry& g,int index) noexcept {
    const float volume=cavityVolume(g,index);
    if(volume<1.e-12f)return 0.f;
    float neckConductance=0.f;
    const float center=g.cavities[index].position;
    for(const auto& o:g.openings){
        const float area=openingArea(g,o);
        const float neck=std::max(.0001f,g.wallMm*.001f+o.neck*g.depthMeters+
            .85f*std::sqrt(area/3.14159265f));
        // Openings preferentially couple to the nearest pocket.
        const float locality=std::max(0.f,1.f-2.f*std::fabs(o.position-center));
        neckConductance+=area/neck*locality;
    }
    if(neckConductance<=1.e-12f){
        // Closed cavity's first longitudinal pressure mode.
        return kAmbientSoundSpeed/(2.f*std::max(.001f,g.lengthMeters*
            std::max(g.cavities[index].length,index==0?std::sqrt(shellVoid(g)):0.f)));
    }
    return kAmbientSoundSpeed/6.28318530718f*std::sqrt(neckConductance/volume);
}
inline float fluidLeak(const ParametricGeometry& g) noexcept {
    float open=materialUnit(g.opening);
    for(const auto& o:g.openings)open+=materialUnit(o.size)*materialUnit(o.size)*.22f;
    return std::clamp(open,0.f,1.f);
}
struct FlowLink {
    uint8_t from{0},to{0};
    float position{.5f}; // position ON target (0..1)
    float modTravel{0.f}; // -1..1, movement of interaction under M
    float aperture{.22f}; // size of interaction area / flow inlet
    float modMin{.5f},modMax{.5f};
    bool modEnabled{false}; // independent M assignment; min>max reverses direction
    ContactLaw law{ContactLaw::Coupled}; // legacy only
    // Object-flow interaction: no material, geometry or resonator of its own.
    float fromPosition{.5f};
    float motion{0.f},angleDeg{0.f},engagement{.8f};
    float friction{.5f},elasticity{.5f},restitution{.5f};
    float clearance{0.f},adhesion{1.f};
    float senderY{.5f},senderZ{.5f},receiverY{.5f},receiverZ{.5f},elevationDeg{0.f};
    float jetDistanceMeters{0.f},edgeBevelDeg{30.f}; // zero distance: ordinary contact
    float normalLoadN{0.f},kineticRatio{.30f},frictionRecoveryMs{2.f};
    float valveGapMeters{0.f},valvePolarity{-1.f}; // negative: pressure closes opening
};
inline constexpr bool isUniversalSource(const FlowNode& n) noexcept {
    return n.kind==FlowKind::Universal&&n.role==PhysicalRole::Source;
}
inline constexpr bool isUniversalOutput(const FlowNode& n) noexcept {
    return n.kind==FlowKind::Universal&&n.role==PhysicalRole::Output;
}
inline constexpr bool physicalVibrates(const FlowNode& n) noexcept {
    return n.kind==FlowKind::Universal?
        n.role!=PhysicalRole::Source&&n.role!=PhysicalRole::Output:
        vibrates(n.kind);
}
struct FlowGraph {
    std::array<FlowNode,kMaxFlowNodes> nodes{};
    std::array<FlowLink,kMaxFlowLinks> links{};
    uint8_t nodeCount{0},linkCount{0};
    float spanMs{48.f}; // full top-bottom travel time, not literal distance
    bool objectFlow{false}; // legacy graphs retain their earlier solver
    bool energySolver{false}; // versioned integrator; legacy tests retain their path
    bool acousticSolver{false};
    bool jetSolver{false};
    bool dispersiveJet{false}; // versioned finite-width jet receptivity
    bool integratedMechanics{false};

    int first(FlowKind kind) const noexcept {
        for(int i=0;i<nodeCount;++i)
            if(nodes[static_cast<size_t>(i)].kind==kind)return i;
        return -1;
    }
    int count(FlowKind kind) const noexcept {
        int n=0;
        for(int i=0;i<nodeCount;++i)if(nodes[i].kind==kind)++n;
        return n;
    }
    bool linked(int from,int to) const noexcept {
        for(int i=0;i<linkCount;++i)
            if(links[i].from==from&&links[i].to==to)return true;
        return false;
    }
    bool compatible(int from,int to) const noexcept {
        if(from<0||to<0||from>=nodeCount||to>=nodeCount||from==to)return false;
        const auto& source=nodes[static_cast<size_t>(from)];
        const auto& target=nodes[static_cast<size_t>(to)];
        if(source.y+.035f>=target.y)return false;
        if(source.kind==FlowKind::Output||target.kind==FlowKind::Exciter||
           isUniversalOutput(source)||isUniversalSource(target))return false;
        if(objectFlow){
            if(source.kind!=FlowKind::Universal||target.kind!=FlowKind::Universal||
               source.role==PhysicalRole::Interaction||target.role==PhysicalRole::Interaction)return false;
            if(isUniversalSource(source))return from==0&&to==1&&target.role==PhysicalRole::Transfer;
            if(target.role==PhysicalRole::Transfer)return false;
            return physicalVibrates(source)&&(physicalVibrates(target)||isUniversalOutput(target));
        }
        // Universal roles may be repeated freely, branching into many
        // physical media. Only chronological arrival order is constrained.
        if(source.kind==FlowKind::Universal||target.kind==FlowKind::Universal){
            if(source.kind!=FlowKind::Universal&&
               source.kind!=FlowKind::Exciter&&
               !vibrates(source.kind)&&source.kind!=FlowKind::Router)return false;
            return true;
        }
        if(source.kind==FlowKind::Exciter)
            return vibrates(target.kind)||target.kind==FlowKind::Router||
                target.kind==FlowKind::Reed||target.kind==FlowKind::Edge;
        if(target.kind==FlowKind::Output)
            return source.kind!=FlowKind::Exciter&&source.kind!=FlowKind::Router;
        if(source.kind==FlowKind::Router)
            return vibrates(target.kind)||target.kind==FlowKind::Reed||
                target.kind==FlowKind::Edge;
        if(source.kind==FlowKind::Reed||source.kind==FlowKind::Edge)
            return vibrates(target.kind)||target.kind==FlowKind::Bridge;
        if(vibrates(source.kind))
            return vibrates(target.kind)||target.kind==FlowKind::Bridge||
                target.kind==FlowKind::Damper;
        if(source.kind==FlowKind::Bridge||source.kind==FlowKind::Damper)
            return vibrates(target.kind)||target.kind==FlowKind::Bridge||
                target.kind==FlowKind::Damper;
        return false;
    }
    bool connect(int from,int to) noexcept {
        if(!compatible(from,to)||linked(from,to)||linkCount>=kMaxFlowLinks)return false;
        FlowLink link;
        link.from=static_cast<uint8_t>(from);
        link.to=static_cast<uint8_t>(to);
        link.position=objectFlow?.5f:std::clamp(nodes[to].x,0.f,1.f);
        link.modMin=link.position;link.modMax=link.position;
        if(nodes[to].kind==FlowKind::Damper)
            link.modTravel=1.f; // M physically approaches the vibrating source
        links[linkCount++]=link;
        return true;
    }
    bool disconnect(int from,int to) noexcept {
        if(objectFlow&&from==0&&to==1)return false;
        for(int i=0;i<linkCount;++i)if(links[i].from==from&&links[i].to==to){
            for(int j=i;j+1<linkCount;++j)links[j]=links[j+1];
            --linkCount;return true;
        }
        return false;
    }
    int addNode(FlowKind kind) noexcept {
        if(kind==FlowKind::Exciter||kind==FlowKind::Output||
           nodeCount>=kMaxFlowNodes)return -1;
        FlowNode n;
        n.kind=kind;
        n.x=(count(kind)%3)*.30f+.15f;
        n.y=(kind==FlowKind::Bridge||kind==FlowKind::Damper)?.58f:
            kind==FlowKind::Body?.70f:kind==FlowKind::Router?.21f:.36f;
        n.material=kind==FlowKind::AirColumn?Material::Wood:
            kind==FlowKind::Membrane?Material::Leather:
            kind==FlowKind::Bridge?Material::Hardwood:
            kind==FlowKind::Damper?Material::Felt:
            kind==FlowKind::Reed?Material::Bamboo:
            kind==FlowKind::Edge?Material::Hardwood:Material::Steel;
        n.geometry=kind==FlowKind::AirColumn?Geometry::Tube:
            kind==FlowKind::Body?Geometry::HollowBox:Geometry::Plate;
        n.coupling=.6f;
        const int i=nodeCount++;
        nodes[static_cast<size_t>(i)]=n;
        return i;
    }
    bool removeNode(int id) noexcept {
        if(id<0||id>=nodeCount||(objectFlow&&id<2))return false;
        if(nodes[id].kind==FlowKind::Exciter||nodes[id].kind==FlowKind::Output)return false;
        for(int i=0;i<linkCount;){
            if(links[i].from==id||links[i].to==id){
                for(int j=i;j+1<linkCount;++j)links[j]=links[j+1];
                --linkCount;continue;
            }
            if(links[i].from>id)--links[i].from;
            if(links[i].to>id)--links[i].to;
            ++i;
        }
        for(int i=id;i+1<nodeCount;++i)nodes[i]=nodes[i+1];
        --nodeCount;
        return true;
    }
    bool moveNode(int id,float x,float y) noexcept {
        if(id<0||id>=nodeCount)return false;
        auto& n=nodes[static_cast<size_t>(id)];
        if(n.kind==FlowKind::Exciter||n.kind==FlowKind::Output)return false;
        // Preserve causal ordering; dragging cannot silently reverse a link.
        float lower=.035f,upper=.955f;
        for(int j=0;j<linkCount;++j){
            const auto& l=links[j];
            if(l.from==id)upper=std::min(upper,nodes[l.to].y-.036f);
            if(l.to==id)lower=std::max(lower,nodes[l.from].y+.036f);
        }
        if(lower>upper)return false;
        n.x=std::clamp(x,0.f,1.f);
        n.y=std::clamp(y,lower,upper);
        for(int i=0;i<linkCount;++i)
            if(!objectFlow&&links[i].to==id)links[i].position=n.x;
        return true;
    }
};
inline FlowGraph blankFlowGraph() noexcept {
    FlowGraph graph;
    graph.nodeCount=2;
    graph.nodes[0].kind=FlowKind::Exciter;
    graph.nodes[0].material=Material::Wood;
    graph.nodes[0].excitation=Excitation::Hammer;
    graph.nodes[0].x=.36f;graph.nodes[0].y=.025f;
    graph.nodes[1].kind=FlowKind::Output;
    graph.nodes[1].x=.45f;graph.nodes[1].y=.94f;
    return graph;
}
inline FlowGraph templateFlowGraph(FlowTemplate type) noexcept {
    FlowGraph g=blankFlowGraph();
    if(type==FlowTemplate::Blank)return g;
    if(type==FlowTemplate::HybridWind){
        // One breath source, movable flow router, independent reed & edge,
        // two different entry locations on the SAME air column.
        g.nodes[0].excitation=Excitation::Blow;
        const int router=g.addNode(FlowKind::Router);
        const int reed=g.addNode(FlowKind::Reed);
        const int edge=g.addNode(FlowKind::Edge);
        const int bore=g.addNode(FlowKind::AirColumn);
        g.nodes[router].x=.5f;g.nodes[router].y=.18f;
        g.nodes[router].position=.20f;g.nodes[router].motion=.60f;
        g.nodes[reed].x=.22f;g.nodes[reed].y=.39f;
        g.nodes[edge].x=.78f;g.nodes[edge].y=.39f;
        g.nodes[bore].x=.5f;g.nodes[bore].y=.67f;
        g.nodes[bore].material=Material::Bamboo;
        g.connect(0,router);
        g.connect(router,reed);
        g.connect(router,edge);
        g.connect(reed,bore);
        g.connect(edge,bore);
        g.connect(bore,1);
        for(int i=0;i<g.linkCount;++i){
            auto& link=g.links[i];
            if(link.from==router&&link.to==reed)link.position=.20f;
            if(link.from==router&&link.to==edge)link.position=.80f;
            if(link.to==bore&&link.from==reed)link.position=.12f;
            if(link.to==bore&&link.from==edge)link.position=.52f;
        }
        return g;
    }
    FlowKind primary=FlowKind::String;
    switch(type){
    case FlowTemplate::Drum:primary=FlowKind::Membrane;break;
    case FlowTemplate::Plate:primary=FlowKind::Plate;break;
    case FlowTemplate::Bar:primary=FlowKind::Bar;break;
    case FlowTemplate::Air:primary=FlowKind::AirColumn;break;
    case FlowTemplate::Bell:primary=FlowKind::Body;break;
    default:break;
    }
    const int a=g.addNode(primary);
    auto& first=g.nodes[static_cast<size_t>(a)];
    first.x=.45f;first.y=.34f;
    first.material=primary==FlowKind::String?Material::Steel:
        primary==FlowKind::Membrane?Material::Leather:
        primary==FlowKind::AirColumn?Material::Bamboo:
        primary==FlowKind::Body?Material::Bronze:Material::Hardwood;
    first.geometry=primary==FlowKind::Body?Geometry::Bell:
        primary==FlowKind::AirColumn?Geometry::Tube:Geometry::Plate;
    if(type==FlowTemplate::Air)g.nodes[0].excitation=Excitation::Blow;
    else if(type==FlowTemplate::String)g.nodes[0].excitation=Excitation::Pluck;
    g.connect(0,a);
    g.connect(a,1); // Dry object radiation can reach the output directly.
    if(type!=FlowTemplate::Bell) {
        const int b=g.addNode(FlowKind::Body);
        auto& second=g.nodes[static_cast<size_t>(b)];
        second.x=.46f;second.y=.67f;second.coupling=.50f;
        second.material=primary==FlowKind::AirColumn?Material::Wood:Material::Hardwood;
        g.connect(a,b);g.connect(b,1);
    }
    return g;
}
inline FlowGraph universalEmptyGraph() noexcept {
    FlowGraph g;
    g.nodeCount=1;
    FlowNode& src=g.nodes[0];
    src.kind=FlowKind::Universal;src.role=PhysicalRole::Source;
    src.sourceAcceleration=32.f;src.sourcePotential=.65f;
    src.sourceTimeMs=4.f;src.sourceFollowNote=false;
    src.x=.43f;src.y=.03f;
    return g;
}
inline int addPhysicalRole(FlowGraph& g,PhysicalRole role) noexcept {
    if(g.nodeCount>=kMaxFlowNodes)return -1;
    FlowNode n;
    n.kind=FlowKind::Universal;n.role=role;
    n.x=.43f;n.y=role==PhysicalRole::Source?.07f:
        role==PhysicalRole::Transfer?.25f:
        role==PhysicalRole::Interaction?.43f:
        role==PhysicalRole::Resonator?.65f:.92f;
    n.form.continuum=role==PhysicalRole::Resonator?
        Continuum::Gas:Continuum::Solid;
    if(role==PhysicalRole::Source){
        n.material=Material::Rubber;
        n.sourceAcceleration=32.f;n.sourcePotential=.65f;
        n.sourceTimeMs=4.f;n.sourceFollowNote=false;
    }else if(role==PhysicalRole::Output){
        n.form.continuum=Continuum::Gas;
        n.material=Material::Air;
    }else if(role==PhysicalRole::Transfer){
        n.form.continuum=Continuum::Filament;
        n.form.widthMeters=.001f;n.form.depthMeters=.001f;
        n.material=Material::Nylon;
    }else if(role==PhysicalRole::Interaction){
        n.form.continuum=Continuum::Sheet;
        n.material=Material::Wood;
    }
    if(role==PhysicalRole::Resonator)n.material=Material::Air;
    const int id=g.nodeCount++;
    g.nodes[id]=n;
    return id;
}
inline FlowGraph universalStarterGraph() noexcept {
    FlowGraph g=universalEmptyGraph();
    const int t=addPhysicalRole(g,PhysicalRole::Transfer);
    const int i=addPhysicalRole(g,PhysicalRole::Interaction);
    const int r=addPhysicalRole(g,PhysicalRole::Resonator);
    const int o=addPhysicalRole(g,PhysicalRole::Output);
    g.nodes[r].form.continuum=Continuum::Solid;
    g.nodes[r].material=Material::Wood;
    g.nodes[r].form.widthMeters=.2f;
    g.connect(0,t);
    g.connect(t,i);
    g.connect(i,r);
    g.connect(r,o);
    return g;
}
inline FlowGraph objectFlowEmptyGraph() noexcept {
    FlowGraph g=universalEmptyGraph();
    g.objectFlow=true;
    const int t=addPhysicalRole(g,PhysicalRole::Transfer);
    g.nodes[t].y=.23f;
    g.connect(0,t);
    return g;
}
inline FlowGraph objectFlowStarterGraph() noexcept {
    FlowGraph g=objectFlowEmptyGraph();
    const int r=addPhysicalRole(g,PhysicalRole::Resonator);
    const int o=addPhysicalRole(g,PhysicalRole::Output);
    g.nodes[r].form.continuum=Continuum::Solid;
    g.nodes[r].material=Material::Wood;
    g.nodes[r].form.widthMeters=.2f;
    g.nodes[r].y=.58f;
    g.connect(1,r);
    auto& contact=g.links[g.linkCount-1];
    contact.motion=.7f;contact.angleDeg=25.f;contact.adhesion=0.f;
    contact.aperture=.65f;
    g.connect(r,o);
    return g;
}
inline FlowGraph constructionEmptyGraph() noexcept {
    FlowGraph g=objectFlowEmptyGraph();
    initializeConstruction(g.nodes[1],true);
    return g;
}
inline FlowGraph constructionStarterGraph() noexcept {
    FlowGraph g=objectFlowStarterGraph();
    initializeConstruction(g.nodes[1],true);
    initializeConstruction(g.nodes[2]);
    return g;
}
// Example only: these are the same editable values exposed by the builder.
// No guitar-specific behavior is selected by the solver.
inline FlowGraph constructionGuitarGraph() noexcept {
    FlowGraph g=constructionEmptyGraph();
    g.spanMs=8.f;
    auto& energy=g.nodes[0];
    energy.sourceAcceleration=85.f;energy.sourcePotential=.85f;energy.sourceTimeMs=5.f;
    auto& pick=g.nodes[1];pick.x=.42f;pick.y=.20f;
    pick.form.lengthMeters=.03f;pick.form.widthMeters=.015f;pick.form.depthMeters=.0008f;
    pick.form.edges=3;pick.form.rounding=.2f;pick.form.fixing=.8f;pick.tensionNewtons=0.f;
    pick.substance.value={.75f,.7f,.95f,.75f,.30f,.32f,.18f,.65f,.25f,.25f,0.f,.32f,.12f};
    const int string=addPhysicalRole(g,PhysicalRole::Resonator);
    const int palm=addPhysicalRole(g,PhysicalRole::Resonator);
    const int output=addPhysicalRole(g,PhysicalRole::Output);
    auto& s=g.nodes[string];initializeConstruction(s);
    s.x=.43f;s.y=.43f;s.form.lengthMeters=.65f;s.form.widthMeters=.0005f;s.form.depthMeters=.0005f;
    s.form.edges=32;s.form.rounding=1.f;s.form.fixing=.8f;s.tensionNewtons=56.f;s.damping=.35f;
    s.substance.value={.933f,.93f,.99f,.908f,.100f,.05f,.0f,.9f,.2f,.2f,0.f,.15f,.12f};
    auto& h=g.nodes[palm];initializeConstruction(h);
    h.x=.13f;h.y=.72f;h.form.lengthMeters=.08f;h.form.widthMeters=.04f;h.form.depthMeters=.016f;
    h.form.rounding=.8f;h.form.fixing=.25f;h.tensionNewtons=0.f;h.damping=2.f;
    h.substance.value={.738f,.12f,.35f,.40f,.34f,.90f,.35f,.15f,.3f,.4f,.02f,.65f,.12f};
    g.nodes[output].x=.73f;g.nodes[output].y=.92f;
    g.connect(1,string);
    auto& strike=g.links[g.linkCount-1];strike.position=.21f;strike.fromPosition=.8f;
    strike.aperture=.85f;strike.engagement=1.f;strike.motion=.8f;strike.angleDeg=30.f;
    strike.adhesion=0.f;strike.friction=.55f;strike.elasticity=.55f;strike.restitution=.55f;
    g.connect(string,palm);
    auto& mute=g.links[g.linkCount-1];mute.position=0.f;mute.modEnabled=true;mute.modMin=0.f;mute.modMax=.5f;
    mute.fromPosition=.12f;mute.aperture=.8f;mute.engagement=1.f;mute.motion=0.f;
    mute.adhesion=0.f;mute.friction=.8f;mute.elasticity=.45f;mute.restitution=.05f;
    g.connect(string,output);
    auto& pickup=g.links[g.linkCount-1];pickup.position=.23f;pickup.aperture=.8f;
    return g;
}
inline FlowGraph energyConstruction(FlowGraph g) noexcept {
    g.energySolver=true;
    for(int i=0;i<g.nodeCount;++i)g.nodes[i].siMechanics=g.nodes[i].substance.enabled;
    return g;
}
inline FlowGraph energyStarterGraph() noexcept {return energyConstruction(constructionStarterGraph());}
inline FlowGraph energyEmptyGraph() noexcept {return energyConstruction(constructionEmptyGraph());}
inline FlowGraph energyGuitarGraph() noexcept {return energyConstruction(constructionGuitarGraph());}
// v14 APT maps editable slider positions to physical capacity; zero is exact.
inline double aptCapacity(double normalized,bool continuous) noexcept {
    const double t=std::clamp(normalized,0.,1.);
    if(t==0.)return 0.;
    return (continuous?1.e-5:1.e-8)*std::pow(10.,t*(continuous?7.:8.));
}
inline double aptCapacitySlider(double capacity,bool continuous) noexcept {
    if(capacity<=0.)return 0.;
    return std::clamp(std::log10(capacity/(continuous?1.e-5:1.e-8))/(continuous?7.:8.),0.000001,1.);
}
inline float aptAccelerationSlider(float a) noexcept {
    return std::clamp(std::log10(std::max(.1f,a)/.1f)/std::log10(50000.f),0.f,1.f);
}
inline float aptAcceleration(float t) noexcept {return .1f*std::pow(50000.f,materialUnit(t));}
inline FlowGraph acousticConstruction(FlowGraph g) noexcept {
    g=energyConstruction(g);g.acousticSolver=true;
    for(int i=0;i<g.nodeCount;++i)g.nodes[i].siAcoustics=true;
    return g;
}
inline FlowGraph acousticEmptyGraph() noexcept {
    auto g=acousticConstruction(constructionEmptyGraph());
    g.nodes[0].sourcePotential=static_cast<float>(aptCapacitySlider(.001,false));return g;
}
inline FlowGraph acousticStarterGraph() noexcept {
    auto g=acousticConstruction(constructionStarterGraph());
    g.nodes[0].sourcePotential=static_cast<float>(aptCapacitySlider(.001,false));return g;
}
inline FlowGraph acousticBareGuitarGraph() noexcept {
    auto g=acousticConstruction(constructionGuitarGraph());
    // Match v13 capacity for the controlled comparison, not a new gain.
    g.nodes[0].sourcePotential=static_cast<float>(aptCapacitySlider(22.e-6*.85,false));
    return g;
}
// All example builders only populate existing editable fields.
inline FlowGraph acousticStringBodyGraph(bool violin=false) noexcept {
    auto g=acousticBareGuitarGraph();g.spanMs=6.f;
    if(violin){g.disconnect(2,3);g.removeNode(3);}
    const int out=g.nodeCount-1;
    g.disconnect(2,out);
    auto& energy=g.nodes[0];
    energy.sourceAcceleration=violin?180.f:280.f;
    energy.sourcePotential=static_cast<float>(aptCapacitySlider(violin?.015:.0006,violin));
    energy.sourceFollowNote=violin;energy.sourceTimeMs=7.f;
    auto& pick=g.nodes[1];auto& string=g.nodes[2];
    if(violin){
        pick.form.lengthMeters=.28f;pick.form.widthMeters=.009f;pick.form.depthMeters=.001f;
        pick.form.rounding=.7f;pick.substance.value[Roughness]=.9f;pick.substance.value[Hardness]=.45f;
        string.form.lengthMeters=.33f;string.tensionNewtons=50.f;
        auto& contact=g.links[1];contact.motion=1.f;contact.angleDeg=89.f;contact.friction=1.f;
        contact.aperture=.8f;contact.engagement=1.f;contact.elasticity=.5f;contact.restitution=.3f;
    }
    string.y=.34f;g.nodes[out].y=.97f;g.nodes[out].x=.72f;
    const int bridge=addPhysicalRole(g,PhysicalRole::Resonator);
    const int board=addPhysicalRole(g,PhysicalRole::Resonator);
    const int air=addPhysicalRole(g,PhysicalRole::Resonator);
    auto wood=std::array<float,MaterialControlCount>{.69f,.5f,.94f,.79f,.22f,.32f,.1f,.55f,.8f,.4f,.10f,.35f,.12f};
    auto& b=g.nodes[bridge];initializeConstruction(b);b.substance.value=wood;b.x=.66f;b.y=.48f;
    b.form.lengthMeters=violin?.04f:.08f;b.form.widthMeters=.018f;b.form.depthMeters=.006f;
    b.form.rounding=.35f;b.tensionNewtons=0.f;b.damping=.5f;
    auto& top=g.nodes[board];initializeConstruction(top);top.substance.value=wood;top.x=.66f;top.y=.64f;
    top.form.lengthMeters=violin?.35f:.47f;top.form.widthMeters=violin?.20f:.35f;
    top.form.depthMeters=violin?.003f:.0028f;top.form.rounding=.8f;top.form.curvature=violin?.25f:.04f;
    top.form.fixing=.75f;top.tensionNewtons=0.f;top.damping=.6f;
    auto& cavity=g.nodes[air];initializeConstruction(cavity);cavity.x=.42f;cavity.y=.82f;
    cavity.substance.value={.0846f,.02f,.99f,0.f,.856f,.2f,.0256f,.05f,0.f,.1f,0.f,0.f,0.f};
    cavity.form.lengthMeters=top.form.lengthMeters;cavity.form.widthMeters=top.form.widthMeters;
    cavity.form.depthMeters=violin?.04f:.095f;cavity.form.rounding=.85f;
    cavity.form.confinement=1.f;cavity.form.opening=0.f;cavity.form.wallMm=3.f;
    cavity.form.openings[0].size=violin?.6f:.85f;cavity.form.openings[0].aspect=violin?.18f:1.f;
    cavity.form.openings[0].position=.55f;cavity.form.openings[0].neck=.02f;
    if(violin){cavity.form.openings[1]=cavity.form.openings[0];cavity.form.openings[1].position=.45f;}
    cavity.damping=.7f;
    auto couple=[&](int a,int b,float x,float y,float area,float elastic){
        if(!g.connect(a,b))return;
        auto& l=g.links[g.linkCount-1];l.fromPosition=x;l.position=y;l.aperture=area;
        l.engagement=1.f;l.adhesion=1.f;l.motion=0.f;l.elasticity=elastic;l.restitution=.8f;
    };
    couple(2,bridge,.055f,.5f,.65f,.85f);
    couple(bridge,board,.5f,.40f,.9f,.85f);
    couple(board,air,.45f,.5f,.9f,.5f);
    if(g.connect(board,out)){auto& l=g.links[g.linkCount-1];l.position=.48f;l.aperture=.9f;l.clearance=.15f;}
    if(g.connect(air,out)){auto& l=g.links[g.linkCount-1];l.position=.50f;l.aperture=.85f;l.clearance=.15f;}
    return acousticConstruction(g);
}
inline FlowGraph acousticBellDrumGraph(bool drum=false) noexcept {
    auto g=acousticConstruction(constructionStarterGraph());g.spanMs=6.f;
    auto& e=g.nodes[0];e.sourceAcceleration=drum?300.f:500.f;
    e.sourcePotential=static_cast<float>(aptCapacitySlider(drum?.006:.025,false));e.sourceTimeMs=10.f;
    auto& mallet=g.nodes[1];mallet.form.lengthMeters=.045f;mallet.form.widthMeters=.026f;mallet.form.depthMeters=.022f;
    mallet.form.rounding=1.f;mallet.tensionNewtons=0.f;
    mallet.substance.value={.78f,.65f,.93f,.74f,.25f,.42f,.18f,.5f,.25f,.2f,0.f,.4f,.12f};
    auto& n=g.nodes[2];n.form.lengthMeters=drum?.28f:.18f;n.form.widthMeters=drum?.28f:.16f;
    n.form.depthMeters=drum?.0008f:.16f;n.form.rounding=1.f;n.form.edges=32;
    n.form.curvature=drum?0.f:.6f;n.form.wallFill=drum?1.f:0.f;
    n.form.wallMm=drum?.8f:4.f;n.form.opening=drum?0.f:1.f;
    n.form.stations=drum?std::array<float,4>{1.f,1.f,1.f,1.f}:std::array<float,4>{.52f,.65f,.82f,1.f};
    n.form.taper=drum?0.f:.55f;n.form.fixing=drum?.9f:.20f;n.tensionNewtons=drum?400.f:0.f;
    n.damping=drum?.7f:.25f;
    n.substance.value=drum?std::array<float,MaterialControlCount>{.74f,.35f,.85f,.70f,.24f,.40f,.2f,.35f,.2f,.3f,0.f,.3f,.12f}:
       std::array<float,MaterialControlCount>{.94f,.9f,.99f,.89f,.12f,.08f,0.f,.9f,.2f,.2f,0.f,.15f,.12f};
    auto& c=g.links[1];c.motion=.8f;c.angleDeg=0.f;c.engagement=1.f;c.aperture=.9f;
    c.elasticity=.7f;c.restitution=.75f;c.fromPosition=.8f;c.position=drum?.35f:.82f;
    g.links[2].position=.32f;g.links[2].aperture=.9f;g.links[2].clearance=.15f;
    return g;
}
inline FlowGraph jetConstruction(FlowGraph g) noexcept {g.jetSolver=true;return g;}
inline bool jetEdgeCandidate(const FlowGraph& g,const FlowLink& l) noexcept {
    return g.jetSolver&&l.from==1&&l.to<g.nodeCount&&
        objectIsFluid(g.nodes[l.from])&&!objectIsFluid(g.nodes[l.to])&&
        physicalVibrates(g.nodes[l.to])&&g.nodes[l.from].form.confinement<.5f;
}
inline FlowGraph jetFluteGraph() noexcept {
    auto g=jetConstruction(acousticEmptyGraph());g.spanMs=5.f;
    auto& source=g.nodes[0];
    source.sourceAcceleration=4500.f;source.sourcePotential=static_cast<float>(aptCapacitySlider(.5,true));
    source.sourceFollowNote=true;source.sourceTimeMs=10.f;
    auto& jet=g.nodes[1];jet.x=.28f;jet.y=.20f;jet.form.lengthMeters=.08f;
    jet.form.widthMeters=.010f;jet.form.depthMeters=.0012f;jet.form.edges=4;jet.form.rounding=.3f;
    jet.form.confinement=.1f;jet.form.spread=.10f;jet.form.flowAngle=0.f;
    jet.substance.value={.0846f,.02f,.99f,0.f,.856f,.12f,.0256f,.05f,0.f,.1f,0.f,0.f,0.f};
    const int edge=addPhysicalRole(g,PhysicalRole::Resonator);
    const int tube=addPhysicalRole(g,PhysicalRole::Resonator);
    const int out=addPhysicalRole(g,PhysicalRole::Output);
    auto& e=g.nodes[edge];initializeConstruction(e);e.siMechanics=e.siAcoustics=true;
    e.x=.52f;e.y=.40f;e.form.lengthMeters=.012f;e.form.widthMeters=.010f;e.form.depthMeters=.0015f;
    e.form.edges=3;e.form.rounding=.04f;e.form.taper=.8f;e.form.fixing=1.f;e.tensionNewtons=0.f;
    e.substance.value={.82f,.85f,.98f,.82f,.18f,.15f,.08f,.75f,.15f,.2f,0.f,.10f,.12f};
    auto& t=g.nodes[tube];initializeConstruction(t);t.siMechanics=t.siAcoustics=true;t.substance=jet.substance;
    t.x=.55f;t.y=.64f;t.form.lengthMeters=.30f;t.form.widthMeters=.014f;t.form.depthMeters=.014f;
    t.form.edges=32;t.form.rounding=1.f;t.form.confinement=1.f;t.form.opening=1.f;t.form.wallMm=2.f;t.damping=.32f;
    g.nodes[out].x=.65f;g.nodes[out].y=.94f;
    g.connect(1,edge);auto& l=g.links[g.linkCount-1];
    l.jetDistanceMeters=.0045f;l.edgeBevelDeg=30.f;l.angleDeg=0.f;l.elevationDeg=0.f;
    l.motion=1.f;l.adhesion=0.f;l.aperture=1.f;l.engagement=1.f;l.position=.52f;
    l.modEnabled=true;l.modMin=.40f;l.modMax=.60f;
    g.connect(edge,tube);auto& window=g.links[g.linkCount-1];window.position=.01f;
    window.aperture=1.f;window.engagement=1.f;window.adhesion=0.f;window.motion=1.f;
    g.connect(tube,out);auto& pickup=g.links[g.linkCount-1];pickup.position=.98f;pickup.aperture=.9f;pickup.clearance=.15f;
    return g;
}
inline FlowGraph dispersiveJetConstruction(FlowGraph g) noexcept {
    g=jetConstruction(g);g.dispersiveJet=true;return g;
}
inline FlowGraph dispersiveJetFluteGraph() noexcept {return dispersiveJetConstruction(jetFluteGraph());}
inline FlowGraph integratedConstruction(FlowGraph g) noexcept {
    g=dispersiveJetConstruction(g);g.integratedMechanics=true;return g;
}
inline FlowGraph integratedPercussionGraph(bool drum=false) noexcept {
    auto g=integratedConstruction(acousticBellDrumGraph(drum));
    // Gentle default strike with digital headroom; full APT range remains.
    g.nodes[0].sourcePotential=static_cast<float>(aptCapacitySlider(drum?.00030:.0025,false));
    return g;
}
inline FlowGraph integratedBowedGraph(bool body=true) noexcept {
    auto g=body?acousticStringBodyGraph(true):acousticBareGuitarGraph();
    if(!body){g.disconnect(2,3);g.removeNode(3);}
    g=integratedConstruction(g);g.spanMs=5.f;
    auto& src=g.nodes[0];src.sourceFollowNote=true;src.sourceAcceleration=14.f;
    src.sourcePotential=static_cast<float>(aptCapacitySlider(.3,true));
    auto& bow=g.nodes[1];bow.form.lengthMeters=.28f;bow.form.widthMeters=.015f;bow.form.depthMeters=.004f;
    bow.form.rounding=.65f;bow.form.fixing=.8f;bow.tensionNewtons=0.f;bow.damping=2.f;
    bow.substance.value={.78f,.55f,.9f,.77f,.23f,.45f,.12f,.55f,.65f,.3f,0.f,.95f,.12f};
    auto& s=g.nodes[2];s.form.lengthMeters=.33f;s.form.widthMeters=.0005f;s.form.depthMeters=.0005f;
    s.tensionNewtons=50.f;s.damping=.35f;
    auto& c=g.links[1];c.motion=1.f;c.angleDeg=90.f;c.friction=.85f;c.aperture=.9f;c.engagement=1.f;
    c.adhesion=0.f;c.normalLoadN=.28f;c.kineticRatio=.12f;c.frictionRecoveryMs=.8f;
    c.position=.12f;c.fromPosition=.5f;c.modEnabled=false;
    if(body)for(int k=0;k<g.linkCount;++k)if(g.links[k].from==2){g.links[k].fromPosition=.008f;g.links[k].aperture=.10f;}
    return g;
}
inline FlowGraph integratedValveGraph(float polarity=-1.f) noexcept {
    auto g=integratedConstruction(jetFluteGraph());
    auto& source=g.nodes[0];source.sourceAcceleration=5000.f;
    source.sourcePotential=static_cast<float>(aptCapacitySlider(4.0,true));
    auto& stream=g.nodes[1];stream.form.lengthMeters=.65f;
    stream.form.widthMeters=.012f;stream.form.depthMeters=.003f;
    auto& reed=g.nodes[2];reed.form.lengthMeters=.020f;reed.form.widthMeters=.010f;reed.form.depthMeters=.00050f;
    reed.form.edges=4;reed.form.rounding=.05f;reed.form.taper=0.f;reed.form.fixing=1.f;reed.damping=1.4f;
    reed.substance.value={.72f,.48f,.97f,.80f,.22f,.42f,.12f,.65f,.65f,.2f,0.f,.35f,.12f};
    auto& tube=g.nodes[3];tube.form.lengthMeters=.36f;tube.form.widthMeters=.014f;tube.form.depthMeters=.014f;
    tube.form.opening=.35f;tube.form.confinement=1.f;tube.damping=.5f;
    auto& c=g.links[1];c.jetDistanceMeters=0.f;c.valveGapMeters=.00040f;
    c.valvePolarity=polarity;c.modEnabled=false;c.position=.5f;c.angleDeg=0.f;
    return g;
}
inline FlowGraph defaultFlowGraph() noexcept {
    return universalStarterGraph();
}

// A keyboard note does not pitch-shift audio: it selects the values of
// user-assigned REAL geometric/mechanical parameters. Multiple assignments
// are independently addressable and precomputed outside the audio callback.
enum class PitchProperty : uint8_t {
    Length, Width, Depth, Wall, Curvature, Taper, Opening, Fixing,
    Tension, Profile0, Profile1, Profile2, Profile3, Coupling,
    LinkPosition, LinkAperture, Count
};
inline constexpr const char* pitchPropertyName(PitchProperty p) noexcept {
    switch(p){
    case PitchProperty::Length:return "Length";
    case PitchProperty::Width:return "Width";
    case PitchProperty::Depth:return "Depth";
    case PitchProperty::Wall:return "Wall thickness";
    case PitchProperty::Curvature:return "Curvature";
    case PitchProperty::Taper:return "Taper";
    case PitchProperty::Opening:return "Opening";
    case PitchProperty::Fixing:return "End fixing";
    case PitchProperty::Tension:return "Tension";
    case PitchProperty::Profile0:return "Profile 0%";
    case PitchProperty::Profile1:return "Profile 33%";
    case PitchProperty::Profile2:return "Profile 67%";
    case PitchProperty::Profile3:return "Profile 100%";
    case PitchProperty::Coupling:return "Coupling";
    case PitchProperty::LinkPosition:return "Link entry X";
    case PitchProperty::LinkAperture:return "Link aperture";
    case PitchProperty::Count:break;
    }
    return "Physical property";
}
inline constexpr int kPitchSlots=4;
struct PitchAssignment {
    int8_t objectIndex{-1}; // node index, or link index for link properties
    PitchProperty property{PitchProperty::Tension};
    float lower{1.f},upper{400.f};
    float priority{1.f}; // positive preference; never an audio gain
};
struct PitchCalibration {
    bool enabled{false},valid{false},acousticRefined{false};
    uint8_t count{0};
    int8_t trackedNode{-1};
    int16_t referenceMidi{60};
    float a4Hz{440.f};
    float measuredHz{0.f}; // spectral estimate from rendered audio
    float modelHz{0.f};    // reduced physical modal prediction
    float confidence{0.f};
    float centsError{0.f};
    uint32_t graphStamp{0};
    std::array<PitchAssignment,kPitchSlots> assignments{};
    std::array<std::array<float,kPitchSlots>,128> values{};
    std::array<uint8_t,128> playable{}; // unreachable notes are NOT pitch shifted
};
// Fine physically solved note positions used for *continuous* pitch bend.
// Built by Find/Apply away from the audio thread. This is not a sample
// resampler: each entry contains physical geometry/mechanical settings.
inline constexpr int kPhysicalBendSubsteps=8;
inline constexpr int kPhysicalBendEntries=127*kPhysicalBendSubsteps+1;
struct PitchBendMap {
    uint64_t calibrationKey{0}; // zero means not prepared
    std::array<std::array<float,kPitchSlots>,kPhysicalBendEntries> values{};
    std::array<uint8_t,kPhysicalBendEntries> playable{};
    std::array<uint16_t,kPhysicalBendEntries> nearest{}; // out-of-range clamp
};
struct Blueprint {
    // Legacy standalone physical voice settings. If graph.nodeCount>0 the
    // graph is authoritative and these values are derived from connected nodes.
    Material stringMaterial{Material::Steel};
    float stringDiameterMm{0.50f};
    float referenceLengthMeters{0.65f};
    float tensionNewtons{80.0f};
    Excitation excitation{Excitation::Pluck};
    Material exciterMaterial{Material::Nylon};
    float excitationPosition{0.19f};
    float hammerHardness{0.65f};
    Geometry bodyGeometry{Geometry::HollowBox};
    Material bodyMaterial{Material::Wood};
    float bodySize{1.0f};
    float bridgeCoupling{0.65f};
    Material damperMaterial{Material::Felt};
    float damperBasePosition{0.07f};
    float damperTravel{0.09f};
    float damperStrength{1.0f};
    float bodyThicknessMm{3.0f};
    float bodyDamping{1.0f};
    FlowGraph graph{};
    PitchCalibration pitch{};
    PitchBendMap bend{}; // voice-private copies keep polyphony independent
};

} // namespace aiora::physical
