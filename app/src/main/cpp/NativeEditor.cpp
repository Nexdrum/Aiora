#include "NativeEditor.h"
#include "NativeOverlay.h"

#include <GLES3/gl3.h>

#include <algorithm>
#include <array>
#include <string>

namespace aiora {
namespace {

constexpr NativeEditor::Rgb kBg{0.0627f,0.0706f,0.0863f};
constexpr NativeEditor::Rgb kPanel{0.0863f,0.1020f,0.1294f};
constexpr NativeEditor::Rgb kButton{0.1373f,0.1569f,0.2000f};
constexpr NativeEditor::Rgb kTrack{0.18f,0.21f,0.27f};
constexpr NativeEditor::Rgb kCyan{0.0f,0.80f,0.80f};
constexpr NativeEditor::Rgb kOrange{1.0f,0.6667f,0.0f};
constexpr NativeEditor::Rgb kPurple{0.788f,0.557f,1.0f};
constexpr NativeEditor::Rgb kGreen{0.23f,0.92f,0.45f};
constexpr NativeEditor::Rgb kRed{0.95f,0.18f,0.18f};
constexpr NativeEditor::Rgb kWhite{0.91f,0.925f,0.945f};

constexpr std::array<PatchParam,7> kFilterParams{
    PatchParam::FilterCutoff,PatchParam::FilterResonance,PatchParam::FilterEnv,
    PatchParam::FilterAttack,PatchParam::FilterDecay,PatchParam::FilterSustain,PatchParam::FilterRelease
};
constexpr std::array<PatchParam,9> kAmpParams{
    PatchParam::AmpAttack,PatchParam::AmpDecay,PatchParam::AmpSustain,PatchParam::AmpRelease,
    PatchParam::Volume,PatchParam::VelocityAmp,PatchParam::VelocityFilter,PatchParam::Unison,PatchParam::Glide
};
constexpr std::array<PatchParam,9> kLfoFxParams{
    PatchParam::LfoRate,PatchParam::LfoAmount,PatchParam::LfoAttack,PatchParam::LfoVelocity,
    PatchParam::Distortion,PatchParam::Delay,PatchParam::DelayTime,PatchParam::DelayFeedback,PatchParam::Reverb
};
constexpr std::array<OperatorParam,7> kOperatorParams{
    OperatorParam::Ratio,OperatorParam::Detune,OperatorParam::Level,
    OperatorParam::Attack,OperatorParam::Decay,OperatorParam::Sustain,OperatorParam::Release
};

NativeEditor::Rgb mix(NativeEditor::Rgb a,NativeEditor::Rgb b,float t) noexcept {
    t=std::clamp(t,0.0f,1.0f);
    return {a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};
}

NativeOverlay::Color overlayColor(NativeEditor::Rgb c,float alpha=1.0f) noexcept {
    return {c.r,c.g,c.b,alpha};
}

const char* modTargetName(ModTarget t) noexcept {
    switch(t){
        case ModTarget::None:return "NONE";
        case ModTarget::Cutoff:return "CUTOFF";
        case ModTarget::Resonance:return "RESO";
        case ModTarget::FilterEnv:return "FENV";
        case ModTarget::AmpAttack:return "A ATK";
        case ModTarget::AmpDecay:return "A DEC";
        case ModTarget::AmpSustain:return "A SUS";
        case ModTarget::AmpRelease:return "A REL";
        case ModTarget::FilterAttack:return "F ATK";
        case ModTarget::FilterDecay:return "F DEC";
        case ModTarget::FilterSustain:return "F SUS";
        case ModTarget::FilterRelease:return "F REL";
        case ModTarget::Op1:return "OP1"; case ModTarget::Op2:return "OP2";
        case ModTarget::Op3:return "OP3"; case ModTarget::Op4:return "OP4";
        case ModTarget::Op5:return "OP5"; case ModTarget::Op6:return "OP6";
        case ModTarget::Morph1:return "MORPH1"; case ModTarget::Morph2:return "MORPH2";
        case ModTarget::Morph3:return "MORPH3"; case ModTarget::Morph4:return "MORPH4";
        case ModTarget::Morph5:return "MORPH5"; case ModTarget::Morph6:return "MORPH6";
        case ModTarget::Fm:return "FM";
        case ModTarget::LfoAmount:return "LFO A";
        case ModTarget::LfoRate:return "LFO R";
        case ModTarget::Unison:return "UNI";
        case ModTarget::Volume:return "VOL";
    }
    return "?";
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

NativeEditor::Rect NativeEditor::contentRect() const noexcept {
    const float usableW=std::max(0,width_-safeLeft_-safeRight_);
    const float margin=std::clamp(usableW*0.018f,10.0f,18.0f);
    const float headerH=std::clamp(usableW*0.118f,80.0f,102.0f);
    const float navH=std::clamp(usableW*0.094f,66.0f,82.0f);
    const float gap=std::clamp(usableW*0.016f,10.0f,16.0f);
    const float switchH=std::clamp(
        (static_cast<float>(height_-safeTop_-safeBottom_))*0.060f,
        48.0f,64.0f);
    const float top=
        static_cast<float>(safeTop_)+headerH+gap+navH+gap+switchH+gap;
    const float bottom=static_cast<float>(height_-safeBottom_);
    return {
        static_cast<float>(safeLeft_)+margin,
        top,
        std::max(0.0f,usableW-margin*2.0f),
        std::max(0.0f,bottom-top-margin)
    };
}

NativeEditor::Rect NativeEditor::editorRect() const noexcept {
    return contentRect();
}

NativeEditor::Rect NativeEditor::patchTransferRect(int index) const noexcept {
    const auto e=editorRect();
    if(index==0)return {};
    const float gap=std::clamp(e.w*0.014f,9.0f,13.0f);
    const float h=46.0f;
    const float w=(e.w-gap*3.0f)*0.5f;
    return {
        e.x+gap+(index-1)*(w+gap),
        e.y+e.h-h-gap,
        w,h
    };
}

NativeEditor::Rect NativeEditor::bodyRect() const noexcept {
    auto r=editorRect();
    const float gap=std::clamp(r.w*0.014f,9.0f,13.0f);
    r.h=std::max(0.0f,r.h-78.0f-gap);
    return r;
}

NativeEditor::Rect NativeEditor::factoryPresetRect(int) const noexcept {
    return {};
}

NativeEditor::Rect NativeEditor::synthTabRect(int index) const noexcept {
    const auto b=bodyRect();
    const float gap=std::clamp(b.w*0.014f,9.0f,13.0f);
    const float h=48.0f;
    const float w=(b.w-gap*3.0f)*0.5f;
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
    const float h=610.0f;
    const int row=index/2,col=index%2;
    return {
        b.x+gap+col*(w+gap),
        tab.y+tab.h+gap+row*(h+gap)-synthScrollY_,
        w,h
    };
}

NativeEditor::Rect NativeEditor::operatorWaveFieldRect(int op) const noexcept {
    const auto c=operatorCardRect(op);
    return {c.x+14.0f,c.y+48.0f,c.w-28.0f,44.0f};
}

NativeEditor::Rect NativeEditor::operatorCardParamRect(int op,int param) const noexcept {
    const auto c=operatorCardRect(op);
    return {
        c.x+14.0f,
        c.y+102.0f+param*48.0f,
        c.w-28.0f,
        42.0f
    };
}

NativeEditor::Rect NativeEditor::operatorCardToggleRect(int op) const noexcept {
    const auto c=operatorCardRect(op);
    return {c.x+14.0f,c.y+444.0f,c.w-28.0f,46.0f};
}

NativeEditor::Rect NativeEditor::operatorCardHarmonicRect(int op,int partial) const noexcept {
    const auto c=operatorCardRect(op);
    const float gap=6.0f;
    const int row=partial/8,col=partial%8;
    const float w=(c.w-28.0f-gap*7.0f)/8.0f;
    return {
        c.x+14.0f+col*(w+gap),
        c.y+504.0f+row*46.0f,
        w,38.0f
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
    const float top=tabs.y+tabs.h+gap;
    const float availH=std::max(0.0f,b.y+b.h-top-gap);
    const float cell=std::max(
        34.0f,
        std::min(
            (b.w-gap*7.0f)/6.0f,
            (availH-gap*7.0f)/6.0f));
    const float gridW=cell*6.0f+gap*5.0f;
    const float gridH=cell*6.0f+gap*5.0f;
    const float ox=b.x+(b.w-gridW)*0.5f;
    const float oy=top+(availH-gridH)*0.5f;
    return {
        ox+carrier*(cell+gap),
        oy+modulator*(cell+gap),
        cell,cell
    };
}

NativeEditor::Rect NativeEditor::fxGroupRect(int index) const noexcept {
    const auto b=bodyRect();
    const float gap=std::clamp(b.w*0.014f,9.0f,13.0f);
    static constexpr float heights[4]={442.0f,482.0f,528.0f,432.0f};
    float y=b.y+gap-fxScrollY_;
    for(int i=0;i<index;++i)y+=heights[i]+gap;
    return {
        b.x+gap,y,
        b.w-gap*2.0f,
        heights[std::clamp(index,0,3)]
    };
}

NativeEditor::Rect NativeEditor::fxSectionParamRect(int section,int row) const noexcept {
    const auto card=fxGroupRect(section);
    float y=card.y+48.0f;
    if(section==0)y=card.y+100.0f+row*46.0f;
    else if(section==1)y=card.y+48.0f+row*46.0f;
    else if(section==2){
        const int visualRow=row+(row>=2?1:0);
        y=card.y+48.0f+visualRow*46.0f;
    }
    return {card.x+18.0f,y,card.w-36.0f,42.0f};
}

NativeEditor::Rect NativeEditor::filterTypeRect(int) const noexcept {
    const auto card=fxGroupRect(0);
    return {
        card.x+card.w*0.58f,
        card.y+48.0f,
        card.w*0.36f,
        42.0f
    };
}

NativeEditor::Rect NativeEditor::lfoTargetRect(int) const noexcept {
    const auto card=fxGroupRect(2);
    return {
        card.x+card.w*0.58f,
        card.y+48.0f+2.0f*46.0f,
        card.w*0.36f,
        42.0f
    };
}

NativeEditor::Rect NativeEditor::fxSliderRect(int row,int,bool) const noexcept {
    return fxSectionParamRect(std::clamp(fxGroup_,0,2),row);
}

NativeEditor::Rect NativeEditor::modRowRect(int slot) const noexcept {
    const auto card=fxGroupRect(3);
    return {
        card.x+18.0f,
        card.y+50.0f+slot*76.0f,
        card.w-36.0f,
        68.0f
    };
}

NativeEditor::Rect NativeEditor::modPartRect(int slot,int part) const noexcept {
    const auto r=modRowRect(slot);
    const float gap=6.0f;
    // Keep six logical parts so existing interaction types remain usable.
    // 1=target, 3=min, 4=max, 5=delete are the visible HTML-style controls.
    static constexpr std::array<float,6> weights{
        0.01f,0.36f,0.01f,0.24f,0.24f,0.10f};
    float total=0.0f;
    for(float w:weights)total+=w;
    const float usable=r.w-gap*5.0f;
    float x=r.x;
    for(int i=0;i<part;++i)
        x+=usable*(weights[static_cast<size_t>(i)]/total)+gap;
    const float w=
        usable*(weights[static_cast<size_t>(part)]/total);
    return {x,r.y,w,r.h};
}

NativeEditor::Rect NativeEditor::modAddRect() const noexcept {
    const auto card=fxGroupRect(3);
    return {
        card.x+18.0f,
        card.y+card.h-62.0f,
        std::min(300.0f,card.w-36.0f),
        46.0f
    };
}

NativeEditor::Range NativeEditor::operatorRange(OperatorParam p) noexcept {
    switch(p){
        case OperatorParam::Ratio:return {0.125f,16.0f};
        case OperatorParam::Detune:return {-100.0f,100.0f};
        case OperatorParam::Level:return {0.0f,1.0f};
        case OperatorParam::Attack:return {0.001f,1.0f};
        case OperatorParam::Decay:return {0.005f,1.5f};
        case OperatorParam::Sustain:return {0.0f,1.0f};
        case OperatorParam::Release:return {0.02f,2.0f};
    }
    return {0.0f,1.0f};
}
NativeEditor::Range NativeEditor::patchRange(PatchParam p) noexcept {
    switch(p){
        case PatchParam::FilterCutoff:return {40.0f,18000.0f};
        case PatchParam::FilterResonance:return {0.1f,18.0f};
        case PatchParam::FilterEnv:return {0.0f,1.0f};
        case PatchParam::FilterAttack:return {0.005f,1.0f};
        case PatchParam::FilterDecay:return {0.01f,1.5f};
        case PatchParam::FilterSustain:return {0.0f,1.0f};
        case PatchParam::FilterRelease:return {0.02f,2.0f};
        case PatchParam::AmpAttack:return {0.001f,1.0f};
        case PatchParam::AmpDecay:return {0.01f,1.5f};
        case PatchParam::AmpSustain:return {0.0f,1.0f};
        case PatchParam::AmpRelease:return {0.02f,2.0f};
        case PatchParam::VelocityAmp:
        case PatchParam::VelocityFilter:
        case PatchParam::LfoAmount:
        case PatchParam::LfoVelocity:
        case PatchParam::Unison:
        case PatchParam::Glide:
        case PatchParam::Volume:
        case PatchParam::Distortion:
        case PatchParam::Delay:
        case PatchParam::Reverb:return {0.0f,1.0f};
        case PatchParam::LfoRate:return {0.1f,20.0f};
        case PatchParam::LfoAttack:return {0.0f,2.0f};
        case PatchParam::DelayTime:return {0.03f,1.0f};
        case PatchParam::DelayFeedback:return {0.0f,0.85f};
    }
    return {0.0f,1.0f};
}
NativeEditor::Range NativeEditor::modRange(ModTarget t) noexcept {
    switch(t){
        case ModTarget::Cutoff:return {40.0f,18000.0f};
        case ModTarget::Resonance:return {0.1f,18.0f};
        case ModTarget::FilterEnv:return {0.0f,1.0f};
        case ModTarget::AmpAttack:return {0.001f,1.0f};
        case ModTarget::AmpDecay:return {0.01f,1.5f};
        case ModTarget::AmpSustain:return {0.0f,1.0f};
        case ModTarget::AmpRelease:return {0.02f,2.0f};
        case ModTarget::FilterAttack:return {0.005f,1.0f};
        case ModTarget::FilterDecay:return {0.01f,1.5f};
        case ModTarget::FilterSustain:return {0.0f,1.0f};
        case ModTarget::FilterRelease:return {0.02f,2.0f};
        case ModTarget::Op1:case ModTarget::Op2:case ModTarget::Op3:
        case ModTarget::Op4:case ModTarget::Op5:case ModTarget::Op6:
        case ModTarget::Morph1:case ModTarget::Morph2:case ModTarget::Morph3:
        case ModTarget::Morph4:case ModTarget::Morph5:case ModTarget::Morph6:
        case ModTarget::Unison:return {0.0f,1.0f};
        case ModTarget::Fm:return {0.0f,1.5f};
        case ModTarget::LfoAmount:return {0.0f,2.0f};
        case ModTarget::LfoRate:return {0.1f,20.0f};
        case ModTarget::Volume:return {0.0f,2.0f};
        case ModTarget::None:return {0.0f,1.0f};
    }
    return {0.0f,1.0f};
}
float NativeEditor::operatorValue(const Patch& p,int op,OperatorParam param) noexcept {
    if(op<0||op>=6)return 0.0f;const auto& o=p.ops[static_cast<size_t>(op)];
    switch(param){
        case OperatorParam::Ratio:return o.ratio;
        case OperatorParam::Detune:return o.detuneCents;
        case OperatorParam::Level:return o.level;
        case OperatorParam::Attack:return o.env.attack;
        case OperatorParam::Decay:return o.env.decay;
        case OperatorParam::Sustain:return o.env.sustain;
        case OperatorParam::Release:return o.env.release;
    }
    return 0.0f;
}
float NativeEditor::patchValue(const Patch& p,PatchParam param) noexcept {
    switch(param){
        case PatchParam::FilterCutoff:return p.filter.cutoff;
        case PatchParam::FilterResonance:return p.filter.resonance;
        case PatchParam::FilterEnv:return p.filter.envAmount;
        case PatchParam::FilterAttack:return p.filter.env.attack;
        case PatchParam::FilterDecay:return p.filter.env.decay;
        case PatchParam::FilterSustain:return p.filter.env.sustain;
        case PatchParam::FilterRelease:return p.filter.env.release;
        case PatchParam::AmpAttack:return p.amp.attack;
        case PatchParam::AmpDecay:return p.amp.decay;
        case PatchParam::AmpSustain:return p.amp.sustain;
        case PatchParam::AmpRelease:return p.amp.release;
        case PatchParam::VelocityAmp:return p.velocityAmp;
        case PatchParam::VelocityFilter:return p.velocityFilter;
        case PatchParam::LfoRate:return p.lfo.rate;
        case PatchParam::LfoAmount:return p.lfo.amount;
        case PatchParam::LfoAttack:return p.lfo.attack;
        case PatchParam::LfoVelocity:return p.lfo.velocitySensitivity;
        case PatchParam::Unison:return p.unison;
        case PatchParam::Glide:return p.glide;
        case PatchParam::Volume:return p.volume;
        case PatchParam::Distortion:return p.fx.distortion;
        case PatchParam::Delay:return p.fx.delay;
        case PatchParam::DelayTime:return p.fx.delayTime;
        case PatchParam::DelayFeedback:return p.fx.delayFeedback;
        case PatchParam::Reverb:return p.fx.reverb;
    }
    return 0.0f;
}
float NativeEditor::normalized(float value,Range r) noexcept {
    if(r.hi<=r.lo)return 0.0f;return std::clamp((value-r.lo)/(r.hi-r.lo),0.0f,1.0f);
}
float NativeEditor::denormalized(float value,Range r) noexcept {
    return r.lo+std::clamp(value,0.0f,1.0f)*(r.hi-r.lo);
}
ModTarget NativeEditor::cycleModTarget(ModTarget current,int direction) noexcept {
    constexpr int first=static_cast<int>(ModTarget::Cutoff);
    constexpr int last=static_cast<int>(ModTarget::Volume);
    int n=static_cast<int>(current);
    if(n<first||n>last)n=first;
    n+=direction;
    if(n>last)n=first;
    if(n<first)n=last;
    return static_cast<ModTarget>(n);
}

float NativeEditor::editorScrollMax(EditorPage page) const noexcept {
    const auto b=bodyRect();
    if(page==EditorPage::Synth){
        if(matrixMode_)return 0.0f;
        const auto last=operatorCardRect(5);
        const float bottom=last.y+synthScrollY_+last.h+12.0f;
        return std::max(0.0f,bottom-(b.y+b.h));
    }
    const auto last=fxGroupRect(3);
    const float bottom=last.y+fxScrollY_+last.h+12.0f;
    return std::max(0.0f,bottom-(b.y+b.h));
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
    fillRect(editorRect(),kPanel);
}

void NativeEditor::drawPatchTransfer() const noexcept {
    const auto e=editorRect();
    const auto copy=patchTransferRect(1);
    const auto paste=patchTransferRect(2);
    auto& ov=NativeOverlay::instance();

    ov.addText(
        "PATCH FILE",
        e.x+12.0f,
        copy.y-24.0f,
        0.86f,
        overlayColor(kWhite));

    drawButton(copy,false,kCyan);
    drawButton(paste,false,kPurple);
    ov.addTextCentered(
        "Export patch",
        {copy.x,copy.y,copy.w,copy.h},
        0.84f,
        overlayColor(kWhite));
    ov.addTextCentered(
        "Import patch",
        {paste.x,paste.y,paste.w,paste.h},
        0.84f,
        overlayColor(kWhite));
}

void NativeEditor::renderSynth() const noexcept {
    drawPadReservedBackground();
    drawPatchTransfer();

    const Patch p=ProjectCore::instance().selectedPatch();
    auto& ov=NativeOverlay::instance();

    static constexpr const char* kTabs[2]={"Operators","FM Matrix"};
    for(int i=0;i<2;++i){
        const auto rr=synthTabRect(i);
        const bool active=(i==1)==matrixMode_;
        drawButton(rr,active,i==1?kPurple:kCyan);
        ov.addTextCentered(
            kTabs[i],
            {rr.x,rr.y,rr.w,rr.h},
            0.96f,
            overlayColor(active?kBg:kWhite));
    }

    if(matrixMode_){
        for(int m=0;m<6;++m){
            for(int carrier=0;carrier<6;++carrier){
                const auto rr=matrixRect(m,carrier);
                const float v=std::clamp(
                    p.matrix[static_cast<size_t>(m)][static_cast<size_t>(carrier)],
                    0.0f,1.0f);
                fillRect(rr,mix(kButton,kPurple,0.12f+v*0.76f));
                const float inset=std::max(3.0f,rr.w*0.10f);
                fillRect({
                    rr.x+inset,
                    rr.y+rr.h-inset-std::max(4.0f,(rr.h-inset*2.0f)*v),
                    rr.w-inset*2.0f,
                    std::max(4.0f,(rr.h-inset*2.0f)*v)},
                    mix(kCyan,kPurple,0.56f));
                ov.addTextCentered(
                    "M"+std::to_string(m+1)+"→"+std::to_string(carrier+1),
                    {rr.x,rr.y,rr.w,rr.h},
                    0.64f,
                    overlayColor(kWhite));
            }
        }
        return;
    }

    static constexpr const char* kWaveNames[6]={
        "sine","sawtooth","square","triangle","custom","noise"};
    static constexpr const char* kLabels[7]={
        "R","F","L","A","D","S","R"};

    const auto shortValue=[](float v){
        std::string s=std::to_string(v);
        while(s.size()>1&&s.back()=='0')s.pop_back();
        if(!s.empty()&&s.back()=='.')s.pop_back();
        if(s.size()>6)s.resize(6);
        return s;
    };

    for(int op=0;op<6;++op){
        const auto card=operatorCardRect(op);
        const auto& o=p.ops[static_cast<size_t>(op)];
        const bool enabled=o.enabled;

        fillRect(card,enabled?kCyan:kButton);
        fillRect({
            card.x+2.0f,card.y+2.0f,
            card.w-4.0f,card.h-4.0f},
            kPanel);

        ov.addText(
            "OP"+std::to_string(op+1),
            card.x+14.0f,card.y+16.0f,
            0.90f,overlayColor(kWhite));

        const auto wave=operatorWaveFieldRect(op);
        fillRect(wave,mix(kRollBg,kButton,0.25f));
        const int waveIndex=std::clamp(static_cast<int>(o.wave),0,5);
        ov.addText(
            kWaveNames[waveIndex],
            wave.x+10.0f,wave.y+11.0f,
            0.84f,overlayColor(kWhite));
        ov.addDownChevron(
            {wave.x+wave.w-38.0f,wave.y,38.0f,wave.h},
            overlayColor(kWhite));

        for(int paramIndex=0;paramIndex<7;++paramIndex){
            const auto param=kOperatorParams[static_cast<size_t>(paramIndex)];
            const auto rr=operatorCardParamRect(op,paramIndex);
            const float value=operatorValue(p,op,param);
            const float norm=normalized(value,operatorRange(param));

            ov.addText(
                kLabels[paramIndex],
                rr.x,rr.y+rr.h*0.26f,
                0.72f,overlayColor(kWhite));

            const float labelW=30.0f;
            const float valueW=50.0f;
            const float trackX=rr.x+labelW;
            const float trackW=std::max(20.0f,rr.w-labelW-valueW-6.0f);

            if(paramIndex==0){
                fillRect(
                    {trackX,rr.y+4.0f,trackW,rr.h-8.0f},
                    mix(kRollBg,kButton,0.22f));
                ov.addText(
                    shortValue(value),
                    trackX+8.0f,rr.y+11.0f,
                    0.78f,overlayColor(kWhite));
            }else{
                const float cy=rr.y+rr.h*0.52f;
                fillRect({trackX,cy-3.0f,trackW,6.0f},kTrack);
                fillRect({trackX,cy-3.0f,trackW*norm,6.0f},
                    paramIndex<3?kCyan:kOrange);
                const float knob=16.0f;
                fillRect({
                    trackX+trackW*norm-knob*0.5f,
                    cy-knob*0.5f,knob,knob},
                    mix(kWhite,paramIndex<3?kCyan:kOrange,0.25f));
            }

            ov.addText(
                shortValue(value),
                rr.x+rr.w-valueW+4.0f,
                rr.y+rr.h*0.26f,
                0.66f,overlayColor(kMuted));
        }

        const auto toggle=operatorCardToggleRect(op);
        drawButton(toggle,enabled,enabled?kGreen:kRed);
        ov.addTextCentered(
            enabled?"ON":"OFF",
            {toggle.x,toggle.y,toggle.w,toggle.h},
            0.90f,overlayColor(kWhite));

        if(o.wave==Wave::Custom){
            for(int partial=0;partial<16;++partial){
                const auto hr=operatorCardHarmonicRect(op,partial);
                const float v=std::clamp(
                    o.harm[static_cast<size_t>(partial)],0.0f,1.0f);
                fillRect(hr,kButton);
                fillRect({
                    hr.x+hr.w*0.24f,
                    hr.y+hr.h*(1.0f-v),
                    hr.w*0.52f,
                    std::max(3.0f,hr.h*v)},
                    mix(kCyan,kPurple,0.40f));
            }
        }
    }
}

void NativeEditor::renderFx() const noexcept {
    drawPadReservedBackground();
    drawPatchTransfer();

    const Patch p=ProjectCore::instance().selectedPatch();
    auto& ov=NativeOverlay::instance();

    static constexpr const char* kTitles[4]={
        "Filter","Amp envelope","LFO / FX","Mod (M line)"};

    const auto shortValue=[](float v){
        std::string s=std::to_string(v);
        while(s.size()>1&&s.back()=='0')s.pop_back();
        if(!s.empty()&&s.back()=='.')s.pop_back();
        if(s.size()>7)s.resize(7);
        return s;
    };

    const auto drawSection=[&](int section){
        const auto card=fxGroupRect(section);
        fillRect(card,kButton);
        fillRect({
            card.x+2.0f,card.y+2.0f,
            card.w-4.0f,card.h-4.0f},kPanel);
        ov.addText(
            kTitles[section],
            card.x+18.0f,card.y+16.0f,
            0.96f,overlayColor(kCyan));
    };

    const auto drawParam=[&](
        int section,int row,const char* label,
        PatchParam param,Rgb accent){
        const auto rr=fxSectionParamRect(section,row);
        const float value=patchValue(p,param);
        const float norm=normalized(value,patchRange(param));
        ov.addText(
            label,rr.x,rr.y+rr.h*0.26f,
            0.76f,overlayColor(kMuted));

        const float labelW=std::clamp(rr.w*0.25f,86.0f,140.0f);
        const float valueW=58.0f;
        const float x0=rr.x+labelW;
        const float w=std::max(24.0f,rr.w-labelW-valueW-8.0f);
        const float cy=rr.y+rr.h*0.52f;

        fillRect({x0,cy-3.0f,w,6.0f},kTrack);
        fillRect({x0,cy-3.0f,w*norm,6.0f},accent);
        const float knob=17.0f;
        fillRect({
            x0+w*norm-knob*0.5f,
            cy-knob*0.5f,knob,knob},
            mix(kWhite,accent,0.28f));

        ov.addText(
            shortValue(value),
            rr.x+rr.w-valueW+4.0f,
            rr.y+rr.h*0.26f,
            0.68f,overlayColor(kMuted));
    };

    // Filter
    drawSection(0);
    {
        const auto card=fxGroupRect(0);
        ov.addText(
            "Type",
            card.x+18.0f,card.y+58.0f,
            0.76f,overlayColor(kMuted));
        const auto field=filterTypeRect(0);
        fillRect(field,mix(kRollBg,kButton,0.24f));
        static constexpr const char* names[3]={
            "lowpass","highpass","bandpass"};
        const int type=std::clamp(static_cast<int>(p.filter.type),0,2);
        ov.addText(
            names[type],
            field.x+10.0f,field.y+11.0f,
            0.78f,overlayColor(kWhite));
        ov.addDownChevron(
            {field.x+field.w-38.0f,field.y,38.0f,field.h},
            overlayColor(kWhite));
    }
    static constexpr const char* filterLabels[7]={
        "Cutoff","Reso","Env→Cut","A","D","S","R"};
    for(int i=0;i<7;++i)
        drawParam(0,i,filterLabels[i],
            kFilterParams[static_cast<size_t>(i)],
            i<3?kCyan:kOrange);

    // Amp envelope
    drawSection(1);
    static constexpr const char* ampLabels[9]={
        "A","D","S","R","Volume",
        "Vel→Amp","Vel→Cut","Unison","Glide"};
    for(int i=0;i<9;++i)
        drawParam(1,i,ampLabels[i],
            kAmpParams[static_cast<size_t>(i)],
            i<5?kCyan:kOrange);

    // LFO / FX
    drawSection(2);
    static constexpr const char* lfoLabels[9]={
        "Rate","Amount","LFO Atk","LFO Vel",
        "Distort","Delay","Delay time","Feedback","Reverb"};
    for(int i=0;i<9;++i)
        drawParam(2,i,lfoLabels[i],
            kLfoFxParams[static_cast<size_t>(i)],
            i<4?kPurple:kGreen);

    {
        const auto target=lfoTargetRect(0);
        const auto card=fxGroupRect(2);
        ov.addText(
            "Target",
            card.x+18.0f,target.y+target.h*0.26f,
            0.76f,overlayColor(kMuted));
        fillRect(target,mix(kRollBg,kButton,0.24f));
        static constexpr const char* targetNames[4]={
            "none","pitch","filter","amp"};
        const int targetIndex=std::clamp(static_cast<int>(p.lfo.target),0,3);
        ov.addText(
            targetNames[targetIndex],
            target.x+10.0f,target.y+11.0f,
            0.78f,overlayColor(kWhite));
        ov.addDownChevron(
            {target.x+target.w-38.0f,target.y,38.0f,target.h},
            overlayColor(kWhite));
    }

    // Mod M-line
    drawSection(3);
    auto& project=ProjectCore::instance();
    const int count=project.selectedModSlotCount();
    for(int slot=0;slot<count&&slot<4;++slot){
        const auto s=project.selectedModSlot(slot);
        const auto row=modRowRect(slot);
        fillRect(row,mix(kPanel,kButton,0.18f));

        ov.addText(
            "M"+std::to_string(slot+1),
            row.x,row.y+19.0f,
            0.82f,overlayColor(kWhite));

        const auto target=modPartRect(slot,1);
        const auto mn=modPartRect(slot,3);
        const auto mx=modPartRect(slot,4);
        const auto del=modPartRect(slot,5);

        fillRect(target,mix(kRollBg,kButton,0.24f));
        ov.addText(
            modTargetName(s.target),
            target.x+8.0f,target.y+19.0f,
            0.72f,overlayColor(kWhite));
        ov.addDownChevron(
            {target.x+target.w-32.0f,target.y,32.0f,target.h},
            overlayColor(kWhite));

        fillRect(mn,mix(kRollBg,kButton,0.20f));
        fillRect(mx,mix(kRollBg,kButton,0.20f));
        ov.addText(
            shortValue(s.min),
            mn.x+8.0f,mn.y+19.0f,
            0.72f,overlayColor(kWhite));
        ov.addText(
            shortValue(s.max),
            mx.x+8.0f,mx.y+19.0f,
            0.72f,overlayColor(kWhite));

        drawButton(del,false,kRed);
        ov.addTextCentered(
            "×",{del.x,del.y,del.w,del.h},
            0.90f,overlayColor(kWhite));
    }

    const auto add=modAddRect();
    drawButton(add,count<4,kGreen);
    ov.addTextCentered(
        "+ Add link (max 4)",
        {add.x,add.y,add.w,add.h},
        0.82f,overlayColor(count<4?kWhite:kMuted));
}

std::optional<int> NativeEditor::hitFactoryPreset(float x,float y) const noexcept {
    for(int i=0;i<5;++i)if(factoryPresetRect(i).contains(x,y))return i;
    return std::nullopt;
}

std::optional<NativeEditor::Hit> NativeEditor::hitSynth(float x,float y) const noexcept {
    for(int i=0;i<2;++i)if(synthTabRect(i).contains(x,y))return Hit{HitKind::SynthTab,i,-1,synthTabRect(i)};
    if(matrixMode_){
        for(int m=0;m<6;++m)for(int c=0;c<6;++c)if(matrixRect(m,c).contains(x,y))return Hit{HitKind::Matrix,m,c,matrixRect(m,c)};
        return std::nullopt;
    }
    for(int i=0;i<6;++i)if(operatorSelectRect(i).contains(x,y))return Hit{HitKind::OperatorSelect,i,-1,operatorSelectRect(i)};
    for(int i=0;i<6;++i)if(waveRect(i).contains(x,y))return Hit{HitKind::WaveSelect,i,-1,waveRect(i)};
    if(operatorToggleRect().contains(x,y))return Hit{HitKind::OperatorToggle,selectedOperator_,-1,operatorToggleRect()};

    const Patch p=ProjectCore::instance().selectedPatch();
    const int op=std::clamp(selectedOperator_,0,5);
    const bool custom=p.ops[static_cast<size_t>(op)].wave==Wave::Custom;
    for(int i=0;i<7;++i)if(operatorSliderRect(i,custom).contains(x,y))return Hit{HitKind::OperatorParam,i,-1,operatorSliderRect(i,custom)};
    if(custom)for(int i=0;i<16;++i)if(harmonicRect(i).contains(x,y))return Hit{HitKind::Harmonic,i,-1,harmonicRect(i)};
    return std::nullopt;
}

std::optional<NativeEditor::Hit> NativeEditor::hitFx(float x,float y) const noexcept {
    for(int i=0;i<4;++i)if(fxGroupRect(i).contains(x,y))return Hit{HitKind::FxGroup,i,-1,fxGroupRect(i)};
    if(fxGroup_==0){
        for(int i=0;i<3;++i)if(filterTypeRect(i).contains(x,y))return Hit{HitKind::FilterType,i,-1,filterTypeRect(i)};
        for(int i=0;i<static_cast<int>(kFilterParams.size());++i){
            auto rr=fxSliderRect(i,static_cast<int>(kFilterParams.size()),true);if(rr.contains(x,y))return Hit{HitKind::PatchParam,static_cast<int>(kFilterParams[static_cast<size_t>(i)]),-1,rr};
        }
    }else if(fxGroup_==1){
        for(int i=0;i<static_cast<int>(kAmpParams.size());++i){
            auto rr=fxSliderRect(i,static_cast<int>(kAmpParams.size()),false);if(rr.contains(x,y))return Hit{HitKind::PatchParam,static_cast<int>(kAmpParams[static_cast<size_t>(i)]),-1,rr};
        }
    }else if(fxGroup_==2){
        for(int i=0;i<4;++i)if(lfoTargetRect(i).contains(x,y))return Hit{HitKind::LfoTarget,i,-1,lfoTargetRect(i)};
        for(int i=0;i<static_cast<int>(kLfoFxParams.size());++i){
            auto rr=fxSliderRect(i,static_cast<int>(kLfoFxParams.size()),true);if(rr.contains(x,y))return Hit{HitKind::PatchParam,static_cast<int>(kLfoFxParams[static_cast<size_t>(i)]),-1,rr};
        }
    }else{
        auto& p=ProjectCore::instance();const int count=p.selectedModSlotCount();
        for(int slot=0;slot<count&&slot<4;++slot){
            for(int part=0;part<6;++part){
                const auto rr=modPartRect(slot,part);if(!rr.contains(x,y))continue;
                const HitKind kinds[6]{HitKind::ModTargetPrev,HitKind::None,HitKind::ModTargetNext,HitKind::ModMin,HitKind::ModMax,HitKind::ModDelete};
                if(kinds[part]!=HitKind::None)return Hit{kinds[part],slot,-1,rr};
            }
        }
        if(modAddRect().contains(x,y))return Hit{HitKind::ModAdd,-1,-1,modAddRect()};
    }
    return std::nullopt;
}

bool NativeEditor::applyHit(const Hit& hit,float x,float y){
    auto& project=ProjectCore::instance();
    const float nx=hit.rect.w>0?std::clamp((x-hit.rect.x)/hit.rect.w,0.0f,1.0f):0.5f;
    const float ny=hit.rect.h>0?std::clamp((y-hit.rect.y)/hit.rect.h,0.0f,1.0f):0.5f;
    switch(hit.kind){
        case HitKind::SynthTab:matrixMode_=hit.a==1;return false;
        case HitKind::OperatorSelect:selectedOperator_=std::clamp(hit.a,0,5);return false;
        case HitKind::WaveSelect:return project.setSelectedOperatorWave(selectedOperator_,static_cast<Wave>(std::clamp(hit.a,0,5)));
        case HitKind::OperatorToggle:{
            const auto p=project.selectedPatch();const int op=std::clamp(selectedOperator_,0,5);
            return project.setSelectedOperatorEnabled(op,!p.ops[static_cast<size_t>(op)].enabled);
        }
        case HitKind::OperatorParam:{
            const auto param=kOperatorParams[static_cast<size_t>(std::clamp(hit.a,0,6))];
            return project.setSelectedOperatorParam(selectedOperator_,param,denormalized(nx,operatorRange(param)));
        }
        case HitKind::Harmonic:return project.setSelectedHarmonic(selectedOperator_,hit.a,1.0f-ny,false);
        case HitKind::Matrix:return project.setSelectedMatrixAmount(hit.a,hit.b,nx);
        case HitKind::FxGroup:fxGroup_=std::clamp(hit.a,0,3);return false;
        case HitKind::FilterType:return project.setSelectedFilterType(static_cast<FilterType>(std::clamp(hit.a,0,2)));
        case HitKind::LfoTarget:return project.setSelectedLfoTarget(static_cast<LfoTarget>(std::clamp(hit.a,0,3)));
        case HitKind::PatchParam:{
            const auto param=static_cast<PatchParam>(hit.a);
            return project.setSelectedPatchParam(param,denormalized(nx,patchRange(param)));
        }
        case HitKind::ModTargetPrev:
        case HitKind::ModTargetNext:{
            const auto s=project.selectedModSlot(hit.a);const int dir=hit.kind==HitKind::ModTargetNext?1:-1;
            const auto target=cycleModTarget(s.target,dir);const auto rr=modRange(target);const float mid=(rr.lo+rr.hi)*0.5f;
            return project.setSelectedModSlot(hit.a,target,mid,mid);
        }
        case HitKind::ModMin:
        case HitKind::ModMax:{
            const auto s=project.selectedModSlot(hit.a);const auto rr=modRange(s.target);const float v=denormalized(nx,rr);
            return hit.kind==HitKind::ModMin
                ?project.setSelectedModSlot(hit.a,s.target,v,s.max)
                :project.setSelectedModSlot(hit.a,s.target,s.min,v);
        }
        case HitKind::ModDelete:return project.removeSelectedModSlot(hit.a);
        case HitKind::ModAdd:return project.addSelectedModSlot(ModTarget::Cutoff);
        case HitKind::None:return false;
    }
    return false;
}

std::optional<PatchTransferAction> NativeEditor::hitPatchTransfer(
    EditorPage page,float x,float y) const noexcept {
    (void)page;
    if(patchTransferRect(0).contains(x,y))return PatchTransferAction::AiFromClipboard;
    if(patchTransferRect(1).contains(x,y))return PatchTransferAction::CopyPatch;
    if(patchTransferRect(2).contains(x,y))return PatchTransferAction::PastePatch;
    return std::nullopt;
}

bool NativeEditor::pointerDown(EditorPage page,float x,float y){
    activePage_=page;changed_=false;
    activeHit_=page==EditorPage::Synth?hitSynth(x,y):hitFx(x,y);
    if(!activeHit_)return false;
    changed_=applyHit(*activeHit_,x,y)||changed_;
    return true;
}
bool NativeEditor::pointerMove(float x,float y){
    if(!activeHit_)return false;
    switch(activeHit_->kind){
        case HitKind::OperatorParam:
        case HitKind::Harmonic:
        case HitKind::Matrix:
        case HitKind::PatchParam:
        case HitKind::ModMin:
        case HitKind::ModMax:
            changed_=applyHit(*activeHit_,x,y)||changed_;return true;
        default:return true;
    }
}
bool NativeEditor::pointerUp(){
    const bool changed=changed_;activeHit_.reset();changed_=false;return changed;
}
void NativeEditor::cancel() noexcept {activeHit_.reset();changed_=false;}

} // namespace aiora
