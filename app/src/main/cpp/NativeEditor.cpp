#include "NativeEditor.h"
#include "NativeOverlay.h"

#include <GLES3/gl3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <string>

namespace aiora {
namespace {

constexpr NativeEditor::Rgb kBg{0.0627f,0.0706f,0.0863f};
constexpr NativeEditor::Rgb kPanel{0.0863f,0.1020f,0.1294f};
constexpr NativeEditor::Rgb kButton{0.1373f,0.1569f,0.2000f};
constexpr NativeEditor::Rgb kRollBg{0.0471f,0.0549f,0.0706f};
constexpr NativeEditor::Rgb kTrack{0.18f,0.21f,0.27f};
constexpr NativeEditor::Rgb kMuted{0.35f,0.39f,0.47f};
constexpr NativeEditor::Rgb kCyan{0.0f,0.80f,0.80f};
constexpr NativeEditor::Rgb kOrange{1.0f,0.6667f,0.0f};
constexpr NativeEditor::Rgb kPurple{0.788f,0.557f,1.0f};
constexpr NativeEditor::Rgb kGreen{0.23f,0.92f,0.45f};
constexpr NativeEditor::Rgb kRed{0.95f,0.18f,0.18f};
constexpr NativeEditor::Rgb kWhite{0.91f,0.925f,0.945f};
int64_t kineticClockMs() noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

constexpr std::array<NativeEditor::Rgb,12> kSpectrumColors{{
    {0.2275f,1.0000f,0.0000f},
    {0.0000f,1.0000f,0.9255f},
    {0.0000f,0.5608f,1.0000f},
    {0.0588f,0.0000f,0.9843f},
    {0.3882f,0.0000f,0.7451f},
    {0.4314f,0.0000f,0.5020f},
    {0.5961f,0.0000f,0.0000f},
    {0.7843f,0.0000f,0.0000f},
    {0.9529f,0.0000f,0.0000f},
    {1.0000f,0.4706f,0.0000f},
    {1.0000f,0.9373f,0.0000f},
    {0.6667f,1.0000f,0.0000f},
}};

constexpr std::array<PatchParam,4> kLfoParams{
    PatchParam::LfoRate,PatchParam::LfoAmount,
    PatchParam::LfoAttack,PatchParam::LfoVelocity
};
constexpr std::array<OperatorParam,4> kOperatorParams{
    OperatorParam::Ratio,OperatorParam::Semitone,
    OperatorParam::Detune,OperatorParam::Level
};

NativeEditor::Rgb mix(NativeEditor::Rgb a,NativeEditor::Rgb b,float t) noexcept {
    t=std::clamp(t,0.0f,1.0f);
    return {a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};
}

NativeOverlay::Color overlayColor(NativeEditor::Rgb c,float alpha=1.0f) noexcept {
    return {c.r,c.g,c.b,alpha};
}

bool selectedTrackIsDrums(){
    auto& p=ProjectCore::instance();
    const int t=p.selectedTrack();
    return t>=0&&p.trackIsDrums(t);
}

} // namespace

NativeEditor& NativeEditor::instance(){static NativeEditor e;return e;}

void NativeEditor::resize(int width,int height) noexcept {
    width_=std::max(0,width);
    height_=std::max(0,height);
}

void NativeEditor::setSafeInsets(int left,int top,int right,int bottom) noexcept {
    safeLeft_=std::clamp(left,0,std::max(0,width_/2));
    safeTop_=std::clamp(top,0,std::max(0,height_/2));
    safeRight_=std::clamp(right,0,std::max(0,width_/2));
    safeBottom_=std::clamp(bottom,0,std::max(0,height_/2));
}

NativeEditor::Rect NativeEditor::viewportRect() const noexcept {
    // Box 4 comes directly from NativeUi::bodyContentRect(). Both the
    // painted editor and its hit targets must use the same native rectangle.
    return viewport_;
}

float NativeEditor::scrollGutterPixels() const noexcept {
    const float shortSide=static_cast<float>(std::min(
        std::max(1,width_-safeLeft_-safeRight_),
        std::max(1,height_-safeTop_-safeBottom_)));
    const float cell=std::clamp(shortSide*0.076f,54.0f,68.0f);
    return std::clamp(cell*1.55f,78.0f,108.0f);
}

NativeEditor::Rect NativeEditor::scrollGutterRect() const noexcept {
    const auto view=viewportRect();
    const float gutter=std::min(
        scrollGutterPixels(),
        std::max(0.0f,view.w-120.0f));
    return {view.x,view.y,gutter,view.h};
}

NativeEditor::Rect NativeEditor::contentRect() const noexcept {
    const auto view=viewportRect();
    const float gutter=std::min(
        scrollGutterPixels(),
        std::max(0.0f,view.w-120.0f));
    return {
        view.x+gutter,
        view.y,
        std::max(0.0f,view.w-gutter),
        view.h
    };
}

NativeEditor::Rect NativeEditor::editorRect() const noexcept {
    return contentRect();
}

NativeEditor::Rect NativeEditor::patchTransferRect(EditorPage page,int index) const noexcept {
    if(index==0)return {};
    const auto e=editorRect();
    const float gap=std::clamp(e.w*0.014f,9.0f,13.0f);
    const float h=page==EditorPage::Synth?52.0f:70.0f;
    const float w=(e.w-gap*3.0f)*0.5f;

    float top=e.y+gap;
    if(page==EditorPage::Synth){
        top=e.y+e.h-63.0f;
    }else{
        top=e.y+e.h-63.0f;
    }

    return {
        e.x+gap+(index-1)*(w+gap),
        top,
        w,h
    };
}

NativeEditor::Rect NativeEditor::bodyRect() const noexcept {
    return editorRect();
}

NativeEditor::Rect NativeEditor::factoryPresetRect(int) const noexcept {
    return {};
}

NativeEditor::Rect NativeEditor::synthTabRect(int index) const noexcept {
    const auto b=bodyRect();
    const float gap=std::clamp(b.w*0.014f,9.0f,13.0f);
    const float h=66.0f;
    const float w=(b.w-gap*4.0f)/3.0f;
    return {
        b.x+gap+index*(w+gap),
        b.y+gap,
        w,h
    };
}

NativeEditor::Rect NativeEditor::operatorCardRect(int index) const noexcept {
    const auto b=bodyRect();
    const auto tab=synthTabRect(0);
    const float gap=std::clamp(b.w*0.014f,9.0f,13.0f);
    const float w=(b.w-gap*3.0f)*0.5f;
    const float h=1022.0f;
    const int row=index/2,col=index%2;
    return {
        b.x+gap+col*(w+gap),
        tab.y+tab.h+gap+row*(h+gap)-synthScrollY_,
        w,h
    };
}

NativeEditor::Rect NativeEditor::operatorWaveFieldRect(int op) const noexcept {
    const auto c=operatorCardRect(op);
    return {c.x+16.0f,c.y+62.0f,c.w-32.0f,66.0f};
}

NativeEditor::Rect NativeEditor::operatorCardParamRect(int op,int param) const noexcept {
    const auto c=operatorCardRect(op);
    return {
        c.x+16.0f,
        c.y+144.0f+param*82.0f,
        c.w-32.0f,
        72.0f
    };
}

NativeEditor::Rect NativeEditor::operatorCardToggleRect(int op) const noexcept {
    const auto c=operatorCardRect(op);
    return {c.x+16.0f,c.y+818.0f,c.w-32.0f,70.0f};
}

NativeEditor::Rect NativeEditor::operatorCardHarmonicRect(int op,int partial) const noexcept {
    const auto c=operatorCardRect(op);
    const float gap=7.0f;
    const int row=partial/8,col=partial%8;
    const float w=(c.w-32.0f-gap*7.0f)/8.0f;
    return {
        c.x+16.0f+col*(w+gap),
        c.y+908.0f+row*56.0f,
        w,48.0f
    };
}

// Legacy geometry helpers retained as wrappers for compatibility.
NativeEditor::Rect NativeEditor::operatorSelectRect(int index) const noexcept {
    return operatorCardRect(index);
}
NativeEditor::Rect NativeEditor::waveRect(int index) const noexcept {
    return operatorWaveFieldRect(std::clamp(index,0,5));
}
NativeEditor::Rect NativeEditor::operatorToggleRect() const noexcept {
    return operatorCardToggleRect(std::clamp(selectedOperator_,0,5));
}
NativeEditor::Rect NativeEditor::operatorSliderRect(int index,bool) const noexcept {
    return operatorCardParamRect(std::clamp(selectedOperator_,0,5),index);
}
NativeEditor::Rect NativeEditor::harmonicRect(int index) const noexcept {
    return operatorCardHarmonicRect(std::clamp(selectedOperator_,0,5),index);
}

NativeEditor::Rect NativeEditor::matrixRect(int modulator,int carrier) const noexcept {
    const auto b=bodyRect();
    const auto tabs=synthTabRect(0);
    const float gap=std::clamp(b.w*0.014f,9.0f,13.0f);
    const float labelW=44.0f;
    const float headerH=34.0f;
    const float hintH=34.0f;
    const float top=tabs.y+tabs.h+gap+hintH+headerH;
    const float availableW=
        std::max(1.0f,b.w-gap*2.0f-labelW);
    const float cellW=
        std::max(54.0f,(availableW-gap*5.0f)/6.0f);
    const float availableH=
        std::max(1.0f,b.y+b.h-top-gap);
    const float rowH=std::clamp(
        (availableH-gap*5.0f)/6.0f,
        64.0f,92.0f);
    const float gridW=cellW*6.0f+gap*5.0f;
    const float ox=b.x+(b.w-(gridW+labelW))*0.5f+labelW;
    const float oy=top;
    return {
        ox+carrier*(cellW+gap),
        oy+modulator*(rowH+gap),
        cellW,rowH
    };
}

NativeEditor::Rect NativeEditor::fxWorkspaceTabRect(int index) const noexcept {
    const auto b=bodyRect();
    const float gap=7.0f;
    const float x=b.x+16;
    const float w=(b.w-32-4*gap)/5.0f;
    return {x+index*(w+gap),b.y+12,w,61};
}
NativeEditor::Rect NativeEditor::lfoWorkspaceRow(int row) const noexcept {
    const auto b=bodyRect();
    return {b.x+22,b.y+186.0f+row*91.0f,b.w-44,77};
}
NativeEditor::Rect NativeEditor::lfoWorkspaceTargetRect() const noexcept {
    const auto b=bodyRect();
    return {b.x+b.w*.53f,b.y+105,b.w*.41f,66};
}
NativeEditor::Rect NativeEditor::fxRackRect() const noexcept {
    const auto b=bodyRect();const float gap=std::clamp(b.w*.014f,9.0f,13.0f);
    const Fx rack=ProjectCore::instance().fxRack(false,fxDrumMasterActive());
    float height=175;
    for(int i=0;i<rack.count;++i){height+=82;if(i==fxRackExpanded_)height+=64.0f*fxParamCount(rack.chain[i].type);}
    return {b.x+gap,b.y+70.0f-fxScrollY_,b.w-2*gap,height};
}
NativeEditor::Rect NativeEditor::fxRackStageRect(int index) const noexcept {
    const auto c=fxRackRect();const Fx rack=ProjectCore::instance().fxRack(false,fxDrumMasterActive());
    float y=c.y+72;
    for(int i=0;i<index&&i<rack.count;++i){y+=82;if(i==fxRackExpanded_)y+=64.0f*fxParamCount(rack.chain[i].type);}
    return {c.x+18,y,c.w-36,75};
}
NativeEditor::Rect NativeEditor::fxRackParamRect(int index,int param) const noexcept {
    const auto r=fxRackStageRect(index);
    return {r.x+10,r.y+r.h+param*64.0f,r.w-20,58};
}
NativeEditor::Rect NativeEditor::fxRackAddRect(bool add) const noexcept {
    const auto c=fxRackRect();const float w=(c.w-50)*.5f;
    return {c.x+20+(add?w+10:0),c.y+c.h-83,w,61};
}
NativeEditor::Range NativeEditor::operatorRange(OperatorParam p) noexcept {
    switch(p){
        case OperatorParam::Ratio:return {.125f,16};
        case OperatorParam::Semitone:return {-12,12};
        case OperatorParam::Detune:return {-50,50};
        case OperatorParam::Level:return {0,1};
    }
    return {0,1};
}
NativeEditor::Range NativeEditor::patchRange(PatchParam p) noexcept {
    switch(p){
        case PatchParam::LfoRate:return {.1f,20};
        case PatchParam::LfoAmount:return {0,2};
        case PatchParam::LfoAttack:return {0,2};
        case PatchParam::LfoVelocity:return {0,1};
        case PatchParam::Unison:return {0,1};
        case PatchParam::Volume:return {0,1};
        case PatchParam::PeakRatio:return {.5f,8};
        case PatchParam::PeakFrequency:return {40,8000};
        case PatchParam::PeakQ:return {.3f,12};
        case PatchParam::PeakGain:return {-18,18};
    }
    return {0,1};
}
float NativeEditor::operatorValue(const Patch& p,int i,OperatorParam param) noexcept {
    const auto& o=p.ops[static_cast<size_t>(i)];
    switch(param){
        case OperatorParam::Ratio:return o.ratio;
        case OperatorParam::Semitone:return o.semitoneOffset;
        case OperatorParam::Detune:return o.detuneCents;
        case OperatorParam::Level:return o.level;
    }
    return 0;
}
float NativeEditor::patchValue(const Patch& p,PatchParam param) noexcept {
    switch(param){
        case PatchParam::LfoRate:return p.lfo.rate;
        case PatchParam::LfoAmount:return p.lfo.amount;
        case PatchParam::LfoAttack:return p.lfo.attack;
        case PatchParam::LfoVelocity:return p.lfo.velocitySensitivity;
        case PatchParam::Unison:return p.unison;
        case PatchParam::Volume:return p.volume;
        case PatchParam::PeakRatio:return p.peak.ratio;
        case PatchParam::PeakFrequency:return p.peak.frequency;
        case PatchParam::PeakQ:return p.peak.q;
        case PatchParam::PeakGain:return p.peak.gainDb;
    }
    return 0;
}
float NativeEditor::normalized(float value,Range r) noexcept {
    if(r.hi<=r.lo)return 0.0f;return std::clamp((value-r.lo)/(r.hi-r.lo),0.0f,1.0f);
}
float NativeEditor::denormalized(float value,Range r) noexcept {
    return r.lo+std::clamp(value,0.0f,1.0f)*(r.hi-r.lo);
}
float NativeEditor::editorScrollMax(EditorPage page) const noexcept {
    const auto b=bodyRect();
    if(page==EditorPage::Synth)return 0.0f;

    if(page==EditorPage::Fx){
        const auto rack=fxRackRect();
        return std::max(0.0f,
            rack.y+fxScrollY_+rack.h+32.0f-(b.y+b.h));
    }
    return 0.0f;
}

void NativeEditor::scrollEditor(EditorPage page,float delta) noexcept {
    if(page==EditorPage::Synth){
        synthScrollY_=std::clamp(
            synthScrollY_+delta,
            0.0f,
            editorScrollMax(EditorPage::Synth));
    }else{
        fxScrollY_=std::clamp(
            fxScrollY_+delta,
            0.0f,
            editorScrollMax(EditorPage::Fx));
    }
}

void NativeEditor::fillRect(Rect r,Rgb c) const noexcept {
    if(width_<=0||height_<=0||r.w<=0||r.h<=0)return;
    NativeOverlay::Rect clipped{r.x,r.y,r.w,r.h};
    if(!NativeOverlay::instance().clipRect(clipped))return;
    const int x=std::max(0,static_cast<int>(clipped.x));
    const int top=std::max(0,static_cast<int>(clipped.y));
    const int w=std::max(0,std::min(width_-x,static_cast<int>(clipped.w)));
    const int h=std::max(0,std::min(height_-top,static_cast<int>(clipped.h)));
    if(w<=0||h<=0)return;
    glScissor(x,height_-top-h,w,h);glClearColor(c.r,c.g,c.b,1.0f);glClear(GL_COLOR_BUFFER_BIT);
}
void NativeEditor::drawScrollGutter() const noexcept {
    const auto gutter=scrollGutterRect();
    if(gutter.w<=0.0f||gutter.h<=0.0f)return;

    auto& ov=NativeOverlay::instance();
    constexpr int bands=72;
    for(int i=0;i<bands;++i){
        const float t0=static_cast<float>(i)/static_cast<float>(bands);
        const float t1=static_cast<float>(i+1)/static_cast<float>(bands);
        const float p=t0*11.0f;
        const int idx=std::clamp(static_cast<int>(std::floor(p)),0,10);
        const float local=p-static_cast<float>(idx);
        const auto spectral=mix(
            kSpectrumColors[static_cast<size_t>(idx)],
            kSpectrumColors[static_cast<size_t>(idx+1)],
            local);
        const auto color=mix(kBg,spectral,0.18f);
        const float y0=gutter.y+gutter.h*t0;
        const float y1=gutter.y+gutter.h*t1;
        ov.addRect(
            {gutter.x,y0,gutter.w,std::max(1.0f,y1-y0+0.5f)},
            overlayColor(color));
    }
    ov.addRect(
        {gutter.x+gutter.w-2.0f,gutter.y,2.0f,gutter.h},
        overlayColor(mix(kBg,kPanel,0.65f)));
}

void NativeEditor::drawSlider(Rect r,float norm,Rgb accent) const noexcept {
    fillRect(r,kButton);
    const float inset=std::max(2.0f,r.h*0.18f);
    Rect track{r.x+inset,r.y+r.h*0.38f,std::max(0.0f,r.w-inset*2.0f),std::max(3.0f,r.h*0.24f)};
    fillRect(track,kTrack);
    const float n=std::clamp(norm,0.0f,1.0f);
    fillRect({track.x,track.y,track.w*n,track.h},accent);
    const float knob=std::max(5.0f,r.h*0.42f);
    fillRect({track.x+track.w*n-knob*0.5f,r.y+(r.h-knob)*0.5f,knob,knob},mix(kWhite,accent,0.38f));
}
void NativeEditor::drawButton(Rect r,bool active,Rgb accent) const noexcept {
    fillRect(r,active?accent:kButton);
    const float inset=std::max(2.0f,r.h*0.08f);
    fillRect({r.x+inset,r.y+inset,std::max(0.0f,r.w-inset*2.0f),std::max(0.0f,r.h-inset*2.0f)},active?mix(kPanel,accent,0.18f):kPanel);
}
void NativeEditor::drawPadReservedBackground() const noexcept {
    fillRect(viewportRect(),kPanel);
    drawScrollGutter();
}

void NativeEditor::drawPatchTransfer(EditorPage page) const noexcept {
    const auto e=editorRect();
    const auto copy=patchTransferRect(page,1);
    const auto paste=patchTransferRect(page,2);
    auto& ov=NativeOverlay::instance();

    ov.addText(
        "PATCH FILE",
        e.x+12.0f,
        copy.y-34.0f,
        1.02f,
        overlayColor(kWhite));

    drawButton(copy,false,kCyan);
    drawButton(paste,false,kPurple);
    ov.addTextCentered(
        "Export patch",
        {copy.x,copy.y,copy.w,copy.h},
        0.96f,
        overlayColor(kWhite));
    ov.addTextCentered(
        "Import patch",
        {paste.x,paste.y,paste.w,paste.h},
        0.96f,
        overlayColor(kWhite));
}


// Dedicated point-envelope surface. The existing FM matrix and FX editor
// remain independent, so neither is affected by operator editing gestures.
namespace {
struct OpSurface {
    using Rect=NativeEditor::Rect;
    int op{0},page{0},selected{-1},drag{-1},bend{-1},harmonicView{1},modSlot{0};
    bool harmonicPhaseView{false};
    bool custom{false};
    unsigned shadows{0};
    float downX{0},downY{0},lastY{0};
    bool moved{false},panning{false},changed{false};
    std::array<std::array<float,4>,6> scroll{};
    std::array<std::array<float,2>,6> harmonicScroll{};
    // Independent volume and pitch timelines for each operator.
    // The scale factors are relative to the original 320 px per second.
    // Default to 8x for fine transient editing.
    std::array<std::array<uint8_t,4>,6> zoom{{
        {{4,4,4,4}},{{4,4,4,4}},{{4,4,4,4}},
        {{4,4,4,4}},{{4,4,4,4}},{{4,4,4,4}}
    }};
    static constexpr std::array<float,6> kTimeZoomFactors{
        0.5f,1.0f,2.0f,4.0f,8.0f,16.0f
    };
    float zoomFactor()const{
        return kTimeZoomFactors[zoom[op][page]];
    }
    float pixelsPerSecond()const{
        return 320.0f*zoomFactor();
    }
    std::string zoomLabel()const{
        const float factor=zoomFactor();
        return factor<1.0f?"0.5x":std::to_string(static_cast<int>(factor))+"x";
    }
    // Zoom in or out one level, keeping the visible top time unchanged.
    // Boundaries do not wrap around to the opposite magnification.
    void adjustTimeZoom(int direction){
        if(page>=4||direction==0)return;
        auto& index=zoom[op][page];
        const int next=std::clamp(static_cast<int>(index)+direction,0,
            static_cast<int>(kTimeZoomFactors.size())-1);
        if(next==index)return;
        const float topSeconds=scroll[op][page]/pixelsPerSecond();
        index=static_cast<uint8_t>(next);
        const float maxScroll=std::max(0.0f,120.0f*pixelsPerSecond()-rect(11).h);
        scroll[op][page]=std::clamp(topSeconds*pixelsPerSecond(),0.0f,maxScroll);
    }
    std::chrono::steady_clock::time_point started{};
    Rect view{};
    Rect rect(int row,int index=0) const {
        const float x=view.x+10,w=view.w-20;
        // The Synth page owns the fixed tab strip; no legacy header is needed here.
        const float y=view.y+12;
        switch(row){
            case 0: {const float ww=(w-25)/6;return {x+index*(ww+5),y,ww,54};}
            // Wider power/phase region makes both side-by-side buttons legible.
            case 1: return {x,y+64,w*.31f,60}; // waveform
            case 2: return {x+w*.31f+6,y+64,w*.20f-6,60}; // ratio
            case 3: return {x+w*.51f+6,y+64,w*.16f-6,60}; // semitone
            case 4: return {x+w*.67f+6,y+64,w*.33f-6,60}; // OP / Phase
            case 5: return {x,y+132,(w-16)/3,45}; // level
            case 6: return {x+(w-16)/3+8,y+132,(w-16)/3,45}; // fine cents
            case 12: return {x+2*((w-16)/3+8),y+132,(w-16)/3,45}; // phase degrees
            case 7: {const int n=5;const float ww=(w-(n-1)*6)/n;return {x+index*(ww+6),y+184,ww,52};}
            case 8: {const float ww=(w-30)/6;return {x+index*(ww+6),y+244,ww,48};}
            case 9: {
                // Bound the graph from its ACTUAL top, not the preceding
                // control row. The old height overran the viewport by 301px
                // and drew Mod endpoints into Android's navigation region.
                const float graphTop=y+301.0f;
                const float footerStart=view.y+view.h-63.0f;
                // Keep the patch-file label and buttons clear of the graph,
                // while preserving as much vertical editing space as fits.
                const float graphBottom=footerStart-72.0f;
                return {x,graphTop,w,std::max(0.0f,graphBottom-graphTop)};
            } // graph ends inside editor, above its fixed patch footer
            case 10:{auto r=rect(9);const float ww=(r.w-22)/4;return {r.x+4+index*(ww+6),r.y+3,ww,43};}
            case 11:{
                auto r=rect(9);
                return {r.x+42,r.y+57,std::max(0.0f,r.w-67),
                    std::max(0.0f,r.h-78)};
            }
            case 14:{ // Both selectors share the Wave / Morph row.
                const auto first=rect(8,2),last=rect(8,5);
                return {first.x,first.y,last.x+last.w-first.x,first.h};
            }
            case 15:{const auto r=rect(14);return {r.x,r.y,(r.w-6)*.5f,r.h};}
            case 16:{const auto r=rect(14);return {r.x+(r.w+6)*.5f,r.y,(r.w-6)*.5f,r.h};}
        }
        return {};
    }
    // Full-height, side-by-side touch targets replace the cramped stack.
    Rect powerRect()const{
        const auto r=rect(4);
        return {r.x,r.y,(r.w-4.0f)*.5f,r.h};
    }
    Rect phaseRect()const{
        const auto r=rect(4);
        return {r.x+(r.w+4.0f)*.5f,r.y,(r.w-4.0f)*.5f,r.h};
    }
    Rect modDestinationRect(int index)const {
        const auto graph=rect(9);
        const float w=(graph.w-28.0f)/5.0f;
        return {graph.x+4.0f+index*(w+5.0f),graph.y+3.0f,w,43.0f};
    }
    float harmonicRowHeight()const{return std::max(104.0f,rect(11).h*.25f);}
    void scrollBy(float dy){
        if(page<4){
            auto& amount=scroll[op][page];
            amount=std::clamp(amount+dy,0.0f,std::max(0.0f,120.0f*pixelsPerSecond()-rect(11).h));
        }else if(page==5){
            auto& amount=harmonicScroll[op][harmonicView==2?1:0];
            amount=std::clamp(amount+dy,0.0f,
                std::max(0.0f,8.0f*harmonicRowHeight()-rect(11).h+10.0f));
        }
    }
    static bool pitchPage(int which){return which==1||which==3;}
    OperatorCurve env(const Operator&o) const {
        if(page==1)return o.pitchCurve.enabled?o.pitchCurve:initialOperatorPitch();
        if(page==2)return o.volumeMorphCurve.enabled?o.volumeMorphCurve:
            (o.volumeCurve.enabled?o.volumeCurve:initialOperatorVolume());
        if(page==3)return o.pitchMorphCurve.enabled?o.pitchMorphCurve:
            (o.pitchCurve.enabled?o.pitchCurve:initialOperatorPitch());
        return o.volumeCurve.enabled?o.volumeCurve:initialOperatorVolume();
    }
    OperatorModChannel channel(const Operator&o) const {
        // A previously configured operator may contain empty Mod slots
        // (for example when imported from an older patch). They must still
        // expose the same editable 0-to-100 response as a new operator.
        // Preserve the slot's enabled state, source and destination.
        auto c=o.modChannelsConfigured?
            o.modChannels[modSlot]:initialOperatorModChannel();
        if(c.response.count<2){
            c.response.count=2;
            c.response.points[0]={0.0f,0.0f};
            c.response.points[1]={1.0f,1.0f};
        }
        return c;
    }
    OperatorResponse response(const Operator&o) const {
        return channel(o).response;
    }
    bool setChannel(OperatorModChannel c){
        return ProjectCore::instance().setSelectedOperatorModChannel(op,modSlot,c);
    }
    std::pair<float,float> position(const OperatorPoint&pt)const{
        auto r=rect(11);
        if(page==4)return {r.x+std::clamp(pt.time,0.0f,1.0f)*r.w,
            r.y+std::clamp(pt.value,0.0f,1.0f)*r.h};
        const float v=pitchPage(page)?(pt.value+12)/24:pt.value;
        return {r.x+std::clamp(v,0.0f,1.0f)*r.w,
            r.y+pt.time*pixelsPerSecond()-scroll[op][page]};
    }
    bool hitRatio(float x,float y) const {return rect(2).contains(x,y);}
    bool hitWave2Ratio(float x,float y) const {
        return page==5&&rect(10,3).contains(x,y);
    }
    bool hitSemi(float x,float y) const {return rect(3).contains(x,y);}
    bool hitWave(float x,float y) const {return rect(1).contains(x,y);}
    void draw() const {
        const Patch p=ProjectCore::instance().selectedPatch();
        const auto&o=p.ops[op];
        auto&ov=NativeOverlay::instance();
        auto text=[&](std::string_view label,Rect r,float scale=.87f,NativeOverlay::Color color=overlayColor(kWhite)){
            ov.addTextCentered(label,{r.x,r.y,r.w,r.h},scale,color);
        };
        auto button=[&](Rect r,bool active,bool purple=false){
            ov.addRect({r.x,r.y,r.w,r.h},overlayColor(active?(purple?kPurple:kCyan):kButton));
        };
        for(int i=0;i<6;++i){
            auto r=rect(0,i);button(r,i==op);
            text("OP"+std::to_string(i+1),r,.84f,overlayColor(i==op?kBg:kWhite));
        }
        auto field=[&](Rect r,std::string_view label,std::string value){
            ov.addRect({r.x,r.y,r.w,r.h},overlayColor(kButton));
            ov.addText(label,r.x+7,r.y+5,.7f,overlayColor(kMuted));
            ov.addText(value,r.x+7,r.y+26,.91f,overlayColor(kWhite));
        };
        char buff[32];
        field(rect(1),"Wave / Edit",
            std::string(kWaveformLabels[std::clamp(static_cast<int>(o.wave),0,10)])+
            (isTextureWave(o.wave)?(o.texturePitched?" · Pitch":" · Random"):""));
        std::snprintf(buff,sizeof(buff),"%.4g",static_cast<double>(o.ratio));field(rect(2),"Ratio",buff);
        std::snprintf(buff,sizeof(buff),"%+.0f",static_cast<double>(o.semitoneOffset));field(rect(3),"Base ST",buff);
        if(page==4){
            const auto ch=channel(o);
            button(powerRect(),true);
            const char* src=ch.response.source==2?"Src K":
                ch.response.source==1?"Src M":"Src V";
            text(src,powerRect(),.76f,overlayColor(kBg));
            button(phaseRect(),ch.enabled,true);
            text(ch.enabled?"Mod ON":"Mod OFF",phaseRect(),.66f,
                overlayColor(ch.enabled?kBg:kWhite));
        }else{
            button(powerRect(),o.enabled);
            text(o.enabled?"OP ON":"OP OFF",powerRect(),.66f,
                overlayColor(o.enabled?kBg:kWhite));
            button(phaseRect(),o.randomPhase,true);
            text(o.randomPhase?"PH RND":"PH FIX",phaseRect(),.66f,
                overlayColor(o.randomPhase?kBg:kWhite));
        }
        for(int i=0;i<2;++i){
            auto r=rect(5+i);button(r,false);
            const float v=i?((o.detuneCents+50)/100):o.level;
            ov.addText(i?"Fine":"Level",r.x+5,r.y+8,.73f,overlayColor(kWhite));
            ov.addRect({r.x+r.w*.34f,r.y+27,r.w*.60f,5},overlayColor(kMuted));
            ov.addRect({r.x+r.w*.34f,r.y+27,r.w*.60f*std::clamp(v,0.0f,1.0f),5},overlayColor(kCyan));
        }
        // Per-operator symmetric range: fine adjustment near zero, full
        // 360-degree coverage at +/-180. Tap or drag the third slider.
        {
            const auto r=rect(12);
            button(r,false);
            const int degrees=static_cast<int>(std::round(o.randomPhaseDegrees));
            ov.addText("Phase +/-"+std::to_string(degrees),
                r.x+5,r.y+7,.75f,overlayColor(kWhite));
            const float amount=std::sqrt(std::clamp(o.randomPhaseDegrees/180.0f,0.0f,1.0f));
            ov.addRect({r.x+8,r.y+27,r.w-16,5},overlayColor(kMuted));
            ov.addRect({r.x+8,r.y+27,(r.w-16)*amount,5},overlayColor(kPurple));
        }
        for(int i=0;i<5;++i){
            auto r=rect(7,i);button(r,i==page,i>=2);
            const char* tab=i==0?"Vol":i==1?"Pitch":i==2?"Morph":
                i==3?"P Morph":"Mod";
            text(tab,r,i==3?.77f:.9f,overlayColor(i==page?kBg:kWhite));
        }
        for(int i=0;i<6;++i){
            auto r=rect(8,i);
            if(page==5&&i>1)continue;
            const bool active=page==5?(harmonicView==i+1):
                page==4?(modSlot==i):
                (i!=op&&(shadows&(1u<<i)));
            button(r,active,true);
            const std::string name=page==5?(i?"Wave Morph":"Wave"):
                page==4?("Mod "+std::to_string(i+1)):
                ("OP"+std::to_string(i+1));
            text(name,r,.74f,overlayColor(active?kBg:kWhite));
        }
        auto graph=rect(9),plot=rect(11);
        ov.addRect({graph.x,graph.y,graph.w,graph.h},overlayColor(kRollBg));
        if(page<4){
            const auto c=env(o);
            for(int i=0;i<3;++i){
                const auto r=rect(10,i);
                button(r,i==2&&c.sustain);
                text(i==0?"Decay":i==1?"Release":
                    (c.sustain?"Sustain":"Dynamic"),r,.76f);
            }
            // The old Delete slot is a three-part time zoom control:
            // [−] [current magnification] [+]. Both directions are
            // independently tappable and disabled at their limits.
            const auto r=rect(10,3);
            const float side=r.w*.29f;
            const Rect minus{r.x,r.y,side,r.h};
            const Rect plus{r.x+r.w-side,r.y,side,r.h};
            const Rect value{r.x+side+2.0f,r.y,
                std::max(0.0f,r.w-2.0f*side-4.0f),r.h};
            const int zoomIndex=zoom[op][page];
            button(minus,zoomIndex>0);
            button(plus,zoomIndex+1<static_cast<int>(kTimeZoomFactors.size()));
            button(value,false);
            text("-",minus,1.2f);
            text(zoomLabel(),value,.85f);
            text("+",plus,1.2f);
            ov.addText(pitchPage(page)?"-12          0           +12 ST":"0              Volume            100%",
                plot.x,plot.y-18,.8f,overlayColor(kMuted));
        }else if(page==4){
            for(int i=0;i<5;++i){
                const auto r=modDestinationRect(i);
                const bool active=static_cast<int>(channel(o).destination)==i;
                button(r,active,true);
                const char* label=i==0?"Vol":i==1?"Pitch":
                    i==2?"Morph":i==3?"P Morph":"W Morph";
                text(label,r,i==4?.69f:.77f,overlayColor(active?kBg:kWhite));
            }
            ov.addText("Output 0-100%  /  Input V, M or K",
                plot.x,plot.y-18,.8f,overlayColor(kMuted));
        }else{
            const Wave bank=harmonicView==2?effectiveWaveMorph(o):o.wave;
            const bool texture=isTextureWave(bank);
            const bool pitched=harmonicView==2&&o.waveMorphConfigured?
                o.textureMorphPitched:o.texturePitched;
            const auto preset=rect(15),textPreset=rect(16);
            button(preset,!texture,true);
            button(textPreset,texture,true);
            text(std::string("Wave: ")+
                 (texture?std::string("Select"):kWaveformLabels[
                    std::clamp(static_cast<int>(bank),0,4)])+" v",
                 preset,.72f,overlayColor(!texture?kBg:kWhite));
            text(std::string("Texture: ")+
                (texture?kWaveformLabels[std::clamp(static_cast<int>(bank),0,10)]:
                 std::string("Select"))+" v",
                textPreset,.72f,overlayColor(texture?kBg:kWhite));
            // Shared Wave Interaction selector and independent Wave 2 ratio.
            const auto modeRect=rect(10,2),ratioRect=rect(10,3);
            button(modeRect,false,true);
            text(std::string("Mode ")+kWaveInteractionLabels[
                    static_cast<size_t>(o.waveInteraction)]+" v",
                modeRect,.71f);
            button(ratioRect,false,true);
            char ratioText[32];
            std::snprintf(ratioText,sizeof(ratioText),"%.6g",
                static_cast<double>(o.waveMorphRatio));
            ov.addText("W2 Ratio",ratioRect.x+7.0f,ratioRect.y+4.0f,
                .68f,overlayColor(kMuted));
            ov.addText(ratioText,ratioRect.x+7.0f,ratioRect.y+21.0f,
                .86f,overlayColor(kWhite));
            for(int mode=0;mode<2;++mode){
                const auto r=rect(10,mode);
                const bool active=texture?(mode==0?pitched:bank==Wave::TextureCustom):
                    (mode==1)==harmonicPhaseView;
                button(r,active,true);
                const char* label=texture?
                    (mode==0?(pitched?"Pitched":"Random"):
                     bank==Wave::TextureCustom?"Source +":"Texture"):
                    mode==0?"Amplitude":"Phase";
                text(label,r,.73f,overlayColor(active?kBg:kWhite));
            }
            ov.addText(texture?
                (bank==Wave::TextureCustom?"Custom procedural texture controls":
                 "Texture: harmonic amplitudes and phases disabled"):
                (harmonicPhaseView?"Phase: 0-100% of cycle":"Amplitude: 0-100%"),
                plot.x,plot.y-18,.73f,overlayColor(kMuted));
        }
        ov.setClip({plot.x-11,plot.y-9,plot.w+22,plot.h+18});
        for(int i=0;i<=4;++i){
            float x=plot.x+i*plot.w*.25f;
            ov.addLine(x,plot.y,x,plot.y+plot.h,1,overlayColor(kMuted,.5f));
            if(page==4){
                float y=plot.y+i*plot.h*.25f;
                ov.addLine(plot.x,y,plot.x+plot.w,y,1,overlayColor(kMuted,.5f));
            }
        }
        if(page==5){
            const Wave bank=harmonicView==2?effectiveWaveMorph(o):o.wave;
            if(isTextureWave(bank)){
                const auto& params=harmonicView==2&&o.waveMorphConfigured?
                    o.textureMorph:o.texture;
                const float levels[5]{
                    params.density,params.size,params.color,
                    params.resonance,params.chaos};
                static constexpr const char* names[5]{
                    "Density","Size","Color","Resonance","Chaos"};
                if(bank==Wave::TextureCustom){
                    for(int i=0;i<5;++i){
                        const float rowH=plot.h/5.0f;
                        const float py=plot.y+i*rowH;
                        const float barX=plot.x+std::min(100.0f,plot.w*.32f);
                        const float barW=std::max(20.0f,plot.x+plot.w-barX-13.0f);
                        ov.addText(names[i],plot.x+3,py+rowH*.28f,
                            .72f,overlayColor(kWhite));
                        ov.addRect({barX,py+rowH*.50f,barW,6},overlayColor(kMuted));
                        ov.addRect({barX,py+rowH*.50f,barW*levels[i],6},
                            overlayColor(kPurple));
                        ov.addFilledCircle(barX+barW*levels[i],
                            py+rowH*.50f+3,7,overlayColor(kWhite));
                    }
                    static constexpr const char* sources[4]{
                        "Noise","Grains","Impulses","Friction"};
                    ov.addText(std::string("Source: ")+
                        sources[std::min<int>(3,params.source)],
                        plot.x+3,plot.y+plot.h-18,.70f,overlayColor(kCyan));
                }else{
                    ov.addText("Pitched or Random texture",
                        plot.x+12,plot.y+20,.86f,overlayColor(kWhite));
                    ov.addText("Select Custom for editable texture parameters",
                        plot.x+12,plot.y+55,.72f,overlayColor(kMuted));
                }
                ov.clearClip();return;
            }
            const bool editable=true;
            const auto amplitude=(bank==Wave::Custom&&
                (harmonicView==2?o.hasHarmMute:o.hasHarm))?
                (harmonicView==2?o.harmMute:o.harm):presetWaveHarmonics(bank);
            const auto phase=(bank==Wave::Custom&&
                (harmonicView==2?o.hasHarmMute:o.hasHarm))?
                (harmonicView==2?o.harmMutePhase:o.harmPhase):presetWavePhases(bank);
            for(int i=0;i<static_cast<int>(kEditableHarmonics);++i){
                const int col=i%8,row=i/8;
                const float w=plot.w/8.0f,h=harmonicRowHeight();
                const float x=plot.x+col*w,
                    y=plot.y+row*h-harmonicScroll[op][harmonicView==2?1:0];
                const float strength=std::clamp(amplitude[static_cast<size_t>(i)],0.0f,1.0f);
                const float phasePos=std::clamp(phase[static_cast<size_t>(i)],0.0f,1.0f);
                const float barHeight=h-38.0f,barBottom=y+h-8.0f;
                ov.addRect({x+3,y+4,w-6,h-8},overlayColor(kButton));
                // Both properties are always visible; whichever is not
                // editable remains as its own translucent shape underneath.
                ov.addRect({x+w*.26f,barBottom-barHeight*strength,
                    w*.48f,barHeight*strength},
                    overlayColor(!editable?kMuted:(harmonicView==2?kPurple:kCyan),
                        harmonicPhaseView?0.25f:1.0f));
                const float marker=barBottom-barHeight*phasePos;
                ov.addRect({x+w*.09f,marker-2.0f,w*.82f,4.0f},
                    overlayColor(!editable?kMuted:kWhite,
                        harmonicPhaseView?1.0f:0.26f));
                ov.addTextCentered(std::to_string(i+1),
                    {x,y+h-34,w,27},.67f,overlayColor(kWhite));
            }
            ov.clearClip();return;
        }
        if(page<4){
            // Adaptive timeline ticks. Zooming reveals millisecond detail,
            // and grid labels always represent exact seconds (no snapping).
            const float scale=pixelsPerSecond();
            constexpr std::array<float,14> steps{
                .001f,.002f,.005f,.01f,.02f,.05f,.1f,.2f,
                .5f,1.0f,2.0f,5.0f,10.0f,20.0f
            };
            float tick=steps.back();
            for(float candidate:steps){
                if(candidate*scale>=78.0f){tick=candidate;break;}
            }
            const float scrollStart=scroll[op][page];
            const int first=std::max(0,static_cast<int>(
                std::floor(scrollStart/(scale*tick))));
            const int last=std::min(static_cast<int>(120.0f/tick),
                static_cast<int>(std::ceil((scrollStart+plot.h)/(scale*tick))));
            for(int i=first;i<=last;++i){
                const float seconds=i*tick;
                const float y=plot.y+seconds*scale-scrollStart;
                if(y<plot.y||y>plot.y+plot.h)continue;
                ov.addLine(plot.x,y,plot.x+plot.w,y,1,overlayColor(kMuted,.3f));
                if(seconds<1.0f){
                    std::snprintf(buff,sizeof(buff),"%.0fms",seconds*1000.0f);
                }else{
                    std::snprintf(buff,sizeof(buff),"%.2gs",static_cast<double>(seconds));
                }
                ov.addText(buff,plot.x+3,y+4,.65f,overlayColor(kMuted));
            }
        }
        auto paint=[&](const OperatorPoint*pts,int count,bool ghost){
            if(count<2)return;
            for(int i=1;i<count;++i)for(int k=1;k<=24;++k){
                float t0=pts[i-1].time+(pts[i].time-pts[i-1].time)*(k-1)/24.0f;
                float t1=pts[i-1].time+(pts[i].time-pts[i-1].time)*k/24.0f;
                OperatorPoint a{t0,operatorCurveValue(pts+i-1,2,t0)};
                OperatorPoint b{t1,operatorCurveValue(pts+i-1,2,t1)};
                auto x=position(a),y=position(b);
                ov.addLine(x.first,x.second,y.first,y.second,ghost?2:3.5f,
                    overlayColor(ghost?kPurple:(page==4?kPurple:pitchPage(page)?kCyan:kGreen),ghost?.32f:1.0f));
            }
        };
        auto paintHandles=[&](const OperatorPoint* pts,int count){
            for(int i=1;i<count;++i){
                const auto&prev=pts[i-1],&cur=pts[i];
                const float middle=prev.time+(cur.time-prev.time)*
                    std::clamp(cur.midpoint,.05f,.95f);
                const OperatorPoint handle{
                    middle,operatorCurveValue(pts+i-1,2,middle,prev.value)};
                const auto xy=position(handle);
                constexpr float d=6.5f;
                ov.addLine(xy.first-d,xy.second-d,xy.first+d,xy.second+d,2.3f,overlayColor(kOrange));
                ov.addLine(xy.first-d,xy.second+d,xy.first+d,xy.second-d,2.3f,overlayColor(kOrange));
            }
        };
        if(page<4){
            for(int i=0;i<6;++i)if(i!=op&&(shadows&(1u<<i))){
                const auto&so=p.ops[i];
                const auto c=env(so);
                paint(c.points.data(),c.count,true);
            }
            auto c=env(o);paint(c.points.data(),c.count,false);
            paintHandles(c.points.data(),c.count);
            for(int i=0;i<c.count;++i){
                auto xy=position(c.points[i]);
                ov.addFilledCircle(xy.first,xy.second,i==selected?9:7,overlayColor(i==selected?kOrange:kWhite));
                // Marker text must remain visible at 0% and 100% volume
                // (and at +/-12 ST). Flip it left of right-edge points.
                // Distinct offsets also distinguish D and S/R if both
                // happen to share one point in an older saved patch.
                const float labelX=std::clamp(
                    xy.first>(plot.x+plot.w-35.0f)?xy.first-23.0f:xy.first+12.0f,
                    plot.x+4.0f,plot.x+plot.w-18.0f);
                const auto labelY=[&](float offset){
                    return std::clamp(xy.second+offset,
                        plot.y+5.0f,plot.y+plot.h-18.0f);
                };
                if(i==c.decayIndex)
                    ov.addText("D",labelX,labelY(8.0f),.82f,overlayColor(kCyan));
                if(i==c.releaseIndex)
                    ov.addText(c.sustain?"S":"R",labelX,labelY(-22.0f),
                        .82f,overlayColor(kOrange));
                if(pitchPage(page)&&c.points[i].free)ov.addCircle(xy.first,xy.second,12,1.5f,overlayColor(kPurple));
            }
        }else if(page==4){
            auto c=response(o);paint(c.points.data(),c.count,false);
            paintHandles(c.points.data(),c.count);
            for(int i=0;i<c.count;++i){
                auto xy=position(c.points[i]);
                ov.addFilledCircle(xy.first,xy.second,i==selected?9:7,overlayColor(i==selected?kOrange:kWhite));
            }
        }
        ov.clearClip();
    }
    bool updatePoint(int idx,float x,float y,bool handle){
        auto& core=ProjectCore::instance();
        const auto o=core.selectedPatch().ops[op];
        auto r=rect(11);
        float xval=std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f);
        if(page==4){
            auto channelCopy=channel(o);
            auto& c=channelCopy.response;
            if(idx<0||idx>=c.count)return false;
            auto&pt=c.points[idx];
            if(handle){
                if(idx<1)return false;
                const auto&before=c.points[idx-1];
                pt.midpoint=std::clamp((x-r.x-r.w*before.time)/
                    std::max(.00001f,r.w*(pt.time-before.time)),.05f,.95f);
                pt.handleActive=true;
                pt.handleValue=std::clamp((y-r.y)/r.h,
                    std::min(before.value,pt.value),std::max(before.value,pt.value));
            }else{
                pt.time=std::clamp(xval,idx?c.points[idx-1].time+.0001f:0.0f,
                    idx+1<c.count?c.points[idx+1].time-.0001f:1.0f);
                pt.value=std::clamp((y-r.y)/r.h,0.0f,1.0f);
            }
            c.enabled=true;
            channelCopy.enabled=true;
            return setChannel(channelCopy);
        }
        auto c=env(o);if(idx<0||idx>=c.count)return false;
        auto&pt=c.points[idx];
        float val=pitchPage(page)?-12+24*xval:xval;
        if(handle){
            if(idx<1)return false;
            const auto&before=c.points[idx-1];
            const float time=(y-r.y+scroll[op][page])/pixelsPerSecond();
            pt.midpoint=std::clamp(
                (time-before.time)/std::max(.00001f,pt.time-before.time),
                .05f,.95f);
            pt.handleActive=true;
            pt.handleValue=std::clamp(val,
                std::min(before.value,pt.value),std::max(before.value,pt.value));
        }else{
            float time=std::max(0.0f,(y-r.y+scroll[op][page])/pixelsPerSecond());
            pt.time=idx==0?0:std::clamp(time,c.points[idx-1].time+.0001f,
                idx+1<c.count?c.points[idx+1].time-.0001f:120.0f);
            pt.value=pitchPage(page)&&!pt.free?std::round(val):val;
        }
        return core.setSelectedOperatorCurve(op,page,c);
    }
    bool addPoint(float x,float y){
        auto&core=ProjectCore::instance();
        const auto o=core.selectedPatch().ops[op];
        auto r=rect(11);
        float xval=std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f);
        OperatorPoint pt{};
        if(page==4){
            auto channelCopy=channel(o);
            auto& c=channelCopy.response;
            if(c.count>=kMaxOperatorPoints)return false;
            pt.time=std::clamp(xval,.0001f,.9999f);
            pt.value=std::clamp((y-r.y)/r.h,0.0f,1.0f);
            int n=0;while(n<c.count&&c.points[n].time<pt.time)++n;
            for(int k=c.count;k>n;--k)c.points[k]=c.points[k-1];
            c.points[n]=pt;++c.count;selected=n;c.enabled=true;
            channelCopy.enabled=true;
            return setChannel(channelCopy);
        }
        auto c=env(o);if(c.count>=kMaxOperatorPoints)return false;
        pt.time=std::clamp((y-r.y+scroll[op][page])/pixelsPerSecond(),.0001f,119.999f);
        pt.value=pitchPage(page)?std::round(-12+24*xval):xval;
        int n=0;while(n<c.count&&c.points[n].time<pt.time)++n;
        // Extending the timeline beyond the last point keeps its silent
        // terminal endpoint, moving it past the new point instead of
        // preventing longer percussion or evolving envelopes.
        if(n==c.count&&c.count<kMaxOperatorPoints){
            c.points[c.count]=c.points[c.count-1];
            c.points[c.count].time=std::min(120.0f,pt.time+.25f);
            c.points[c.count-1]=pt;
            selected=c.count-1;++c.count;
            return core.setSelectedOperatorCurve(op,page,c);
        }
        if(n==0||n==c.count||pt.time-c.points[n-1].time<.0001f||c.points[n].time-pt.time<.0001f)return false;
        for(int k=c.count;k>n;--k)c.points[k]=c.points[k-1];
        c.points[n]=pt;++c.count;if(c.decayIndex>=n)++c.decayIndex;
        if(c.releaseIndex>=n)++c.releaseIndex;
        selected=n;return core.setSelectedOperatorCurve(op,page,c);
    }
    bool down(float x,float y){
        downX=x;downY=lastY=y;started=std::chrono::steady_clock::now();
        moved=panning=changed=false;drag=-1;bend=-1;
        for(int i=0;i<6;++i)if(rect(0,i).contains(x,y)){op=i;selected=-1;return true;}
        auto&core=ProjectCore::instance();
        const auto o=core.selectedPatch().ops[op];
        custom=o.wave==Wave::Custom;
        if(rect(1).contains(x,y)){
            page=5;harmonicView=1;selected=-1;return true;
        }
        if(powerRect().contains(x,y)){
            if(page==4){
                auto c=channel(o);
                c.response.source=static_cast<uint8_t>((c.response.source+1)%3);
                changed=setChannel(c);
            }else changed=core.setSelectedOperatorEnabled(op,!o.enabled);
            return true;
        }
        if(phaseRect().contains(x,y)){
            if(page==4){
                auto c=channel(o);c.enabled=!c.enabled;
                changed=setChannel(c);
            }else changed=core.setSelectedOperatorRandomPhase(op,!o.randomPhase);
            return true;
        }
        for(int i=0;i<2;++i)if(rect(5+i).contains(x,y)){drag=-10-i;return move(x,y);}
        if(rect(12).contains(x,y)){drag=-12;return move(x,y);}
        for(int i=0;i<5;++i)if(rect(7,i).contains(x,y)){
            page=i;selected=-1;
            if(page==5)harmonicView=1;
            return true;
        }
        for(int i=0;i<6;++i)if(rect(8,i).contains(x,y)){
            if(page==5){
                if(i<2)harmonicView=i+1;
            }else if(page==4){
                modSlot=i;selected=-1;
            }else if(i!=op)shadows^=1u<<i;
            return true;
        }
        if(page==4)for(int i=0;i<5;++i)if(modDestinationRect(i).contains(x,y)){
            auto c=channel(o);
            c.destination=static_cast<OperatorModDestination>(i);
            c.enabled=true;
            changed=setChannel(c);
            return true;
        }
        if(page<4)for(int i=0;i<4;++i)if(rect(10,i).contains(x,y)){
            // Two dedicated zoom targets; tapping the central readout
            // leaves the time scale unchanged. Point deletion remains
            // the quick-tap gesture used by the piano-roll curves.
            if(i==3){
                const auto r=rect(10,3);
                const float side=r.w*.29f;
                if(x<r.x+side)adjustTimeZoom(-1);
                else if(x>=r.x+r.w-side)adjustTimeZoom(1);
                return true;
            }
            auto c=env(o);
            if(i==2)c.sustain=!c.sustain;
            else if(selected>=0&&selected<c.count-1){
                if(i==0)c.decayIndex=selected;
                else c.releaseIndex=selected;
            }else return true;
            changed=core.setSelectedOperatorCurve(op,page,c);return true;
        }
        if(page==5){
            // W2 Ratio opens Android's numeric editor before pointerDown,
            // just like the primary operator Ratio field.
            if(rect(10,3).contains(x,y))return true;
            const Wave bank=harmonicView==2?effectiveWaveMorph(o):o.wave;
            const bool morph=harmonicView==2;
            if(isTextureWave(bank)){
                if(rect(10,0).contains(x,y)){
                    const bool pitched=morph&&o.waveMorphConfigured?
                        o.textureMorphPitched:o.texturePitched;
                    changed=core.setSelectedTexturePitched(op,!pitched,morph);
                    return true;
                }
                if(rect(10,1).contains(x,y)){
                    if(bank==Wave::TextureCustom){
                        const auto& params=morph&&o.waveMorphConfigured?
                            o.textureMorph:o.texture;
                        changed=core.setSelectedTextureSource(
                            op,(static_cast<int>(params.source)+1)%4,morph);
                    }
                    return true;
                }
                const auto plot=rect(11);
                if(plot.contains(x,y)){
                    if(bank==Wave::TextureCustom){
                        const int row=std::clamp(static_cast<int>(
                            (y-plot.y)/std::max(1.0f,plot.h)*5.0f),0,4);
                        drag=-100-row;
                        return move(x,y);
                    }
                    return true;
                }
            }else{
                if(rect(10,0).contains(x,y)){harmonicPhaseView=false;return true;}
                if(rect(10,1).contains(x,y)){harmonicPhaseView=true;return true;}
                if(rect(11).contains(x,y)){
                    auto plot=rect(11);
                    const int col=std::clamp(static_cast<int>((x-plot.x)/plot.w*8),0,7);
                    const float relativeY=y-plot.y+harmonicScroll[op][harmonicView==2?1:0];
                    const int row=std::clamp(static_cast<int>(relativeY/harmonicRowHeight()),0,7);
                    drag=-50-(row*8+col);return move(x,y);
                }
            }
        }
        const auto gr=rect(11);
        const bool inside=gr.contains(x,y);
        if(!inside&&(x<gr.x-20.0f||x>gr.x+gr.w+20.0f||
                     y<gr.y-20.0f||y>gr.y+gr.h+20.0f))return false;
        const auto c=env(o);
        const auto e=response(o);
        const auto*pts=page==4?e.points.data():c.points.data();
        const int count=page==4?e.count:c.count;
        // Every visible orange X is independently draggable without
        // selecting an endpoint first. Pick the closest eligible target
        // rather than letting a 29 px node hitbox mask its curve handle.
        float pointDistance=1.0e9f;
        int pointCandidate=-1;
        for(int i=0;i<count;++i){
            const auto at=position(pts[i]);
            const float dx=x-at.first,dy=y-at.second;
            const float d2=dx*dx+dy*dy;
            const float radius=(i==0||i==count-1)?37.0f:25.0f;
            if(d2<radius*radius&&d2<pointDistance){
                pointDistance=d2;pointCandidate=i;
            }
        }
        if(pointCandidate==0||pointCandidate==count-1){
            selected=drag=pointCandidate;return true;
        }
        float handleDistance=22.0f*22.0f;
        int handleCandidate=-1;
        for(int i=1;i<count;++i){
            const float t=pts[i-1].time+(pts[i].time-pts[i-1].time)*
                std::clamp(pts[i].midpoint,.05f,.95f);
            const auto at=position({t,operatorCurveValue(pts+i-1,2,t)});
            const float dx=x-at.first,dy=y-at.second;
            const float d2=dx*dx+dy*dy;
            if(d2<handleDistance){handleDistance=d2;handleCandidate=i;}
        }
        if(handleCandidate>=0&&
           (pointCandidate<0||handleDistance<=pointDistance)){
            bend=handleCandidate;
            return true;
        }
        if(pointCandidate>=0){selected=drag=pointCandidate;return true;}
        if(!inside)return true;
        drag=-2;return true;
    }
    bool move(float x,float y){
        if(drag<=-100&&drag>=-104){
            const int param=-100-drag;
            const auto r=rect(11);
            const float barX=r.x+std::min(100.0f,r.w*.32f);
            const float barW=std::max(20.0f,r.x+r.w-barX-13.0f);
            changed=ProjectCore::instance().setSelectedTextureParam(
                op,param,std::clamp((x-barX)/barW,0.0f,1.0f),
                harmonicView==2)||changed;
            return true;
        }
        if(drag==-2){
            // Graph gestures edit points and X handles only. Vertical
            // scrolling belongs exclusively to the colored side gutter.
            if(std::fabs(x-downX)+std::fabs(y-downY)>13)moved=true;
            return true;
        }
        if(drag<=-50){
            const int i=-drag-50,row=i/8;auto r=rect(11);
            const float h=harmonicRowHeight();
            const float top=r.y+row*h-harmonicScroll[op][harmonicView==2?1:0];
            const float amount=std::clamp(
                (top+h-8.0f-y)/std::max(1.0f,h-38.0f),0.0f,1.0f);
            auto& project=ProjectCore::instance();
            changed=(harmonicPhaseView?
                project.setSelectedHarmonicPhase(op,i,amount,harmonicView==2):
                project.setSelectedHarmonic(op,i,amount,harmonicView==2))||changed;
            return true;
        }
        if(drag==-12){
            const auto r=rect(12);
            const float pos=std::clamp((x-r.x-8.0f)/std::max(1.0f,r.w-16.0f),0.0f,1.0f);
            const float degrees=std::round(180.0f*pos*pos);
            changed=ProjectCore::instance().setSelectedOperatorRandomPhaseDegrees(op,degrees)||changed;
            return true;
        }
        if(drag<=-10){
            auto r=rect(drag==-10?5:6);
            float v=std::clamp((x-r.x-r.w*.34f)/(r.w*.60f),0.0f,1.0f);
            changed=ProjectCore::instance().setSelectedOperatorParam(op,drag==-10?OperatorParam::Level:OperatorParam::Detune,
                drag==-10?v:std::round(v*100-50))||changed;
            return true;
        }
        if(drag>=0||bend>=0){
            if(std::fabs(x-downX)+std::fabs(y-downY)>9){
                moved=true;changed=updatePoint(bend>=0?bend:drag,x,y,bend>=0)||changed;
            }
        }
        return true;
    }
    bool up(){
        if(drag==-2&&!moved)changed=addPoint(downX,downY)||changed;
        // A tap on an X inserts a point; dragging its X changes the curve.
        if(bend>=1&&!moved)changed=addPoint(downX,downY)||changed;
        if(drag>=0&&!moved){
            auto&core=ProjectCore::instance();
            const auto o=core.selectedPatch().ops[op];
            const bool longPress=std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-started).count()>=450;
            if(pitchPage(page)&&longPress){
                auto c=env(o);
                if(drag<c.count){
                    c.points[drag].free=!c.points[drag].free;
                    if(!c.points[drag].free)c.points[drag].value=std::round(c.points[drag].value);
                    changed=core.setSelectedOperatorCurve(op,page,c)||changed;
                }
            }else if(!longPress&&page==4){
                auto channelCopy=channel(o);
                auto& c=channelCopy.response;
                if(drag>0&&drag<c.count-1){
                    for(int i=drag;i<c.count-1;++i)c.points[i]=c.points[i+1];
                    --c.count;selected=-1;
                    changed=setChannel(channelCopy)||changed;
                }
            }else if(!longPress&&page<4){
                auto c=env(o);
                if(drag>0&&drag<c.count-1){
                    for(int i=drag;i<c.count-1;++i)c.points[i]=c.points[i+1];
                    --c.count;
                    if(c.decayIndex>=drag&&c.decayIndex>0)--c.decayIndex;
                    if(c.releaseIndex>=drag&&c.releaseIndex>0)--c.releaseIndex;
                    c.decayIndex=std::min<int>(c.decayIndex,c.count-2);
                    c.releaseIndex=std::min<int>(c.releaseIndex,c.count-2);
                    selected=-1;
                    changed=core.setSelectedOperatorCurve(op,page,c)||changed;
                }
            }
        }
        drag=bend=-1;
        const bool result=changed;changed=false;return result;
    }
};
OpSurface opSurface;

