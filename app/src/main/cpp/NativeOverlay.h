#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include <GLES3/gl3.h>
#include <android/native_activity.h>

namespace aiora {

class NativeOverlay {
public:
    struct Color { float r{},g{},b{},a{1.0f}; };
    struct Rect { float x{},y{},w{},h{}; };

    static NativeOverlay& instance();

    bool init(ANativeActivity* activity);
    void shutdown() noexcept;
    void begin(int width,int height);
    void flush();

    void addRect(Rect rect,Color color);
    void addQuad(float x0,float y0,float x1,float y1,float x2,float y2,float x3,float y3,Color color);
    void addGradientQuad(float x0,float y0,float x1,float y1,float x2,float y2,float x3,float y3,Color topColor,Color bottomColor);
    void addText(std::string_view text,float x,float y,float scale,Color color);
    void addTextCentered(std::string_view text,Rect rect,float scale,Color color);
    void addPitchGlyph(int pitchClass,Rect rect,Color color);
    void addLogo(Rect rect,Color color);
    void addSpectrumLogo(Rect rect);
    void addLine(float x1,float y1,float x2,float y2,float thickness,Color color);
    void addCircle(float cx,float cy,float radius,float thickness,Color color);
    void addFilledCircle(float cx,float cy,float radius,Color color);
    void addNavIcon(int index,Rect rect,Color color);
    void addDrumIcon(int index,Rect rect,Color color);
    void addChevron(Rect rect,bool right,Color color);
    void addDownChevron(Rect rect,Color color);

    void setClip(Rect rect) noexcept { clip_=rect; clipEnabled_=true; }
    void clearClip() noexcept { clipEnabled_=false; }
    bool clipRect(Rect& rect) const noexcept;

    [[nodiscard]] float textWidth(std::string_view text,float scale) const noexcept;

private:
    NativeOverlay() = default;

    struct Vertex {
        float x{},y{};
        float r{},g{},b{},a{};
    };

    struct TextVertex {
        float x{},y{};
        float u{},v{};
        float r{},g{},b{},a{};
    };

    struct GlyphInfo {
        float u0{},v0{},u1{},v1{};
        float xBearing{};
        float yBearing{};
        float width{};
        float height{};
        float advance{};
        bool valid{false};
    };

    void addMaskRun(float x,float y,float w,float h,Color color);
    void addMaskRows(const uint32_t* rows,int rowCount,int columnCount,Rect rect,Color color);
    void addMaskRows64(const uint64_t* rows,int rowCount,int columnCount,Rect rect,Color color);
    static uint8_t fontRow(char c,int row) noexcept;
    static char normalizedChar(char c) noexcept;
    bool buildProgram();
    bool buildTextProgram();
    bool buildFontAtlas(ANativeActivity* activity);
    bool buildGlyphAtlas();
    void addGlyphQuad(
        Rect rect,float u0,float v0,float u1,float v1,Color color);
    void addGlyphGradientQuad(
        Rect rect,float u0,float v0,float u1,float v1,
        Color topLeft,Color topRight,Color bottomLeft,Color bottomRight);
    void addBitmapText(std::string_view text,float x,float y,float scale,Color color);
    void addTextQuad(
        float x,float y,float w,float h,
        float u0,float v0,float u1,float v1,
        Color color);

    GLuint program_{0};
    GLuint vbo_{0};
    GLuint textProgram_{0};
    GLuint textVbo_{0};
    GLuint fontTexture_{0};
    GLuint glyphTexture_{0};
    int glyphAtlasWidth_{0};
    int glyphAtlasHeight_{0};
    int atlasWidth_{0};
    int atlasHeight_{0};
    static constexpr int kAsciiGlyphs=95;
    static constexpr int kExtraGlyphs=8;
    static constexpr int kGlyphCount=kAsciiGlyphs+kExtraGlyphs;
    std::array<GlyphInfo,kGlyphCount> glyphs_{};
    int width_{0};
    int height_{0};
    float fontScale_{1.0f};
    std::vector<Vertex> vertices_;
    std::vector<TextVertex> glyphVertices_;
    std::vector<TextVertex> textVertices_;
    bool clipEnabled_{false};
    Rect clip_{};
};

} // namespace aiora
