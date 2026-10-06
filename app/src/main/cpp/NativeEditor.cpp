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
    const float gap=std::max(4.0f,height_*0.008f);
    const float h=std::clamp(e.h*0.085f,30.0f,42.0f);
    const float w=(e.w-gap*4.0f)/3.0f;
    return {e.x+gap+index*(w+gap),e.y+e.h-h,w,h};
}

NativeEditor::Rect NativeEditor::bodyRect() const noexcept {
    auto r=editorRect();
    const float gap=std::max(4.0f,height_*0.008f);
    const float transferH=patchTransferRect(0).h;
    r.h=std::max(0.0f,r.h-transferH-gap);
    return r;
}

NativeEditor::Rect NativeEditor::factoryPresetRect(int index) const noexcept {
    const auto b=bodyRect();
    const float gap=std::max(3.0f,width_*0.004f);
    const float h=std::clamp(b.h*0.075f,30.0f,40.0f);
    const float w=(b.w-gap*6.0f)/5.0f;
    return {b.x+gap+index*(w+gap),b.y,w,h};
}

NativeEditor::Rect NativeEditor::synthTabRect(int index) const noexcept {
    const auto b=bodyRect();const float gap=std::max(4.0f,width_*0.006f);
    const auto preset=factoryPresetRect(0);
    const float top=preset.y+preset.h+gap;
    const float h=std::clamp(b.h*0.085f,34.0f,48.0f);
    const float w=(b.w-gap*3.0f)*0.5f;
    return {b.x+gap+index*(w+gap),top,w,h};
}
NativeEditor::Rect NativeEditor::operatorSelectRect(int index) const noexcept {
    const auto b=bodyRect();const float gap=std::max(3.0f,width_*0.004f);
    const auto tabs=synthTabRect(0);const float top=tabs.y+tabs.h+gap;
    const float h=std::clamp(b.h*0.085f,32.0f,44.0f);
    const float w=(b.w-gap*7.0f)/6.0f;
    return {b.x+gap+index*(w+gap),top,w,h};
}
NativeEditor::Rect NativeEditor::waveRect(int index) const noexcept {
    const auto b=bodyRect();const float gap=std::max(3.0f,width_*0.004f);
    const auto op=operatorSelectRect(0);const float top=op.y+op.h+gap;
    const float h=std::clamp(b.h*0.075f,30.0f,40.0f);
    const float w=(b.w-gap*7.0f)/6.0f;
    return {b.x+gap+index*(w+gap),top,w,h};
}
NativeEditor::Rect NativeEditor::operatorToggleRect() const noexcept {
    const auto b=bodyRect();const float gap=std::max(3.0f,width_*0.004f);
    const auto w=waveRect(0);
    return {b.x+gap,w.y+w.h+gap,b.w-gap*2.0f,std::clamp(b.h*0.065f,28.0f,38.0f)};
}
NativeEditor::Rect NativeEditor::operatorSliderRect(int index,bool custom) const noexcept {
    const auto b=bodyRect();const float gap=std::max(3.0f,height_*0.005f);
    const auto toggle=operatorToggleRect();
    const float top=toggle.y+toggle.h+gap;
    const float harmonicReserve=custom?std::clamp(b.h*0.19f,72.0f,112.0f):0.0f;
    const float available=std::max(0.0f,b.y+b.h-top-harmonicReserve-gap*(custom?1.0f:0.0f));
    const float rowH=std::max(18.0f,(available-gap*6.0f)/7.0f);
    return {b.x+std::max(6.0f,b.w*0.025f),top+index*(rowH+gap),b.w-std::max(12.0f,b.w*0.05f),rowH};
}
NativeEditor::Rect NativeEditor::harmonicRect(int index) const noexcept {
    const auto b=bodyRect();const float gap=std::max(2.0f,width_*0.003f);
    const float reserve=std::clamp(b.h*0.19f,72.0f,112.0f);
    const float top=b.y+b.h-reserve;
    const int row=index/8,col=index%8;
    const float w=(b.w-gap*9.0f)/8.0f;
    const float h=(reserve-gap*3.0f)/2.0f;
    return {b.x+gap+col*(w+gap),top+gap+row*(h+gap),w,h};
}
NativeEditor::Rect NativeEditor::matrixRect(int modulator,int carrier) const noexcept {
    const auto b=bodyRect();const float gap=std::max(3.0f,std::min(width_,height_)*0.005f);
    const auto tabs=synthTabRect(0);
    const float top=tabs.y+tabs.h+gap;
    const float availH=std::max(0.0f,b.y+b.h-top-gap);
    const float cell=std::max(18.0f,std::min((b.w-gap*7.0f)/6.0f,(availH-gap*7.0f)/6.0f));
    const float gridW=cell*6.0f+gap*5.0f,gridH=cell*6.0f+gap*5.0f;
    const float ox=b.x+(b.w-gridW)*0.5f,oy=top+(availH-gridH)*0.5f;
    return {ox+carrier*(cell+gap),oy+modulator*(cell+gap),cell,cell};
}

