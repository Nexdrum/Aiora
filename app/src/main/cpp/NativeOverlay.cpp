#include "NativeOverlay.h"
#include "NativeGlyphMasks.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace aiora {
namespace {

constexpr char kVs[] = R"(#version 300 es
layout(location=0) in vec2 aPos;
layout(location=1) in vec4 aColor;
out vec4 vColor;
void main(){
    gl_Position=vec4(aPos,0.0,1.0);
    vColor=aColor;
})";

constexpr char kFs[] = R"(#version 300 es
precision mediump float;
in vec4 vColor;
out vec4 fragColor;
void main(){
    fragColor=vColor;
})";

GLuint compile(GLenum type,const char* src){
    const GLuint shader=glCreateShader(type);
    glShaderSource(shader,1,&src,nullptr);
    glCompileShader(shader);
    GLint ok=GL_FALSE;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(ok!=GL_TRUE){glDeleteShader(shader);return 0;}
    return shader;
}

std::array<uint8_t,7> glyph(char c){
    switch(c){
        case 'A':return {14,17,17,31,17,17,17};
        case 'B':return {30,17,17,30,17,17,30};
        case 'C':return {14,17,16,16,16,17,14};
        case 'D':return {30,17,17,17,17,17,30};
        case 'E':return {31,16,16,30,16,16,31};
        case 'F':return {31,16,16,30,16,16,16};
        case 'G':return {14,17,16,23,17,17,15};
        case 'H':return {17,17,17,31,17,17,17};
        case 'I':return {31,4,4,4,4,4,31};
        case 'J':return {7,2,2,2,18,18,12};
        case 'K':return {17,18,20,24,20,18,17};
        case 'L':return {16,16,16,16,16,16,31};
        case 'M':return {17,27,21,21,17,17,17};
        case 'N':return {17,25,21,19,17,17,17};
        case 'O':return {14,17,17,17,17,17,14};
        case 'P':return {30,17,17,30,16,16,16};
        case 'Q':return {14,17,17,17,21,18,13};
        case 'R':return {30,17,17,30,20,18,17};
        case 'S':return {15,16,16,14,1,1,30};
        case 'T':return {31,4,4,4,4,4,4};
        case 'U':return {17,17,17,17,17,17,14};
        case 'V':return {17,17,17,17,17,10,4};
        case 'W':return {17,17,17,21,21,21,10};
        case 'X':return {17,17,10,4,10,17,17};
        case 'Y':return {17,17,10,4,4,4,4};
        case 'Z':return {31,1,2,4,8,16,31};
        case '0':return {14,17,19,21,25,17,14};
        case '1':return {4,12,4,4,4,4,14};
        case '2':return {14,17,1,2,4,8,31};
        case '3':return {30,1,1,14,1,1,30};
        case '4':return {2,6,10,18,31,2,2};
        case '5':return {31,16,16,30,1,1,30};
        case '6':return {14,16,16,30,17,17,14};
        case '7':return {31,1,2,4,8,8,8};
        case '8':return {14,17,17,14,17,17,14};
        case '9':return {14,17,17,15,1,1,14};
        case '+':return {0,4,4,31,4,4,0};
        case '-':return {0,0,0,31,0,0,0};
        case '.':return {0,0,0,0,0,12,12};
        case '/':return {1,2,2,4,8,8,16};
        case ':':return {0,12,12,0,12,12,0};
        case '_':return {0,0,0,0,0,0,31};
        case '=':return {0,31,0,31,0,0,0};
        case '?':return {14,17,1,2,4,0,4};
        case ' ':return {0,0,0,0,0,0,0};
        default:return {14,17,1,2,4,0,4};
    }
}

} // namespace

NativeOverlay& NativeOverlay::instance(){static NativeOverlay o;return o;}