struct FilterSurface {
    using Rect=NativeEditor::Rect;
    Rect view{};
    bool open{false},moved{false},changed{false};
    int page{0},lane{0},slot{0},slopeShape{2},selected{-1},drag{-1},bend{-1},transition{-1};
    float downX{},downY{},scroll{0};
    int zoom{2};
    std::chrono::steady_clock::time_point started{};
    static constexpr std::array<float,6> zooms{{.5f,1,2,4,8,16}};
    static const char* laneName(int lane){
        return lane%3==0?"Frequency":lane%3==1?"Amount":"Q / Slope";
    }
    static const char* shapeName(FilterShape shape){
        return shape==FilterShape::LowShelf?"L":shape==FilterShape::Bell?"B":"H";
    }
    float rate()const{return 320.0f*zooms[zoom];}
    Rect bar(int row,int i,int n)const{
        const float x=view.x+13,w=view.w-26;
        const float gap=6,cell=(w-gap*(n-1))/n;
        // Compact filter rows begin directly beneath the Synth tab strip.
        const float y=view.y+(row==0?13:row==1?77:row==2?140:row==3?203:266);
        return {x+i*(cell+gap),y,cell,row==0?54.0f:50.0f};
    }
    Rect graph()const{
        const float y=view.y+329;
        return {view.x+70,y,view.w-88,
            std::max(70.0f,view.y+view.h-82-y)};
    }
    Rect pane(int i)const{
        Rect r=graph();
        const float w=(r.w-12)/3;
        return {r.x+i*(w+6),r.y,w,r.h};
    }
    static bool staticLine(const OperatorCurve& c){
        return c.count==2&&c.releaseIndex==0&&
            c.points[1].time>=119.0f&&
            std::fabs(c.points[0].value-c.points[1].value)<.00001f;
    }
    float outputX(int lane,float value)const{
        const auto r=pane(lane);
        return r.x+std::clamp(value,0.0f,1.0f)*r.w;
    }
    std::pair<float,float> location(const OperatorPoint& p,int which)const{
        if(page==2){
            const auto r=graph();
            return {r.x+std::clamp(p.time,0.0f,1.0f)*r.w,
                r.y+std::clamp(p.value,0.0f,1.0f)*r.h};
        }
        const auto r=pane(which%3);
        return {r.x+std::clamp(p.value,0.0f,1.0f)*r.w,
            r.y+p.time*rate()-scroll};
    }
    bool qCurve()const{return page==3?false:page==2?
        getModel().mods[slot].destination==2:lane==2;}
    EvolvingFilter getModel()const{
        const auto m=ProjectCore::instance().selectedPatch().filter.evolving;
        return m.configured?m:initialEvolvingFilter();
    }
    int laneIndex()const{return lane+(page==1?3:0);}
    OperatorCurve getCurve(const EvolvingFilter& m)const{
        return page==2?modCurve(m.mods[slot]):m.lanes[laneIndex()].curve;
    }
    static OperatorCurve modCurve(const FilterModChannel& mod){
        OperatorCurve c{};
        c.enabled=mod.response.enabled;c.count=mod.response.count;
        c.points=mod.response.points;
        return c;
    }
    FilterShape currentShape(const EvolvingFilter& m,int n)const{
        return page==2?m.mods[slot].shape[n]:m.lanes[laneIndex()].shape[n];
    }
    float currentTransition(const EvolvingFilter& m,int n)const{
        return page==2?m.mods[slot].transition[n]:m.lanes[laneIndex()].transition[n];
    }
    void setShape(EvolvingFilter& m,int n,FilterShape shape)const{
        if(page==2)m.mods[slot].shape[n]=shape;
        else m.lanes[laneIndex()].shape[n]=shape;
    }
    void setTransition(EvolvingFilter& m,int n,float position)const{
        if(page==2)m.mods[slot].transition[n]=position;
        else m.lanes[laneIndex()].transition[n]=position;
    }
    bool save(EvolvingFilter& m,const OperatorCurve& curve){
        if(page==2){
            auto& response=m.mods[slot].response;
            response.enabled=true;
            response.count=curve.count;response.points=curve.points;
        }else{
            m.lanes[laneIndex()].curve=curve;
        }
        m.configured=true;
        return ProjectCore::instance().setSelectedEvolvingFilter(m);
    }
    void scrollBy(float delta){
        if(page==2)return;
        scroll=std::clamp(scroll+delta,0.0f,
            std::max(0.0f,120.0f*rate()-graph().h));
    }
    void draw()const{
        auto& o=NativeOverlay::instance();
        o.addRect({view.x,view.y,view.w,view.h},overlayColor(kBg));
        const auto model=getModel();
        auto cell=[&](Rect r,const std::string& label,bool active,
                      NativeEditor::Rgb color=kCyan,float size=.75f){
            o.addRect({r.x,r.y,r.w,r.h},overlayColor(active?color:kButton));
            o.addTextCentered(label,{r.x,r.y,r.w,r.h},size,
                overlayColor(active?kBg:kWhite));
        };
        if(page==2){
            const auto& mod=model.mods[slot];
            cell(bar(0,0,2),mod.response.source==2?"Source K":
                mod.response.source==1?"Source M":"Source V",true,kCyan,.85f);
            cell(bar(0,1,2),mod.enabled?"Mod ON":"Mod OFF",mod.enabled,kGreen,.86f);
        }else{
            cell(bar(0,0,2),"Frequency / Amount / Q",true,kCyan,.78f);
            cell(bar(0,1,2),model.configured?"Filter active":"Enable filter",model.configured,kGreen,.86f);
        }
        static const char* subTabs[4]={"Envelope","Morph","Mod","Slope"};
        for(int i=0;i<4;++i)cell(bar(1,i,4),subTabs[i],page==i,kPurple,.80f);
        if(page<2){
            cell(bar(2,0,3),model.keyTrack?"Key Track ON":"Key Track OFF",
                model.keyTrack,kGreen,.78f);
            const auto patch=ProjectCore::instance().selectedPatch();
            cell(bar(2,1,3),"K Low "+std::to_string(patch.keyMapLow),
                false,kCyan,.78f);
            cell(bar(2,2,3),"K High "+std::to_string(patch.keyMapHigh),
                false,kPurple,.78f);
            const auto c=getCurve(model);
            cell(bar(3,0,6),"D",selected==c.decayIndex,kCyan,1);
            cell(bar(3,1,6),"R",selected==c.releaseIndex,kOrange,1);
            cell(bar(3,2,6),c.sustain?"S hold":"Dynamic",c.sustain,kGreen,.73f);
            cell(bar(3,3,6),"-",false,kCyan,1.3f);
            cell(bar(3,4,6),zooms[zoom]<1?"0.5x":std::to_string(static_cast<int>(zooms[zoom]))+"x",false,kCyan,.77f);
            cell(bar(3,5,6),"+",false,kCyan,1.3f);
        }else if(page==2){
            // Mod's controls must not be rendered under the Slope page's
            // shape/preset buttons (they share the same two rows).
            for(int i=0;i<6;++i)
                cell(bar(2,i,6),"Mod "+std::to_string(i+1),slot==i,kPurple,.78f);
            const auto& m=model.mods[slot];
            static const char* dests[6]={"Freq","Amount","Q","F Morph","A Morph","Q Morph"};
            for(int i=0;i<6;++i)cell(bar(3,i,6),
                i==0&&model.keyTrack?"F locked":dests[i],
                i==m.destination,kPurple,.73f);
        }
        if(page==3){
            const auto& slope=model.slopes[slopeShape];
            for(int i=0;i<3;++i)
                cell(bar(2,i,3),i==0?"Low shelf":i==1?"Bell":"High shelf",
                    slopeShape==i,kOrange,.81f);
            for(int i=0;i<4;++i)
                cell(bar(3,i,4),i==0?(slope.enabled?"Custom ON":"Custom OFF"):
                    i==1?"Shelf":i==2?"Pass":"Flat",i==0&&slope.enabled,
                    i==0?kGreen:kCyan,.85f);
            const auto r=graph();
            o.addRect({r.x-5,r.y-7,r.w+10,r.h+14},overlayColor(kRollBg));
            o.addText("Relative frequency: cutoff-centered octaves",
                r.x,r.y-35,.83f,overlayColor(kWhite));
            for(int i=0;i<=8;++i){
                const float x=r.x+r.w*i/8.0f;
                o.addLine(x,r.y,x,r.y+r.h,1,overlayColor(kMuted,.25f));
                if(i%2==0)o.addText(std::to_string(i-4)+" oct",
                    x-12,r.y+r.h+9,.74f,overlayColor(kMuted));
            }
            for(int i=0;i<=4;++i){
                const float y=r.y+r.h*i/4.0f;
                o.addLine(r.x,y,r.x+r.w,y,1,overlayColor(kMuted,.25f));
                o.addText(std::to_string(100-i*25)+"%",
                    r.x-51,y-7,.75f,overlayColor(kWhite));
            }
            const auto& c=slope.profile;
            auto pos=[&](OperatorPoint p){
                return std::pair<float,float>{
                    r.x+r.w*p.time,r.y+r.h*(1.0f-p.value)};
            };
            for(int j=1;j<c.count;++j){
                const auto& a=c.points[j-1];const auto& b=c.points[j];
                for(int k=1;k<=30;++k){
                    const float t0=a.time+(b.time-a.time)*(k-1)/30.0f;
                    const float t1=a.time+(b.time-a.time)*k/30.0f;
                    const auto v0=pos({t0,operatorCurveValue(c.points.data()+j-1,2,t0)});
                    const auto v1=pos({t1,operatorCurveValue(c.points.data()+j-1,2,t1)});
                    o.addLine(v0.first,v0.second,v1.first,v1.second,3,
                        overlayColor(kCyan));
                }
                const float t=a.time+(b.time-a.time)*b.midpoint;
                const auto mid=pos({t,operatorCurveValue(c.points.data()+j-1,2,t)});
                o.addLine(mid.first-6,mid.second-6,mid.first+6,mid.second+6,2,overlayColor(kOrange));
                o.addLine(mid.first-6,mid.second+6,mid.first+6,mid.second-6,2,overlayColor(kOrange));
            }
            for(int j=0;j<c.count;++j){
                const auto p=pos(c.points[j]);
                o.addFilledCircle(p.first,p.second,selected==j?9:6,
                    overlayColor(selected==j?kWhite:kCyan));
            }
            o.addText("100% = unchanged  |  0% = full cut at Amount -100",
                view.x+20,view.y+view.h-35,.78f,overlayColor(kMuted));
            return;
        }
        if(page<2){
            const auto r=graph();
            o.addRect({r.x-5,r.y-6,r.w+10,r.h+12},overlayColor(kRollBg));
            for(int i=0;i<3;++i){
                auto paneRect=pane(i);
                const auto col=i==0?kCyan:i==1?kPurple:kOrange;
                const auto& f=model.lanes[i+(page==1?3:0)];
                const auto& curve=f.curve;
                o.addRect({paneRect.x,paneRect.y,paneRect.w,paneRect.h},
                    overlayColor(kButton,.18f));
                o.addText(i==0?"40Hz - 18k":i==1?"-100  /  +100":"Q 0-100",
                    paneRect.x,paneRect.y-20,.69f,overlayColor(col));
                for(int k=0;k<=4;++k){
                    const float x=paneRect.x+k*paneRect.w*.25f;
                    o.addLine(x,paneRect.y,x,paneRect.y+paneRect.h,1,
                        overlayColor(kMuted,.30f));
                }
                o.setClip({paneRect.x,paneRect.y,paneRect.w,paneRect.h});
                if(staticLine(curve)){
                    const float line=outputX(i,curve.points[0].value);
                    o.addLine(line,paneRect.y,line,paneRect.y+paneRect.h,3,
                        overlayColor(col));
                }
                for(int j=1;j<curve.count&&!staticLine(curve);++j){
                    const auto& a=curve.points[j-1];const auto& b=curve.points[j];
                    for(int k=1;k<=22;++k){
                        const float t0=a.time+(b.time-a.time)*(k-1)/22;
                        const float t1=a.time+(b.time-a.time)*k/22;
                        auto x=location({t0,operatorCurveValue(curve.points.data()+j-1,2,t0)},i);
                        auto y=location({t1,operatorCurveValue(curve.points.data()+j-1,2,t1)},i);
                        o.addLine(x.first,x.second,y.first,y.second,3,
                            overlayColor(col));
                    }
                }
                o.clearClip();
                for(int j=1;j<curve.count;++j){
                    if(staticLine(curve))break;
                    const float time=curve.points[j-1].time+
                        (curve.points[j].time-curve.points[j-1].time)*
                        std::clamp(curve.points[j].midpoint,.05f,.95f);
                    auto x=location({time,operatorCurveValue(curve.points.data()+j-1,2,time)},i);
                    if(x.second>=paneRect.y&&x.second<=paneRect.y+paneRect.h){
                        o.addLine(x.first-6,x.second-6,x.first+6,x.second+6,2,
                            overlayColor(kOrange));
                        o.addLine(x.first-6,x.second+6,x.first+6,x.second-6,2,
                            overlayColor(kOrange));
                    }
                    if(i==2&&f.shape[j-1]!=f.shape[j]){
                        const float t=curve.points[j-1].time+
                            (curve.points[j].time-curve.points[j-1].time)*f.transition[j];
                        auto x=location({t,operatorCurveValue(curve.points.data()+j-1,2,t)},i);
                        if(x.second>=paneRect.y&&x.second<=paneRect.y+paneRect.h)
                            o.addText("T",x.first-5,x.second-14,.8f,overlayColor(kPurple));
                    }
                }
                for(int j=0;j<curve.count;++j){
                    if(staticLine(curve))break;
                    auto x=location(curve.points[j],i);
                    if(x.second<paneRect.y||x.second>paneRect.y+paneRect.h)continue;
                    o.addFilledCircle(x.first,x.second,(i==lane&&j==selected)?8:6,
                        overlayColor(i==lane&&j==selected?kWhite:col));
                    if(i==2)o.addText(shapeName(f.shape[j]),x.first+9,x.second-12,
                        .78f,overlayColor(kOrange));
                    if(i==lane&&j==curve.decayIndex)
                        o.addText("D",x.first+8,x.second+8,.8f,overlayColor(kCyan));
                    if(i==lane&&j==curve.releaseIndex)
                        o.addText(curve.sustain?"S":"R",x.first+8,x.second-20,
                            .8f,overlayColor(kOrange));
                }
            }
            // Visible time labels use the same zoom / scrolling as operators.
            static constexpr std::array<float,14> tickSizes{
                .001f,.002f,.005f,.01f,.02f,.05f,.1f,.2f,
                .5f,1,2,5,10,20};
            float step=tickSizes.back();
            for(float t:tickSizes)if(t*rate()>=60.0f){step=t;break;}
            const int first=std::max(0,static_cast<int>(std::floor(
                scroll/(rate()*step))));
            const int last=std::min(static_cast<int>(120.0f/step),
                static_cast<int>(std::ceil((scroll+r.h)/(rate()*step))));
            for(int k=first;k<=last;++k){
                const float t=k*step,y=r.y+t*rate()-scroll;
                if(y<r.y||y>r.y+r.h)continue;
                o.addLine(r.x,y,r.x+r.w,y,1,overlayColor(kMuted,.22f));
                char label[30];std::snprintf(label,sizeof(label),
                    t<1.0f?"%.3fs":t<10.0f?"%.2fs":"%.1fs",
                    static_cast<double>(t));
                o.addText(label,r.x-60,y-9,.76f,overlayColor(kWhite));
            }
        }else{
            auto r=graph();const auto& m=model.mods[slot];
            const auto& c=m.response;
            o.addRect({r.x-5,r.y-6,r.w+10,r.h+12},overlayColor(kRollBg));
            o.addText("V / M input: 0 to 100%",r.x,r.y-20,.77f,overlayColor(kMuted));
            for(int k=0;k<=4;++k){
                const float x=r.x+r.w*k*.25f,y=r.y+r.h*k*.25f;
                o.addLine(x,r.y,x,r.y+r.h,1,overlayColor(kMuted,.35f));
                o.addLine(r.x,y,r.x+r.w,y,1,overlayColor(kMuted,.35f));
            }
            for(int j=1;j<c.count;++j){
                for(int k=1;k<=30;++k){
                    const float t0=c.points[j-1].time+
                        (c.points[j].time-c.points[j-1].time)*(k-1)/30;
                    const float t1=c.points[j-1].time+
                        (c.points[j].time-c.points[j-1].time)*k/30;
                    auto a=location({t0,operatorCurveValue(c.points.data()+j-1,2,t0)},0);
                    auto b=location({t1,operatorCurveValue(c.points.data()+j-1,2,t1)},0);
                    o.addLine(a.first,a.second,b.first,b.second,3,overlayColor(kPurple));
                }
                const float t=c.points[j-1].time+
                    (c.points[j].time-c.points[j-1].time)*c.points[j].midpoint;
                const auto a=location({t,operatorCurveValue(c.points.data()+j-1,2,t)},0);
                o.addLine(a.first-6,a.second-6,a.first+6,a.second+6,2,overlayColor(kOrange));
                o.addLine(a.first-6,a.second+6,a.first+6,a.second-6,2,overlayColor(kOrange));
                if(m.destination==2&&m.shape[j-1]!=m.shape[j]){
                    const float mid=c.points[j-1].time+
                        (c.points[j].time-c.points[j-1].time)*m.transition[j];
                    const auto xy=location({mid,operatorCurveValue(c.points.data()+j-1,2,mid)},0);
                    o.addText("T",xy.first-5,xy.second-15,.8f,overlayColor(kPurple));
                }
            }
            for(int j=0;j<c.count;++j){
                auto xy=location(c.points[j],0);
                o.addFilledCircle(xy.first,xy.second,j==selected?9:6,
                    overlayColor(j==selected?kOrange:kWhite));
                if(m.destination==2)o.addText(shapeName(m.shape[j]),
                    xy.first+10,xy.second-12,.8f,overlayColor(kOrange));
            }
        }
        if(qCurve()){
            const auto modelNow=getModel();
            const auto c=getCurve(modelNow);
            const int point=std::clamp(selected,0,static_cast<int>(c.count)-1);
            for(int i=0;i<3;++i){
                const auto sh=static_cast<FilterShape>(i);
                cell(bar(4,i,3),i==0?"Low shelf":i==1?"Bell band":"High shelf",
                    currentShape(modelNow,point)==sh,kOrange,.83f);
            }
        }else{
            o.addText("Tap to select / add point   Drag orange X to bend   Hold node to delete",
                view.x+20,view.y+view.h-60,.72f,overlayColor(kMuted));
        }
    }
    bool update(float x,float y){
        if(page==3){
            auto m=getModel();
            auto& c=m.slopes[slopeShape].profile;
            const int i=bend>=0?bend:drag;
            if(i<0||i>=c.count)return false;
            const auto r=graph();
            const float t=std::clamp((x-r.x)/r.w,0.0f,1.0f);
            const float v=std::clamp(1.0f-(y-r.y)/r.h,0.0f,1.0f);
            if(bend>=1){
                const auto& a=c.points[bend-1];
                c.points[bend].midpoint=std::clamp((t-a.time)/
                    std::max(.000001f,c.points[bend].time-a.time),.05f,.95f);
                c.points[bend].handleValue=std::clamp(v,
                    std::min(a.value,c.points[bend].value),
                    std::max(a.value,c.points[bend].value));
                c.points[bend].handleActive=true;
            }else{
                const float lo=i==0?0:c.points[i-1].time+.0001f;
                const float hi=i==c.count-1?1:c.points[i+1].time-.0001f;
                c.points[i].time=(i==0?0:i==c.count-1?1:
                    std::clamp(t,lo,std::max(lo,hi)));
                c.points[i].value=v;
            }
            m.slopes[slopeShape].enabled=true;
            return ProjectCore::instance().setSelectedEvolvingFilter(m);
        }
        auto model=getModel();auto curve=getCurve(model);
        const Rect r=page==2?graph():pane(lane);
        if(page<2&&drag==-3&&staticLine(curve)){
            const auto r=pane(lane);
            const float value=std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f);
            curve.points[0].value=curve.points[1].value=value;
            return save(model,curve);
        }
        const int idx=transition>=0?transition:(bend>=0?bend:drag);
        if(idx<0||idx>=curve.count)return false;
        if(transition>=1){
            const float axis=page==2?
                std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f):
                std::clamp((y-r.y+scroll)/rate(),0.0f,120.0f);
            const float a=curve.points[transition-1].time;
            const float b=curve.points[transition].time;
            setTransition(model,transition,
                std::clamp((axis-a)/std::max(.000001f,b-a),0.0f,.95f));
            return save(model,curve);
        }
        auto& p=curve.points[idx];
        const float coord=page==2?
            std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f):
            std::clamp((y-r.y+scroll)/rate(),0.0f,120.0f);
        const float v=page==2?
            std::clamp((y-r.y)/std::max(1.0f,r.h),0.0f,1.0f):
            std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f);
        if(bend>=1){
            const auto& a=curve.points[idx-1];
            p.midpoint=std::clamp((coord-a.time)/
                std::max(.000001f,p.time-a.time),.05f,.95f);
            p.handleValue=std::clamp(v,std::min(a.value,p.value),
                std::max(a.value,p.value));
            p.handleActive=true;
        }else if(drag>=0){
            const float min=idx>0?curve.points[idx-1].time+.0001f:0.0f;
            const float max=idx+1<curve.count?
                curve.points[idx+1].time-.0001f:(page==2?1.0f:120.0f);
            p.time=std::clamp(coord,min,std::max(min,max));
            p.value=v;
        }
        return save(model,curve);
    }
    bool changeShape(int which){
        auto model=getModel();const auto curve=getCurve(model);
        if(!qCurve()||curve.count==0)return false;
        const int index=std::clamp(selected,0,static_cast<int>(curve.count)-1);
        setShape(model,index,static_cast<FilterShape>(which));
        if(page!=2&&staticLine(curve)){
            setShape(model,0,static_cast<FilterShape>(which));
            setShape(model,1,static_cast<FilterShape>(which));
        }
        return save(model,curve);
    }
    bool addPoint(float x,float y){
        if(page==3){
            auto m=getModel();auto& c=m.slopes[slopeShape].profile;
            if(c.count>=kMaxOperatorPoints)return false;
            const auto r=graph();
            const float t=std::clamp((x-r.x)/r.w,.0001f,.9999f);
            const float v=std::clamp(1.0f-(y-r.y)/r.h,0.0f,1.0f);
            int pos=0;while(pos<c.count&&c.points[pos].time<t)++pos;
            if(pos==0||pos==c.count||t-c.points[pos-1].time<.0001f||
                c.points[pos].time-t<.0001f)return false;
            for(int i=c.count;i>pos;--i)c.points[i]=c.points[i-1];
            c.points[pos]={t,v};++c.count;
            selected=pos;m.slopes[slopeShape].enabled=true;
            return ProjectCore::instance().setSelectedEvolvingFilter(m);
        }
        auto m=getModel();auto c=getCurve(m);
        if(c.count>=kMaxOperatorPoints)return false;
        const auto r=page==2?graph():pane(lane);
        if(page!=2&&staticLine(c)){
            const float t=std::clamp((y-r.y+scroll)/rate(),.005f,119.65f);
            const float value=std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f);
            const float original=c.points[0].value;
            // First tap off the static line creates the starting, selected
            // and finishing nodes. Release is now an editable timed stage.
            c.count=3;c.sustain=false;c.decayIndex=1;c.releaseIndex=1;
            c.points[0]={0.0f,original};
            c.points[1]={t,value};
            c.points[2]={std::min(120.0f,t+.35f),original};
            setShape(m,1,currentShape(m,0));
            setShape(m,2,currentShape(m,0));
            selected=1;return save(m,c);
        }
        OperatorPoint p;
        p.time=page==2?
            std::clamp((x-r.x)/std::max(1.0f,r.w),.0001f,.9999f):
            std::clamp((y-r.y+scroll)/rate(),.0001f,119.999f);
        p.value=page==2?
            std::clamp((y-r.y)/std::max(1.0f,r.h),0.0f,1.0f):
            std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f);
        int n=0;while(n<c.count&&c.points[n].time<p.time)++n;
        if(n==0||n==c.count||c.points[n].time-p.time<.0001f||
            p.time-c.points[n-1].time<.0001f)return false;
        for(int k=c.count;k>n;--k){
            c.points[k]=c.points[k-1];
            setShape(m,k,currentShape(m,k-1));
            setTransition(m,k,currentTransition(m,k-1));
        }
        c.points[n]=p;setShape(m,n,currentShape(m,n-1));setTransition(m,n,.65f);
        ++c.count;
        if(page!=2){
            if(c.decayIndex>=n)++c.decayIndex;
            if(c.releaseIndex>=n)++c.releaseIndex;
        }
        selected=n;
        return save(m,c);
    }
    bool down(float x,float y){
        moved=changed=false;drag=bend=transition=-1;
        downX=x;downY=y;started=std::chrono::steady_clock::now();
        if(bar(0,0,2).contains(x,y)){
            if(page==2){
                auto m=getModel();auto& c=m.mods[slot];
                c.response.source=static_cast<uint8_t>((c.response.source+1)%3);
                changed=ProjectCore::instance().setSelectedEvolvingFilter(m);
            }
            return true;
        }
        if(bar(0,1,2).contains(x,y)){
            auto m=getModel();
            if(page==2)m.mods[slot].enabled=!m.mods[slot].enabled;
            else m.configured=!ProjectCore::instance().selectedPatch().filter.evolving.configured;
            changed=ProjectCore::instance().setSelectedEvolvingFilter(m);
            return true;
        }
        for(int i=0;i<4;++i)if(bar(1,i,4).contains(x,y)){
            page=i;selected=-1;return true;
        }
        if(page==3){
            auto m=getModel();
            for(int i=0;i<3;++i)if(bar(2,i,3).contains(x,y)){
                slopeShape=i;selected=-1;return true;
            }
            for(int i=0;i<4;++i)if(bar(3,i,4).contains(x,y)){
                auto& slope=m.slopes[slopeShape];
                if(i==0)slope.enabled=!slope.enabled;
                else if(i==3){
                    slope.profile=initialFilterSlope(slopeShape).profile;
                    for(int j=0;j<slope.profile.count;++j)slope.profile.points[j].value=1.0f;
                    slope.enabled=true;
                }else{
                    slope=initialFilterSlope(slopeShape);
                    if(i==1){
                        // A shelf retains a finite -24 dB plateau.
                        for(int j=0;j<slope.profile.count;++j)
                            slope.profile.points[j].value=.0630957f+
                                .9369043f*slope.profile.points[j].value;
                    }
                    slope.enabled=true;
                }
                changed=ProjectCore::instance().setSelectedEvolvingFilter(m);
                return true;
            }
            const auto r=graph();
            const bool inside=r.contains(x,y);
            if(x<r.x-20.0f||x>r.x+r.w+20.0f||
               y<r.y-20.0f||y>r.y+r.h+20.0f)return true;
            const auto& c=m.slopes[slopeShape].profile;
            float best=1.0e9f;int near=-1;
            for(int i=0;i<c.count;++i){
                const float px=r.x+r.w*c.points[i].time;
                const float py=r.y+r.h*(1.0f-c.points[i].value);
                const float d=(x-px)*(x-px)+(y-py)*(y-py);
                const float radius=(i==0||i==c.count-1)?37.0f:24.0f;
                if(d<radius*radius&&d<best){best=d;near=i;}
            }
            if(near==0||near==c.count-1){selected=drag=near;return true;}
            float bestBend=20.0f*20.0f;int nearBend=-1;
            for(int i=1;i<c.count;++i){
                const auto& a=c.points[i-1],&b=c.points[i];
                const float t=a.time+(b.time-a.time)*b.midpoint;
                const float v=operatorCurveValue(c.points.data()+i-1,2,t);
                const float px=r.x+r.w*t,py=r.y+r.h*(1-v);
                const float d=(x-px)*(x-px)+(y-py)*(y-py);
                if(d<bestBend){bestBend=d;nearBend=i;}
            }
            if(nearBend>=0&&(near<0||bestBend<best)){bend=nearBend;return true;}
            if(near>=0){selected=drag=near;return true;}
            if(!inside)return true;
            drag=-2;return true;
        }
        if(page<2){
            if(bar(2,0,3).contains(x,y)){
                auto m=getModel();m.keyTrack=!m.keyTrack;
                changed=ProjectCore::instance().setSelectedEvolvingFilter(m);
                return true;
            }
            for(int i=1;i<3;++i)if(bar(2,i,3).contains(x,y)){
                const auto patch=ProjectCore::instance().selectedPatch();
                const auto r=bar(2,i,3);
                // Outer fifths jump by 12 keys; middle regions adjust by one.
                // K endpoints are patch properties, independent of Filter Key Track.
                const float fraction=(x-r.x)/std::max(1.0f,r.w);
                const int delta=fraction<.20f?-12:fraction<.50f?-1:
                    fraction<.80f?1:12;
                const int low=i==1?std::clamp(patch.keyMapLow+delta,
                    0,patch.keyMapHigh-1):patch.keyMapLow;
                const int high=i==2?std::clamp(patch.keyMapHigh+delta,
                    patch.keyMapLow+1,127):patch.keyMapHigh;
                changed=ProjectCore::instance().setSelectedKeyMapRange(low,high);
                return true;
            }
            // Each visible lane selects itself by touch; there is no
            // separate frequency / amount / Q focus switch.
            for(int i=0;i<6;++i)if(bar(3,i,6).contains(x,y)){
                auto m=getModel();auto c=getCurve(m);
                if(i==0&&selected>=0&&selected<c.count-1)c.decayIndex=selected;
                if(i==1&&selected>=0&&selected<c.count-1)c.releaseIndex=selected;
                if(i==2)c.sustain=!c.sustain;
                if(i==3||i==5){
                    const float top=scroll/rate();
                    zoom=std::clamp(zoom+(i==3?-1:1),0,5);
                    scroll=std::clamp(top*rate(),0.0f,
                        std::max(0.0f,120.0f*rate()-graph().h));
                }else changed=save(m,c);
                return true;
            }
        }else{
            for(int i=0;i<6;++i)if(bar(2,i,6).contains(x,y)){
                slot=i;selected=-1;
                return true;
            }
            for(int i=0;i<6;++i)if(bar(3,i,6).contains(x,y)){
                auto m=getModel();
                if(i==0&&m.keyTrack)return true;
                m.mods[slot].destination=static_cast<uint8_t>(i);
                m.mods[slot].enabled=true;
                selected=-1;
                changed=ProjectCore::instance().setSelectedEvolvingFilter(m);
                return true;
            }
        }
        if(qCurve())for(int i=0;i<3;++i)if(bar(4,i,3).contains(x,y)){
            changed=changeShape(i);return true;
        }
        auto m=getModel();auto c=getCurve(m);
        if(page<2){
            const auto gr=graph();
            for(int i=0;i<3;++i)if(pane(i).contains(x,y)){
                if(lane!=i){lane=i;selected=-1;}
                break;
            }
        }
        c=getCurve(m);
        const Rect r=page==2?graph():pane(lane);
        const bool inside=r.contains(x,y);
        if(x<r.x-20.0f||x>r.x+r.w+20.0f||
           y<r.y-20.0f||y>r.y+r.h+20.0f)return true;
        if(page<2&&staticLine(c)){
            const float line=outputX(lane,c.points[0].value);
            if(std::fabs(x-line)<22.0f){
                selected=0;drag=-3;return true;
            }
            drag=-2;return true;
        }
        float pointDistance=1.0e9f;int nearest=-1;
        for(int i=0;i<c.count;++i){
            const auto xy=location(c.points[i],lane);
            const float d=(x-xy.first)*(x-xy.first)+(y-xy.second)*(y-xy.second);
            const float radius=(i==0||i==c.count-1)?37.0f:24.0f;
            if(d<radius*radius&&d<pointDistance){
                pointDistance=d;nearest=i;
            }
        }
        if(nearest==0||nearest==c.count-1){
            selected=drag=nearest;return true;
        }
        float bendDistance=20*20;int nearBend=-1;
        float transitionDistance=17*17;int nearTransition=-1;
        for(int i=1;i<c.count;++i){
            const auto& a=c.points[i-1];const auto& b=c.points[i];
            const float mid=a.time+(b.time-a.time)*b.midpoint;
            const auto pt=location({mid,operatorCurveValue(c.points.data()+i-1,2,mid)},lane);
            const float d=(x-pt.first)*(x-pt.first)+(y-pt.second)*(y-pt.second);
            if(d<bendDistance){bendDistance=d;nearBend=i;}
            if(qCurve()&&currentShape(m,i-1)!=currentShape(m,i)){
                const float t=a.time+(b.time-a.time)*currentTransition(m,i);
                const auto q=location({t,operatorCurveValue(c.points.data()+i-1,2,t)},lane);
                const float dt=(x-q.first)*(x-q.first)+(y-q.second)*(y-q.second);
                if(dt<transitionDistance){transitionDistance=dt;nearTransition=i;}
            }
        }
        if(nearTransition>=0&&(nearest<0||transitionDistance<pointDistance)&&
           (nearBend<0||transitionDistance<=bendDistance)){
            transition=nearTransition;return true;
        }
        if(nearBend>=0&&(nearest<0||bendDistance<=pointDistance)){
            bend=nearBend;return true;
        }
        if(nearest>=0){selected=drag=nearest;return true;}
        if(!inside)return true;
        drag=-2;return true;
    }
    bool move(float x,float y){
        if(drag>=0||drag==-3||bend>=0||transition>=0){
            if(std::fabs(x-downX)+std::fabs(y-downY)>8.0f){
                moved=true;changed=update(x,y)||changed;
            }
        }else if(drag==-2){
            if(std::fabs(x-downX)+std::fabs(y-downY)>12.0f)moved=true;
        }
        return true;
    }
    bool up(){
        if(drag==-2&&!moved)changed=addPoint(downX,downY)||changed;
        if(drag>=1&&!moved){
            const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-started).count();
            if(ms>=500){
                if(page==3){
                    auto m=getModel();auto& c=m.slopes[slopeShape].profile;
                    if(c.count>2&&drag<c.count-1){
                        for(int i=drag;i<c.count-1;++i)c.points[i]=c.points[i+1];
                        --c.count;selected=-1;
                        changed=ProjectCore::instance().setSelectedEvolvingFilter(m)||changed;
                    }
                }else{
                auto m=getModel();auto c=getCurve(m);
                if(c.count>2&&drag<c.count-1){
                    for(int i=drag;i<c.count-1;++i){
                        c.points[i]=c.points[i+1];
                        setShape(m,i,currentShape(m,i+1));
                        setTransition(m,i,currentTransition(m,i+1));
                    }
                    --c.count;
                    if(page<2){
                        if(c.decayIndex>=drag&&c.decayIndex>0)--c.decayIndex;
                        if(c.releaseIndex>=drag&&c.releaseIndex>0)--c.releaseIndex;
                        c.decayIndex=std::min<int>(c.decayIndex,c.count-2);
                        c.releaseIndex=std::min<int>(c.releaseIndex,c.count-2);
                    }
                    selected=-1;changed=save(m,c)||changed;
                }
                }
            }
        }
        drag=bend=transition=-1;
        const bool result=changed;changed=false;
        return result;
    }
};
FilterSurface filterSurface;

