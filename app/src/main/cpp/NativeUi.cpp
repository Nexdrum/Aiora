#include "NativeUi.h"
#include "NativeEditor.h"
#include "NativeOverlay.h"

#include <GLES3/gl3.h>

#include <algorithm>
#include <array>
#include <string>
#include <cmath>

#include "ProjectCore.h"
#include "AudioEngine.h"

namespace aiora {
namespace {

constexpr NativeUi::Rgb kBg{0.0627f, 0.0706f, 0.0863f};
constexpr NativeUi::Rgb kTop{0.0784f, 0.0902f, 0.1137f};
constexpr NativeUi::Rgb kPanel{0.0863f, 0.1020f, 0.1294f};
constexpr NativeUi::Rgb kRollBg{0.0471f, 0.0549f, 0.0706f};
constexpr NativeUi::Rgb kCell{0.1020f, 0.1216f, 0.1608f};
constexpr NativeUi::Rgb kBeat{0.1255f, 0.1490f, 0.2039f};
constexpr NativeUi::Rgb kBar{0.1412f, 0.1725f, 0.2275f};
constexpr NativeUi::Rgb kButton{0.1373f, 0.1569f, 0.2000f};
constexpr NativeUi::Rgb kCyan{0.0f, 0.80f, 0.80f};
constexpr NativeUi::Rgb kOrange{1.0f, 0.6667f, 0.0f};
constexpr NativeUi::Rgb kGreen{0.18f, 0.82f, 0.42f};
constexpr NativeUi::Rgb kRed{0.90f, 0.18f, 0.20f};
constexpr NativeUi::Rgb kWhite{0.91f, 0.925f, 0.945f};
constexpr NativeUi::Rgb kPurple{0.788f, 0.557f, 1.0f};
constexpr NativeUi::Rgb kMuted{0.35f, 0.39f, 0.47f};

constexpr std::array<NativeUi::Rgb, 12> kPitchColors{{
    {0.2275f, 1.0000f, 0.0000f},
    {0.0000f, 1.0000f, 0.9255f},
    {0.0000f, 0.5608f, 1.0000f},
    {0.0588f, 0.0000f, 0.9843f},
    {0.3882f, 0.0000f, 0.7451f},
    {0.4314f, 0.0000f, 0.5020f},
    {0.5961f, 0.0000f, 0.0000f},
    {0.7843f, 0.0000f, 0.0000f},
    {0.9529f, 0.0000f, 0.0000f},
    {1.0000f, 0.4706f, 0.0000f},
    {1.0000f, 0.9373f, 0.0000f},
    {0.6667f, 1.0000f, 0.0000f},
}};

NativeUi::Rgb mix(NativeUi::Rgb a, NativeUi::Rgb b, float amount) noexcept {
    amount = std::clamp(amount, 0.0f, 1.0f);
    return {
        a.r + (b.r - a.r) * amount,
        a.g + (b.g - a.g) * amount,
        a.b + (b.b - a.b) * amount
    };
}

NativeOverlay::Color overlayColor(NativeUi::Rgb c,float alpha=1.0f) noexcept {
    return {c.r,c.g,c.b,alpha};
}

bool pageHasPadQuick(NativePage page) noexcept {
    return page == NativePage::Drums ||
           page == NativePage::Synth ||
           page == NativePage::Fx;
}

} // namespace

void NativeUi::resize(int width, int height) noexcept {
    width_ = std::max(0, width);
    height_ = std::max(0, height);
    NativeEditor::instance().resize(width_, height_);
}

void NativeUi::setPitchActive(int midi, bool active) noexcept {
    if (midi < kGridLow || midi > kGridHigh) return;
    active_[static_cast<size_t>(midi - kGridLow)] = active;
}

void NativeUi::clearPitchActivity() noexcept {
    active_.fill(false);
}

NativeUi::Rect NativeUi::headerScopeRect() const noexcept {
    const float margin=std::max(5.0f,width_*0.010f);
    const float headerH=std::max(38.0f,height_*0.105f);
    const float brand=std::clamp(width_*0.26f,96.0f,230.0f);
    const float left=margin+100.0f;
    const float right=margin+brand-5.0f;
    if(right-left<56.0f)return {};
    return {left,6.0f,right-left,std::max(24.0f,headerH-12.0f)};
}

NativeUi::Rect NativeUi::headerControlRect(int index) const noexcept {
    const float margin=std::max(5.0f,width_*0.010f);
    const float gap=std::max(3.0f,width_*0.004f);
    const float headerH=std::max(38.0f,height_*0.105f);
    const float brand=std::clamp(width_*0.26f,96.0f,230.0f);
    const float available=std::max(120.0f,static_cast<float>(width_)-margin*2.0f-brand-gap*4.0f);
    static constexpr float ratios[5]={0.30f,0.16f,0.16f,0.14f,0.24f};
    float x=margin+brand;
    for(int i=0;i<index;++i)x+=available*ratios[i]+gap;
    return {x,4.0f,available*ratios[index],std::max(30.0f,headerH-8.0f)};
}

NativeUi::Rect NativeUi::navRect(int index) const noexcept {
    const float margin = std::max(4.0f, width_ * 0.008f);
    const float headerH = std::max(38.0f, height_ * 0.105f);
    const float navH = std::max(38.0f, height_ * 0.105f);
    const float gap = std::max(3.0f, width_ * 0.004f);
    const float available = std::max(
        0.0f, static_cast<float>(width_) - margin * 2.0f - gap * 5.0f);
    const float buttonW = available / 6.0f;

    return {
        margin + index * (buttonW + gap),
        headerH + gap,
        buttonW,
        navH - gap
    };
}

NativeUi::Rect NativeUi::contentRect() const noexcept {
    const float margin = std::max(4.0f, width_ * 0.008f);
    const auto nav = navRect(0);
    const float gap = std::max(4.0f, height_ * 0.010f);
    const float top = nav.y + nav.h + gap;
    return {
        margin,
        top,
        std::max(0.0f, static_cast<float>(width_) - margin * 2.0f),
        std::max(0.0f, static_cast<float>(height_) - top - margin)
    };
}

NativeUi::Rect NativeUi::gridAreaRect() const noexcept {
    auto area = contentRect();
    if (page_ == NativePage::Drums) {
        const float gap = std::max(4.0f, height_ * 0.010f);
        const float padH = std::clamp(area.h * 0.14f, 34.0f, 62.0f);
        area.y += padH + gap;
        area.h = std::max(0.0f, area.h - padH - gap);
        const auto editor = drumEditorRect();
        if(area.w >= area.h * 1.15f){
            area.w = std::max(0.0f, editor.x - gap - area.x);
        }else{
            area.h = std::max(0.0f, editor.y - gap - area.y);
        }
    }
    return area;
}

NativeUi::Rect NativeUi::gridRect(int visualRow, int column) const noexcept {
    const auto area = gridAreaRect();
    const float gap = std::max(2.0f, std::min(width_, height_) * 0.006f);
    const float usableW = std::max(0.0f, area.w - gap * 6.0f);
    const float usableH = std::max(0.0f, area.h - gap * 6.0f);
    const float cell = std::max(1.0f, std::min(usableW / 7.0f, usableH / 7.0f));
    const float gridW = cell * 7.0f + gap * 6.0f;
    const float gridH = cell * 7.0f + gap * 6.0f;
    const float originX = area.x + (area.w - gridW) * 0.5f;
    const float originY = area.y + (area.h - gridH) * 0.5f;

    return {
        originX + column * (cell + gap),
        originY + visualRow * (cell + gap),
        cell,
        cell
    };
}

NativeUi::Rect NativeUi::projectTransferRect(int index) const noexcept {
    const auto content=contentRect();
    const float gap=std::max(4.0f,height_*0.008f);
    const float h=std::clamp(content.h*0.08f,28.0f,38.0f);
    const float w=(content.w-gap*3.0f)*0.5f;
    return {content.x+gap+index*(w+gap),content.y+gap,w,h};
}

NativeUi::Rect NativeUi::addTrackRect(TrackAddKind kind) const noexcept {
    const auto content=contentRect();
    const float gap=std::max(5.0f,width_*0.008f);
    const float h=std::clamp(content.h*0.12f,34.0f,52.0f);
    const float w=(content.w-gap*3.0f)*0.5f;
    const int side=kind==TrackAddKind::Drums?1:0;
    return {content.x+gap+side*(w+gap),content.y+content.h-h-gap,w,h};
}

NativeUi::Rect NativeUi::masterSliderRect(int index) const noexcept {
    const auto content=contentRect();const float gap=std::max(4.0f,height_*0.008f);
    const auto add=addTrackRect(TrackAddKind::Melodic);
    const float h=std::clamp(content.h*0.085f,28.0f,38.0f);
    const float w=(content.w-gap*3.0f)*0.5f;
    return {content.x+gap+index*(w+gap),add.y-h-gap,w,h};
}

NativeUi::Rect NativeUi::trackRect(int index,int count) const noexcept {
    const auto content=contentRect();
    const float gap=std::max(3.0f,height_*0.007f);
    const auto master=masterSliderRect(0);
    const auto transfer=projectTransferRect(0);
    const float top=transfer.y+transfer.h+gap;
    const float bottom=master.y-gap;
    const float usable=std::max(0.0f,bottom-top);
    const int rows=std::max(1,count);
    const float rowH=std::min(60.0f,std::max(20.0f,(usable-gap*(rows-1))/rows));
    return {content.x+gap,top+index*(rowH+gap),std::max(0.0f,content.w-gap*2.0f),rowH};
}

NativeUi::Rect NativeUi::trackPartRect(int index,int count,int part) const noexcept {
    const auto r=trackRect(index,count);const float gap=std::max(2.0f,r.h*0.08f);
    const float controlsX=r.x+r.w*0.38f;
    const float button=std::clamp(r.h*0.72f,14.0f,30.0f);
    const float controlsW=std::max(0.0f,r.x+r.w-controlsX-gap);
    const float sliderW=std::max(28.0f,(controlsW-button*3.0f-gap*4.0f)*0.5f);
    if(part==0)return {controlsX,r.y+(r.h-button)*0.5f,sliderW,button};
    if(part==1)return {controlsX+sliderW+gap,r.y+(r.h-button)*0.5f,sliderW,button};
    const float bx=controlsX+sliderW*2.0f+gap*2.0f+(part-2)*(button+gap);
    return {bx,r.y+(r.h-button)*0.5f,button,button};
}

NativeUi::Rect NativeUi::padQuickRect(int index, int count) const noexcept {
    const auto content = contentRect();
    const int cells = std::max(1, count);
    const float gap = std::max(3.0f, width_ * 0.004f);
    const float h = std::clamp(content.h * 0.14f, 34.0f, 62.0f);
    const float available = std::max(0.0f, content.w - gap * (cells + 1));
    const float w = available / cells;
    return {
        content.x + gap + index * (w + gap),
        content.y,
        w,
        h
    };
}

NativeUi::Rect NativeUi::drumEditorRect() const noexcept {
    auto area=contentRect();
    const float gap=std::max(4.0f,height_*0.010f);
    const float padH=std::clamp(area.h*0.14f,34.0f,62.0f);
    area.y+=padH+gap;
    area.h=std::max(0.0f,area.h-padH-gap);
    if(area.w>=area.h*1.15f){
        const float w=std::clamp(area.w*0.38f,180.0f,320.0f);
        return {area.x+area.w-w,area.y,w,area.h};
    }
    const float h=std::clamp(area.h*0.40f,132.0f,220.0f);
    return {area.x,area.y+area.h-h,area.w,h};
}

NativeUi::Rect NativeUi::drumActionRect(int index) const noexcept {
    const auto e=drumEditorRect();const float gap=std::max(3.0f,width_*0.004f);
    const float h=std::clamp(e.h*0.15f,28.0f,42.0f);
    const float w=(e.w-gap*4.0f)/3.0f;
    return {e.x+gap+index*(w+gap),e.y+gap,w,h};
}

NativeUi::Rect NativeUi::drumSliderRect(int index) const noexcept {
    const auto e=drumEditorRect();const float gap=std::max(3.0f,height_*0.006f);
    const auto a=drumActionRect(0);
    const float top=a.y+a.h+gap;
    const float h=std::clamp(e.h*0.13f,24.0f,36.0f);
    return {e.x+gap,top+index*(h+gap),e.w-gap*2.0f,h};
}

NativeUi::Rect NativeUi::drumIconRect(int index) const noexcept {
    const auto e=drumEditorRect();const float gap=std::max(2.0f,std::min(width_,height_)*0.004f);
    const auto s=drumSliderRect(1);
    const float top=s.y+s.h+gap;
    const float availableH=std::max(0.0f,e.y+e.h-top-gap);
    const int row=index/4,col=index%4;
    const float w=(e.w-gap*5.0f)/4.0f;
    const float h=(availableH-gap*2.0f)/3.0f;
    return {e.x+gap+col*(w+gap),top+row*(h+gap),w,h};
}

NativeUi::Rect NativeUi::rollModeRect(int index) const noexcept {
    const auto content = contentRect();
    const float gap = std::max(3.0f, width_ * 0.004f);
    const float h = std::clamp(content.h * 0.11f, 34.0f, 48.0f);
    const float available = std::max(0.0f, content.w - gap * 5.0f);
    const float w = available / 4.0f;
    return {
        content.x + gap + index * (w + gap),
        content.y,
        w,
        h
    };
}

NativeUi::Rect NativeUi::rollViewportRect() const noexcept {
    auto content = contentRect();
    const float gap = std::max(4.0f, height_ * 0.010f);
    const float modeH = rollModeRect(0).h;
    content.y += modeH + gap;
    content.h = std::max(0.0f, content.h - modeH - gap);
    return content;
}

float NativeUi::rollCellPixels() const noexcept {
    const auto viewport = rollViewportRect();
    const float gutter = rollGutterPixels();
    const float header = rollHeaderPixels();
    const float byWidth = std::max(1.0f, (viewport.w - gutter) / 9.0f);
    const float byHeight = std::max(1.0f, (viewport.h - header) / 10.0f);
    return std::clamp(std::min(byWidth, byHeight), 22.0f, 36.0f);
}

float NativeUi::rollGutterPixels() const noexcept {
    return std::clamp(width_ * 0.075f, 34.0f, 52.0f);
}

float NativeUi::rollHeaderPixels() const noexcept {
    return std::clamp(height_ * 0.075f, 32.0f, 48.0f);
}

std::vector<int> NativeUi::rollColumns() const {
    auto& project = ProjectCore::instance();
    const int track = project.selectedTrack();
    std::vector<int> columns;

    if (track >= 0 && project.trackIsDrums(track)) {
        const int pads = project.padCount(track);
        for (int p = 0; p < pads; ++p) {
            int lo = project.padLow(track, p);
            int hi = project.padHigh(track, p);
            if (lo > hi) std::swap(lo, hi);
            if (lo < 0 || hi < 0) continue;
            for (int midi = lo; midi <= hi; ++midi) columns.push_back(midi);
        }
        std::sort(columns.begin(), columns.end());
        columns.erase(std::unique(columns.begin(), columns.end()), columns.end());
    }

    if (columns.empty()) {
        columns.reserve(97);
        for (int midi = 14; midi <= 110; ++midi) columns.push_back(midi);
    }
    return columns;
}

int NativeUi::rollTotalRows() const noexcept {
    return std::max(64, ProjectCore::instance().playLengthSteps() + 32);
}

void NativeUi::scrollRoll(int pitchDelta, int stepDelta) noexcept {
    const auto columns = rollColumns();
    const auto viewport = rollViewportRect();
    const float cell = rollCellPixels();
    const int visibleCols = std::max(
        1, static_cast<int>(std::floor((viewport.w - rollGutterPixels()) / cell)));
    const int visibleRows = std::max(
        1, static_cast<int>(std::floor((viewport.h - rollHeaderPixels()) / cell)));

    const int maxPitch = std::max(0, static_cast<int>(columns.size()) - visibleCols);
    const int maxStep = std::max(0, rollTotalRows() - visibleRows);
    rollPitchOffset_ = std::clamp(rollPitchOffset_ + pitchDelta, 0, maxPitch);
    rollStepOffset_ = std::clamp(rollStepOffset_ + stepDelta, 0, maxStep);
}

NativeUi::Rgb NativeUi::pitchColor(int midi) const noexcept {
    const int pc = ((midi % 12) + 12) % 12;
    return kPitchColors[static_cast<size_t>(pc)];
}

void NativeUi::fillRect(const Rect& rect, Rgb color) const noexcept {
    if (rect.w <= 0.0f || rect.h <= 0.0f || width_ <= 0 || height_ <= 0) return;

    const int x = std::max(0, static_cast<int>(rect.x));
    const int top = std::max(0, static_cast<int>(rect.y));
    const int w = std::max(0, std::min(width_ - x, static_cast<int>(rect.w)));
    const int h = std::max(0, std::min(height_ - top, static_cast<int>(rect.h)));
    if (w <= 0 || h <= 0) return;

    const int glY = height_ - top - h;
    glScissor(x, glY, w, h);
    glClearColor(color.r, color.g, color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void NativeUi::drawScope() const noexcept {
    const auto r=headerScopeRect();
    if(r.w<56.0f||r.h<20.0f)return;

    std::array<float,AudioEngine::kScopeReadSamples> samples{};
    AudioEngine::instance().copyScope(samples);

    fillRect(r,mix(kTop,kBg,0.38f));
    fillRect({r.x,r.y+r.h*0.5f,r.w,1.0f},mix(kMuted,kTop,0.35f));

    size_t trigger=0;
    const size_t searchEnd=samples.size()/2;
    for(size_t i=1;i<searchEnd;++i){
        if(samples[i-1]<=0.0f&&samples[i]>0.0f){
            trigger=i;
            break;
        }
    }

    const int points=std::clamp(static_cast<int>(r.w/2.0f),24,96);
    float prevY=r.y+r.h*0.5f;
    for(int i=0;i<points;++i){
        const float t=points>1?static_cast<float>(i)/static_cast<float>(points-1):0.0f;
        const size_t available=samples.size()-trigger;
        const size_t idx=trigger+std::min(
            available-1,
            static_cast<size_t>(t*static_cast<float>(available-1)));
        const float sample=std::clamp(samples[idx],-1.0f,1.0f);
        const float y=r.y+r.h*0.5f-sample*(r.h*0.43f);
        const float x=r.x+t*r.w;

        if(i>0){
            const float top=std::min(prevY,y);
            const float bottom=std::max(prevY,y);
            const auto color=pitchColor((i*12)/std::max(1,points));
            fillRect({x-2.0f,top,3.0f,std::max(2.0f,bottom-top)},mix(kTop,color,0.46f));
            fillRect({x-0.75f,top,1.5f,std::max(1.5f,bottom-top)},color);
        }
        prevY=y;
    }
}

void NativeUi::drawGrid() const noexcept {
    int selectedLow=-1,selectedHigh=-1,selectedPad=-1,selectedCenter=-1,track=-1;
    bool rangeMode=false;
    auto& project=ProjectCore::instance();

    if(page_==NativePage::Drums){
        track=project.selectedTrack();
        if(track>=0&&project.trackIsDrums(track)){
            selectedPad=project.selectedPad(track);
            if(selectedPad>=0&&selectedPad<project.padCount(track)){
                selectedLow=project.padLow(track,selectedPad);
                selectedHigh=project.padHigh(track,selectedPad);
                selectedCenter=project.padCenter(track,selectedPad);
                if(selectedLow>selectedHigh)std::swap(selectedLow,selectedHigh);
                rangeMode=drumRangeMode();
            }
        }
    }

    for(int visualRow=0;visualRow<7;++visualRow){
        const int block=6-visualRow;
        for(int column=0;column<7;++column){
            const int midi=kGridLow+block*7+column;
            const bool active=active_[static_cast<size_t>(midi-kGridLow)];
            const bool selected=selectedLow>=0&&midi>=selectedLow&&midi<=selectedHigh;
            bool unavailable=false;

            if(track>=0&&selectedPad>=0){
                int testLo=midi,testHi=midi;
                if(rangeMode&&selectedCenter>=0){testLo=std::min(selectedCenter,midi);testHi=std::max(selectedCenter,midi);}
                for(int p=0;p<project.padCount(track);++p){
                    if(p==selectedPad)continue;
                    const int lo=std::min(project.padLow(track,p),project.padHigh(track,p));
                    const int hi=std::max(project.padLow(track,p),project.padHigh(track,p));
                    if(!(testHi<lo||testLo>hi)){unavailable=true;break;}
                }
            }

            const auto color=pitchColor(midi);
            const auto rect=gridRect(visualRow,column);
            const auto borderColor=unavailable?kMuted:(selected?mix(color,kOrange,0.58f):color);
            fillRect(rect,mix(kBg,borderColor,active?0.98f:(selected?0.86f:0.62f)));

            const float border=std::max(2.0f,rect.w*(selected?0.075f:0.055f));
            Rect inner{
                rect.x+border,rect.y+border,
                std::max(0.0f,rect.w-border*2.0f),
                std::max(0.0f,rect.h-border*2.0f)
            };
            fillRect(inner,unavailable?mix(kPanel,kMuted,0.28f):mix(kPanel,color,active?0.52f:(selected?0.30f:0.16f)));

            auto& overlay=NativeOverlay::instance();
            const float glyphInset=rect.w*0.19f;
            overlay.addPitchGlyph(
                midi%12,
                {rect.x+glyphInset,rect.y+glyphInset,rect.w-glyphInset*2.0f,rect.h-glyphInset*2.0f},
                overlayColor(unavailable?kMuted:(active?mix(color,kWhite,0.30f):color)));
            if(unavailable)overlay.addTextCentered("X",{rect.x,rect.y,rect.w,rect.h},std::max(0.75f,rect.w/26.0f),overlayColor(kRed,0.8f));
            if(((midi%12)+12)%12==2){
                const int octave=(midi-62)/12;
                const std::string label=octave>0?("+"+std::to_string(octave)):std::to_string(octave);
                overlay.addText(label,rect.x+rect.w*0.63f,rect.y+rect.h*0.72f,std::max(0.85f,rect.w/34.0f),overlayColor(kMuted));
            }
        }
    }
}

void NativeUi::drawTracks() const noexcept {
    auto& project=ProjectCore::instance();
    const auto content=contentRect();fillRect(content,kPanel);
    auto& overlay=NativeOverlay::instance();

    const auto copyR=projectTransferRect(0),pasteR=projectTransferRect(1);
    fillRect(copyR,mix(kButton,kCyan,0.30f));fillRect(pasteR,mix(kButton,kPurple,0.30f));
    overlay.addTextCentered("COPY SONG",{copyR.x,copyR.y,copyR.w,copyR.h},0.92f,overlayColor(kWhite));
    overlay.addTextCentered("PASTE SONG",{pasteR.x,pasteR.y,pasteR.w,pasteR.h},0.92f,overlayColor(kWhite));

    const int count=project.trackCount();
    const int selected=project.selectedTrack();
    for(int i=0;i<count;++i){
        const auto rect=trackRect(i,count);
        const bool drum=project.trackIsDrums(i),isSelected=i==selected;
        fillRect(rect,isSelected?kCyan:(drum?mix(kButton,kOrange,0.35f):kButton));
        const float inset=std::max(2.0f,rect.h*0.055f);
        fillRect({rect.x+inset,rect.y+inset,std::max(0.0f,rect.w-inset*2.0f),std::max(0.0f,rect.h-inset*2.0f)},isSelected?mix(kPanel,kCyan,0.12f):kPanel);

        std::string name=project.trackName(i);
        if(name.size()>18)name=name.substr(0,18);
        overlay.addText(
            name,rect.x+inset*2.0f,
            rect.y+(rect.h-7.0f*std::max(0.72f,rect.h/34.0f))*0.5f,
            std::max(0.72f,rect.h/34.0f),overlayColor(isSelected?kWhite:(drum?kOrange:kCyan)));

        const float values[2]={project.trackVolume(i),(project.trackPan(i)+1.0f)*0.5f};
        const Rgb sliderColors[2]={kOrange,kCyan};
        const char* sliderLabels[2]={"V","P"};
        for(int p=0;p<2;++p){
            const auto sr=trackPartRect(i,count,p);fillRect(sr,kButton);
            const float labelW=std::min(16.0f,sr.w*0.18f);
            const Rect bar{sr.x+labelW+3.0f,sr.y+sr.h*0.40f,std::max(2.0f,sr.w-labelW-7.0f),std::max(3.0f,sr.h*0.20f)};
            fillRect(bar,kMuted);fillRect({bar.x,bar.y,bar.w*std::clamp(values[p],0.0f,1.0f),bar.h},sliderColors[p]);
            const float knob=std::max(4.0f,sr.h*0.38f);
            fillRect({bar.x+bar.w*values[p]-knob*0.5f,sr.y+(sr.h-knob)*0.5f,knob,knob},kWhite);
            overlay.addText(sliderLabels[p],sr.x+2.0f,sr.y+sr.h*0.31f,std::max(0.55f,sr.h/36.0f),overlayColor(kWhite));
        }

        const bool flags[3]={project.trackMute(i),project.trackSolo(i),true};
        const Rgb accents[3]={kMuted,kOrange,kRed};
        const char* labels[3]={"M","S","X"};
        for(int p=0;p<3;++p){
            const auto br=trackPartRect(i,count,p+2);
            fillRect(br,flags[p]?mix(kButton,accents[p],0.72f):kButton);
            overlay.addTextCentered(labels[p],{br.x,br.y,br.w,br.h},std::max(0.62f,br.h/28.0f),overlayColor(kWhite));
        }
    }

    for(int i=0;i<2;++i){
        const auto mr=masterSliderRect(i);
        const float value=i==0?project.masterVolume():project.masterReverb();
        const Rgb accent=i==0?kOrange:kPurple;
        fillRect(mr,kButton);
        const float labelW=std::min(64.0f,mr.w*0.28f);
        const Rect bar{mr.x+labelW+4.0f,mr.y+mr.h*0.40f,std::max(2.0f,mr.w-labelW-8.0f),std::max(3.0f,mr.h*0.20f)};
        fillRect(bar,kMuted);fillRect({bar.x,bar.y,bar.w*value,bar.h},accent);
        const float knob=std::max(5.0f,mr.h*0.42f);
        fillRect({bar.x+bar.w*value-knob*0.5f,mr.y+(mr.h-knob)*0.5f,knob,knob},kWhite);
        overlay.addText(i==0?"MASTER":"REVERB",mr.x+4.0f,mr.y+mr.h*0.31f,std::max(0.62f,mr.h/36.0f),overlayColor(kWhite));
    }

    const auto addTrack=addTrackRect(TrackAddKind::Melodic),addDrum=addTrackRect(TrackAddKind::Drums);
    fillRect(addTrack,mix(kButton,kCyan,0.28f));fillRect(addDrum,mix(kButton,kOrange,0.32f));
    overlay.addTextCentered("+ TRACK",{addTrack.x,addTrack.y,addTrack.w,addTrack.h},1.15f,overlayColor(kCyan));
    overlay.addTextCentered("+ DRUM",{addDrum.x,addDrum.y,addDrum.w,addDrum.h},1.15f,overlayColor(kOrange));
}

bool NativeUi::trackPointerDown(float x,float y){
    if(page_!=NativePage::Tracks)return false;
    auto& project=ProjectCore::instance();trackControlChanged_=false;trackActiveSlider_=-1;trackActiveIndex_=-1;

    for(int i=0;i<2;++i){
        const auto r=masterSliderRect(i);if(!r.contains(x,y))continue;
        const float labelW=std::min(64.0f,r.w*0.28f);
        const float start=r.x+labelW+4.0f;
        const float width=std::max(2.0f,r.w-labelW-8.0f);
        const float n=std::clamp((x-start)/width,0.0f,1.0f);
        if(i==0)project.setMasterVolume(n);else project.setMasterReverb(n);
        trackActiveSlider_=i+2;trackControlChanged_=true;return true;
    }

    const int count=project.trackCount();
    for(int t=0;t<count;++t){
        for(int p=0;p<5;++p){
            const auto r=trackPartRect(t,count,p);if(!r.contains(x,y))continue;
            if(p<2){
                const float labelW=std::min(16.0f,r.w*0.18f);
                const float start=r.x+labelW+3.0f;
                const float width=std::max(2.0f,r.w-labelW-7.0f);
                const float n=std::clamp((x-start)/width,0.0f,1.0f);
                if(p==0)project.setTrackVolume(t,n);else project.setTrackPan(t,n*2.0f-1.0f);
                trackActiveSlider_=p;trackActiveIndex_=t;trackControlChanged_=true;
            }else if(p==2){project.setTrackMute(t,!project.trackMute(t));trackControlChanged_=true;}
            else if(p==3){project.setTrackSolo(t,!project.trackSolo(t));trackControlChanged_=true;}
            else {trackControlChanged_=project.deleteTrack(t);}
            return true;
        }
    }
    return false;
}

bool NativeUi::trackPointerMove(float x,float){
    auto& project=ProjectCore::instance();
    if(page_!=NativePage::Tracks||trackActiveSlider_<0)return false;

    if(trackActiveSlider_>=2){
        const int i=trackActiveSlider_-2;const auto r=masterSliderRect(i);
        const float labelW=std::min(64.0f,r.w*0.28f);
        const float start=r.x+labelW+4.0f,width=std::max(2.0f,r.w-labelW-8.0f);
        const float n=std::clamp((x-start)/width,0.0f,1.0f);
        if(i==0)project.setMasterVolume(n);else project.setMasterReverb(n);
    }else{
        const int count=project.trackCount();
        if(trackActiveIndex_<0||trackActiveIndex_>=count)return false;
        const auto r=trackPartRect(trackActiveIndex_,count,trackActiveSlider_);
        const float labelW=std::min(16.0f,r.w*0.18f);
        const float start=r.x+labelW+3.0f,width=std::max(2.0f,r.w-labelW-7.0f);
        const float n=std::clamp((x-start)/width,0.0f,1.0f);
        if(trackActiveSlider_==0)project.setTrackVolume(trackActiveIndex_,n);else project.setTrackPan(trackActiveIndex_,n*2.0f-1.0f);
    }
    trackControlChanged_=true;return true;
}

bool NativeUi::trackPointerUp(){
    const bool changed=trackControlChanged_;
    trackControlChanged_=false;trackActiveSlider_=-1;trackActiveIndex_=-1;
    return changed;
}

void NativeUi::drawPadQuick() const noexcept {
    auto& project = ProjectCore::instance();
    const int track = project.selectedTrack();
    if (track < 0 || !project.trackIsDrums(track)) return;

    const int count = project.padCount(track);
    const int selected = project.selectedPad(track);
    for (int i = 0; i < count; ++i) {
        const auto rect = padQuickRect(i, count);
        const auto color = pitchColor(project.padCenter(track, i));
        fillRect(rect, i == selected ? kOrange : mix(kButton, color, 0.28f));

        const float inset = std::max(2.0f, rect.h * 0.075f);
        Rect inner{
            rect.x + inset,
            rect.y + inset,
            std::max(0.0f, rect.w - inset * 2.0f),
            std::max(0.0f, rect.h - inset * 2.0f)
        };
        fillRect(inner, i == selected ? mix(kPanel, kOrange, 0.22f) : kPanel);

        auto& overlay=NativeOverlay::instance();
        std::string name=project.padIcon(track,i);
        if(name.empty())name="PAD";
        overlay.addTextCentered(
            name,
            {rect.x+2.0f,rect.y+2.0f,rect.w-4.0f,rect.h-4.0f},
            std::max(0.72f,std::min(1.1f,rect.w/(std::max<size_t>(1,name.size())*7.0f))),
            overlayColor(i==selected?kOrange:color));
    }
}

bool NativeUi::drumRangeMode() const noexcept {
    if(drumRangeArmed_)return true;
    auto& project=ProjectCore::instance();const int track=project.selectedTrack();
    if(track<0||!project.trackIsDrums(track))return false;
    const int pad=project.selectedPad(track);
    if(pad<0||pad>=project.padCount(track))return false;
    return project.padLow(track,pad)!=project.padHigh(track,pad);
}

void NativeUi::drawDrumEditor() const noexcept {
    auto& project=ProjectCore::instance();const int track=project.selectedTrack();
    const auto editor=drumEditorRect();fillRect(editor,mix(kPanel,kButton,0.12f));
    if(track<0||!project.trackIsDrums(track))return;

    const int pad=project.selectedPad(track);const int count=project.padCount(track);
    auto& overlay=NativeOverlay::instance();

    static constexpr const char* actions[3]={"RANGE","+ PAD","DELETE"};
    const bool states[3]={drumRangeMode(),count<24,pad>=0&&pad<count};
    const Rgb accents[3]={kCyan,kGreen,kRed};
    for(int i=0;i<3;++i){
        const auto r=drumActionRect(i);
        fillRect(r,states[i]?mix(kButton,accents[i],i==0&&states[i]?0.75f:0.36f):mix(kButton,kMuted,0.35f));
        overlay.addTextCentered(actions[i],{r.x,r.y,r.w,r.h},std::max(0.62f,std::min(0.95f,r.w/(std::char_traits<char>::length(actions[i])*6.5f))),overlayColor(states[i]?kWhite:kMuted));
    }
    if(pad<0||pad>=count)return;

    const float values[2]={project.padVolume(track,pad),(project.padPan(track,pad)+1.0f)*0.5f};
    static constexpr const char* labels[2]={"VOL","PAN"};
    const Rgb sliderColors[2]={kOrange,kCyan};
    for(int i=0;i<2;++i){
        const auto r=drumSliderRect(i);fillRect(r,kButton);
        const float inset=std::max(4.0f,r.h*0.22f);
        const Rect bar{r.x+inset+r.w*0.14f,r.y+r.h*0.39f,std::max(0.0f,r.w-inset*2.0f-r.w*0.14f),std::max(3.0f,r.h*0.22f)};
        fillRect(bar,kMuted);fillRect({bar.x,bar.y,bar.w*std::clamp(values[i],0.0f,1.0f),bar.h},sliderColors[i]);
        const float knob=std::max(5.0f,r.h*0.42f);
        fillRect({bar.x+bar.w*values[i]-knob*0.5f,r.y+(r.h-knob)*0.5f,knob,knob},kWhite);
        overlay.addText(labels[i],r.x+4.0f,r.y+r.h*0.32f,std::max(0.65f,r.h/34.0f),overlayColor(kWhite));
    }

    static constexpr const char* icons[12]={"KICK","SNARE","TOM","FLOOR","HAT","CRASH","RIDE","BONGO","CONGA","CLAP","SHAKER","COW"};
    static constexpr const char* ids[12]={"kick","snare","tom","floortom","hihat","crash","ride","bongo","conga","clap","shaker","cowbell"};
    const std::string current=project.padIcon(track,pad);
    for(int i=0;i<12;++i){
        const auto r=drumIconRect(i);const bool selected=current==ids[i];
        fillRect(r,selected?mix(kButton,kOrange,0.72f):kButton);
        overlay.addTextCentered(icons[i],{r.x,r.y,r.w,r.h},std::max(0.52f,std::min(0.78f,r.w/(std::char_traits<char>::length(icons[i])*6.2f))),overlayColor(selected?kBg:kWhite));
    }
}

void NativeUi::drawDrums() const noexcept {
    fillRect(contentRect(),kPanel);
    drawPadQuick();
    drawGrid();
    drawDrumEditor();
}

bool NativeUi::drumPointerDown(float x,float y){
    if(page_!=NativePage::Drums)return false;
    auto& project=ProjectCore::instance();const int track=project.selectedTrack();
    if(track<0||!project.trackIsDrums(track))return false;
    drumControlChanged_=false;drumActiveSlider_=-1;

    for(int i=0;i<3;++i){
        if(!drumActionRect(i).contains(x,y))continue;
        if(i==0){
            const int pad=project.selectedPad(track);
            if(pad<0||pad>=project.padCount(track))return true;
            if(drumRangeMode()){
                const int center=project.padCenter(track,pad);
                drumControlChanged_=project.setPadRange(track,pad,center,center);
                drumRangeArmed_=false;
            }else drumRangeArmed_=true;
        }else if(i==1){
            drumControlChanged_=project.addDrumPad(track)>=0;
            drumRangeArmed_=false;
        }else{
            const int pad=project.selectedPad(track);
            drumControlChanged_=pad>=0&&project.deleteDrumPad(track,pad);
            drumRangeArmed_=false;
        }
        return true;
    }

    const int pad=project.selectedPad(track);
    if(pad<0||pad>=project.padCount(track))return false;
    for(int i=0;i<2;++i){
        const auto r=drumSliderRect(i);if(!r.contains(x,y))continue;
        drumActiveSlider_=i;
        const float n=std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f);
        if(i==0)project.setPadVolume(track,pad,n);else project.setPadPan(track,pad,n*2.0f-1.0f);
        drumControlChanged_=true;return true;
    }

    static constexpr const char* ids[12]={"kick","snare","tom","floortom","hihat","crash","ride","bongo","conga","clap","shaker","cowbell"};
    for(int i=0;i<12;++i)if(drumIconRect(i).contains(x,y)){
        drumControlChanged_=project.setPadIcon(track,pad,ids[i]);return true;
    }
    return false;
}

bool NativeUi::drumPointerMove(float x,float){
    if(page_!=NativePage::Drums||drumActiveSlider_<0)return false;
    auto& project=ProjectCore::instance();const int track=project.selectedTrack();
    if(track<0||!project.trackIsDrums(track))return false;
    const int pad=project.selectedPad(track);if(pad<0||pad>=project.padCount(track))return false;
    const auto r=drumSliderRect(drumActiveSlider_);
    const float n=std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f);
    if(drumActiveSlider_==0)project.setPadVolume(track,pad,n);else project.setPadPan(track,pad,n*2.0f-1.0f);
    drumControlChanged_=true;return true;
}

bool NativeUi::drumPointerUp(){
    const bool changed=drumControlChanged_;
    drumControlChanged_=false;drumActiveSlider_=-1;
    return changed;
}

bool NativeUi::drumPitchTap(int midi){
    if(page_!=NativePage::Drums)return false;
    auto& project=ProjectCore::instance();const int track=project.selectedTrack();
    if(track<0||!project.trackIsDrums(track))return false;
    const int pad=project.selectedPad(track);if(pad<0||pad>=project.padCount(track))return false;
    midi=std::clamp(midi,kGridLow,kGridHigh);
    if(drumRangeMode()){
        const int center=project.padCenter(track,pad);
        const bool ok=project.setPadRange(track,pad,std::min(center,midi),std::max(center,midi));
        if(ok)drumRangeArmed_=true;
        return ok;
    }
    return project.setPadCenter(track,pad,midi);
}

void NativeUi::drawRoll() const noexcept {
    auto& project = ProjectCore::instance();
    const auto content = contentRect();
    fillRect(content, kPanel);

    static constexpr const char* kModeNames[4]={"NOTES","BEND","V","M"};
    for (int i = 0; i < 4; ++i) {
        const auto mode = static_cast<RollMode>(i);
        const Rgb modeColor =
            mode == rollMode_
                ? (mode == RollMode::Bend ? kCyan :
                   mode == RollMode::Velocity ? kWhite :
                   mode == RollMode::Mod ? kPurple : kOrange)
                : kButton;
        const auto mr=rollModeRect(i);
        fillRect(mr, modeColor);
        NativeOverlay::instance().addTextCentered(
            kModeNames[i],{mr.x,mr.y,mr.w,mr.h},1.2f,
            overlayColor(mode==rollMode_?kBg:kWhite));
    }

    const int track = project.selectedTrack();
    if (track < 0) return;

    const auto viewport = rollViewportRect();
    fillRect(viewport, kRollBg);

    const auto columns = rollColumns();
    const float cell = rollCellPixels();
    const float gutter = rollGutterPixels();
    const float header = rollHeaderPixels();
    const int visibleCols = std::max(
        1, static_cast<int>(std::floor((viewport.w - gutter) / cell)));
    const int visibleRows = std::max(
        1, static_cast<int>(std::floor((viewport.h - header) / cell)));

    const int pitchOffset = std::clamp(
        rollPitchOffset_, 0, std::max(0, static_cast<int>(columns.size()) - visibleCols));
    const int stepOffset = std::clamp(
        rollStepOffset_, 0, std::max(0, rollTotalRows() - visibleRows));

    fillRect({viewport.x, viewport.y, gutter - 1.0f, header - 1.0f}, kPanel);

    for (int c = 0; c < visibleCols && pitchOffset + c < static_cast<int>(columns.size()); ++c) {
        const int midi = columns[static_cast<size_t>(pitchOffset + c)];
        const float x = viewport.x + gutter + c * cell;
        fillRect({x, viewport.y, cell - 1.0f, header - 2.0f}, kPanel);
        const auto pcColor=pitchColor(midi);
        fillRect({x, viewport.y + header - 4.0f, cell - 1.0f, 3.0f}, pcColor);
        NativeOverlay::instance().addPitchGlyph(
            midi%12,{x+cell*0.19f,viewport.y+3.0f,cell*0.62f,std::min(cell*0.62f,header*0.68f)},overlayColor(pcColor));
        if (((midi % 12) + 12) % 12 == 2) {
            const int octave=(midi-62)/12;
            const std::string label=octave>0?("+"+std::to_string(octave)):std::to_string(octave);
            NativeOverlay::instance().addText(
                label,x+cell*0.60f,viewport.y+header*0.62f,std::max(0.65f,cell/40.0f),overlayColor(kMuted));
        }
    }

    const int beatLen = std::max(1, project.divisions());
    const int barLen = std::max(1, project.beats()) * beatLen;

    for (int r = 0; r < visibleRows; ++r) {
        const int step = stepOffset + r;
        const float y = viewport.y + header + r * cell;
        const Rgb rowColor =
            step % barLen == 0 ? kBar :
            step % beatLen == 0 ? kBeat : kCell;

        fillRect({viewport.x, y, gutter - 2.0f, cell - 1.0f}, rowColor);
        if(step%barLen==0){
            const int bar=step/barLen+1;
            NativeOverlay::instance().addText(
                std::to_string(bar),viewport.x+4.0f,y+cell*0.33f,std::max(0.72f,cell/34.0f),overlayColor(kWhite));
        }
        for (int c = 0; c < visibleCols && pitchOffset + c < static_cast<int>(columns.size()); ++c) {
            const float x = viewport.x + gutter + c * cell;
            fillRect({x, y, cell - 1.0f, cell - 1.0f}, rowColor);
        }
    }

    const int notes = project.noteCount(track);
    for (int n = 0; n < notes; ++n) {
        const int midi = project.noteMidi(track, n);
        const auto it = std::lower_bound(columns.begin(), columns.end(), midi);
        if (it == columns.end() || *it != midi) continue;
        const int colIndex = static_cast<int>(std::distance(columns.begin(), it));
        const int visibleCol = colIndex - pitchOffset;
        if (visibleCol < 0 || visibleCol >= visibleCols) continue;

        const float start = project.noteStart(track, n);
        const float length = std::max(1.0f, project.noteLength(track, n));
        if (start + length <= stepOffset || start >= stepOffset + visibleRows) continue;

        const float x = viewport.x + gutter + visibleCol * cell + 1.0f;
        const float y = viewport.y + header + (start - stepOffset) * cell + 1.0f;
        const float h = std::max(cell, length * cell) - 2.0f;
        fillRect({x, y, cell - 3.0f, h}, pitchColor(midi));
        fillRect({x, y + h - 3.0f, cell - 3.0f, 3.0f}, kTop);

        if (rollMode_ == RollMode::Notes) continue;
        const int kind = static_cast<int>(rollMode_) - 1;
        const int points = project.curvePointCount(track, n, kind);
        const Rgb pointColor =
            rollMode_ == RollMode::Bend ? kCyan :
            rollMode_ == RollMode::Velocity ? kWhite : kPurple;

        for (int p = 0; p < points; ++p) {
            const float relStep = project.curvePointStep(track, n, kind, p);
            const float value = project.curvePointValue(track, n, kind, p);
            const bool free = project.curvePointFree(track, n, kind, p);
            const float py = viewport.y + header + (start + relStep + 0.5f - stepOffset) * cell;
            if (py < viewport.y + header || py > viewport.y + viewport.h) continue;

            if (rollMode_ == RollMode::Bend) {
                const float px = x + (cell - 3.0f) * 0.5f +
                    std::clamp(value / 12.0f, -1.0f, 1.0f) * (cell * 0.42f);
                const float s = free ? 7.0f : 5.0f;
                fillRect({px - s * 0.5f, py - s * 0.5f, s, s}, pointColor);
            } else {
                const float half = std::clamp(value, 0.0f, 1.0f) * (cell * 0.42f);
                const float center = x + (cell - 3.0f) * 0.5f;
                const float s = free ? 7.0f : 5.0f;
                fillRect({center - half - s * 0.5f, py - s * 0.5f, s, s}, pointColor);
                fillRect({center + half - s * 0.5f, py - s * 0.5f, s, s}, pointColor);
            }
        }
    }
    if(AudioEngine::instance().transportPlaying()){
        const int ph=AudioEngine::instance().playheadStep();
        if(ph>=stepOffset && ph<stepOffset+visibleRows){
            const float py=viewport.y+header+(ph-stepOffset)*cell;
            fillRect({viewport.x+gutter,py,std::max(0.0f,viewport.w-gutter),2.0f},kCyan);
        }
    }

}

void NativeUi::drawPlaceholder() const noexcept {
    const auto content = contentRect();
    fillRect(content, kPanel);

    const float gap = std::max(6.0f, content.w * 0.012f);
    const float topOffset = pageHasPadQuick(page_) ? std::clamp(content.h * 0.14f, 34.0f, 62.0f) + gap : 0.0f;
    const float rowH = std::max(28.0f, (content.h - topOffset) * 0.12f);
    for (int i = 0; i < 4; ++i) {
        Rect row{
            content.x + gap,
            content.y + topOffset + gap + i * (rowH + gap),
            std::max(0.0f, content.w - gap * 2.0f),
            rowH
        };
        fillRect(row, i == 0 ? mix(kButton, kCyan, 0.22f) : kButton);
    }

    if (pageHasPadQuick(page_)) drawPadQuick();
}

void NativeUi::render() const noexcept {
    if (width_ <= 0 || height_ <= 0) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    NativeOverlay::instance().begin(width_,height_);

    glScissor(0, 0, width_, height_);
    glClearColor(kBg.r, kBg.g, kBg.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const float headerH = std::max(38.0f, height_ * 0.105f);
    fillRect({0.0f, 0.0f, static_cast<float>(width_), headerH}, kTop);
    auto& overlay=NativeOverlay::instance();
    const float logoSize=std::min(30.0f,headerH-6.0f);
    overlay.addLogo({8.0f,(headerH-logoSize)*0.5f,logoSize,logoSize},overlayColor(kCyan));
    overlay.addText("AIORA",14.0f+logoSize,(headerH-14.0f)*0.5f,2.0f,overlayColor(kWhite));
    drawScope();

    auto& project=ProjectCore::instance();
    auto& audio=AudioEngine::instance();
    const auto bpmR=headerControlRect(0),beatR=headerControlRect(1),divR=headerControlRect(2),dzR=headerControlRect(3),playR=headerControlRect(4);
    fillRect(bpmR,kButton);fillRect(beatR,kButton);fillRect(divR,kButton);
    fillRect(dzR,project.dozenal()?kCyan:kButton);
    fillRect(playR,audio.transportPlaying()?kOrange:kGreen);
    overlay.addTextCentered("BPM "+std::to_string(static_cast<int>(std::lround(project.bpm()))),{bpmR.x,bpmR.y,bpmR.w,bpmR.h},0.85f,overlayColor(kWhite));
    overlay.addTextCentered("B "+std::to_string(project.beats()),{beatR.x,beatR.y,beatR.w,beatR.h},0.85f,overlayColor(kWhite));
    overlay.addTextCentered("D "+std::to_string(project.divisions()),{divR.x,divR.y,divR.w,divR.h},0.85f,overlayColor(kWhite));
    overlay.addTextCentered("DZ",{dzR.x,dzR.y,dzR.w,dzR.h},0.85f,overlayColor(project.dozenal()?kBg:kWhite));
    overlay.addTextCentered(audio.transportPlaying()?"STOP":"PLAY",{playR.x,playR.y,playR.w,playR.h},0.85f,overlayColor(kBg));

    static constexpr const char* kNavNames[6]={"TRACKS","DRUMS","ROLL","SYNTH","FX","PLAY"};
    for (int i = 0; i < 6; ++i) {
        const auto page = static_cast<NativePage>(i);
        const auto nr=navRect(i);
        fillRect(nr, page == page_ ? kCyan : kButton);
        overlay.addTextCentered(
            kNavNames[i],{nr.x,nr.y,nr.w,nr.h},
            std::max(0.75f,std::min(1.25f,nr.w/(std::char_traits<char>::length(kNavNames[i])*7.0f))),
            overlayColor(page==page_?kBg:kWhite));
    }

    switch (page_) {
        case NativePage::Tracks:
            drawTracks();
            break;
        case NativePage::Drums:
            drawDrums();
            break;
        case NativePage::Roll:
            drawRoll();
            break;
        case NativePage::Synth:
            fillRect(contentRect(), kPanel);
            drawPadQuick();
            NativeEditor::instance().renderSynth();
            break;
        case NativePage::Fx:
            fillRect(contentRect(), kPanel);
            drawPadQuick();
            NativeEditor::instance().renderFx();
            break;
        case NativePage::Play:
            fillRect(contentRect(), kPanel);
            drawGrid();
            break;
        default:
            drawPlaceholder();
            break;
    }

    glDisable(GL_SCISSOR_TEST);
    NativeOverlay::instance().flush();
}

std::optional<HeaderAction> NativeUi::hitHeader(float x,float y) const noexcept {
    for(int i=0;i<5;++i){
        const auto r=headerControlRect(i);
        if(!r.contains(x,y))continue;
        if(i==0)return x<r.x+r.w*0.5f?HeaderAction::BpmDown:HeaderAction::BpmUp;
        if(i==1)return x<r.x+r.w*0.5f?HeaderAction::BeatsDown:HeaderAction::BeatsUp;
        if(i==2)return x<r.x+r.w*0.5f?HeaderAction::DivDown:HeaderAction::DivUp;
        if(i==3)return HeaderAction::DozenalToggle;
        return HeaderAction::TransportToggle;
    }
    return std::nullopt;
}

std::optional<NativePage> NativeUi::hitNav(float x, float y) const noexcept {
    for (int i = 0; i < 6; ++i) {
        if (navRect(i).contains(x, y)) {
            return static_cast<NativePage>(i);
        }
    }
    return std::nullopt;
}

std::optional<int> NativeUi::hitPitch(float x, float y) const noexcept {
    if (page_ != NativePage::Play && page_ != NativePage::Drums) {
        return std::nullopt;
    }

    for (int visualRow = 0; visualRow < 7; ++visualRow) {
        for (int column = 0; column < 7; ++column) {
            if (!gridRect(visualRow, column).contains(x, y)) continue;
            const int block = 6 - visualRow;
            return kGridLow + block * 7 + column;
        }
    }
    return std::nullopt;
}

std::optional<int> NativeUi::hitTrack(float x, float y) const noexcept {
    if (page_ != NativePage::Tracks) return std::nullopt;
    const int count = ProjectCore::instance().trackCount();
    for (int i = 0; i < count; ++i) {
        if (trackRect(i, count).contains(x, y)) return i;
    }
    return std::nullopt;
}

std::optional<ProjectTransferAction> NativeUi::hitProjectTransfer(float x,float y) const noexcept {
    if(page_!=NativePage::Tracks)return std::nullopt;
    if(projectTransferRect(0).contains(x,y))return ProjectTransferAction::CopyProject;
    if(projectTransferRect(1).contains(x,y))return ProjectTransferAction::PasteProject;
    return std::nullopt;
}

std::optional<TrackAddKind> NativeUi::hitAddTrack(float x, float y) const noexcept {
    if (page_ != NativePage::Tracks) return std::nullopt;
    if (addTrackRect(TrackAddKind::Melodic).contains(x, y)) return TrackAddKind::Melodic;
    if (addTrackRect(TrackAddKind::Drums).contains(x, y)) return TrackAddKind::Drums;
    return std::nullopt;
}

std::optional<int> NativeUi::hitPadQuick(float x, float y) const noexcept {
    if (!pageHasPadQuick(page_)) return std::nullopt;

    auto& project = ProjectCore::instance();
    const int track = project.selectedTrack();
    if (track < 0 || !project.trackIsDrums(track)) return std::nullopt;

    const int count = project.padCount(track);
    for (int i = 0; i < count; ++i) {
        if (padQuickRect(i, count).contains(x, y)) return i;
    }
    return std::nullopt;
}

std::optional<RollMode> NativeUi::hitRollMode(float x, float y) const noexcept {
    if (page_ != NativePage::Roll) return std::nullopt;
    for (int i = 0; i < 4; ++i) {
        if (rollModeRect(i).contains(x, y)) return static_cast<RollMode>(i);
    }
    return std::nullopt;
}

std::optional<RollCellHit> NativeUi::hitRollCell(float x, float y) const noexcept {
    if (page_ != NativePage::Roll) return std::nullopt;

    const auto viewport = rollViewportRect();
    const float cell = rollCellPixels();
    const float gutter = rollGutterPixels();
    const float header = rollHeaderPixels();
    if (x < viewport.x + gutter || y < viewport.y + header ||
        x >= viewport.x + viewport.w || y >= viewport.y + viewport.h) {
        return std::nullopt;
    }

    const auto columns = rollColumns();
    const int visibleCol = static_cast<int>(std::floor((x - viewport.x - gutter) / cell));
    const int visibleRow = static_cast<int>(std::floor((y - viewport.y - header) / cell));
    const int colIndex = rollPitchOffset_ + visibleCol;
    const int step = rollStepOffset_ + visibleRow;
    if (visibleCol < 0 || visibleRow < 0 ||
        colIndex < 0 || colIndex >= static_cast<int>(columns.size()) ||
        step < 0 || step >= rollTotalRows()) {
        return std::nullopt;
    }

    const float cellLeft = viewport.x + gutter + visibleCol * cell;
    return RollCellHit{
        columns[static_cast<size_t>(colIndex)],
        step,
        std::clamp((x - cellLeft) / cell, 0.0f, 1.0f)
    };
}

int NativeUi::padIndexForMidi(int midi) noexcept {
    const int clamped = std::clamp(midi, kGridLow, kGridHigh);
    const int lowToHighBlock = (clamped - kGridLow) / 7;
    return 6 - lowToHighBlock;
}

} // namespace aiora