bool NativeOverlay::buildProgram(){
    const GLuint vs=compile(GL_VERTEX_SHADER,kVs);
    const GLuint fs=compile(GL_FRAGMENT_SHADER,kFs);
    if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return false;}
    program_=glCreateProgram();
    glAttachShader(program_,vs);glAttachShader(program_,fs);glLinkProgram(program_);
    glDeleteShader(vs);glDeleteShader(fs);
    GLint ok=GL_FALSE;glGetProgramiv(program_,GL_LINK_STATUS,&ok);
    if(ok!=GL_TRUE){glDeleteProgram(program_);program_=0;return false;}
    glGenBuffers(1,&vbo_);
    return vbo_!=0;
}

bool NativeOverlay::init(){
    if(program_&&vbo_)return true;
    return buildProgram();
}

void NativeOverlay::shutdown() noexcept {
    vertices_.clear();
    if(vbo_){glDeleteBuffers(1,&vbo_);vbo_=0;}
    if(program_){glDeleteProgram(program_);program_=0;}
}

void NativeOverlay::begin(int width,int height){
    width_=std::max(1,width);
    height_=std::max(1,height);
    const float shortSide=static_cast<float>(std::min(width_,height_));
    fontScale_=std::clamp(shortSide/620.0f,1.10f,1.55f);
    vertices_.clear();
    vertices_.reserve(12000);
}

void NativeOverlay::addRect(Rect r,Color c){
    if(r.w<=0.0f||r.h<=0.0f)return;
    const float x0=r.x/static_cast<float>(width_)*2.0f-1.0f;
    const float x1=(r.x+r.w)/static_cast<float>(width_)*2.0f-1.0f;
    const float y0=1.0f-r.y/static_cast<float>(height_)*2.0f;
    const float y1=1.0f-(r.y+r.h)/static_cast<float>(height_)*2.0f;
    const Vertex a{x0,y0,c.r,c.g,c.b,c.a};
    const Vertex b{x1,y0,c.r,c.g,c.b,c.a};
    const Vertex d{x0,y1,c.r,c.g,c.b,c.a};
    const Vertex e{x1,y1,c.r,c.g,c.b,c.a};
    vertices_.insert(vertices_.end(),{a,d,b,b,d,e});
}

void NativeOverlay::addMaskRun(float x,float y,float w,float h,Color c){
    addRect({x,y,w,h},c);
}

void NativeOverlay::addMaskRows(const uint32_t* rows,int rowCount,int columnCount,Rect rect,Color color){
    const float px=rect.w/static_cast<float>(columnCount);
    const float py=rect.h/static_cast<float>(rowCount);
    for(int row=0;row<rowCount;++row){
        const uint32_t bits=rows[row];
        int col=0;
        while(col<columnCount){
            const uint32_t mask=1u<<(columnCount-1-col);
            if((bits&mask)==0u){++col;continue;}
            const int start=col;
            while(col<columnCount&&(bits&(1u<<(columnCount-1-col)))!=0u)++col;
            addMaskRun(rect.x+start*px,rect.y+row*py,(col-start)*px,py,color);
        }
    }
}

char NativeOverlay::normalizedChar(char c) noexcept {
    if(c>='a'&&c<='z')return static_cast<char>(c-'a'+'A');
    return c;
}

uint8_t NativeOverlay::fontRow(char c,int row) noexcept {
    if(row<0||row>=7)return 0;
    return glyph(normalizedChar(c))[static_cast<size_t>(row)];
}

float NativeOverlay::textWidth(std::string_view text,float scale) const noexcept {
    if(text.empty())return 0.0f;
    const float effective=scale*fontScale_;
    return static_cast<float>(text.size())*6.0f*effective-effective;
}

void NativeOverlay::addText(std::string_view text,float x,float y,float scale,Color color){
    if(scale<=0.0f)return;
    scale*=fontScale_;
    float pen=x;
    for(char raw:text){
        const char c=normalizedChar(raw);
        for(int row=0;row<7;++row){
            const uint8_t bits=fontRow(c,row);
            int col=0;
            while(col<5){
                if((bits&(1u<<(4-col)))==0u){++col;continue;}
                const int start=col;
                while(col<5&&(bits&(1u<<(4-col)))!=0u)++col;
                addRect({pen+start*scale,y+row*scale,(col-start)*scale,scale},color);
            }
        }
        pen+=6.0f*scale;
    }
}

