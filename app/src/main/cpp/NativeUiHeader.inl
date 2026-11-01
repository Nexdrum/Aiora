// Header and navigation are a single coordinated spectral UI shell.
drawExpressionPad();
const auto transport=transportShellRect();
// Matte graphite module with thin purple/cyan spectral rails.
fillRect(transport,mix(kBg,kPanel,.52f));
fillRect({transport.x+2.0f,transport.y+2.0f,
          std::max(1.0f,transport.w-4.0f),
          std::max(1.0f,transport.h-4.0f)},
         mix(kRollBg,kTop,.59f));
auto& overlay=NativeOverlay::instance();
const float xr=transport.x+transport.w,yr=transport.y+transport.h;
overlay.addLine(transport.x+1.0f,transport.y+1.0f,xr-1.0f,
                transport.y+1.0f,2.0f,overlayColor(kPurple,.84f));
overlay.addLine(transport.x+1.0f,transport.y+1.0f,
                transport.x+1.0f,yr-1.0f,2.0f,overlayColor(kCyan,.40f));
overlay.addLine(xr-1.0f,transport.y+1.0f,
                xr-1.0f,yr-1.0f,2.0f,overlayColor(kPurple,.48f));
overlay.addLine(transport.x+1.0f,yr-1.0f,xr-1.0f,yr-1.0f,
                2.0f,overlayColor(kCyan,.77f));
const float logoSize=std::clamp(transport.h*.18f,28.f,58.f);
const NativeOverlay::Rect logoR{
    transport.x+transport.w*.11f,transport.y+transport.h*.065f,
    logoSize,logoSize};
overlay.addSpectrumLogo(logoR);
overlay.addText("A I O R A",logoR.x+logoSize+9.f,
                logoR.y+logoSize*.25f,.92f,overlayColor(kWhite));
 auto& project=ProjectCore::instance();
auto& audio=AudioEngine::instance();
const auto startR=headerControlRect(0),playR=headerControlRect(1),
           endR=headerControlRect(2);
const auto bezel=[&](Rect r,Rgb accent){
    // Illuminated perimeter with an inset hardware face.
    fillRect(r,mix(kPanel,accent,.85f));
    fillRect({r.x+3,r.y+3,std::max(1.f,r.w-6),
             std::max(1.f,r.h-6)},mix(kButton,kBg,.34f));
    fillRect({r.x+5,r.y+5,std::max(1.f,r.w-10),1.4f},
             mix(kPanel,accent,.30f));
};
bezel(startR,kBlue);
bezel(playR,audio.transportPlaying()?kOrange:kGreen);
bezel(endR,kPurple);
overlay.addTextCentered("|<",{startR.x,startR.y,startR.w,startR.h},
                        1.32f,overlayColor(kWhite));
if(audio.transportPlaying()){
    const float a=playR.h*.28f;
    overlay.addRect({playR.x+(playR.w-a)*.5f,playR.y+(playR.h-a)*.5f,a,a},
                    overlayColor(kGreen));
}else overlay.addTextCentered("▶",{playR.x,playR.y,playR.w,playR.h},
                              1.65f,overlayColor(kWhite));
overlay.addTextCentered(">|",{endR.x,endR.y,endR.w,endR.h},
                        1.32f,overlayColor(kWhite));
drawScope();
static constexpr std::array<Rgb,7> accents{
    kCyan,kOrange,kPurple,kRed,kBlue,kGreen,kGold};
// These are the displayed navigation names; the stable NativePage enum
// values remain unchanged for existing saves and page actions.
static constexpr std::array<const char*,7> navLabels{
    "Tracks","Drums","Synth","FX","Roll","Score","Live"};
for(int i=0;i<7;++i){
    const auto nr=navRect(i);
    const bool active=kNavPages[static_cast<size_t>(i)]==page_;
    const auto accent=accents[static_cast<size_t>(i)];
    fillRect(nr,active?accent:mix(kButton,accent,.54f));
    const float rim=active?3.f:2.f;
    fillRect({nr.x+rim,nr.y+rim,std::max(1.f,nr.w-2*rim),
              std::max(1.f,nr.h-2*rim)},
             active?mix(kPanel,accent,.26f):kButton);
    // Icon above its compact label; preserve the existing touch targets.
    const float inset=std::max(4.f,std::min(nr.w,nr.h)*.14f);
    const float iconBand=nr.h*.68f;
    overlay.addNavIcon(kNavIconIndices[static_cast<size_t>(i)],
                       {nr.x+inset,nr.y+2.0f,
                        std::max(1.0f,nr.w-2*inset),std::max(1.0f,iconBand-3.0f)},
                       overlayColor(active?accent:kWhite));
    overlay.addTextCentered(navLabels[static_cast<size_t>(i)],
                       {nr.x+1.0f,nr.y+iconBand,
                        std::max(1.0f,nr.w-2.0f),std::max(1.0f,nr.h-iconBand-1.0f)},
                       .61f,overlayColor(active?accent:kWhite));
}