NativeEditor::Rect NativeEditor::fxGroupRect(int index) const noexcept {
    const auto b=bodyRect();const float gap=std::max(3.0f,width_*0.004f);
    const float h=std::clamp(b.h*0.085f,34.0f,46.0f);
    const float w=(b.w-gap*5.0f)/4.0f;
    return {b.x+gap+index*(w+gap),b.y,w,h};
}
NativeEditor::Rect NativeEditor::filterTypeRect(int index) const noexcept {
    const auto b=bodyRect();const float gap=std::max(3.0f,width_*0.004f);
    const auto g=fxGroupRect(0);const float top=g.y+g.h+gap;
    const float h=std::clamp(b.h*0.07f,30.0f,40.0f);
    const float w=(b.w-gap*4.0f)/3.0f;
    return {b.x+gap+index*(w+gap),top,w,h};
}
NativeEditor::Rect NativeEditor::lfoTargetRect(int index) const noexcept {
    const auto b=bodyRect();const float gap=std::max(3.0f,width_*0.004f);
    const auto g=fxGroupRect(0);const float top=g.y+g.h+gap;
    const float h=std::clamp(b.h*0.07f,30.0f,40.0f);
    const float w=(b.w-gap*5.0f)/4.0f;
    return {b.x+gap+index*(w+gap),top,w,h};
}
NativeEditor::Rect NativeEditor::fxSliderRect(int row,int rowCount,bool hasChoiceRow) const noexcept {
    const auto b=bodyRect();const float gap=std::max(3.0f,height_*0.005f);
    const auto group=fxGroupRect(0);
    float top=group.y+group.h+gap;
    if(hasChoiceRow){
        const auto choice=fxGroup_==0?filterTypeRect(0):lfoTargetRect(0);
        top=choice.y+choice.h+gap;
    }
    const float available=std::max(0.0f,b.y+b.h-top-gap);
    const float rowH=std::max(18.0f,(available-gap*std::max(0,rowCount-1))/std::max(1,rowCount));
    const float side=std::max(6.0f,b.w*0.025f);
    return {b.x+side,top+row*(rowH+gap),b.w-side*2.0f,rowH};
}
NativeEditor::Rect NativeEditor::modRowRect(int slot) const noexcept {
    const auto b=bodyRect();const float gap=std::max(4.0f,height_*0.006f);
    const auto group=fxGroupRect(0);const float top=group.y+group.h+gap;
    const float addH=std::clamp(b.h*0.08f,32.0f,42.0f);
    const float avail=std::max(0.0f,b.y+b.h-top-addH-gap*2.0f);
    const float rowH=std::max(34.0f,(avail-gap*3.0f)/4.0f);
    return {b.x+gap,top+slot*(rowH+gap),b.w-gap*2.0f,rowH};
}
NativeEditor::Rect NativeEditor::modPartRect(int slot,int part) const noexcept {
    const auto r=modRowRect(slot);const float gap=std::max(2.0f,width_*0.003f);
    const std::array<float,6> weights{0.08f,0.14f,0.08f,0.29f,0.29f,0.08f};
    float total=0.0f;for(float w:weights)total+=w;
    const float usable=r.w-gap*5.0f;float x=r.x;
    for(int i=0;i<part;++i)x+=usable*(weights[static_cast<size_t>(i)]/total)+gap;
    const float w=usable*(weights[static_cast<size_t>(part)]/total);
    return {x,r.y,w,r.h};
}
NativeEditor::Rect NativeEditor::modAddRect() const noexcept {
    const auto b=bodyRect();const float gap=std::max(4.0f,height_*0.006f);
    const float h=std::clamp(b.h*0.08f,32.0f,42.0f);
    return {b.x+gap,b.y+b.h-h,b.w-gap*2.0f,h};
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
    const auto ai=patchTransferRect(0);
    const auto copy=patchTransferRect(1);
    const auto paste=patchTransferRect(2);
    drawButton(ai,false,kGreen);
    drawButton(copy,false,kCyan);
    drawButton(paste,false,kPurple);
    auto& ov=NativeOverlay::instance();
    ov.addTextCentered("AI FROM CLIP",{ai.x,ai.y,ai.w,ai.h},0.80f,overlayColor(kWhite));
    ov.addTextCentered("COPY PATCH",{copy.x,copy.y,copy.w,copy.h},0.80f,overlayColor(kWhite));
    ov.addTextCentered("PASTE PATCH",{paste.x,paste.y,paste.w,paste.h},0.80f,overlayColor(kWhite));
}

