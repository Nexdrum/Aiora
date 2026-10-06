#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include <GLES3/gl3.h>

namespace aiora {

class NativeOverlay {
public:
    struct Color { float r{},g{},b{},a{1.0f}; };
    struct Rect { float x{},y{},w{},h{}; };

    static NativeOverlay& instance();

    bool init();
    void shutdown() noexcept;
    void begin(int width,int height);
    void flush();

    void addRect(Rect rect,Color color);
    void addText(std::string_view text,float x,float y,float scale,Color color);
    void addTextCentered(std::string_view text,Rect rect,float scale,Color color);
    void addPitchGlyph(int pitchClass,Rect rect,Color color);
    void addLogo(Rect rect,Color color);
    void addLine(float x1,float y1,float x2,float y2,float thickness,Color color);
    void addCircle(float cx,float cy,float radius,float thickness,Color color);
    void addNavIcon(int index,Rect rect,Color color);
    void addChevron(Rect rect,bool right,Color color);
    void addDownChevron(Rect rect,Color color);

    [[nodiscard]] float textWidth(std::string_view text,float scale) const noexcept;

private:
    NativeOverlay() = default;

    struct Vertex {
        float x{},y{};
        float r{},g{},b{},a{};
    };

    void addMaskRun(float x,float y,float w,float h,Color color);
    void addMaskRows(const uint32_t* rows,int rowCount,int columnCount,Rect rect,Color color);
    static uint8_t fontRow(char c,int row) noexcept;
    static char normalizedChar(char c) noexcept;
    bool buildProgram();

    GLuint program_{0};
    GLuint vbo_{0};
    int width_{0};
    int height_{0};
    float fontScale_{1.0f};
    std::vector<Vertex> vertices_;
};

} // namespace aiora