void NativeOverlay::addTextCentered(std::string_view text,Rect r,float scale,Color color){
    const float w=textWidth(text,scale);
    const float h=7.0f*scale*fontScale_;
    addText(text,r.x+(r.w-w)*0.5f,r.y+(r.h-h)*0.5f,scale,color);
}

void NativeOverlay::addPitchGlyph(int pitchClass,Rect rect,Color color){
    const int pc=((pitchClass%12)+12)%12;
    addMaskRows(glyphmask::kPitch[static_cast<size_t>(pc)].data(),28,28,rect,color);
}

void NativeOverlay::addLogo(Rect rect,Color color){
    addMaskRows(glyphmask::kLogo.data(),32,32,rect,color);
}

void NativeOverlay::addLine(float x1,float y1,float x2,float y2,float thickness,Color c){
    const float dx=x2-x1,dy=y2-y1;
    const float len=std::sqrt(dx*dx+dy*dy);
    if(len<=0.001f||thickness<=0.0f)return;
    const float nx=-dy/len*thickness*0.5f;
    const float ny= dx/len*thickness*0.5f;
    auto vtx=[&](float x,float y){
        return Vertex{
            x/static_cast<float>(width_)*2.0f-1.0f,
            1.0f-y/static_cast<float>(height_)*2.0f,
            c.r,c.g,c.b,c.a
        };
    };
    const Vertex a=vtx(x1+nx,y1+ny);
    const Vertex b=vtx(x2+nx,y2+ny);
    const Vertex d=vtx(x1-nx,y1-ny);
    const Vertex e=vtx(x2-nx,y2-ny);
    vertices_.insert(vertices_.end(),{a,d,b,b,d,e});
}

void NativeOverlay::addCircle(float cx,float cy,float radius,float thickness,Color color){
    if(radius<=0.0f||thickness<=0.0f)return;
    constexpr int segments=24;
    float px=cx+radius,py=cy;
    for(int i=1;i<=segments;++i){
        const float a=static_cast<float>(i)*6.28318530718f/static_cast<float>(segments);
        const float x=cx+std::cos(a)*radius;
        const float y=cy+std::sin(a)*radius;
        addLine(px,py,x,y,thickness,color);
        px=x;py=y;
    }
}

void NativeOverlay::addChevron(Rect r,bool right,Color c){
    const float t=std::max(2.0f,std::min(r.w,r.h)*0.11f);
    const float cx=r.x+r.w*0.5f,cy=r.y+r.h*0.5f;
    const float dx=r.w*0.16f,dy=r.h*0.19f;
    if(right){
        addLine(cx-dx,cy-dy,cx+dx,cy,t,c);
        addLine(cx+dx,cy,cx-dx,cy+dy,t,c);
    }else{
        addLine(cx+dx,cy-dy,cx-dx,cy,t,c);
        addLine(cx-dx,cy,cx+dx,cy+dy,t,c);
    }
}

void NativeOverlay::addDownChevron(Rect r,Color c){
    const float t=std::max(1.5f,std::min(r.w,r.h)*0.08f);
    const float cx=r.x+r.w*0.5f,cy=r.y+r.h*0.52f;
    const float dx=r.w*0.16f,dy=r.h*0.12f;
    addLine(cx-dx,cy-dy,cx,cy+dy,t,c);
    addLine(cx,cy+dy,cx+dx,cy-dy,t,c);
}