void NativeEditor::renderSynth() const noexcept {
    drawPadReservedBackground();
    drawPatchTransfer();
    const Patch p=ProjectCore::instance().selectedPatch();

    static constexpr const char* kPresets[5]={"INIT","SUBULA","SPECTRELLO","NEBULAR","NEXDRUM"};
    for(int i=0;i<5;++i){
        const auto rr=factoryPresetRect(i);
        const char* fullName=
            i==0?"Spectrachord Init":
            i==1?"Subula":
            i==2?"Spectrello":
            i==3?"Nebular":"Nexdrum";
        const bool active=p.name==fullName;
        drawButton(rr,active,i==4?kOrange:kCyan);
        NativeOverlay::instance().addTextCentered(
            kPresets[i],{rr.x,rr.y,rr.w,rr.h},
            std::max(0.50f,std::min(0.82f,rr.w/(std::char_traits<char>::length(kPresets[i])*6.3f))),
            overlayColor(active?kBg:kWhite));
    }
    static constexpr const char* kTabs[2]={"OPERATORS","FM MATRIX"};
    for(int i=0;i<2;++i){
        const auto tr=synthTabRect(i);
        drawButton(tr,(i==1)==matrixMode_,i? kPurple:kCyan);
        NativeOverlay::instance().addTextCentered(
            kTabs[i],{tr.x,tr.y,tr.w,tr.h},1.15f,
            overlayColor((i==1)==matrixMode_?kBg:kWhite));
    }

    if(matrixMode_){
        for(int m=0;m<6;++m)for(int c=0;c<6;++c){
            const auto r=matrixRect(m,c);const float v=std::clamp(p.matrix[static_cast<size_t>(m)][static_cast<size_t>(c)],0.0f,1.0f);
            fillRect(r,mix(kButton,kPurple,0.18f+v*0.72f));
            const float inset=std::max(2.0f,r.w*0.08f);
            fillRect({r.x+inset,r.y+r.h-inset-std::max(3.0f,r.h*v),r.w-inset*2.0f,std::max(3.0f,r.h*v)},mix(kCyan,kPurple,0.55f));
            const std::string label="M"+std::to_string(m+1)+"C"+std::to_string(c+1);
            NativeOverlay::instance().addTextCentered(label,{r.x,r.y,r.w,r.h},std::max(0.62f,r.w/34.0f),overlayColor(kWhite));
        }
        return;
    }

    const int op=std::clamp(selectedOperator_,0,5);
    for(int i=0;i<6;++i){
        const auto rr=operatorSelectRect(i);drawButton(rr,i==op,kCyan);
        NativeOverlay::instance().addTextCentered(
            "OP"+std::to_string(i+1),{rr.x,rr.y,rr.w,rr.h},1.0f,overlayColor(i==op?kBg:kWhite));
    }
    static constexpr const char* kWaves[6]={"SINE","SAW","SQUARE","TRI","CUSTOM","NOISE"};
    for(int i=0;i<6;++i){
        const auto rr=waveRect(i);const bool active=static_cast<int>(p.ops[static_cast<size_t>(op)].wave)==i;
        drawButton(rr,active,kPurple);
        NativeOverlay::instance().addTextCentered(kWaves[i],{rr.x,rr.y,rr.w,rr.h},std::max(0.62f,std::min(0.95f,rr.w/(std::char_traits<char>::length(kWaves[i])*6.5f))),overlayColor(active?kBg:kWhite));
    }
    const auto toggle=operatorToggleRect();
    drawButton(toggle,p.ops[static_cast<size_t>(op)].enabled,p.ops[static_cast<size_t>(op)].enabled?kGreen:kRed);
    NativeOverlay::instance().addTextCentered(
        p.ops[static_cast<size_t>(op)].enabled?"ON":"OFF",{toggle.x,toggle.y,toggle.w,toggle.h},1.2f,overlayColor(kWhite));

    const bool custom=p.ops[static_cast<size_t>(op)].wave==Wave::Custom;
    static constexpr const char* kOpLabels[7]={"RATIO","FINE","LEVEL","A","D","S","R"};
    for(int i=0;i<7;++i){
        const auto param=kOperatorParams[static_cast<size_t>(i)];
        const auto rr=operatorSliderRect(i,custom);
        drawSlider(rr,normalized(operatorValue(p,op,param),operatorRange(param)),i<3?kCyan:kOrange);
        NativeOverlay::instance().addText(kOpLabels[i],rr.x+5.0f,rr.y+rr.h*0.32f,std::max(0.68f,rr.h/34.0f),overlayColor(kWhite));
    }
    if(custom){
        for(int i=0;i<16;++i){
            const auto rr=harmonicRect(i);const float v=std::clamp(p.ops[static_cast<size_t>(op)].harm[static_cast<size_t>(i)],0.0f,1.0f);
            fillRect(rr,kButton);
            fillRect({rr.x+rr.w*0.22f,rr.y+rr.h*(1.0f-v),rr.w*0.56f,rr.h*v},mix(kCyan,kPurple,0.42f));
            NativeOverlay::instance().addText(
                std::to_string(i+1),rr.x+2.0f,rr.y+2.0f,std::max(0.55f,rr.w/30.0f),overlayColor(kWhite));
        }
    }
}

