// Three independently selectable V/M/K response maps for the master Amp.
struct AmpModSurface {
    using Rect=NativeEditor::Rect;
    Rect view{};
    int slot{0},drag{-1};
    float downX{0},downY{0};
    bool changed{false},moved{false};
    std::chrono::steady_clock::time_point started{};
    Rect cell(int index)const {
        const float gap=7.0f,w=(view.w-32.0f-2.0f*gap)/3.0f;
        return {view.x+16.0f+index*(w+gap),view.y+12.0f,w,55.0f};
    }
    Rect sourceButton()const{return {view.x+16.0f,view.y+83.0f,view.w*.47f-20.0f,56.0f};}
    Rect enableButton()const{return {view.x+view.w*.51f,view.y+83.0f,view.w*.47f-10.0f,56.0f};}
    Rect graph()const{
        return {view.x+60.0f,view.y+188.0f,std::max(40.0f,view.w-85.0f),
            std::max(80.0f,view.h-265.0f)};
    }
    OperatorResponse response()const{
        const auto p=ProjectCore::instance().selectedPatch();
        auto c=p.ampMods[static_cast<size_t>(slot)];
        if(c.count<2){c=initialOperatorResponse();c.enabled=false;}
        return c;
    }
    bool save(const OperatorResponse& c){
        return ProjectCore::instance().setSelectedAmpMod(slot,c);
    }
    std::pair<float,float> position(const OperatorPoint& pt) const{
        const auto r=graph();
        return {r.x+r.w*std::clamp(pt.time,0.0f,1.0f),
                r.y+r.h*std::clamp(pt.value,0.0f,1.0f)};
    }
    void draw()const{
        auto& o=NativeOverlay::instance();
        o.addRect({view.x,view.y,view.w,view.h},overlayColor(kBg));
        const auto c=response();
        for(int i=0;i<3;++i){
            const auto r=cell(i);bool active=i==slot;
            o.addRect({r.x,r.y,r.w,r.h},overlayColor(active?kPurple:kButton));
            o.addTextCentered("Mod "+std::to_string(i+1),
                {r.x,r.y,r.w,r.h},.87f,overlayColor(active?kBg:kWhite));
        }
        const auto src=sourceButton(),enable=enableButton();
        o.addRect({src.x,src.y,src.w,src.h},overlayColor(kButton));
        o.addTextCentered(c.source==0?"Source V":c.source==1?"Source M":"Source K",
            {src.x,src.y,src.w,src.h},.83f,overlayColor(kWhite));
        o.addRect({enable.x,enable.y,enable.w,enable.h},
                  overlayColor(c.enabled?kGreen:kButton));
        o.addTextCentered(c.enabled?"Mod ON":"Mod OFF",
            {enable.x,enable.y,enable.w,enable.h},.83f,
            overlayColor(c.enabled?kBg:kWhite));
        const auto r=graph();
        o.addRect({r.x-4,r.y-4,r.w+8,r.h+8},overlayColor(kRollBg));
        for(int i=0;i<=4;++i){
            const float x=r.x+i*r.w*.25f,y=r.y+i*r.h*.25f;
            o.addLine(x,r.y,x,r.y+r.h,1.0f,overlayColor(kMuted,.35f));
            o.addLine(r.x,y,r.x+r.w,y,1.0f,overlayColor(kMuted,.35f));
        }
        o.addText("Output  0",view.x+2,r.y-11,.75f,overlayColor(kMuted));
        o.addText("100",view.x+12,r.y+r.h-5,.75f,overlayColor(kMuted));
        o.addText("Input V / M / K: 0 to 100",
            r.x,r.y-26,.75f,overlayColor(kMuted));
        for(int j=1;j<c.count;++j){
            const auto& a=c.points[j-1],&b=c.points[j];
            for(int k=1;k<=30;++k){
                const float t0=a.time+(b.time-a.time)*(k-1)/30.0f;
                const float t1=a.time+(b.time-a.time)*k/30.0f;
                const auto p0=position({t0,operatorCurveValue(&a,2,t0)});
                const auto p1=position({t1,operatorCurveValue(&a,2,t1)});
                o.addLine(p0.first,p0.second,p1.first,p1.second,
                    3.0f,overlayColor(c.enabled?kPurple:kMuted));
            }
        }
        for(int j=0;j<c.count;++j){
            const auto p=position(c.points[j]);
            o.addFilledCircle(p.first,p.second,j==drag?8.0f:6.0f,
                overlayColor(j==drag?kOrange:kWhite));
        }
        o.addText("Tap: add   Drag: shape   Hold point: delete",
            view.x+18,view.y+view.h-36,.74f,overlayColor(kMuted));
    }
    bool down(float x,float y){
        changed=moved=false;drag=-1;downX=x;downY=y;
        started=std::chrono::steady_clock::now();
        for(int i=0;i<3;++i)if(cell(i).contains(x,y)){slot=i;return true;}
        auto c=response();
        if(sourceButton().contains(x,y)){
            c.source=static_cast<uint8_t>((c.source+1)%3);
            changed=save(c);return true;
        }
        if(enableButton().contains(x,y)){
            c.enabled=!c.enabled;changed=save(c);return true;
        }
        if(!graph().contains(x,y))return true;
        float closest=24.0f*24.0f;
        for(int i=0;i<c.count;++i){
            const auto p=position(c.points[i]);
            const float d=(p.first-x)*(p.first-x)+(p.second-y)*(p.second-y);
            if(d<closest){closest=d;drag=i;}
        }
        return true;
    }
    void move(float x,float y){
        if(drag<0)return;
        if(std::fabs(x-downX)+std::fabs(y-downY)<8.0f)return;
        moved=true;
        auto c=response();const auto r=graph();
        auto& pt=c.points[static_cast<size_t>(drag)];
        pt.time=drag==0?0.0f:drag==c.count-1?1.0f:
            std::clamp((x-r.x)/r.w,c.points[drag-1].time+.001f,
                c.points[drag+1].time-.001f);
        pt.value=std::clamp((y-r.y)/r.h,0.0f,1.0f);
        c.enabled=true;changed=save(c)||changed;
    }
    bool up(){
        // Match Filter Mod: a stationary >=500 ms hold removes only
        // interior points, preserving the two response endpoints.
        if(drag>0&&!moved){
            const auto millis=std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-started).count();
            if(millis>=500){
                auto c=response();
                if(c.count>2&&drag<c.count-1){
                    for(int i=drag;i<c.count-1;++i)
                        c.points[i]=c.points[i+1];
                    --c.count;
                    changed=save(c)||changed;
                }
            }
        }
        if(drag<0&&!moved&&graph().contains(downX,downY)){
            auto c=response();const auto r=graph();
            if(c.count<kMaxOperatorPoints){
                OperatorPoint p{};
                p.time=std::clamp((downX-r.x)/r.w,.0001f,.9999f);
                p.value=std::clamp((downY-r.y)/r.h,0.0f,1.0f);
                int at=0;
                while(at<c.count&&c.points[at].time<p.time)++at;
                if(at>0&&at<c.count){
                    for(int i=c.count;i>at;--i)c.points[i]=c.points[i-1];
                    c.points[at]=p;++c.count;c.enabled=true;
                    changed=save(c)||changed;
                }
            }
        }
        drag=-1;const bool result=changed;changed=false;return result;
    }
};
AmpModSurface ampModSurface;