struct UnisonSurface {
    using Rect=NativeEditor::Rect;
    Rect view{};
    int selected{-1},drag{-1},bend{-1},zoom{2};
    float scroll{0},downX{0},downY{0};
    bool moved{false},changed{false};
    bool ampMode{false};
    std::chrono::steady_clock::time_point started{};
    static constexpr std::array<float,6> zooms{{.5f,1,2,4,8,16}};
    float rate()const{return 320.0f*zooms[zoom];}
    OperatorCurve curve()const{
        const auto& p=ProjectCore::instance().selectedPatch();
        if(ampMode)return p.masterAmpCurve.count>=2?
            p.masterAmpCurve:initialMasterAmpCurve();
        return p.unisonCurve.enabled&&p.unisonCurve.count>=2?
            p.unisonCurve:initialUnisonCurve(p.unison);
    }
    static bool flat(const OperatorCurve& c){
        return c.count==2&&c.releaseIndex==0&&c.points[1].time>=119.0f&&
            std::fabs(c.points[0].value-c.points[1].value)<.00001f;
    }
    bool save(const OperatorCurve& c){
        if(ampMode){
            auto enabledCurve=c;enabledCurve.enabled=true;
            return ProjectCore::instance().setSelectedMasterAmpCurve(enabledCurve);
        }
        return ProjectCore::instance().setSelectedUnisonCurve(c);
    }
    Rect ampToggle()const{
        return {view.x+view.w-165,view.y+12,140,54};
    }
    Rect control(int i)const{
        const float w=view.w-32,gap=6,cw=(w-5*gap)/6;
        return {view.x+16+i*(cw+gap),view.y+96,cw,54};
    }
    Rect graph()const{
        const float y=view.y+202.0f;
        return {view.x+75,y,view.w-98,
            std::max(75.0f,view.y+view.h-83-y)};
    }
    std::pair<float,float> at(const OperatorPoint& p)const{
        const auto r=graph();
        return {r.x+std::clamp(p.value,0.0f,1.0f)*r.w,
                r.y+p.time*rate()-scroll};
    }
    void scrollBy(float dy){
        scroll=std::clamp(scroll+dy,0.0f,
            std::max(0.0f,120.0f*rate()-graph().h));
    }
    void draw()const{
        auto& o=NativeOverlay::instance();
        const auto c=curve();
        o.addRect({view.x,view.y,view.w,view.h},overlayColor(kBg));
        o.addText(ampMode?"Master Amp":
            "Unison - per-note detune strength (0 to 100)",
            view.x+24,view.y+27,.92f,overlayColor(kCyan));
        if(ampMode){
            const auto a=ampToggle();
            const bool active=ProjectCore::instance().selectedPatch().masterAmpCurve.enabled;
            o.addRect({a.x,a.y,a.w,a.h},
                overlayColor(active?kGreen:kButton));
            o.addTextCentered(active?"Enabled":"Enable Amp",
                {a.x,a.y,a.w,a.h},.88f,
                overlayColor(active?kBg:kWhite));
        }
        o.addText(flat(c)?
            (ampMode?"Flat level. Tap away from line to add envelope points.":
                "Flat detune. Drag the line; tap away to add envelope points."):
            (ampMode?"Post-synthesis envelope, independent of operator volume.":
                "Envelope follows time and releases on note-off."),
            view.x+24,view.y+62,.82f,overlayColor(kMuted));
        const char* label[6]={"D","R",c.sustain?"S Hold":"Dynamic","-",
                             "Zoom","+"};
        for(int i=0;i<6;++i){
            const auto r=control(i);
            const bool on=i==2&&c.sustain;
            o.addRect({r.x,r.y,r.w,r.h},overlayColor(on?kCyan:kButton));
            const std::string v=i==4?(zooms[zoom]<1.0f?"0.5x":
                std::to_string(static_cast<int>(zooms[zoom]))+"x"):label[i];
            o.addTextCentered(v,{r.x,r.y,r.w,r.h},.84f,
                overlayColor(on?kBg:kWhite));
        }
        const auto r=graph();
        o.addRect({r.x-6,r.y-6,r.w+12,r.h+12},overlayColor(kRollBg));
        for(int i=0;i<=4;++i){
            const float x=r.x+i*r.w*.25f;
            o.addLine(x,r.y,x,r.y+r.h,1,overlayColor(kMuted,.45f));
            o.addText(std::to_string(i*25),
                x-9,r.y-23,.76f,overlayColor(kCyan));
        }
        o.setClip({r.x,r.y,r.w,r.h});
        for(int i=1;i<c.count;++i){
            for(int k=1;k<=28;++k){
                const float a=c.points[i-1].time+
                    (c.points[i].time-c.points[i-1].time)*(k-1)/28.0f;
                const float b=c.points[i-1].time+
                    (c.points[i].time-c.points[i-1].time)*k/28.0f;
                const auto va=at({a,operatorCurveValue(c.points.data()+i-1,2,a)});
                const auto vb=at({b,operatorCurveValue(c.points.data()+i-1,2,b)});
                o.addLine(va.first,va.second,vb.first,vb.second,3,
                    overlayColor(kCyan));
            }
        }
        o.clearClip();
        if(!flat(c)){
            for(int i=1;i<c.count;++i){
                const float t=c.points[i-1].time+
                    (c.points[i].time-c.points[i-1].time)*
                    std::clamp(c.points[i].midpoint,.05f,.95f);
                const auto xy=at({t,operatorCurveValue(c.points.data()+i-1,2,t)});
                if(xy.second<r.y||xy.second>r.y+r.h)continue;
                o.addLine(xy.first-6,xy.second-6,xy.first+6,xy.second+6,2,
                    overlayColor(kOrange));
                o.addLine(xy.first-6,xy.second+6,xy.first+6,xy.second-6,2,
                    overlayColor(kOrange));
            }
            for(int i=0;i<c.count;++i){
                const auto xy=at(c.points[i]);
                if(xy.second<r.y||xy.second>r.y+r.h)continue;
                o.addFilledCircle(xy.first,xy.second,selected==i?9:6,
                    overlayColor(selected==i?kWhite:kCyan));
                if(i==c.decayIndex)o.addText("D",xy.first+8,xy.second+10,
                    .78f,overlayColor(kCyan));
                if(i==c.releaseIndex)o.addText(c.sustain?"S":"R",
                    xy.first+8,xy.second-19,.82f,overlayColor(kOrange));
            }
        }
        static constexpr std::array<float,14> ticks{{
            .001f,.002f,.005f,.01f,.02f,.05f,.1f,.2f,
            .5f,1,2,5,10,20}};
        float interval=ticks.back();
        for(float t:ticks)if(t*rate()>=65){interval=t;break;}
        const int first=std::max(0,static_cast<int>(std::floor(
            scroll/(rate()*interval))));
        const int last=std::min(static_cast<int>(120.0f/interval),
            static_cast<int>(std::ceil((scroll+r.h)/(rate()*interval))));
        for(int i=first;i<=last;++i){
            const float t=i*interval,y=r.y+t*rate()-scroll;
            if(y<r.y||y>r.y+r.h)continue;
            o.addLine(r.x,y,r.x+r.w,y,1,overlayColor(kMuted,.25f));
            char label[24];std::snprintf(label,sizeof(label),
                t<1?"%.3fs":t<10?"%.2fs":"%.1fs",static_cast<double>(t));
            o.addText(label,r.x-62,y-8,.75f,overlayColor(kWhite));
        }
    }
    bool setControl(int i){
        auto c=curve();
        if(i==0&&selected>=0&&selected<c.count-1)c.decayIndex=selected;
        else if(i==1&&selected>=0&&selected<c.count-1)c.releaseIndex=selected;
        else if(i==2)c.sustain=!c.sustain;
        else if(i==3||i==5){
            const float top=scroll/rate();
            zoom=std::clamp(zoom+(i==3?-1:1),0,5);
            scroll=std::clamp(top*rate(),0.0f,
                std::max(0.0f,120.0f*rate()-graph().h));
            return false;
        }else return false;
        return save(c);
    }
    bool update(float x,float y){
        auto c=curve();const auto r=graph();
        if(drag==-3&&flat(c)){
            const float value=std::clamp((x-r.x)/r.w,0.0f,1.0f);
            c.points[0].value=c.points[1].value=value;
            return save(c);
        }
        const int index=bend>=0?bend:drag;
        if(index<0||index>=c.count)return false;
        const float time=std::clamp((y-r.y+scroll)/rate(),0.0f,120.0f);
        const float value=std::clamp((x-r.x)/r.w,0.0f,1.0f);
        auto& p=c.points[index];
        if(bend>=1){
            const auto& a=c.points[index-1];
            p.midpoint=std::clamp((time-a.time)/
                std::max(.000001f,p.time-a.time),.05f,.95f);
            p.handleValue=std::clamp(value,std::min(a.value,p.value),
                std::max(a.value,p.value));
            p.handleActive=true;
        }else{
            const float min=index?c.points[index-1].time+.0001f:0;
            const float max=index+1<c.count?
                c.points[index+1].time-.0001f:120;
            p.time=std::clamp(time,min,std::max(min,max));
            p.value=value;
        }
        return save(c);
    }
    bool addPoint(float x,float y){
        auto c=curve();if(c.count>=kMaxOperatorPoints)return false;
        const auto r=graph();
        const float t=std::clamp((y-r.y+scroll)/rate(),.005f,119.65f);
        const float v=std::clamp((x-r.x)/r.w,0.0f,1.0f);
        if(flat(c)){
            const float previous=c.points[0].value;
            c.count=3;c.sustain=false;c.decayIndex=1;c.releaseIndex=1;
            c.points[0]={0,previous};
            c.points[1]={t,v};
            c.points[2]={t+.35f,previous};
            selected=1;return save(c);
        }
        int pos=0;while(pos<c.count&&c.points[pos].time<t)++pos;
        if(pos==0||pos==c.count||t-c.points[pos-1].time<.0002f||
           c.points[pos].time-t<.0002f)return false;
        for(int i=c.count;i>pos;--i)c.points[i]=c.points[i-1];
        c.points[pos]={t,v};++c.count;
        if(c.decayIndex>=pos)++c.decayIndex;
        if(c.releaseIndex>=pos)++c.releaseIndex;
        selected=pos;return save(c);
    }
    bool down(float x,float y){
        moved=changed=false;drag=bend=-1;downX=x;downY=y;
        started=std::chrono::steady_clock::now();
        if(ampMode&&ampToggle().contains(x,y)){
            auto c=curve();
            const bool current=ProjectCore::instance().selectedPatch().masterAmpCurve.enabled;
            c.enabled=!current;
            changed=ProjectCore::instance().setSelectedMasterAmpCurve(c);
            return true;
        }
        for(int i=0;i<6;++i)if(control(i).contains(x,y)){
            changed=setControl(i);return true;
        }
        const auto r=graph();
        const bool inside=r.contains(x,y);
        if(x<r.x-20.0f||x>r.x+r.w+20.0f||
           y<r.y-20.0f||y>r.y+r.h+20.0f)return true;
        const auto c=curve();
        if(flat(c)){
            const float px=at(c.points[0]).first;
            drag=std::fabs(x-px)<23.0f?-3:-2;
            if(drag==-3)selected=0;
            return true;
        }
        float nearestDistance=1.0e9f;int nearest=-1;
        for(int i=0;i<c.count;++i){
            const auto atPoint=at(c.points[i]);
            const float distance=(x-atPoint.first)*(x-atPoint.first)+
                (y-atPoint.second)*(y-atPoint.second);
            const float radius=(i==0||i==c.count-1)?37.0f:24.0f;
            if(distance<radius*radius&&distance<nearestDistance){
                nearestDistance=distance;nearest=i;
            }
        }
        if(nearest==0||nearest==c.count-1){
            selected=drag=nearest;return true;
        }
        float nearestBend=20*20;int bendIndex=-1;
        for(int i=1;i<c.count;++i){
            const float t=c.points[i-1].time+
                (c.points[i].time-c.points[i-1].time)*
                c.points[i].midpoint;
            const auto xy=at({t,operatorCurveValue(c.points.data()+i-1,2,t)});
            const float distance=(x-xy.first)*(x-xy.first)+
                (y-xy.second)*(y-xy.second);
            if(distance<nearestBend){nearestBend=distance;bendIndex=i;}
        }
        if(bendIndex>=0&&(nearest<0||nearestBend<=nearestDistance)){
            bend=bendIndex;return true;
        }
        if(nearest>=0){selected=drag=nearest;return true;}
        if(!inside)return true;
        drag=-2;return true;
    }
    void move(float x,float y){
        if(drag>=0||drag==-3||bend>=0){
            if(std::fabs(x-downX)+std::fabs(y-downY)>6){
                changed=update(x,y)||changed;moved=true;
            }
        }else if(drag==-2&&
                 std::fabs(x-downX)+std::fabs(y-downY)>12)moved=true;
    }
    bool up(){
        if(drag==-2&&!moved)changed=addPoint(downX,downY)||changed;
        if(drag>0&&!moved){
            const auto millis=std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-started).count();
            if(millis>=500){
                auto c=curve();
                if(c.count==3&&drag==1){
                    c=ampMode?initialMasterAmpCurve():
                        initialUnisonCurve(c.points[0].value);
                    selected=-1;changed=save(c)||changed;
                }else if(c.count>3&&drag<c.count-1){
                    for(int i=drag;i<c.count-1;++i)c.points[i]=c.points[i+1];
                    --c.count;
                    if(c.decayIndex>=drag&&c.decayIndex>0)--c.decayIndex;
                    if(c.releaseIndex>=drag&&c.releaseIndex>0)--c.releaseIndex;
                    c.decayIndex=std::min<int>(c.decayIndex,c.count-2);
                    c.releaseIndex=std::min<int>(c.releaseIndex,c.count-2);
                    selected=-1;changed=save(c)||changed;
                }
            }
        }
        drag=bend=-1;
        const bool result=changed;changed=false;return result;
    }
};
UnisonSurface unisonSurface;
} // namespace

