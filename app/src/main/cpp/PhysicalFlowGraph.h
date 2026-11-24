#pragma once
// Instrument Flow Lab v2. A small, typed, fixed-capacity physical mechanism
// graph. Each edge has a mechanical meaning; invalid edge types are rejected.
// In v2 one vibrating string supports parallel body branches and contacts.
// No allocations in graph traversal or in the audio thread.

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
enum class Excitation : uint8_t { Pluck, Hammer };
enum class FlowKind : uint8_t { Exciter, String, Bridge, Body, Damper, Output };
inline constexpr const char* flowKindName(FlowKind k) noexcept {
    switch(k){
    case FlowKind::Exciter:return "EXCITER";
    case FlowKind::String:return "STRING";
    case FlowKind::Bridge:return "BRIDGE";
    case FlowKind::Body:return "RESONATOR";
    case FlowKind::Damper:return "DAMPER";
    case FlowKind::Output:return "OUTPUT";
    }
    return "NODE";
}
inline constexpr int kMaxFlowNodes=11;
inline constexpr int kMaxFlowLinks=20;
inline constexpr int kMaxFlowBodies=4;
inline constexpr int kMaxFlowDampers=3;

struct FlowNode {
    FlowKind kind{FlowKind::Body};
    Material material{Material::Wood};
    Geometry geometry{Geometry::HollowBox};
    Excitation excitation{Excitation::Pluck};
    float x{0.5f}, y{0.5f};             // graph presentation only, normalized
    float size{1.0f};                    // resonator dimension scale
    float thicknessMm{3.0f};            // resonator wall thickness
    float damping{1.0f};                // structural loss multiplier
    float coupling{0.65f};              // junction / contact strength
    float position{0.18f};              // contact/strike position
    float tensionNewtons{80.0f};
    float diameterMm{0.50f};
    float hardness{0.65f};
};
struct FlowLink { uint8_t from{0}, to{0}; };
struct FlowGraph {
    std::array<FlowNode,kMaxFlowNodes> nodes{};
    std::array<FlowLink,kMaxFlowLinks> links{};
    uint8_t nodeCount{0},linkCount{0};

    int first(FlowKind kind) const noexcept {
        for(int i=0;i<nodeCount;++i)if(nodes[static_cast<size_t>(i)].kind==kind)return i;
        return -1;
    }
    int count(FlowKind kind) const noexcept {
        int n=0;
        for(int i=0;i<nodeCount;++i)if(nodes[static_cast<size_t>(i)].kind==kind)++n;
        return n;
    }
    bool linked(int from,int to) const noexcept {
        for(int i=0;i<linkCount;++i)
            if(links[static_cast<size_t>(i)].from==from&&
               links[static_cast<size_t>(i)].to==to)return true;
        return false;
    }
    bool compatible(int from,int to) const noexcept {
        if(from<0||to<0||from>=nodeCount||to>=nodeCount||from==to)return false;
        const auto a=nodes[static_cast<size_t>(from)].kind;
        const auto b=nodes[static_cast<size_t>(to)].kind;
        return (a==FlowKind::Exciter&&b==FlowKind::String)||
               (a==FlowKind::String&&b==FlowKind::Bridge)||
               (a==FlowKind::Damper&&b==FlowKind::String)||
               (a==FlowKind::Bridge&&(b==FlowKind::Body||b==FlowKind::Output))||
               (a==FlowKind::Body&&b==FlowKind::Output);
    }
    bool connect(int from,int to) noexcept {
        if(!compatible(from,to)||linkCount>=kMaxFlowLinks||linked(from,to))return false;
        links[linkCount++]={static_cast<uint8_t>(from),static_cast<uint8_t>(to)};
        return true;
    }
    bool disconnect(int from,int to) noexcept {
        for(int i=0;i<linkCount;++i)if(links[static_cast<size_t>(i)].from==from&&
            links[static_cast<size_t>(i)].to==to){
            for(int j=i;j+1<linkCount;++j)links[j]=links[j+1];
            --linkCount;return true;
        }
        return false;
    }
    int addNode(FlowKind kind) noexcept {
        if(kind!=FlowKind::Body&&kind!=FlowKind::Damper)return -1;
        if(nodeCount>=kMaxFlowNodes)return -1;
        if(kind==FlowKind::Body&&count(kind)>=kMaxFlowBodies)return -1;
        if(kind==FlowKind::Damper&&count(kind)>=kMaxFlowDampers)return -1;
        FlowNode node;
        node.kind=kind;
        node.material=kind==FlowKind::Body?Material::Wood:Material::Felt;
        node.x=kind==FlowKind::Body?.70f:.15f;
        node.y=std::clamp(.48f+.12f*count(kind),.12f,.85f);
        node.coupling=kind==FlowKind::Body?.55f:.65f;
        const int id=nodeCount++;
        nodes[static_cast<size_t>(id)]=node;
        const int source=first(kind==FlowKind::Body?FlowKind::Bridge:FlowKind::String);
        const int output=first(FlowKind::Output);
        if(kind==FlowKind::Body){
            connect(source,id);connect(id,output);
        }else connect(id,source);
        return id;
    }
    bool removeNode(int id) noexcept {
        // Mandatory core nodes (0-5) are always preserved in v2.
        if(id<6||id>=nodeCount)return false;
        for(int i=0;i<linkCount;){
            const auto edge=links[static_cast<size_t>(i)];
            if(edge.from==id||edge.to==id){
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
};
inline FlowGraph defaultFlowGraph() noexcept {
    FlowGraph graph;
    graph.nodeCount=6;
    auto& n=graph.nodes;
    n[0].kind=FlowKind::Exciter;
    n[0].material=Material::Nylon;
    n[0].x=.02f;n[0].y=.04f;
    n[0].position=.19f;
    n[1].kind=FlowKind::String;
    n[1].material=Material::Steel;
    n[1].x=.36f;n[1].y=.04f;
    n[2].kind=FlowKind::Bridge;
    n[2].material=Material::Wood;
    n[2].x=.70f;n[2].y=.04f;
    n[3].kind=FlowKind::Body;
    n[3].material=Material::Wood;
    n[3].x=.68f;n[3].y=.46f;
    n[4].kind=FlowKind::Damper;
    n[4].material=Material::Felt;
    n[4].x=.18f;n[4].y=.50f;n[4].position=.07f;
    n[5].kind=FlowKind::Output;
    n[5].material=Material::Wood;
    n[5].x=.68f;n[5].y=.84f;
    graph.connect(0,1);
    graph.connect(1,2);
    graph.connect(2,3);
    graph.connect(2,5); // direct dry bridge radiation
    graph.connect(3,5);
    graph.connect(4,1);
    return graph;
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