void NativeOverlay::addNavIcon(int index,Rect r,Color c){
    const float s=std::min(r.w,r.h);
    const float cx=r.x+r.w*0.5f,cy=r.y+r.h*0.5f;
    const float t=std::max(2.0f,s*0.055f);

    switch(index){
        case 0:{ // tracks / mixer
            for(int i=0;i<3;++i){
                const float y=cy+s*(-0.22f+0.22f*i);
                addLine(cx-s*0.24f,y,cx+s*0.24f,y,t,c);
            }
            addCircle(cx+s*0.12f,cy-s*0.22f,s*0.055f,t,c);
            addCircle(cx-s*0.08f,cy,s*0.055f,t,c);
            addCircle(cx+s*0.05f,cy+s*0.22f,s*0.055f,t,c);
            break;
        }
        case 1:{ // drum
            const float top=cy-s*0.20f,bottom=cy+s*0.20f;
            addCircle(cx,top,s*0.20f,t,c);
            addLine(cx-s*0.20f,top,cx-s*0.17f,bottom,t,c);
            addLine(cx+s*0.20f,top,cx+s*0.17f,bottom,t,c);
            addLine(cx-s*0.17f,bottom,cx-s*0.08f,bottom+s*0.06f,t,c);
            addLine(cx+s*0.17f,bottom,cx+s*0.08f,bottom+s*0.06f,t,c);
            addLine(cx-s*0.08f,bottom+s*0.06f,cx+s*0.08f,bottom+s*0.06f,t,c);
            break;
        }
        case 2:{ // piano roll grid
            const float cell=s*0.12f,g=s*0.04f;
            const float total=cell*3.0f+g*2.0f;
            const float x0=cx-total*0.5f,y0=cy-total*0.5f;
            for(int y=0;y<3;++y)for(int x=0;x<3;++x)
                addRect({x0+x*(cell+g),y0+y*(cell+g),cell,cell},c);
            break;
        }
        case 3:{ // synth sliders
            const float xs[3]{cx-s*0.18f,cx,cx+s*0.18f};
            const float ys[3]{cy+s*0.11f,cy-s*0.13f,cy+s*0.02f};
            for(int i=0;i<3;++i){
                addLine(xs[i],cy-s*0.28f,xs[i],cy+s*0.28f,t,c);
                addCircle(xs[i],ys[i],s*0.06f,t,c);
            }
            break;
        }
        case 4:{ // fx wave
            float px=cx-s*0.27f,py=cy;
            constexpr int n=8;
            for(int i=1;i<n;++i){
                const float u=static_cast<float>(i)/static_cast<float>(n-1);
                const float x=cx-s*0.27f+u*s*0.54f;
                const float y=cy+std::sin(u*6.28318530718f)*s*0.13f;
                addLine(px,py,x,y,t,c);px=x;py=y;
            }
            addCircle(cx+s*0.22f,cy+s*0.12f,s*0.045f,t,c);
            break;
        }
        case 5:{ // keyboard
            const float x0=cx-s*0.27f,y0=cy-s*0.18f,w=s*0.54f,h=s*0.36f;
            addLine(x0,y0,x0+w,y0,t,c);
            addLine(x0,y0+h,x0+w,y0+h,t,c);
            addLine(x0,y0,x0,y0+h,t,c);
            addLine(x0+w,y0,x0+w,y0+h,t,c);
            for(int i=1;i<5;++i){
                const float x=x0+w*i/5.0f;
                addLine(x,y0,x,y0+h,t*0.7f,c);
            }
            for(float u:{0.20f,0.40f,0.70f,0.90f})
                addRect({x0+w*u-s*0.035f,y0,s*0.07f,h*0.52f},c);
            break;
        }
        default:break;
    }
}

void NativeOverlay::flush(){
    if(vertices_.empty()||!program_||!vbo_)return;
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program_);
    glBindBuffer(GL_ARRAY_BUFFER,vbo_);
    glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(vertices_.size()*sizeof(Vertex)),vertices_.data(),GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(sizeof(float)*2));
    glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(vertices_.size()));
    glDisableVertexAttribArray(0);glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER,0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}

} // namespace aiora