#include "NativeEditorAmpMod.inl"

void NativeEditor::renderSynth() const noexcept {
    drawPadReservedBackground();
    auto& ov=NativeOverlay::instance();
    const auto b=bodyRect(),tab=synthTabRect(0);
    const float gap=std::clamp(b.w*.014f,9.0f,13.0f);
    const float top=tab.y+tab.h+gap;
    const Rect surface{b.x,top,b.w,std::max(0.0f,b.y+b.h-top)};
    if(synthWorkspace_==0){
        opSurface.view=surface;
        const auto current=ProjectCore::instance().selectedPatch().ops[
            static_cast<size_t>(opSurface.op)];
        opSurface.custom=current.wave==Wave::Custom;
        opSurface.draw();
        drawPatchTransfer(EditorPage::Synth);
    }else if(synthWorkspace_==1){
        filterSurface.view=surface;
        filterSurface.draw();
    }else{
        const float subH=55.0f;
        const Rect editor{surface.x,surface.y+subH+gap,
            surface.w,std::max(0.0f,surface.h-subH-gap)};
        if(ampSubtab_==0){
            unisonSurface.ampMode=true;
            unisonSurface.view=editor;
            unisonSurface.draw();
        }else{
            ampModSurface.view=editor;
            ampModSurface.draw();
        }
        for(int i=0;i<2;++i){
            const Rect r{surface.x+12.0f+i*(surface.w-31.0f)*.5f+
                            i*7.0f,surface.y+5.0f,
                         (surface.w-31.0f)*.5f,subH};
            ov.addRect({r.x,r.y,r.w,r.h},
                overlayColor(ampSubtab_==i?kPurple:kButton));
            ov.addTextCentered(i?"Mod":"Envelope",
                {r.x,r.y,r.w,r.h},.90f,
                overlayColor(ampSubtab_==i?kBg:kWhite));
        }
    }
    // Fixed page tabs above each editor; never share the FM layout.
    ov.addRect({b.x,b.y,b.w,std::max(0.0f,top-b.y)},
               overlayColor(kPanel));
    static constexpr const char* tabs[3]={"Operators","Filter","Amp"};
    for(int i=0;i<3;++i){
        const auto r=synthTabRect(i);
        const bool selected=i==synthWorkspace_;
        ov.addRect({r.x,r.y,r.w,r.h},
            overlayColor(selected?i==1?kCyan:i==2?kPurple:kGreen:kButton));
        ov.addRect({r.x+3,r.y+3,r.w-6,r.h-6},
            overlayColor(selected?mix(kPanel,i==2?kPurple:kCyan,.34f):kPanel));
        ov.addTextCentered(tabs[i],{r.x,r.y,r.w,r.h},.96f,
            overlayColor(kWhite));
    }
}

