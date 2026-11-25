// Universal material roles v5 editor; first-arrival time Y, physical entry X.
// Roles are freely repeatable. Source energy V, physical position M.
void NativeEditor::syncPhysicalBinding() noexcept {
    const auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    const int target=(synthWorkspace_==3&&track>=0&&!project.trackIsDrums(track))
        ?track:-1;
    if(target==physicalConfiguredTrack_&&!physicalDirty_)return;
    AudioEngine::instance().setPhysicalExperiment(target,physicalBlueprint_);
    physicalConfiguredTrack_=target;physicalDirty_=false;
}
NativeEditor::Rect NativeEditor::flowTemplateRect(int index) const noexcept {
    const auto b=bodyRect(),tab=synthTabRect(0);
    const float gap=std::clamp(b.w*.014f,9.f,13.f);
    const float y=tab.y+tab.h+gap-physicalScrollY_;
    const float w=std::max(30.f,(b.w-24.f-7*5.f)/8.f);
    return {b.x+12.f+index*(w+5.f),y+63.f,w,44.f};
}
NativeEditor::Rect NativeEditor::flowActionRect(int index) const noexcept {
    const auto b=bodyRect(),tab=synthTabRect(0);
    const float gap=std::clamp(b.w*.014f,9.f,13.f);
    const float top=tab.y+tab.h+gap-physicalScrollY_;
    const float w=std::max(38.f,(b.w-24.f-5*5.f)/6.f);
    return {b.x+12.f+(index%6)*(w+5.f),
            top+63.f+(index/6)*49.f,w,44.f};
}
NativeEditor::Rect NativeEditor::flowTimeRect() const noexcept {
    const auto b=bodyRect(),tab=synthTabRect(0);
    const float gap=std::clamp(b.w*.014f,9.f,13.f);
    return {b.x+12.f,tab.y+tab.h+gap-physicalScrollY_+167.f,
            b.w-24.f,48.f};
}
NativeEditor::Rect NativeEditor::flowCanvasRect() const noexcept {
    const auto b=bodyRect(),tab=synthTabRect(0);
    const float gap=std::clamp(b.w*.014f,9.f,13.f);
    return {b.x+12.f,tab.y+tab.h+gap-physicalScrollY_+225.f,
            std::max(150.f,b.w-24.f),614.f};
}
NativeEditor::Rect NativeEditor::flowNodeRect(int index) const noexcept {
    const auto graph=flowCanvasRect();
    if(index<0||index>=physicalBlueprint_.graph.nodeCount)return {};
    const auto& node=physicalBlueprint_.graph.nodes[static_cast<size_t>(index)];
    const float w=std::clamp(graph.w*.275f,77.f,162.f),h=72.f;
    return {graph.x+12.f+node.x*std::max(1.f,graph.w-w-24.f),
            graph.y+12.f+node.y*(graph.h-h-24.f),w,h};
}
NativeEditor::Rect NativeEditor::flowPropertyRect(int index) const noexcept {
    const auto canvas=flowCanvasRect();
    return {canvas.x,canvas.y+canvas.h+70.f+index*73.f,canvas.w,66.f};
}
bool NativeEditor::flowTemplate(int i) noexcept {
    if(i<0||i>1)return false;
    physicalBlueprint_.graph=i==0?physical::universalStarterGraph():
        physical::universalEmptyGraph();
    flowSelectedNode_=physicalBlueprint_.graph.nodeCount>2?2:0;
    flowLinkMode_=0;flowLinkSource_=-1;flowSelectedLink_=-1;flowError_=false;
    physicalDirty_=true;return true;
}
bool NativeEditor::editFlowTime(float x) noexcept {
    const auto rr=flowTimeRect();
    const float t=std::clamp((x-rr.x-14.f)/std::max(1.f,rr.w-28.f),0.f,1.f);
    physicalBlueprint_.graph.spanMs=3.f+79.f*t;
    physicalDirty_=true;return true;
}
int NativeEditor::flowPropertyCount() const noexcept {
    const auto& g=physicalBlueprint_.graph;
    if(flowSelectedLink_>=0&&flowSelectedLink_<g.linkCount)return 6;
    if(flowSelectedNode_>=0&&flowSelectedNode_<g.nodeCount&&
       g.nodes[flowSelectedNode_].kind==physical::FlowKind::Universal){
        switch(g.nodes[flowSelectedNode_].role){
        case physical::PhysicalRole::Source:return 8;
        case physical::PhysicalRole::Output:return 3;
        default:return 21;
        }
    }
    if(flowSelectedNode_<0||flowSelectedNode_>=g.nodeCount)return 0;
    switch(g.nodes[flowSelectedNode_].kind){
    case physical::FlowKind::Exciter:return 4;
    case physical::FlowKind::String:return 5;
    case physical::FlowKind::Membrane:return 5;
    case physical::FlowKind::Plate:return 5;
    case physical::FlowKind::Bar:return 5;
    case physical::FlowKind::AirColumn:return 5;
    case physical::FlowKind::Body:return 6;
    case physical::FlowKind::Bridge:return 2;
    case physical::FlowKind::Damper:return 4;
    case physical::FlowKind::Router:return 2;
    case physical::FlowKind::Reed:return 4;
    case physical::FlowKind::Edge:return 4;
    case physical::FlowKind::Output:return 0;
    case physical::FlowKind::Universal:return 0;
    }
    return 0;
}
const char* NativeEditor::flowPropertyLabel(int i) const noexcept {
    const auto& g=physicalBlueprint_.graph;
    if(flowSelectedLink_>=0&&flowSelectedLink_<g.linkCount){
        constexpr const char* l[]={
            "ENTRY X (TARGET SURFACE)","M ASSIGNED","M MIN POSITION",
            "M MAX POSITION","INTERACTION WIDTH","TRANSFER LAW"
        };
        return i>=0&&i<6?l[i]:"";
    }
    if(flowSelectedNode_>=0&&flowSelectedNode_<g.nodeCount&&
       g.nodes[flowSelectedNode_].kind==physical::FlowKind::Universal){
        const auto role=g.nodes[flowSelectedNode_].role;
        if(role==physical::PhysicalRole::Source){
            constexpr const char* l[]={
                "HUMAN ENERGY SOURCE","MOTION / DELIVERY","MOTION DURATION",
                "PRESSURE / FORCE RISE","DIRECTION","M ASSIGNED",
                "M MIN POSITION","M MAX POSITION"
            };
            return i>=0&&i<8?l[i]:"";
        }
        if(role==physical::PhysicalRole::Output){
            constexpr const char* l[]={
                "M ASSIGNED","M MIN POSITION","M MAX POSITION"
            };
            return i>=0&&i<3?l[i]:"";
        }
        constexpr const char* l[]={
            "PHYSICAL MATERIAL","CONTINUUM / STRUCTURE",
            "LENGTH (METERS)","WIDTH (METERS)","DEPTH (METERS)",
            "WALL THICKNESS","CURVATURE","TAPER","OPEN BOUNDARY",
            "END FIXING","PROFILE 0%","PROFILE 33%",
            "PROFILE 67%","PROFILE 100%","TENSION",
            "STRUCTURAL LOSS","CONTACT COUPLING","INTERACTION LAW",
            "M ASSIGNED","M MIN POSITION","M MAX POSITION"
        };
        return i>=0&&i<21?l[i]:"";
    }
    if(flowSelectedNode_<0||flowSelectedNode_>=g.nodeCount)return "";
    using physical::FlowKind;
    switch(g.nodes[flowSelectedNode_].kind){
    case FlowKind::Exciter:{
        constexpr const char* l[]={"EXCITER MATERIAL","ACTION","CONTACT POSITION","IMPACT HARDNESS"};
        return i>=0&&i<4?l[i]:"";
    }
    case FlowKind::String:{
        constexpr const char* l[]={"STRING MATERIAL","TENSION","DIAMETER","LENGTH","STRUCTURAL LOSS"};
        return i>=0&&i<5?l[i]:"";
    }
    case FlowKind::Membrane:{
        constexpr const char* l[]={"MEMBRANE MATERIAL","TENSION","THICKNESS","DIAMETER / SIZE","STRUCTURAL LOSS"};
        return i>=0&&i<5?l[i]:"";
    }
    case FlowKind::Plate:
    case FlowKind::Bar:{
        constexpr const char* l[]={"MATERIAL","THICKNESS","LENGTH / SIZE","STRUCTURAL LOSS","CONTACT COUPLING"};
        return i>=0&&i<5?l[i]:"";
    }
    case FlowKind::AirColumn:{
        constexpr const char* l[]={"TUBE MATERIAL","OPEN / CLOSED","TUBE LENGTH","WALL DAMPING","CONTACT COUPLING"};
        return i>=0&&i<5?l[i]:"";
    }
    case FlowKind::Body:{
        constexpr const char* l[]={"BODY MATERIAL","BODY SHAPE","BODY SIZE","WALL THICKNESS","STRUCTURAL DAMPING","COUPLING"};
        return i>=0&&i<6?l[i]:"";
    }
    case FlowKind::Bridge:{
        constexpr const char* l[]={"COUPLER MATERIAL","COUPLING STRENGTH"};
        return i>=0&&i<2?l[i]:"";
    }
    case FlowKind::Damper:{
        constexpr const char* l[]={"CONTACT MATERIAL","CONTACT POSITION","M DEPTH","REST PRESSURE"};
        return i>=0&&i<4?l[i]:"";
    }
    case FlowKind::Router:{
        constexpr const char* l[]={"OUTLET REST X","M OUTLET MOVEMENT"};
        return i>=0&&i<2?l[i]:"";
    }
    case FlowKind::Reed:{
        constexpr const char* l[]={"REED MATERIAL","REED STIFFNESS","REED SIZE","AIR COUPLING"};
        return i>=0&&i<4?l[i]:"";
    }
    case FlowKind::Edge:{
        constexpr const char* l[]={"EDGE MATERIAL","JET GAP","EDGE OFFSET","AIR COUPLING"};
        return i>=0&&i<4?l[i]:"";
    }
    case FlowKind::Output:return "";
    case FlowKind::Universal:return "";
    }
    return "";
}
bool NativeEditor::flowAction(int index) noexcept {
    using physical::PhysicalRole;
    auto& g=physicalBlueprint_.graph;
    if(index>=0&&index<5){
        const PhysicalRole role=static_cast<PhysicalRole>(index);
        const int id=physical::addPhysicalRole(g,role);
        if(id<0)return false;
        // New roles can appear anywhere along the causally ordered grid.
        // Linking remains deliberate: no hidden sound-path assignments.
        flowSelectedNode_=id;flowSelectedLink_=-1;
        flowError_=false;physicalDirty_=true;return true;
    }
    if(index==5||index==6){
        const int m=index==5?1:2;
        flowLinkMode_=flowLinkMode_==m?0:m;
        flowLinkSource_=-1;flowError_=false;
        return false;
    }
    if(index==7){
        if(flowSelectedLink_>=0&&flowSelectedLink_<g.linkCount){
            const auto link=g.links[static_cast<size_t>(flowSelectedLink_)];
            const bool changed=g.disconnect(link.from,link.to);
            flowSelectedLink_=-1;
            if(changed)physicalDirty_=true;
            return changed;
        }
        if(!g.removeNode(flowSelectedNode_))return false;
        flowSelectedNode_=std::max(0,std::min(flowSelectedNode_,int(g.nodeCount)-1));
        flowLinkSource_=-1;flowSelectedLink_=-1;physicalDirty_=true;return true;
    }
    if(index==8){
        g=physical::universalEmptyGraph();
        flowSelectedNode_=0;flowLinkSource_=-1;flowSelectedLink_=-1;
        flowLinkMode_=0;flowError_=false;physicalDirty_=true;return true;
    }
    if(index==9){
        g=physical::universalStarterGraph();
        flowSelectedNode_=0;flowLinkSource_=-1;flowSelectedLink_=-1;
        flowLinkMode_=0;flowError_=false;physicalDirty_=true;return true;
    }
    return false;
}
bool NativeEditor::flowNodeTouch(int index) noexcept {
    auto& g=physicalBlueprint_.graph;
    if(index<0||index>=g.nodeCount)return false;
    flowSelectedNode_=index;
    if(flowLinkMode_==0){flowSelectedLink_=-1;return false;}
    if(flowLinkSource_<0){
        flowLinkSource_=index;flowError_=false;return false;
    }
    const bool adding=flowLinkMode_==1;
    const bool changed=adding?
        g.connect(flowLinkSource_,index):
        g.disconnect(flowLinkSource_,index);
    flowError_=!changed;flowLinkMode_=0;flowLinkSource_=-1;
    if(changed){
        physicalDirty_=true;
        flowSelectedLink_=-1;
        if(adding&&g.linkCount>0)flowSelectedLink_=g.linkCount-1;
    }
    return changed;
}
bool NativeEditor::flowLinkTouch(int index) noexcept {
    const auto& g=physicalBlueprint_.graph;
    if(index<0||index>=g.linkCount)return false;
    flowSelectedLink_=index;flowLinkMode_=0;flowLinkSource_=-1;flowError_=false;
    return false;
}
bool NativeEditor::editFlowProperty(int i,float x,bool first) noexcept {
    using physical::FlowKind;
    using physical::Excitation;
    auto& g=physicalBlueprint_.graph;
    if(i<0||i>=flowPropertyCount())return false;
    const auto rr=flowPropertyRect(i);
    const float t=std::clamp((x-rr.x-14.f)/std::max(1.f,rr.w-28.f),0.f,1.f);
    if(flowSelectedLink_>=0&&flowSelectedLink_<g.linkCount){
        auto& link=g.links[static_cast<size_t>(flowSelectedLink_)];
        if(i==0)link.position=t;
        else if(i==1){if(!first)return false;link.modEnabled=!link.modEnabled;}
        else if(i==2)link.modMin=t;
        else if(i==3)link.modMax=t;
        else if(i==4)link.aperture=.035f+.965f*t;
        else if(i==5){
            if(!first)return false;
            link.law=static_cast<physical::ContactLaw>(
                (int(link.law)+1)%4);
        }
        physicalDirty_=true;return true;
    }
    if(flowSelectedNode_>=0&&flowSelectedNode_<g.nodeCount&&
       g.nodes[flowSelectedNode_].kind==FlowKind::Universal){
        auto& n=g.nodes[flowSelectedNode_];
        const bool src=n.role==physical::PhysicalRole::Source;
        const bool out=n.role==physical::PhysicalRole::Output;
        if(src){
            if(i==0){if(!first)return false;n.driver=static_cast<physical::Driver>((int(n.driver)+1)%4);}
            else if(i==1){if(!first)return false;n.gesture=static_cast<physical::Gesture>((int(n.gesture)+1)%3);}
            else if(i==2)n.sourceDurationMs=.4f+499.6f*t;
            else if(i==3)n.sourceRiseMs=.4f+249.6f*t;
            else if(i==4)n.sourceDirection=t;
            else if(i==5){if(!first)return false;n.movingEnabled=!n.movingEnabled;}
            else if(i==6)n.movingMin=t;
            else if(i==7)n.movingMax=t;
        }else if(out){
            if(i==0){if(!first)return false;n.movingEnabled=!n.movingEnabled;}
            else if(i==1)n.movingMin=t;
            else if(i==2)n.movingMax=t;
        }else{
            auto& form=n.form;
            if(i==0)return false; // material is selected via dropdown
            else if(i==1){
                if(!first)return false;
                form.continuum=static_cast<physical::Continuum>(
                    (int(form.continuum)+1)%4);
            }else if(i==2)form.lengthMeters=.025f+1.975f*t;
            else if(i==3)form.widthMeters=.0005f*std::pow(1000.f,t);
            else if(i==4)form.depthMeters=.0002f*std::pow(1000.f,t);
            else if(i==5)form.wallMm=.2f+19.8f*t;
            else if(i==6)form.curvature=2.f*t-1.f;
            else if(i==7)form.taper=2.f*t-1.f;
            else if(i==8)form.opening=t;
            else if(i==9)form.fixing=t;
            else if(i>=10&&i<=13)
                form.stations[static_cast<size_t>(i-10)]=.25f+2.75f*t;
            else if(i==14)n.tensionNewtons=1.f+399.f*t;
            else if(i==15)n.damping=.15f+7.85f*t;
            else if(i==16)n.coupling=.02f+.98f*t;
            else if(i==17){
                if(!first)return false;
                n.law=static_cast<physical::ContactLaw>((int(n.law)+1)%4);
            }else if(i==18){if(!first)return false;n.movingEnabled=!n.movingEnabled;}
            else if(i==19)n.movingMin=t;
            else if(i==20)n.movingMax=t;
        }
        flowError_=false;physicalDirty_=true;return true;
    }
    if(flowSelectedNode_<0||flowSelectedNode_>=g.nodeCount)return false;
    auto& n=g.nodes[flowSelectedNode_];
    switch(n.kind){
    case FlowKind::Exciter:
        if(i==0)return false;
        if(i==1){
            if(!first)return false;
            n.excitation=static_cast<Excitation>((int(n.excitation)+1)%4);
        }else if(i==2)n.position=.04f+.92f*t;
        else n.hardness=.02f+.98f*t;
        break;
    case FlowKind::String:
        if(i==0)return false;
        if(i==1)n.tensionNewtons=10.f+390.f*t;
        else if(i==2)n.diameterMm=.12f+2.88f*t;
        else if(i==3)n.size=.25f+2.75f*t;
        else if(i==4)n.damping=.15f+7.85f*t;
        break;
    case FlowKind::Membrane:
        if(i==0)return false;
        if(i==1)n.tensionNewtons=10.f+390.f*t;
        else if(i==2)n.thicknessMm=.30f+19.7f*t;
        else if(i==3)n.size=.25f+2.75f*t;
        else n.damping=.15f+7.85f*t;
        break;
    case FlowKind::Plate:
    case FlowKind::Bar:
        if(i==0)return false;
        if(i==1)n.thicknessMm=.30f+19.7f*t;
        else if(i==2)n.size=.25f+2.75f*t;
        else if(i==3)n.damping=.15f+7.85f*t;
        else n.coupling=.02f+.98f*t;
        break;
    case FlowKind::AirColumn:
        if(i==0||i==1)return false;
        if(i==2)n.size=.25f+2.75f*t;
        else if(i==3)n.damping=.15f+7.85f*t;
        else n.coupling=.02f+.98f*t;
        break;
    case FlowKind::Body:
        if(i<=1)return false;
        if(i==2)n.size=.25f+2.75f*t;
        else if(i==3)n.thicknessMm=.30f+19.7f*t;
        else if(i==4)n.damping=.15f+7.85f*t;
        else n.coupling=.02f+.98f*t;
        break;
    case FlowKind::Bridge:
        if(i==0)return false;
        n.coupling=.02f+.98f*t;break;
    case FlowKind::Damper:
        if(i==0)return false;
        if(i==1)n.position=.01f+.98f*t;
        else if(i==2)n.coupling=.02f+.98f*t;
        else n.restPressure=t;
        break;
    case FlowKind::Router:
        if(i==0)n.position=t;
        else n.motion=2.f*t-1.f;
        break;
    case FlowKind::Reed:
        if(i==0)return false;
        if(i==1)n.hardness=t;
        else if(i==2)n.size=.25f+2.75f*t;
        else n.coupling=.02f+.98f*t;
        break;
    case FlowKind::Edge:
        if(i==0)return false;
        if(i==1)n.thicknessMm=.30f+19.7f*t;
        else if(i==2)n.position=t;
        else n.coupling=.02f+.98f*t;
        break;
    case FlowKind::Output:return false;
    case FlowKind::Universal:return false;
    }
    flowError_=false;physicalDirty_=true;
    return true;
}
// Present physical measurements, not waveform or envelope terminology.
struct FlowField {
    char label[48]{};
    float position{.5f};
    bool picker{false};
};
static FlowField physicalField(const physical::FlowNode& n,int i) {
    using physical::FlowKind;
    FlowField f;
    auto fmt=[&](const char* format,float value){
        std::snprintf(f.label,sizeof(f.label),format,value);
    };
    if(n.kind==FlowKind::Universal){
        const bool source=n.role==physical::PhysicalRole::Source;
        const bool output=n.role==physical::PhysicalRole::Output;
        if(source){
            if(i==0){f.picker=true;std::snprintf(f.label,sizeof(f.label),"%s",physical::driverName(n.driver));}
            else if(i==1){f.picker=true;std::snprintf(f.label,sizeof(f.label),"%s",physical::gestureName(n.gesture));}
            else if(i==2){f.position=(n.sourceDurationMs-.4f)/499.6f;fmt("%.1f ms",n.sourceDurationMs);}
            else if(i==3){f.position=(n.sourceRiseMs-.4f)/249.6f;fmt("%.1f ms",n.sourceRiseMs);}
            else if(i==4){f.position=n.sourceDirection;fmt("%.0f%%",n.sourceDirection*100.f);}
            else if(i==5){f.picker=true;std::snprintf(f.label,sizeof(f.label),"%s",n.movingEnabled?"Assigned":"Unassigned");}
            else if(i==6){f.position=n.movingMin;fmt("%.0f%%",n.movingMin*100.f);}
            else if(i==7){f.position=n.movingMax;fmt("%.0f%%",n.movingMax*100.f);}
        }else if(output){
            if(i==0){f.picker=true;std::snprintf(f.label,sizeof(f.label),"%s",n.movingEnabled?"Assigned":"Unassigned");}
            else if(i==1){f.position=n.movingMin;fmt("%.0f%%",n.movingMin*100.f);}
            else if(i==2){f.position=n.movingMax;fmt("%.0f%%",n.movingMax*100.f);}
        }else{
            const auto& g=n.form;
            if(i==0){f.picker=true;std::snprintf(f.label,sizeof(f.label),"%s",physical::materialName(n.material));}
            else if(i==1){f.picker=true;std::snprintf(f.label,sizeof(f.label),"%s",physical::continuumName(g.continuum));}
            else if(i==2){f.position=(g.lengthMeters-.025f)/1.975f;fmt("%.3f m",g.lengthMeters);}
            else if(i==3){f.position=std::log(std::max(.0005f,g.widthMeters)/.0005f)/std::log(1000.f);fmt("%.4f m",g.widthMeters);}
            else if(i==4){f.position=std::log(std::max(.0002f,g.depthMeters)/.0002f)/std::log(1000.f);fmt("%.4f m",g.depthMeters);}
            else if(i==5){f.position=(g.wallMm-.2f)/19.8f;fmt("%.1f mm",g.wallMm);}
            else if(i==6){f.position=(g.curvature+1.f)*.5f;fmt("%+.2f",g.curvature);}
            else if(i==7){f.position=(g.taper+1.f)*.5f;fmt("%+.2f",g.taper);}
            else if(i==8){f.position=g.opening;fmt("%.0f%%",g.opening*100.f);}
            else if(i==9){f.position=g.fixing;fmt("%.0f%%",g.fixing*100.f);}
            else if(i>=10&&i<=13){const float v=g.stations[static_cast<size_t>(i-10)];f.position=(v-.25f)/2.75f;fmt("%.2f x",v);}
            else if(i==14){f.position=(n.tensionNewtons-1.f)/399.f;fmt("%.0f N",n.tensionNewtons);}
            else if(i==15){f.position=(n.damping-.15f)/7.85f;fmt("%.2f x",n.damping);}
            else if(i==16){f.position=(n.coupling-.02f)/.98f;fmt("%.0f%%",n.coupling*100.f);}
            else if(i==17){f.picker=true;std::snprintf(f.label,sizeof(f.label),"%s",physical::contactLawName(n.law));}
            else if(i==18){f.picker=true;std::snprintf(f.label,sizeof(f.label),"%s",n.movingEnabled?"Assigned":"Unassigned");}
            else if(i==19){f.position=n.movingMin;fmt("%.0f%%",n.movingMin*100.f);}
            else if(i==20){f.position=n.movingMax;fmt("%.0f%%",n.movingMax*100.f);}
        }
        return f;
    }
    if(i==0&&n.kind!=FlowKind::Output&&n.kind!=FlowKind::Router){
        f.picker=true;
        std::snprintf(f.label,sizeof(f.label),"%s",physical::materialName(n.material));
        return f;
    }
    switch(n.kind){
    case FlowKind::Exciter:
        if(i==1){
            f.picker=true;
            constexpr const char* names[]={"Pluck","Strike","Blow","Scrape"};
            std::snprintf(f.label,sizeof(f.label),"%s",names[int(n.excitation)%4]);
        }else if(i==2){f.position=(n.position-.04f)/.92f;fmt("%.0f%%",n.position*100.f);}
        else if(i==3){f.position=(n.hardness-.02f)/.98f;fmt("%.0f%%",n.hardness*100.f);}
        break;
    case FlowKind::String:
        if(i==1){f.position=(n.tensionNewtons-10.f)/390.f;fmt("%.0f N",n.tensionNewtons);}
        else if(i==2){f.position=(n.diameterMm-.12f)/2.88f;fmt("%.2f mm",n.diameterMm);}
        else if(i==3){f.position=(n.size-.25f)/2.75f;fmt("%.2f x",n.size);}
        else if(i==4){f.position=(n.damping-.15f)/7.85f;fmt("%.2f x",n.damping);}
        break;
    case FlowKind::Membrane:
        if(i==1){f.position=(n.tensionNewtons-10.f)/390.f;fmt("%.0f N",n.tensionNewtons);}
        else if(i==2){f.position=(n.thicknessMm-.30f)/19.7f;fmt("%.2f mm",n.thicknessMm);}
        else if(i==3){f.position=(n.size-.25f)/2.75f;fmt("%.2f x",n.size);}
        else if(i==4){f.position=(n.damping-.15f)/7.85f;fmt("%.2f x",n.damping);}
        break;
    case FlowKind::Plate:
    case FlowKind::Bar:
        if(i==1){f.position=(n.thicknessMm-.30f)/19.7f;fmt("%.2f mm",n.thicknessMm);}
        else if(i==2){f.position=(n.size-.25f)/2.75f;fmt("%.2f x",n.size);}
        else if(i==3){f.position=(n.damping-.15f)/7.85f;fmt("%.2f x",n.damping);}
        else if(i==4){f.position=(n.coupling-.02f)/.98f;fmt("%.0f%%",n.coupling*100.f);}
        break;
    case FlowKind::AirColumn:
        if(i==1){
            f.picker=true;
            std::snprintf(f.label,sizeof(f.label),"%s",
                n.geometry==physical::Geometry::HollowCylinder?"Closed":"Open");
        }else if(i==2){f.position=(n.size-.25f)/2.75f;fmt("%.2f x",n.size);}
        else if(i==3){f.position=(n.damping-.15f)/7.85f;fmt("%.2f x",n.damping);}
        else if(i==4){f.position=(n.coupling-.02f)/.98f;fmt("%.0f%%",n.coupling*100.f);}
        break;
    case FlowKind::Body:
        if(i==1){
            f.picker=true;
            std::snprintf(f.label,sizeof(f.label),"%s",physical::geometryName(n.geometry));
        }else if(i==2){f.position=(n.size-.25f)/2.75f;fmt("%.2f x",n.size);}
        else if(i==3){f.position=(n.thicknessMm-.30f)/19.7f;fmt("%.2f mm",n.thicknessMm);}
        else if(i==4){f.position=(n.damping-.15f)/7.85f;fmt("%.2f x",n.damping);}
        else if(i==5){f.position=(n.coupling-.02f)/.98f;fmt("%.0f%%",n.coupling*100.f);}
        break;
    case FlowKind::Bridge:
        if(i==1){f.position=(n.coupling-.02f)/.98f;fmt("%.0f%%",n.coupling*100.f);}
        break;
    case FlowKind::Damper:
        if(i==1){f.position=(n.position-.01f)/.98f;fmt("%.0f%%",n.position*100.f);}
        else if(i==2){f.position=(n.coupling-.02f)/.98f;fmt("%.0f%%",n.coupling*100.f);}
        else if(i==3){f.position=n.restPressure;fmt("%.0f%%",n.restPressure*100.f);}
        break;
    case FlowKind::Router:
        if(i==0){f.position=n.position;fmt("%.0f%%",n.position*100.f);}
        else if(i==1){f.position=(n.motion+1.f)*.5f;fmt("%+.0f%%",n.motion*100.f);}
        break;
    case FlowKind::Reed:
        if(i==1){f.position=n.hardness;fmt("%.0f%%",n.hardness*100.f);}
        else if(i==2){f.position=(n.size-.25f)/2.75f;fmt("%.2f x",n.size);}
        else if(i==3){f.position=(n.coupling-.02f)/.98f;fmt("%.0f%%",n.coupling*100.f);}
        break;
    case FlowKind::Edge:
        if(i==1){f.position=(n.thicknessMm-.30f)/19.7f;fmt("%.2f mm",n.thicknessMm);}
        else if(i==2){f.position=n.position;fmt("%.0f%%",n.position*100.f);}
        else if(i==3){f.position=(n.coupling-.02f)/.98f;fmt("%.0f%%",n.coupling*100.f);}
        break;
    case FlowKind::Output:break;
    case FlowKind::Universal:break;
    }
    return f;
}
void NativeEditor::drawPhysical() const noexcept {
    const auto& g=physicalBlueprint_.graph;
    const auto b=bodyRect(),tab=synthTabRect(0),canvas=flowCanvasRect();
    const float gap=std::clamp(b.w*.014f,9.f,13.f);
    const float top=tab.y+tab.h+gap-physicalScrollY_;
    const Rect clip{b.x,tab.y+tab.h+gap,b.w,
        std::max(0.f,b.y+b.h-(tab.y+tab.h+gap))};
    auto& ov=NativeOverlay::instance();
    ov.setClip({clip.x,clip.y,clip.w,clip.h});
    ov.addRect({b.x+12.f,top+7.f,b.w-24.f,48.f},overlayColor(kRollBg));
    ov.addText("PHYSICAL BUILDER   /   Y = ENERGY ARRIVAL",
        b.x+20.f,top+15.f,.88f,overlayColor(kCyan));
    ov.addText("Human source first; material + geometry form the instrument",
        b.x+20.f,top+36.f,.73f,overlayColor(kMuted));
    constexpr const char* actions[]={
        "+SOURCE","+TRANSFER","+INTERACT","+RESONATE","+OUTPUT","LINK",
        "UNLINK","DELETE","CLEAR","EXAMPLE"
    };
    for(int i=0;i<10;++i){
        const auto rr=flowActionRect(i);
        const bool selected=(i==5&&flowLinkMode_==1)||(i==6&&flowLinkMode_==2);
        ov.addRect({rr.x,rr.y,rr.w,rr.h},overlayColor(selected?kCyan:kButton));
        ov.addTextCentered(actions[i],{rr.x+1,rr.y,rr.w-2,rr.h},.67f,
            overlayColor(selected?kBg:kWhite));
    }
    {
        const auto rr=flowTimeRect();
        ov.addRect({rr.x,rr.y,rr.w,rr.h},overlayColor(kPanel));
        char name[72]{};
        std::snprintf(name,sizeof(name),"FIRST-ARRIVAL SPAN   %.0f ms (rings continue)",g.spanMs);
        ov.addText(name,rr.x+12.f,rr.y+6.f,.82f,overlayColor(kWhite));
        const float t=std::clamp((g.spanMs-3.f)/79.f,0.f,1.f);
        ov.addRect({rr.x+12.f,rr.y+34.f,rr.w-24.f,5.f},overlayColor(kRollBg));
        ov.addRect({rr.x+12.f,rr.y+34.f,(rr.w-24.f)*t,5.f},
            overlayColor(kCyan));
    }
    ov.addRect({canvas.x,canvas.y,canvas.w,canvas.h},overlayColor(kRollBg));
    const float timeX=canvas.x+9.f;
    ov.addLine(timeX,canvas.y+25.f,timeX,canvas.y+canvas.h-25.f,2.f,
        overlayColor(kCyan,.55f));
    for(int j=0;j<=4;++j){
        const float yy=canvas.y+20.f+j*(canvas.h-40.f)*.25f;
        char tick[24]{};
        std::snprintf(tick,sizeof(tick),"%.0f",g.spanMs*j*.25f);
        ov.addText(tick,canvas.x+14.f,yy+4.f,.66f,overlayColor(kMuted));
    }
    for(int i=0;i<g.linkCount;++i){
        const auto link=g.links[static_cast<size_t>(i)];
        if(link.from>=g.nodeCount||link.to>=g.nodeCount)continue;
        const auto a=flowNodeRect(link.from),z=flowNodeRect(link.to);
        const float x0=a.x+a.w*.5f,y0=a.y+a.h;
        const float x1=z.x+z.w*std::clamp(link.position,0.f,1.f),y1=z.y;
        const bool selected=i==flowSelectedLink_;
        ov.addLine(x0,y0,x1,y1,selected?5.f:3.0f,
            overlayColor(selected?kOrange:kCyan,selected?1.f:.75f));
        ov.addFilledCircle(x1,y1,selected?7.f:5.f,
            overlayColor(selected?kOrange:kCyan));
    }
    for(int i=0;i<g.nodeCount;++i){
        const auto& node=g.nodes[static_cast<size_t>(i)];
        const auto rr=flowNodeRect(i);
        const bool selected=i==flowSelectedNode_;
        const bool connecting=i==flowLinkSource_;
        ov.addRect({rr.x-2,rr.y-2,rr.w+4,rr.h+4},
            overlayColor(connecting?kOrange:selected?kCyan:kTrack));
        ov.addRect({rr.x+2,rr.y+2,rr.w-4,rr.h-4},overlayColor(kButton));
        const bool universal=node.kind==physical::FlowKind::Universal;
        ov.addTextCentered(universal?physical::roleName(node.role):
            physical::flowKindName(node.kind),
            {rr.x+2,rr.y+6.f,rr.w-4,25.f},.63f,
            overlayColor(selected?kCyan:kWhite));
        const char* detail=universal?
            (node.role==physical::PhysicalRole::Source?
                physical::driverName(node.driver):
             node.role==physical::PhysicalRole::Output?"Acoustic radiation":
                physical::continuumName(node.form.continuum)):
            (node.kind==physical::FlowKind::Output?
                "Audio":physical::materialName(node.material));
        ov.addTextCentered(detail,
            {rr.x+2,rr.y+33.f,rr.w-4,28.f},.67f,overlayColor(kMuted));
    }
    if(flowLinkMode_||flowError_){
        ov.addText(flowError_?"Link invalid: source must precede target":
           flowLinkSource_<0?"Tap the earlier source object":"Tap a later destination object",
           canvas.x+45.f,canvas.y+canvas.h-32.f,.82f,
           overlayColor(kOrange));
    }
    const physical::FlowNode* chosen=
        flowSelectedNode_>=0&&flowSelectedNode_<g.nodeCount?
        &g.nodes[static_cast<size_t>(flowSelectedNode_)]:nullptr;
    const float titleY=canvas.y+canvas.h+13.f;
    ov.addRect({canvas.x,titleY,canvas.w,49.f},overlayColor(kPanel));
    const bool linkSelected=flowSelectedLink_>=0&&
        flowSelectedLink_<g.linkCount;
    char selectedLabel[96]{};
    if(linkSelected){
        const auto& link=g.links[static_cast<size_t>(flowSelectedLink_)];
        const auto& from=g.nodes[link.from];
        const auto& to=g.nodes[link.to];
        std::snprintf(selectedLabel,sizeof(selectedLabel),"LINK: %s > %s",
            from.kind==physical::FlowKind::Universal?
                physical::roleName(from.role):physical::flowKindName(from.kind),
            to.kind==physical::FlowKind::Universal?
                physical::roleName(to.role):physical::flowKindName(to.kind));
    }
    ov.addText(linkSelected?selectedLabel:
        chosen?(chosen->kind==physical::FlowKind::Universal?
            physical::roleName(chosen->role):physical::flowKindName(chosen->kind)):
            "SELECT OBJECT",
        canvas.x+12.f,titleY+12.f,.90f,overlayColor(kCyan));
    if(!linkSelected&&chosen&&chosen->kind==physical::FlowKind::Damper)
        ov.addText("REST preloads contact; M moves its physical link",
            canvas.x+120.f,titleY+17.f,.66f,overlayColor(kMuted));
    for(int i=0;i<flowPropertyCount();++i){
        const auto rr=flowPropertyRect(i);
        if(rr.y+rr.h<clip.y||rr.y>clip.y+clip.h)continue;
        ov.addRect({rr.x,rr.y,rr.w,rr.h},overlayColor(kButton));
        ov.addText(flowPropertyLabel(i),rr.x+12.f,rr.y+7.f,.78f,
            overlayColor(kMuted));
        FlowField field{};
        if(linkSelected){
            const auto& link=g.links[static_cast<size_t>(flowSelectedLink_)];
            if(i==0){
                field.position=link.position;
                std::snprintf(field.label,sizeof(field.label),"%.0f%%",link.position*100.f);
            }else if(i==1){
                field.picker=true;
                std::snprintf(field.label,sizeof(field.label),"%s",
                    link.modEnabled?"Assigned":"Unassigned");
            }else if(i==2){
                field.position=link.modMin;
                std::snprintf(field.label,sizeof(field.label),"%.0f%%",link.modMin*100.f);
            }else if(i==3){
                field.position=link.modMax;
                std::snprintf(field.label,sizeof(field.label),"%.0f%%",link.modMax*100.f);
            }else if(i==4){
                field.position=(link.aperture-.035f)/.965f;
                std::snprintf(field.label,sizeof(field.label),"%.0f%%",link.aperture*100.f);
            }else{
                field.picker=true;
                std::snprintf(field.label,sizeof(field.label),"%s",
                    physical::contactLawName(link.law));
            }
        }else if(chosen)field=physicalField(*chosen,i);
        if(field.picker){
            ov.addTextCentered(field.label,{rr.x+10.f,rr.y+31.f,rr.w-20.f,30.f},
                .94f,overlayColor(kWhite));
        }else{
            ov.addText(field.label,rr.x+rr.w-107.f,rr.y+7.f,.80f,
                overlayColor(kWhite));
            const float x=rr.x+14.f,w=rr.w-28.f;
            const float t=std::clamp(field.position,0.f,1.f);
            ov.addRect({x,rr.y+46.f,w,9.f},overlayColor(kRollBg));
            ov.addRect({x,rr.y+46.f,w*t,9.f},overlayColor(kCyan));
            ov.addRect({x+t*w-3.f,rr.y+40.f,6.f,23.f},overlayColor(kWhite));
        }
    }
    ov.clearClip();
}
