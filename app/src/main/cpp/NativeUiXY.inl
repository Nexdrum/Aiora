// Dark, hardware-style XY expression surface. Only accent geometry glows;
// the large panel and pad interiors remain near-black.
void NativeUi::drawExpressionPad() const noexcept {
    const auto box=expressionShellRect(),p=expressionPadRect();
    auto& o=NativeOverlay::instance();
    fillRect(box,mix(kBg,kPanel,0.48f));
    fillRect({box.x+2.0f,box.y+2.0f,
              std::max(1.0f,box.w-4.0f),std::max(1.0f,box.h-4.0f)},
             mix(kRollBg,kTop,0.57f));
    const auto teal=overlayColor(kCyan,0.78f);
    const auto violet=overlayColor(kPurple,0.78f);
    const float right=box.x+box.w,bottom=box.y+box.h;
    // Thin high-contrast hardware rails, not full-panel color overlays.
    o.addLine(box.x+1.0f,box.y+1.0f,right-1.0f,box.y+1.0f,
              2.0f,teal);
    o.addLine(box.x+1.0f,box.y+1.0f,box.x+1.0f,bottom-1.0f,
              2.0f,overlayColor(kCyan,0.43f));
    o.addLine(right-1.0f,box.y+1.0f,right-1.0f,bottom-1.0f,
              2.0f,overlayColor(kPurple,0.60f));
    o.addLine(box.x+1.0f,bottom-1.0f,right-1.0f,bottom-1.0f,
              2.0f,violet);
    o.addText("EXPRESSION X / Y",box.x+12.0f,box.y+9.0f,
              0.84f,overlayColor(kWhite));

    // A 2-pixel illuminated bezel surrounds the dark XY grid.
    fillRect({p.x-3.0f,p.y-3.0f,p.w+6.0f,p.h+6.0f},
             mix(kCyan,kPurple,0.45f));
    fillRect({p.x-1.0f,p.y-1.0f,p.w+2.0f,p.h+2.0f},
             mix(kTop,kRollBg,0.65f));
    fillRect(p,mix(kRollBg,kTop,0.24f));

    for(int i=1;i<=4;++i){
        const float t=static_cast<float>(i)/5.0f;
        o.addLine(p.x+p.w*t,p.y,p.x+p.w*t,p.y+p.h,
                  1.0f,overlayColor(kCyan,0.18f));
        o.addLine(p.x,p.y+p.h*t,p.x+p.w,p.y+p.h*t,
                  1.0f,overlayColor(kPurple,0.18f));
    }
    // The center reference remains subtle even at the default 50/50 point.
    o.addLine(p.x+p.w*.5f,p.y,p.x+p.w*.5f,p.y+p.h,
              1.0f,overlayColor(kCyan,0.20f));
    o.addLine(p.x,p.y+p.h*.5f,p.x+p.w,p.y+p.h*.5f,
              1.0f,overlayColor(kPurple,0.20f));

    const float x=p.x+p.w*expressionV_,y=p.y+p.h*expressionM_;
    o.addLine(x,p.y,x,p.y+p.h,1.6f,overlayColor(kCyan,0.82f));
    o.addLine(p.x,y,p.x+p.w,y,1.6f,overlayColor(kPurple,0.82f));
    o.addFilledCircle(x,y,20.0f,overlayColor(kCyan,0.13f));
    o.addFilledCircle(x,y,14.0f,overlayColor(kPurple,0.24f));
    o.addFilledCircle(x,y,10.0f,overlayColor(kPurple,0.98f));
    o.addFilledCircle(x,y,5.0f,overlayColor(kWhite));

    const float labelY=p.y+p.h+10.0f;
    o.addText("V "+std::to_string(static_cast<int>(std::lround(expressionV_*100))),
              box.x+13.0f,labelY,0.91f,teal);
    o.addText("M "+std::to_string(static_cast<int>(std::lround(expressionM_*100))),
              box.x+box.w*.54f,labelY,0.91f,violet);
}