void NativeEditor::renderFx() const noexcept {
    drawPadReservedBackground();
    auto& ov=NativeOverlay::instance();
    const auto b=bodyRect();
    // Full-page FX rack; Pulsar and Binary are ordinary chain modules.
        const Fx rack=ProjectCore::instance().fxRack(false,fxDrumMasterActive());
        const auto rackRect=fxRackRect();
        ov.setClip({b.x,b.y+62,b.w,std::max(0.0f,b.h-62)});
        fillRect(rackRect,kButton);
        fillRect({rackRect.x+2,rackRect.y+2,rackRect.w-4,rackRect.h-4},kPanel);
        ov.addText(fxDrumMasterActive()?
            "Drum kit master FX (after pad FX)":
            "Instrument / Pad post-processing FX",
            rackRect.x+20,rackRect.y+18,1.02f,overlayColor(kCyan));
        for(int i=0;i<rack.count;++i){
            const auto& module=rack.chain[i];
            const auto r=fxRackStageRect(i);
            fillRect(r,mix(kPanel,kButton,.45f));
            const float bw=std::clamp(r.w*.10f,39.0f,67.0f);
            const float right=r.x+r.w;
            ov.addText(std::to_string(i+1)+". "+fxTypeName(module.type)+
                (i==fxRackExpanded_?"   -":"   +"),
                r.x+12,r.y+25,.89f,
                overlayColor(module.enabled?kWhite:kMuted));
            const char* labels[4]={"^","v",module.enabled?"On":"Off","X"};
            const Rgb colors[4]={kCyan,kCyan,module.enabled?kGreen:kMuted,kRed};
            for(int j=0;j<4;++j){
                const Rect cell{right-(4-j)*bw,r.y+5,bw-3,r.h-10};
                drawButton(cell,false,colors[j]);
                ov.addTextCentered(labels[j],{cell.x,cell.y,cell.w,cell.h},
                    .85f,overlayColor(kWhite));
            }
            if(i==fxRackExpanded_){
                for(size_t k=0;k<fxParamCount(module.type);++k){
                    const auto pr=fxRackParamRect(i,static_cast<int>(k));
                    const auto d=fxParamDesc(module.type,k);
                    const float n=normalized(module.params[k],{d.lo,d.hi});
                    const float lw=std::clamp(pr.w*.27f,115.0f,182.0f),vw=71.0f;
                    const float x=pr.x+lw,w=std::max(24.0f,pr.w-lw-vw-9.0f);
                    const float y=pr.y+pr.h*.52f;
                    ov.addText(d.name,pr.x,pr.y+pr.h*.26f,
                        .74f,overlayColor(kMuted));
                    fillRect({x,y-3,w,6},kTrack);
                    fillRect({x,y-3,w*n,6},kPurple);
                    fillRect({x+w*n-10,y-10,20,20},mix(kWhite,kPurple,.28f));
                    char shown[30];std::snprintf(shown,sizeof(shown),"%.4g",
                        static_cast<double>(module.params[k]));
                    ov.addText(shown,pr.x+pr.w-vw+3,
                        pr.y+pr.h*.26f,.77f,overlayColor(kWhite));
                }
            }
        }
        const auto select=fxRackAddRect(false),add=fxRackAddRect(true);
        drawButton(select,false,kCyan);drawButton(add,true,kGreen);
        ov.addTextCentered(std::string(fxTypeName(fxRackNewType_)),
            {select.x,select.y,select.w,select.h},.82f,overlayColor(kWhite));
        ov.addDownChevron({select.x+select.w-37,select.y,37,select.h},
            overlayColor(kWhite));
        ov.addTextCentered("+ Add FX",{add.x,add.y,add.w,add.h},
            .90f,overlayColor(kWhite));
    ov.clearClip();
    // Fixed rack heading; none of the old Filter/Amp/Unison/LFO tabs remain.
    ov.addRect({b.x,b.y,b.w,63.0f},overlayColor(kPanel));
    ov.addText("EFFECTS RACK",b.x+20.0f,b.y+15.0f,
               1.04f,overlayColor(kCyan));
}