void NativeEditor::renderFx() const noexcept {
    drawPadReservedBackground();
    drawPatchTransfer();
    const Patch p=ProjectCore::instance().selectedPatch();
    const std::array<Rgb,4> groupColors{kCyan,kOrange,kPurple,kGreen};
    static constexpr const char* kGroups[4]={"FILTER","AMP","LFO/FX","MOD"};
    for(int i=0;i<4;++i){
        const auto rr=fxGroupRect(i);drawButton(rr,fxGroup_==i,groupColors[static_cast<size_t>(i)]);
        NativeOverlay::instance().addTextCentered(
            kGroups[i],{rr.x,rr.y,rr.w,rr.h},1.0f,overlayColor(fxGroup_==i?kBg:kWhite));
    }

    if(fxGroup_==0){
        static constexpr const char* kTypes[3]={"LOW","HIGH","BAND"};
        for(int i=0;i<3;++i){
            const auto rr=filterTypeRect(i);const bool active=static_cast<int>(p.filter.type)==i;drawButton(rr,active,kCyan);
            NativeOverlay::instance().addTextCentered(kTypes[i],{rr.x,rr.y,rr.w,rr.h},1.0f,overlayColor(active?kBg:kWhite));
        }
        static constexpr const char* kLabels[7]={"CUTOFF","RESO","ENV","A","D","S","R"};
        for(int i=0;i<static_cast<int>(kFilterParams.size());++i){
            const auto param=kFilterParams[static_cast<size_t>(i)];const auto rr=fxSliderRect(i,static_cast<int>(kFilterParams.size()),true);
            drawSlider(rr,normalized(patchValue(p,param),patchRange(param)),i<3?kCyan:kOrange);
            NativeOverlay::instance().addText(kLabels[i],rr.x+5.0f,rr.y+rr.h*0.32f,std::max(0.68f,rr.h/34.0f),overlayColor(kWhite));
        }
    }else if(fxGroup_==1){
        static constexpr const char* kLabels[9]={"A","D","S","R","VOLUME","VEL AMP","VEL FLT","UNISON","GLIDE"};
        for(int i=0;i<static_cast<int>(kAmpParams.size());++i){
            const auto param=kAmpParams[static_cast<size_t>(i)];const auto rr=fxSliderRect(i,static_cast<int>(kAmpParams.size()),false);
            drawSlider(rr,normalized(patchValue(p,param),patchRange(param)),i<5?kOrange:kCyan);
            NativeOverlay::instance().addText(kLabels[i],rr.x+5.0f,rr.y+rr.h*0.32f,std::max(0.64f,rr.h/35.0f),overlayColor(kWhite));
        }
    }else if(fxGroup_==2){
        static constexpr const char* kTargets[4]={"NONE","PITCH","FILTER","AMP"};
        for(int i=0;i<4;++i){
            const auto rr=lfoTargetRect(i);const bool active=static_cast<int>(p.lfo.target)==i;drawButton(rr,active,kPurple);
            NativeOverlay::instance().addTextCentered(kTargets[i],{rr.x,rr.y,rr.w,rr.h},0.9f,overlayColor(active?kBg:kWhite));
        }
        static constexpr const char* kLabels[9]={"RATE","AMOUNT","ATTACK","VEL","DISTORT","DELAY","DLY TIME","DLY FB","REVERB"};
        for(int i=0;i<static_cast<int>(kLfoFxParams.size());++i){
            const auto param=kLfoFxParams[static_cast<size_t>(i)];const auto rr=fxSliderRect(i,static_cast<int>(kLfoFxParams.size()),true);
            drawSlider(rr,normalized(patchValue(p,param),patchRange(param)),i<4?kPurple:kGreen);
            NativeOverlay::instance().addText(kLabels[i],rr.x+5.0f,rr.y+rr.h*0.32f,std::max(0.62f,rr.h/36.0f),overlayColor(kWhite));
        }
    }else{
        auto& project=ProjectCore::instance();
        const int count=project.selectedModSlotCount();
        for(int slot=0;slot<4;++slot){
            const auto row=modRowRect(slot);
            if(slot>=count){fillRect(row,mix(kPanel,kButton,0.25f));continue;}
            const auto s=project.selectedModSlot(slot);
            const auto prev=modPartRect(slot,0),target=modPartRect(slot,1),next=modPartRect(slot,2),mn=modPartRect(slot,3),mx=modPartRect(slot,4),del=modPartRect(slot,5);
            drawButton(prev,false,kCyan);
            fillRect(target,mix(kButton,kPurple,normalized(static_cast<float>(static_cast<int>(s.target)),{0.0f,static_cast<float>(static_cast<int>(ModTarget::Volume))})));
            drawButton(next,false,kCyan);
            drawSlider(mn,normalized(s.min,modRange(s.target)),kCyan);
            drawSlider(mx,normalized(s.max,modRange(s.target)),kPurple);
            drawButton(del,false,kRed);
            auto& ov=NativeOverlay::instance();
            ov.addTextCentered("-",{prev.x,prev.y,prev.w,prev.h},1.0f,overlayColor(kWhite));
            ov.addTextCentered(modTargetName(s.target),{target.x,target.y,target.w,target.h},std::max(0.54f,std::min(0.82f,target.w/(std::char_traits<char>::length(modTargetName(s.target))*6.5f))),overlayColor(kWhite));
            ov.addTextCentered("+",{next.x,next.y,next.w,next.h},1.0f,overlayColor(kWhite));
            ov.addText("MIN",mn.x+4.0f,mn.y+mn.h*0.34f,std::max(0.55f,mn.h/38.0f),overlayColor(kWhite));
            ov.addText("MAX",mx.x+4.0f,mx.y+mx.h*0.34f,std::max(0.55f,mx.h/38.0f),overlayColor(kWhite));
            ov.addTextCentered("X",{del.x,del.y,del.w,del.h},1.0f,overlayColor(kWhite));
        }
        const auto add=modAddRect();drawButton(add,count<4,kGreen);
        NativeOverlay::instance().addTextCentered("+ LINK",{add.x,add.y,add.w,add.h},1.0f,overlayColor(count<4?kWhite:kButton));
    }
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
