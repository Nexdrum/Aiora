// Included inside namespace aiora in NativeEditor.cpp.
// Fixed-capacity physical graph editor. UI positions are presentation only;
// sound is compiled from actual typed connection edges in the audio voice.

NativeEditor::Rect NativeEditor::flowCanvasRect() const noexcept {
    const auto b=bodyRect(),tab=synthTabRect(0);
    const float gap=std::clamp(b.w*.014f,9.0f,13.0f);
    const float y=tab.y+tab.h+gap-physicalScrollY_;
    return {b.x+12.f,y+150.f,std::max(100.f,b.w-24.f),432.f};
}
NativeEditor::Rect NativeEditor::flowActionRect(int index) const noexcept {
    const auto b=bodyRect(),tab=synthTabRect(0);
    const float gap=std::clamp(b.w*.014f,9.0f,13.0f);
    const float top=tab.y+tab.h+gap-physicalScrollY_;
    const float w=std::max(38.f,(b.w-24.f-4*6.f)/5.f);
    return {b.x+12.f+index*(w+6.f),top+91.f,w,51.f};
}
NativeEditor::Rect NativeEditor::flowNodeRect(int index) const noexcept {
    const auto graph=flowCanvasRect();
    if(index<0||index>=physicalBlueprint_.graph.nodeCount)return {};
    const auto& node=physicalBlueprint_.graph.nodes[static_cast<size_t>(index)];
    const float w=std::clamp(graph.w*.285f,67.f,215.f);
    const float h=73.f;
    const float availableW=std::max(1.f,graph.w-w-22.f);
    return {graph.x+11.f+std::clamp(node.x,0.f,1.f)*availableW,
            graph.y+13.f+std::clamp(node.y,0.f,1.f)*(graph.h-h-26.f),
            w,h};
}
NativeEditor::Rect NativeEditor::flowPropertyRect(int index) const noexcept {
    const auto canvas=flowCanvasRect();
    return {canvas.x,canvas.y+canvas.h+76.f+index*76.f,
            canvas.w,68.f};
}
int NativeEditor::flowPropertyCount() const noexcept {
    const auto& graph=physicalBlueprint_.graph;
    if(flowSelectedNode_<0||flowSelectedNode_>=graph.nodeCount)return 0;
    switch(graph.nodes[static_cast<size_t>(flowSelectedNode_)].kind){
    case physical::FlowKind::Exciter:return 4;
    case physical::FlowKind::String:return 3;
    case physical::FlowKind::Bridge:return 2;
    case physical::FlowKind::Body:return 6;
    case physical::FlowKind::Damper:return 3;
    case physical::FlowKind::Output:return 0;
    }
    return 0;
}
const char* NativeEditor::flowPropertyLabel(int index) const noexcept {
    const auto& graph=physicalBlueprint_.graph;
    if(flowSelectedNode_<0||flowSelectedNode_>=graph.nodeCount)return "";
    using physical::FlowKind;
    switch(graph.nodes[static_cast<size_t>(flowSelectedNode_)].kind){
    case FlowKind::Exciter:{
        constexpr const char* labels[]={"IMPACT MATERIAL","ACTION","STRIKE POSITION","HAMMER HARDNESS"};
        return index>=0&&index<4?labels[index]:"";
    }
    case FlowKind::String:{
        constexpr const char* labels[]={"STRING MATERIAL","TENSION","DIAMETER"};
        return index>=0&&index<3?labels[index]:"";
    }
    case FlowKind::Bridge:{
        constexpr const char* labels[]={"BRIDGE MATERIAL","ENERGY COUPLING"};
        return index>=0&&index<2?labels[index]:"";
    }
    case FlowKind::Body:{
        constexpr const char* labels[]={"BODY MATERIAL","BODY SHAPE","BODY SIZE","WALL THICKNESS","STRUCTURAL DAMPING","COUPLING"};
        return index>=0&&index<6?labels[index]:"";
    }
    case FlowKind::Damper:{
        constexpr const char* labels[]={"CONTACT MATERIAL","CONTACT POSITION","M PRESSURE"};
        return index>=0&&index<3?labels[index]:"";
    }
    case FlowKind::Output:return "";
    }
    return "";
}
bool NativeEditor::flowAction(int index) noexcept {
    using physical::FlowKind;
    auto& graph=physicalBlueprint_.graph;
    switch(index){
    case 0:{
        const int id=graph.addNode(FlowKind::Body);
        if(id<0)return false;
        flowSelectedNode_=id;physicalDirty_=true;return true;
    }
    case 1:{
        const int id=graph.addNode(FlowKind::Damper);
        if(id<0)return false;
        flowSelectedNode_=id;physicalDirty_=true;return true;
    }
    case 2: flowLinkMode_=flowLinkMode_==1?0:1;flowLinkSource_=-1;return false;
    case 3: flowLinkMode_=flowLinkMode_==2?0:2;flowLinkSource_=-1;return false;
    case 4:{
        if(!graph.removeNode(flowSelectedNode_))return false;
        flowSelectedNode_=std::min(flowSelectedNode_,static_cast<int>(graph.nodeCount)-1);
        physicalDirty_=true;flowLinkSource_=-1;return true;
    }
    }
    return false;
}
bool NativeEditor::flowNodeTouch(int index) noexcept {
    auto& graph=physicalBlueprint_.graph;
    if(index<0||index>=graph.nodeCount)return false;
    flowSelectedNode_=index;
    if(!flowLinkMode_)return false;
    if(flowLinkSource_<0){flowLinkSource_=index;return false;}
    bool updated=false;
    if(flowLinkMode_==1)updated=graph.connect(flowLinkSource_,index);
    else updated=graph.disconnect(flowLinkSource_,index);
    flowLinkSource_=-1;flowLinkMode_=0;
    if(updated)physicalDirty_=true;
    return updated;
}
bool NativeEditor::editFlowProperty(int index,float x,bool initial) noexcept {
    using physical::FlowKind;
    using physical::Excitation;
    auto& graph=physicalBlueprint_.graph;
    if(flowSelectedNode_<0||flowSelectedNode_>=graph.nodeCount)return false;
    auto& n=graph.nodes[static_cast<size_t>(flowSelectedNode_)];
    if(index<0||index>=flowPropertyCount())return false;
    const auto rr=flowPropertyRect(index);
    const float t=std::clamp((x-rr.x-15.f)/std::max(1.f,rr.w-30.f),0.f,1.f);
    switch(n.kind){
    case FlowKind::Exciter:
        if(index==0)return false; // opens material dropdown
        if(index==1){
            if(!initial)return false;
            n.excitation=n.excitation==Excitation::Pluck?Excitation::Hammer:Excitation::Pluck;
        }else if(index==2)n.position=.05f+.90f*t;
        else if(index==3)n.hardness=.03f+.97f*t;
        break;
    case FlowKind::String:
        if(index==0)return false;
        if(index==1)n.tensionNewtons=10.f+390.f*t;
        else if(index==2)n.diameterMm=.15f+1.80f*t;
        break;
    case FlowKind::Bridge:
        if(index==0)return false;
        if(index==1)n.coupling=.005f+.995f*t;
        break;
    case FlowKind::Body:
        if(index<=1)return false; // material and geometry dropdowns
        if(index==2)n.size=.35f+2.65f*t;
        else if(index==3)n.thicknessMm=.3f+19.7f*t;
        else if(index==4)n.damping=.15f+5.85f*t;
        else if(index==5)n.coupling=.005f+.995f*t;
        break;
    case FlowKind::Damper:
        if(index==0)return false;
        if(index==1)n.position=.01f+.39f*t;
        else if(index==2)n.coupling=.005f+.995f*t;
        break;
    case FlowKind::Output:return false;
    }
    physicalDirty_=true;return true;
}
void NativeEditor::drawPhysical() const noexcept {
    const auto& graph=physicalBlueprint_.graph;
    const auto b=bodyRect(),tab=synthTabRect(0),canvas=flowCanvasRect();
    const float gap=std::clamp(b.w*.014f,9.0f,13.0f);
    const float top=tab.y+tab.h+gap-physicalScrollY_;
    const Rect clip{b.x,tab.y+tab.h+gap,b.w,
        std::max(0.f,b.y+b.h-(tab.y+tab.h+gap))};
    auto& ov=NativeOverlay::instance();
    ov.setClip({clip.x,clip.y,clip.w,clip.h});
    ov.addRect({b.x+12.f,top+8.f,b.w-24.f,73.f},overlayColor(kRollBg));
    ov.addText("INSTRUMENT FLOW / PHYSICAL GRAPH",
        b.x+22.f,top+18.f,.96f,overlayColor(kCyan));
    ov.addText("V: strike energy    M: contact pressure    Drag nodes to arrange",
        b.x+22.f,top+48.f,.80f,overlayColor(kMuted));
    const char* actions[]={"ADD BODY","ADD MUTE","LINK","UNLINK","DELETE"};
    for(int i=0;i<5;++i){
        const auto rr=flowActionRect(i);
        const bool on=(i==2&&flowLinkMode_==1)||(i==3&&flowLinkMode_==2);
        ov.addRect({rr.x,rr.y,rr.w,rr.h},overlayColor(on?kCyan:kButton));
        ov.addTextCentered(actions[i],{rr.x+2,rr.y,rr.w-4,rr.h},.72f,
            overlayColor(on?kBg:kWhite));
    }
    ov.addRect({canvas.x,canvas.y,canvas.w,canvas.h},overlayColor(kRollBg));
    for(int i=0;i<graph.linkCount;++i){
        const auto edge=graph.links[static_cast<size_t>(i)];
        if(edge.from>=graph.nodeCount||edge.to>=graph.nodeCount)continue;
        const auto a=flowNodeRect(edge.from),z=flowNodeRect(edge.to);
        const float x0=a.x+a.w*.5f,y0=a.y+a.h*.5f;
        const float x1=z.x+z.w*.5f,y1=z.y+z.h*.5f;
        ov.addLine(x0,y0,x1,y1,4.f,overlayColor(kCyan,.65f));
        ov.addFilledCircle(x1,y1,5.f,overlayColor(kCyan));
    }
    for(int i=0;i<graph.nodeCount;++i){
        const auto& node=graph.nodes[static_cast<size_t>(i)];
        const auto rr=flowNodeRect(i);
        const bool selected=i==flowSelectedNode_;
        const bool pending=i==flowLinkSource_;
        ov.addRect({rr.x-2,rr.y-2,rr.w+4,rr.h+4},
            overlayColor(pending?kOrange:selected?kCyan:kTrack));
        ov.addRect({rr.x+2,rr.y+2,rr.w-4,rr.h-4},overlayColor(kButton));
        ov.addTextCentered(physical::flowKindName(node.kind),
            {rr.x+3,rr.y+4,rr.w-6,25.f},.80f,
            overlayColor(selected?kCyan:kWhite));
        const char* detail=node.kind==physical::FlowKind::Output?
            "Audio" : physical::materialName(node.material);
        ov.addTextCentered(detail,{rr.x+3,rr.y+31,rr.w-6,30.f},
            .72f,overlayColor(kMuted));
    }
    if(flowLinkMode_){
        ov.addText(flowLinkSource_<0?"Select source node":
            "Select destination node",canvas.x+11,canvas.y+canvas.h-32.f,
            .92f,overlayColor(kOrange));
    }
    const auto chosen=flowSelectedNode_>=0&&flowSelectedNode_<graph.nodeCount?
        &graph.nodes[static_cast<size_t>(flowSelectedNode_)]:nullptr;
    const float hy=canvas.y+canvas.h+14.f;
    ov.addRect({canvas.x,hy,canvas.w,51.f},overlayColor(kPanel));
    ov.addText(chosen?physical::flowKindName(chosen->kind):"SELECT AN OBJECT",
        canvas.x+12.f,hy+13.f,1.f,overlayColor(kCyan));
    if(chosen&&chosen->kind==physical::FlowKind::Output)
        ov.addText("OUTPUT / no editable properties",canvas.x+12.f,hy+39.f,
            .8f,overlayColor(kMuted));
    for(int i=0;i<flowPropertyCount();++i){
        const auto rr=flowPropertyRect(i);
        if(rr.y+rr.h<clip.y||rr.y>clip.y+clip.h)continue;
        ov.addRect({rr.x,rr.y,rr.w,rr.h},overlayColor(kButton));
        ov.addText(flowPropertyLabel(i),rr.x+13.f,rr.y+7.f,.84f,
            overlayColor(kMuted));
        bool select=false;
        float t=0.5f;
        char value[64]{};
        if(chosen){
            using physical::FlowKind;
            switch(chosen->kind){
            case FlowKind::Exciter:
                if(i==0){select=true;std::snprintf(value,sizeof(value),"%s",physical::materialName(chosen->material));}
                else if(i==1){select=true;std::snprintf(value,sizeof(value),"%s",chosen->excitation==physical::Excitation::Pluck?"Pluck":"Hammer");}
                else if(i==2){t=(chosen->position-.05f)/.90f;std::snprintf(value,sizeof(value),"%.0f%%",chosen->position*100.f);}
                else if(i==3){t=(chosen->hardness-.03f)/.97f;std::snprintf(value,sizeof(value),"%.0f%%",chosen->hardness*100.f);}
                break;
            case FlowKind::String:
                if(i==0){select=true;std::snprintf(value,sizeof(value),"%s",physical::materialName(chosen->material));}
                else if(i==1){t=(chosen->tensionNewtons-10.f)/390.f;std::snprintf(value,sizeof(value),"%.0f N",chosen->tensionNewtons);}
                else if(i==2){t=(chosen->diameterMm-.15f)/1.8f;std::snprintf(value,sizeof(value),"%.2f mm",chosen->diameterMm);}
                break;
            case FlowKind::Bridge:
                if(i==0){select=true;std::snprintf(value,sizeof(value),"%s",physical::materialName(chosen->material));}
                else if(i==1){t=(chosen->coupling-.005f)/.995f;std::snprintf(value,sizeof(value),"%.0f%%",chosen->coupling*100.f);}
                break;
            case FlowKind::Body:
                if(i==0){select=true;std::snprintf(value,sizeof(value),"%s",physical::materialName(chosen->material));}
                else if(i==1){select=true;std::snprintf(value,sizeof(value),"%s",physical::geometryName(chosen->geometry));}
                else if(i==2){t=(chosen->size-.35f)/2.65f;std::snprintf(value,sizeof(value),"%.2fx",chosen->size);}
                else if(i==3){t=(chosen->thicknessMm-.3f)/19.7f;std::snprintf(value,sizeof(value),"%.2f mm",chosen->thicknessMm);}
                else if(i==4){t=(chosen->damping-.15f)/5.85f;std::snprintf(value,sizeof(value),"%.2fx",chosen->damping);}
                else if(i==5){t=(chosen->coupling-.005f)/.995f;std::snprintf(value,sizeof(value),"%.0f%%",chosen->coupling*100.f);}
                break;
            case FlowKind::Damper:
                if(i==0){select=true;std::snprintf(value,sizeof(value),"%s",physical::materialName(chosen->material));}
                else if(i==1){t=(chosen->position-.01f)/.39f;std::snprintf(value,sizeof(value),"%.0f%%",chosen->position*100.f);}
                else if(i==2){t=(chosen->coupling-.005f)/.995f;std::snprintf(value,sizeof(value),"%.0f%%",chosen->coupling*100.f);}
                break;
            case FlowKind::Output:break;
            }
        }
        if(select){
            ov.addTextCentered(value,{rr.x+12.f,rr.y+31.f,rr.w-24.f,32.f},
                .99f,overlayColor(kWhite));
        }else{
            t=std::clamp(t,0.f,1.f);
            ov.addText(value,rr.x+rr.w-117.f,rr.y+7.f,.8f,overlayColor(kWhite));
            const float x=rr.x+15.f,w=rr.w-30.f;
            ov.addRect({x,rr.y+45.f,w,10.f},overlayColor(kRollBg));
            ov.addRect({x,rr.y+45.f,t*w,10.f},overlayColor(kCyan));
            ov.addRect({x+t*w-3.f,rr.y+37.f,6.f,25.f},overlayColor(kWhite));
        }
    }
    ov.clearClip();
}
