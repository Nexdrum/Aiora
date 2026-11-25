#pragma once
// Aiora universal physical roles + parametrically constructed geometry v5.
// Human source -> free repeatable physical roles -> output. Physical material,
// geometry, spatial interaction, and arrival time define behavior, not presets.
// All graph edits happen outside the real-time audio thread.

#include <algorithm>
#include <array>
#include <cstdint>

namespace aiora::physical {

enum class Material : uint8_t {
    Steel, Nylon, Bronze, Wood, Glass, Ceramic, Rubber, Felt,
    Aluminum, Copper, Brass, Iron, Titanium, Silver, Gold, CarbonFiber,
    Bamboo, Hardwood, Plywood, Cork, Acrylic, ABS, Polycarbonate,
    Silicone, Leather, Wool, Stone, Concrete, Porcelain, Paper, Air,
    Count
};
inline constexpr int kMaterialCount=static_cast<int>(Material::Count);
inline constexpr std::array<const char*,kMaterialCount> kMaterialNames{
    "Steel","Nylon","Bronze","Wood","Glass","Ceramic","Rubber","Felt",
    "Aluminum","Copper","Brass","Iron","Titanium","Silver","Gold",
    "Carbon fiber","Bamboo","Hardwood","Plywood","Cork","Acrylic",
    "ABS plastic","Polycarbonate","Silicone","Leather","Wool",
    "Stone","Concrete","Porcelain","Paper","Air"
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
    case PhysicalRole::Source:return "ENERGY SOURCE";
    case PhysicalRole::Transfer:return "TRANSFER MEDIUM";
    case PhysicalRole::Interaction:return "INTERACTION";
    case PhysicalRole::Resonator:return "RESONATING MEDIUM";
    case PhysicalRole::Output:return "OUTPUT";
    }
    return "ROLE";
}
enum class Continuum : uint8_t { Filament, Sheet, Solid, Gas };
inline constexpr const char* continuumName(Continuum c) noexcept {
    switch(c){
    case Continuum::Filament:return "Filament";
    case Continuum::Sheet:return "Surface";
    case Continuum::Solid:return "Solid";
    case Continuum::Gas:return "Air cavity";
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
struct FlowNode {
    FlowKind kind{FlowKind::Plate};
    Material material{Material::Steel};
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
    float sourceDirection{0.f};  // normalized physical direction
    float movingMin{.5f},movingMax{.5f};
    bool movingEnabled{false};   // M may reposition ANY physical object
    ContactLaw law{ContactLaw::Coupled}; // interaction with connected media (normalized position)
};
struct FlowLink {
    uint8_t from{0},to{0};
    float position{.5f}; // position ON target (0..1)
    float modTravel{0.f}; // -1..1, movement of interaction under M
    float aperture{.22f}; // size of interaction area / flow inlet
    float modMin{.5f},modMax{.5f};
    bool modEnabled{false}; // independent M assignment; min>max reverses direction
    ContactLaw law{ContactLaw::Coupled};
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
        link.position=std::clamp(nodes[to].x,0.f,1.f);
        link.modMin=link.position;link.modMax=link.position;
        if(nodes[to].kind==FlowKind::Damper)
            link.modTravel=1.f; // M physically approaches the vibrating source
        links[linkCount++]=link;
        return true;
    }
    bool disconnect(int from,int to) noexcept {
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
        if(id<0||id>=nodeCount)return false;
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
            if(links[i].to==id)links[i].position=n.x;
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
    src.driver=Driver::Hand;src.gesture=Gesture::Impulse;
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
        n.driver=Driver::Hand;n.gesture=Gesture::Impulse;
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
    bool enabled{false},valid{false};
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