std::optional<int> NativeEditor::hitFactoryPreset(float,float) const noexcept {
    // Factory patch selection lives on the Tracks/Patch page in the HTML layout.
    return std::nullopt;
}

bool NativeEditor::openDropdownAt(
    EditorPage page,float x,float y,NativeUi& ui) const {
    const auto anchor=[](Rect r){return NativeUi::Rect{r.x,r.y,r.w,r.h};};
    if(page==EditorPage::Fx){
        const auto rr=fxRackAddRect(false);
        if(!rr.contains(x,y))return false;
        std::vector<std::string> names;
        for(const auto type:kAvailableFxTypes)
            names.emplace_back(fxTypeName(type));
        ui.openDropdown(DropdownKind::FxType,0,anchor(rr),
            fxTypeMenuIndex(fxRackNewType_),std::move(names));
        return true;
    }
    if(page!=EditorPage::Synth||synthWorkspace_!=0||opSurface.page!=5)
        return false;
    const auto b=bodyRect(),tab=synthTabRect(0);
    const float gap=std::clamp(b.w*.014f,9.0f,13.0f);
    if(y<tab.y+tab.h+gap||y>=b.y+b.h)return false;
    const auto modeRect=opSurface.rect(10,2);
    if(modeRect.contains(x,y)){
        const auto p=ProjectCore::instance().selectedPatch();
        std::vector<std::string> modes{"CF - Crossfade",
            "PM - Phase","FM - Frequency","RM - Ring"};
        ui.openDropdown(DropdownKind::WaveInteraction,opSurface.op,
            anchor(modeRect),
            static_cast<int>(p.ops[static_cast<size_t>(opSurface.op)].waveInteraction),
            std::move(modes));
        return true;
    }
    const auto waveRect=opSurface.rect(15);
    const auto textureRect=opSurface.rect(16);
    const bool tonal=waveRect.contains(x,y);
    if(!tonal&&!textureRect.contains(x,y))return false;
    const auto p=ProjectCore::instance().selectedPatch();
    const int index=opSurface.op;
    const bool morph=opSurface.harmonicView==2;
    const Wave selected=morph?effectiveWaveMorph(p.ops[index]):p.ops[index].wave;
    std::vector<std::string> labels;
    const auto& choices=tonal?kTonalWaves:kTextureWaves;
    int active=-1;
    for(int i=0;i<5;++i){
        labels.emplace_back(tonal?kTonalLabels[i]:kTextureLabels[i]);
        if(selected==choices[static_cast<size_t>(i)]||
           (!tonal&&selected==Wave::Hiss&&i==0))active=i;
    }
    ui.openDropdown(tonal?DropdownKind::Wave:DropdownKind::Texture,
        index+(morph?6:0),anchor(tonal?waveRect:textureRect),active,
        std::move(labels));
    return true;
}

