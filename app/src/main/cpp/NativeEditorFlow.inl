// Material-Time Instrument Flow v3 editor. Included inside namespace aiora.
// Y is chronological time, not a cosmetic graph coordinate.
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
    const float w=std::max(30.f,(b.w-24.f-6*5.f)/7.f);
    return {b.x+12.f+index*(w+5.f),y+63.f,w,44.f};
}
NativeEditor::Rect NativeEditor::flowActionRect(int index) const noexcept {
    const auto b=bodyRect(),tab=synthTabRect(0);
    const float gap=std::clamp(b.w*.014f,9.f,13.f);
    const float top=tab.y+tab.h+gap-physicalScrollY_;
    const float w=std::max(38.f,(b.w-24.f-5*5.f)/6.f);
    return {b.x+12.f+(index%6)*(w+5.f),
            top+113.f+(index/6)*49.f,w,44.f};
}
NativeEditor::Rect NativeEditor::flowTimeRect() const noexcept {
    const auto b=bodyRect(),tab=synthTabRect(0);
    const float gap=std::clamp(b.w*.014f,9.f,13.f);
    return {b.x+12.f,tab.y+tab.h+gap-physicalScrollY_+264.f,
            b.w-24.f,48.f};
}
NativeEditor::Rect NativeEditor::flowCanvasRect() const noexcept {
    const auto b=bodyRect(),tab=synthTabRect(0);
    const float gap=std::clamp(b.w*.014f,9.f,13.f);
    return {b.x+12.f,tab.y+tab.h+gap-physicalScrollY_+322.f,
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
    if(i<0||i>7)return false;
    physicalBlueprint_.graph=physical::templateFlowGraph(
        static_cast<physical::FlowTemplate>(i));
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
    if(flowSelectedLink_>=0&&flowSelectedLink_<g.linkCount)return 3;
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
    }
    return 0;
}
const char* NativeEditor::flowPropertyLabel(int i) const noexcept {
    const auto& g=physicalBlueprint_.graph;
    if(flowSelectedLink_>=0&&flowSelectedLink_<g.linkCount){
        constexpr const char* l[]={"ENTRY X (ON TARGET)","M POSITION TRAVEL","INTERACTION WIDTH"};
        return i>=0&&i<3?l[i]:"";
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
    }
    return "";
}
bool NativeEditor::flowAction(int index) noexcept {
    using physical::FlowKind;
    auto& g=physicalBlueprint_.graph;
    constexpr std::array<FlowKind,11> types{
        FlowKind::String,FlowKind::Membrane,FlowKind::Plate,FlowKind::Bar,
        FlowKind::AirColumn,FlowKind::Body,FlowKind::Bridge,FlowKind::Damper,
        FlowKind::Reed,FlowKind::Edge,FlowKind::Router
    };
    if(index>=0&&index<static_cast<int>(types.size())){
        // Selecting a vibrating object and tapping +TOUCH physically attaches
        // a contact to it. No sound-path rewiring or bypass deletion needed.
        const int source=flowSelectedNode_;
        const bool attach=index==7&&source>=0&&source<g.nodeCount&&
            physical::vibrates(g.nodes[static_cast<size_t>(source)].kind);
        const int id=g.addNode(types[static_cast<size_t>(index)]);
        if(id<0)return false;
        if(attach){
            auto& touch=g.nodes[static_cast<size_t>(id)];
            const auto& vibrator=g.nodes[static_cast<size_t>(source)];
            touch.y=std::clamp(vibrator.y+.14f,.10f,.90f);
            touch.x=std::clamp(vibrator.x+.24f,.04f,.95f);
            if(!g.connect(source,id))flowError_=true;
        }
        flowSelectedNode_=id;flowSelectedLink_=-1;physicalDirty_=true;return true;
    }
    if(index==11||index==12){
        const int m=index==11?1:2;
        flowLinkMode_=flowLinkMode_==m?0:m;
        flowLinkSource_=-1;flowError_=false;
        return false;
    }
    if(index==13){
        if(!g.removeNode(flowSelectedNode_))return false;
        flowSelectedNode_=std::max(0,std::min(flowSelectedNode_,int(g.nodeCount)-1));
        flowLinkSource_=-1;flowSelectedLink_=-1;physicalDirty_=true;return true;
    }
    if(index==14){
        g=physical::blankFlowGraph();
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
    const bool changed=flowLinkMode_==1?
        g.connect(flowLinkSource_,index):
        g.disconnect(flowLinkSource_,index);
    flowError_=!changed;flowLinkMode_=0;flowLinkSource_=-1;
    if(changed){
        physicalDirty_=true;
        flowSelectedLink_=-1;
        if(flowLinkMode_==0&&g.linkCount>0)flowSelectedLink_=g.linkCount-1;
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
        else if(i==1)link.modTravel=2.f*t-1.f;
        else if(i==2)link.aperture=.035f+.965f*t;
        physicalDirty_=true;return true;
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
    if(i==0&&n.kind!=FlowKind::Output){
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
    ov.addText("UNIVERSAL FLOW   /   Y = FIRST ENERGY ARRIVAL",
        b.x+20.f,top+15.f,.88f,overlayColor(kCyan));
    ov.addText("X: entry position  /  V: source energy  /  M: linked motion",
        b.x+20.f,top+36.f,.73f,overlayColor(kMuted));
    constexpr const char* templates[]={"STRING","DRUM","PLATE","BAR","AIR","BELL","EMPTY","HYBRID"};
    for(int i=0;i<8;++i){
        const auto rr=flowTemplateRect(i);
        ov.addRect({rr.x,rr.y,rr.w,rr.h},overlayColor(kTrack));
        ov.addTextCentered(templates[i],{rr.x+1,rr.y,rr.w-2,rr.h},.70f,
            overlayColor(kWhite));
    }
    constexpr const char* actions[]={
        "+STRING","+DRUM","+PLATE","+BAR","+AIR","+BODY",
        "+COUPL","+TOUCH","+REED","+EDGE","+ROUTE","LINK",
        "UNLINK","DELETE","CLEAR"
    };
    for(int i=0;i<15;++i){
        const auto rr=flowActionRect(i);
        const bool selected=(i==11&&flowLinkMode_==1)||(i==12&&flowLinkMode_==2);
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
        ov.addTextCentered(physical::flowKindName(node.kind),
            {rr.x+2,rr.y+6.f,rr.w-4,25.f},.72f,
            overlayColor(selected?kCyan:kWhite));
        ov.addTextCentered(node.kind==physical::FlowKind::Output?
            "Audio":physical::materialName(node.material),
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
        std::snprintf(selectedLabel,sizeof(selectedLabel),"LINK: %s > %s",
            physical::flowKindName(g.nodes[link.from].kind),
            physical::flowKindName(g.nodes[link.to].kind));
    }
    ov.addText(linkSelected?selectedLabel:
        chosen?physical::flowKindName(chosen->kind):"SELECT OBJECT",
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
                field.position=(link.modTravel+1.f)*.5f;
                std::snprintf(field.label,sizeof(field.label),"%+.0f%%",link.modTravel*100.f);
            }else{
                field.position=(link.aperture-.035f)/.965f;
                std::snprintf(field.label,sizeof(field.label),"%.0f%%",link.aperture*100.f);
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
