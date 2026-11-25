#pragma once
// Aiora universal physical mechanism graph v4. Objects retain vibrational
// state; nodes mark first arrival in time (Y), links carry energy into their
// target at a particular local position (X), and assigned M moves mechanisms.
// All graph edits happen outside the real-time audio thread.

#include <algorithm>
#include <array>
#include <cstdint>

namespace aiora::physical {

enum class Material : uint8_t {
    Steel, Nylon, Bronze, Wood, Glass, Ceramic, Rubber, Felt,
    Aluminum, Copper, Brass, Iron, Titanium, Silver, Gold, CarbonFiber,
    Bamboo, Hardwood, Plywood, Cork, Acrylic, ABS, Polycarbonate,
    Silicone, Leather, Wool, Stone, Concrete, Porcelain, Paper,
    Count
};
inline constexpr int kMaterialCount=static_cast<int>(Material::Count);
inline constexpr std::array<const char*,kMaterialCount> kMaterialNames{
    "Steel","Nylon","Bronze","Wood","Glass","Ceramic","Rubber","Felt",
    "Aluminum","Copper","Brass","Iron","Titanium","Silver","Gold",
    "Carbon fiber","Bamboo","Hardwood","Plywood","Cork","Acrylic",
    "ABS plastic","Polycarbonate","Silicone","Leather","Wool",
    "Stone","Concrete","Porcelain","Paper"
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
    Damper, Output, Router, Reed, Edge
};
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
    float motion{0.f}; // router outlet travel under M (normalized position)
};
struct FlowLink {
    uint8_t from{0},to{0};
    float position{.5f}; // position ON target (0..1)
    float modTravel{0.f}; // -1..1, movement of interaction under M
    float aperture{.22f}; // size of interaction area / flow inlet
};
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
        if(source.kind==FlowKind::Output||target.kind==FlowKind::Exciter)return false;
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
inline FlowGraph defaultFlowGraph() noexcept {
    return templateFlowGraph(FlowTemplate::Plate); // no compulsory string or bridge
}

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
};

} // namespace aiora::physical