bool NativeEditor::applyDropdownChoice(const DropdownChoice& choice){
    auto& project=ProjectCore::instance();
    switch(choice.kind){
        case DropdownKind::FxType:
            if(choice.option<0||
               choice.option>=static_cast<int>(kAvailableFxTypes.size()))return false;
            fxRackNewType_=kAvailableFxTypes[static_cast<size_t>(choice.option)];
            return false;
        case DropdownKind::WaveInteraction:
            if(choice.context<0||choice.context>=6||
               choice.option<0||choice.option>=4)return false;
            return project.setSelectedWaveInteraction(choice.context,
                static_cast<WaveInteraction>(choice.option));
        case DropdownKind::Wave:
        case DropdownKind::Texture:
            if(choice.context<0||choice.context>=12||choice.option<0||
               choice.option>=5)return false;
            return project.setSelectedOperatorWave(
                choice.context%6,
                choice.kind==DropdownKind::Wave?
                    kTonalWaves[static_cast<size_t>(choice.option)]:
                    kTextureWaves[static_cast<size_t>(choice.option)],
                choice.context>=6);

        case DropdownKind::LfoTarget:
            if(choice.option<0||choice.option>=4)return false;
            return project.setSelectedLfoTarget(
                static_cast<LfoTarget>(choice.option));

        default:
            return false;
    }
}

std::optional<int> NativeEditor::hitOperatorRatio(float x,float y) const noexcept {
    if(synthWorkspace_!=0)return std::nullopt;
    const auto b=bodyRect(),tab=synthTabRect(0);
    const float gap=std::clamp(b.w*.014f,9.0f,13.0f);
    const float top=tab.y+tab.h+gap;
    opSurface.view={b.x,top,b.w,std::max(0.0f,b.y+b.h-top)};
    // Do not fall through to retired six-card geometry.
    if(opSurface.hitRatio(x,y))return opSurface.op;
    return std::nullopt;
}

std::optional<int> NativeEditor::hitWave2Ratio(float x,float y) const noexcept {
    if(synthWorkspace_!=0||opSurface.page!=5)return std::nullopt;
    const auto b=bodyRect(),tab=synthTabRect(0);
    const float gap=std::clamp(b.w*.014f,9.0f,13.0f);
    const float top=tab.y+tab.h+gap;
    opSurface.view={b.x,top,b.w,std::max(0.0f,b.y+b.h-top)};
    if(opSurface.hitWave2Ratio(x,y))return opSurface.op;
    return std::nullopt;
}

std::optional<int> NativeEditor::hitOperatorSemitone(float x,float y) const noexcept {
    if(synthWorkspace_!=0)return std::nullopt;
    const auto b=bodyRect(),tab=synthTabRect(0);
    const float gap=std::clamp(b.w*.014f,9.0f,13.0f);
    const float top=tab.y+tab.h+gap;
    opSurface.view={b.x,top,b.w,std::max(0.0f,b.y+b.h-top)};
    // Only the visible Base ST field can open the semitone editor.
    // Legacy semitone rows overlap the new Mod and shadow controls.
    if(opSurface.hitSemi(x,y))return opSurface.op;
    return std::nullopt;
}

std::optional<NativeEditor::Hit> NativeEditor::hitSynth(float x,float y) const noexcept {
    for(int i=0;i<2;++i){
        const auto rr=synthTabRect(i);
        if(rr.contains(x,y))
            return Hit{HitKind::SynthTab,i,-1,rr};
    }

    const auto b=bodyRect();
    const auto tab=synthTabRect(0);
    const float gap=std::clamp(b.w*0.014f,9.0f,13.0f);
    const float contentTop=tab.y+tab.h+gap;
    if(y<contentTop)return std::nullopt;

    const Patch p=ProjectCore::instance().selectedPatch();
    for(int op=0;op<6;++op){
        const auto wave=operatorWaveFieldRect(op);
        if(wave.contains(x,y))
            return Hit{HitKind::WaveSelect,op,-1,wave};

        const auto toggle=operatorCardToggleRect(op);
        if(toggle.contains(x,y))
            return Hit{HitKind::OperatorToggle,op,-1,toggle};

        for(int param=0;param<8;++param){
            const auto rr=operatorCardParamRect(op,param);
            if(rr.contains(x,y))
                return Hit{HitKind::OperatorParam,op,param,rr};
        }

        if(p.ops[static_cast<size_t>(op)].wave==Wave::Custom){
            for(int partial=0;partial<16;++partial){
                const auto rr=operatorCardHarmonicRect(op,partial);
                if(rr.contains(x,y))
                    return Hit{HitKind::Harmonic,op,partial,rr};
            }
        }
    }
    return std::nullopt;
}

std::optional<NativeEditor::Hit> NativeEditor::hitFx(float x,float y) const noexcept {
    const auto rack=ProjectCore::instance().fxRack(false,fxDrumMasterActive());
    for(int i=0;i<rack.count;++i){
        const auto r=fxRackStageRect(i);
        const float bw=std::clamp(r.w*.10f,39.0f,67.0f);
        const float right=r.x+r.w;
        const HitKind actions[4]={
            HitKind::RackUp,HitKind::RackDown,HitKind::RackBypass,HitKind::RackDelete};
        for(int j=0;j<4;++j){
            const Rect cell{right-(4-j)*bw,r.y+5,bw-3,r.h-10};
            if(cell.contains(x,y))return Hit{actions[j],i,-1,cell};
        }
        if(r.contains(x,y))return Hit{HitKind::RackExpand,i,-1,r};
        if(i==fxRackExpanded_){
            for(size_t param=0;param<fxParamCount(rack.chain[i].type);++param){
                const auto pr=fxRackParamRect(i,static_cast<int>(param));
                if(pr.contains(x,y))return Hit{HitKind::RackParameter,i,static_cast<int>(param),pr};
            }
        }
    }
    if(const auto r=fxRackAddRect(true);r.contains(x,y))
        return Hit{HitKind::RackAdd,-1,-1,r};
    // Other FX workspaces use their own dedicated point/curve surfaces.
    return std::nullopt;
}

bool NativeEditor::applyHit(const Hit& hit,float x,float y){
    auto& project=ProjectCore::instance();
    const float nx=hit.rect.w>0
        ?std::clamp((x-hit.rect.x)/hit.rect.w,0.0f,1.0f)
        :0.5f;
    const float ny=hit.rect.h>0
        ?std::clamp((y-hit.rect.y)/hit.rect.h,0.0f,1.0f)
        :0.5f;

    switch(hit.kind){
        case HitKind::RackExpand:
            fxRackExpanded_=fxRackExpanded_==hit.a?-1:hit.a;
            return false;
        case HitKind::RackUp:
        case HitKind::RackDown:{
            const int shift=hit.kind==HitKind::RackUp?-1:1;
            if(project.moveFxStage(false,hit.a,shift,fxDrumMasterActive())){
                if(fxRackExpanded_==hit.a)fxRackExpanded_+=shift;
                else if(fxRackExpanded_==hit.a+shift)fxRackExpanded_=hit.a;
                return true;
            }
            return false;
        }
        case HitKind::RackBypass:{
            const Fx rack=project.fxRack(false,fxDrumMasterActive());
            if(hit.a<0||hit.a>=rack.count)return false;
            return project.setFxStageEnabled(false,hit.a,!rack.chain[hit.a].enabled,fxDrumMasterActive());
        }
        case HitKind::RackDelete:{
            if(!project.removeFxStage(false,hit.a,fxDrumMasterActive()))return false;
            if(fxRackExpanded_==hit.a)fxRackExpanded_=-1;
            else if(fxRackExpanded_>hit.a)--fxRackExpanded_;
            return true;
        }
        case HitKind::RackParameter:{
            const Fx rack=project.fxRack(false,fxDrumMasterActive());
            if(hit.a<0||hit.a>=rack.count)return false;
            const auto d=fxParamDesc(rack.chain[hit.a].type,static_cast<size_t>(hit.b));
            const float lw=std::clamp(hit.rect.w*.27f,115.0f,182.0f),vw=71.0f;
            const float range=std::max(24.0f,hit.rect.w-lw-vw-9.0f);
            const float fraction=std::clamp((x-(hit.rect.x+lw))/range,0.0f,1.0f);
            return project.setFxStageParameter(false,hit.a,hit.b,
                d.lo+fraction*(d.hi-d.lo),fxDrumMasterActive());
        }
        case HitKind::RackAdd:{
            if(!project.addFxStage(false,fxRackNewType_,fxDrumMasterActive()))return false;
            fxRackExpanded_=project.fxRack(false,fxDrumMasterActive()).count-1;
            return true;
        }
        case HitKind::SynthTab:
            synthWorkspace_=std::clamp(hit.a,0,2);
            return false;

        case HitKind::OperatorSelect:
            selectedOperator_=std::clamp(hit.a,0,5);
            return false;

        case HitKind::WaveSelect:{
            // Legacy one-tap control cycles tonal presets only; the
            // visible editor uses separate five-item Wave/Texture menus.
            const int op=std::clamp(hit.a,0,5);
            const auto p=project.selectedPatch();
            const Wave current=p.ops[static_cast<size_t>(op)].wave;
            int index=-1;
            for(int i=0;i<5;++i)
                if(kTonalWaves[static_cast<size_t>(i)]==current)index=i;
            return project.setSelectedOperatorWave(
                op,kTonalWaves[static_cast<size_t>((index+1)%5)]);
        }

        case HitKind::OperatorToggle:{
            const int op=std::clamp(hit.a,0,5);
            const auto p=project.selectedPatch();
            return project.setSelectedOperatorEnabled(
                op,!p.ops[static_cast<size_t>(op)].enabled);
        }

        case HitKind::OperatorParam:{
            const int op=std::clamp(hit.a,0,5);
            const int paramIndex=std::clamp(hit.b,0,3);
            const auto param=kOperatorParams[static_cast<size_t>(paramIndex)];

            // The left portion of the row is the label in the HTML layout.
            const float labelFraction=30.0f/std::max(1.0f,hit.rect.w);
            const float controlNx=std::clamp(
                (nx-labelFraction)/std::max(0.01f,1.0f-labelFraction),
                0.0f,1.0f);

            float value=denormalized(
                controlNx,operatorRange(param));
            if(param==OperatorParam::Semitone||
               param==OperatorParam::Detune)
                value=std::round(value);
            return project.setSelectedOperatorParam(
                op,param,value);
        }

        case HitKind::Harmonic:
            return project.setSelectedHarmonic(
                std::clamp(hit.a,0,5),
                std::clamp(hit.b,0,15),
                1.0f-ny,
                false);

        case HitKind::Matrix:
            return false; // removed legacy FM routing

        case HitKind::FxGroup:
            fxGroup_=std::clamp(hit.a,0,3);
            return false;

        case HitKind::FilterOpen:
            filterSurface.open=true;
            filterSurface.view=bodyRect();
            return false;
        case HitKind::PeakToggle:{
            const auto p=project.selectedPatch();
            return project.setSelectedPeakEnabled(!p.peak.enabled);
        }
        case HitKind::PeakMode:{
            const auto p=project.selectedPatch();
            return project.setSelectedPeakFollowNote(!p.peak.followNote);
        }

        case HitKind::LfoTarget:{
            const auto p=project.selectedPatch();
            const int current=std::clamp(
                static_cast<int>(p.lfo.target),0,3);
            return project.setSelectedLfoTarget(
                static_cast<LfoTarget>((current+1)%4));
        }

        case HitKind::PatchParam:{
            const auto param=static_cast<PatchParam>(hit.a);
            const float labelFraction=
                std::clamp(120.0f/std::max(1.0f,hit.rect.w),0.12f,0.36f);
            const float valueFraction=
                std::clamp(58.0f/std::max(1.0f,hit.rect.w),0.08f,0.22f);
            const float controlNx=std::clamp(
                (nx-labelFraction)/
                    std::max(0.01f,1.0f-labelFraction-valueFraction),
                0.0f,1.0f);
            return project.setSelectedPatchParam(
                param,denormalized(controlNx,patchRange(param)));
        }

        case HitKind::None:
            return false;
    }
    return false;
}

std::optional<PatchTransferAction> NativeEditor::hitPatchTransfer(
    EditorPage page,float x,float y) const noexcept {
    // Patch File buttons exist only on Operators; no invisible Filter/Amp hits.
    if(page!=EditorPage::Synth||synthWorkspace_!=0)return std::nullopt;

    const auto copy=patchTransferRect(page,1);
    if(copy.contains(x,y))return PatchTransferAction::CopyPatch;

    const auto paste=patchTransferRect(page,2);
    if(paste.contains(x,y))return PatchTransferAction::PastePatch;

    return std::nullopt;
}

bool NativeEditor::pointerDown(EditorPage page,float x,float y){
    // The editor must never intercept touches in expression, transport or
    // navigation, even when a secondary editor surface contains stale coords.
    if(!viewportRect().contains(x,y))return false;
    kineticScroll_.stop();
    activePage_=page;
    changed_=false;
    scrolling_=false;
    activeHit_.reset();

    const auto b=bodyRect();
    const auto first=synthTabRect(0);
    const float gap=std::clamp(b.w*.014f,9.0f,13.0f);
    const float surfaceTop=first.y+first.h+gap;
    const Rect surface{b.x,surfaceTop,b.w,
        std::max(0.0f,b.y+b.h-surfaceTop)};
    if(page==EditorPage::Synth){
        for(int i=0;i<3;++i){
            if(synthTabRect(i).contains(x,y)){
                synthWorkspace_=i;
                return true;
            }
        }
    }
    if(scrollGutterRect().contains(x,y)){
        scrolling_=true;
        scrollLastY_=y;
        kineticScroll_.begin(y,kineticClockMs());
        return true;
    }
    if(page==EditorPage::Synth){
        if(synthWorkspace_==0){
            opSurface.view=surface;
            if(opSurface.down(x,y)){
                activeHit_=Hit{HitKind::OperatorSelect,-1,-1,{}};
                return true;
            }
        }else if(synthWorkspace_==1){
            filterSurface.view=surface;
            if(filterSurface.down(x,y)){
                activeHit_=Hit{HitKind::FilterEditor,-1,-1,{}};
                return true;
            }
        }else{
            for(int i=0;i<2;++i){
                const float w=(surface.w-31.0f)*.5f;
                const Rect sub{surface.x+12.0f+i*(w+7.0f),
                    surface.y+5.0f,w,55.0f};
                if(sub.contains(x,y)){ampSubtab_=i;return true;}
            }
            const Rect editor{surface.x,surface.y+55.0f+gap,
                surface.w,std::max(0.0f,surface.h-55.0f-gap)};
            if(ampSubtab_==0){
                unisonSurface.ampMode=true;
                unisonSurface.view=editor;
                if(unisonSurface.down(x,y)){
                    activeHit_=Hit{HitKind::UnisonEditor,-1,-1,{}};
                    return true;
                }
            }else{
                ampModSurface.view=editor;
                if(ampModSurface.down(x,y)){
                    activeHit_=Hit{HitKind::AmpModEditor,-1,-1,{}};
                    return true;
                }
            }
        }
        // The retired six-card controls and FM matrix cannot receive taps.
        if(b.contains(x,y))return true;
    }
    activeHit_=hitFx(x,y);
    if(activeHit_){
        changed_=applyHit(*activeHit_,x,y)||changed_;
        return true;
    }

    return false;
}

bool NativeEditor::applyKineticScrollDelta(float delta) noexcept {
    if(activePage_==EditorPage::Synth){
        if(synthWorkspace_==0){
            if(opSurface.page<4){
                const float old=opSurface.scroll[opSurface.op][opSurface.page];
                opSurface.scrollBy(delta);
                return old!=opSurface.scroll[opSurface.op][opSurface.page];
            }
            if(opSurface.page==5){
                const int i=opSurface.harmonicView==2?1:0;
                const float old=opSurface.harmonicScroll[opSurface.op][i];
                opSurface.scrollBy(delta);
                return old!=opSurface.harmonicScroll[opSurface.op][i];
            }
        }else if(synthWorkspace_==1){
            const float old=filterSurface.scroll;
            filterSurface.scrollBy(delta);
            return old!=filterSurface.scroll;
        }else if(ampSubtab_==0){
            const float old=unisonSurface.scroll;
            unisonSurface.scrollBy(delta);
            return old!=unisonSurface.scroll;
        }
        return false;
    }
    const float old=fxScrollY_;
    scrollEditor(EditorPage::Fx,delta);
    return old!=fxScrollY_;
}
void NativeEditor::advanceKineticScrolling(float dt) noexcept {
    if(!kineticScroll_.active())return;
    const float delta=kineticScroll_.advance(dt);
    if(delta!=0.0f&&!applyKineticScrollDelta(delta))
        kineticScroll_.stop();
}
bool NativeEditor::pointerMove(float x,float y){
    if(scrolling_){
        const float delta=kineticScroll_.move(y,kineticClockMs());
        applyKineticScrollDelta(delta);
        scrollLastY_=y;
        return true;
    }

    if(!activeHit_)return false;
    if(activePage_==EditorPage::Synth&&
       activeHit_->kind==HitKind::FilterEditor){
        filterSurface.move(x,y);return true;
    }
    if(activePage_==EditorPage::Synth&&
       activeHit_->kind==HitKind::UnisonEditor){
        unisonSurface.move(x,y);return true;
    }
    if(activePage_==EditorPage::Synth&&synthWorkspace_==0&&
       activeHit_->kind==HitKind::OperatorSelect){
        opSurface.move(x,y);return true;
    }
    if(activePage_==EditorPage::Synth&&
       activeHit_->kind==HitKind::AmpModEditor){
        ampModSurface.move(x,y);return true;
    }
    switch(activeHit_->kind){
        case HitKind::OperatorParam:
        case HitKind::Harmonic:
        case HitKind::Matrix:
        case HitKind::PatchParam:
        case HitKind::RackParameter:
            changed_=applyHit(*activeHit_,x,y)||changed_;
            return true;

        default:
            return true;
    }
}

bool NativeEditor::pointerUp(){
    if(activePage_==EditorPage::Synth&&activeHit_&&
       activeHit_->kind==HitKind::FilterEditor)
        changed_=filterSurface.up()||changed_;
    if(activePage_==EditorPage::Synth&&activeHit_&&
       activeHit_->kind==HitKind::UnisonEditor)
        changed_=unisonSurface.up()||changed_;
    if(activePage_==EditorPage::Synth&&synthWorkspace_==0&&activeHit_&&
       activeHit_->kind==HitKind::OperatorSelect)
        changed_=opSurface.up()||changed_;
    if(activePage_==EditorPage::Synth&&activeHit_&&
       activeHit_->kind==HitKind::AmpModEditor)
        changed_=ampModSurface.up()||changed_;
    const bool changed=changed_;
    activeHit_.reset();
    if(scrolling_)kineticScroll_.end(kineticClockMs());
    scrolling_=false;
    changed_=false;
    return changed;
}

void NativeEditor::cancel() noexcept {
    activeHit_.reset();
    scrolling_=false;
    kineticScroll_.stop();
    changed_=false;
}

} // namespace aiora
