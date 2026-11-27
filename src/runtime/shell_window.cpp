#include <jsd/game_shell.hpp>
#include <jsd/detail/shell_font.hpp>
#include <frontier/detail/gl.hpp>
#include <frontier/platform.hpp>
#include <frontier/renderer.hpp>
#include <chrono>
#include <stdexcept>
#include <string>

namespace jsd::shell {

const std::array<std::uint8_t, detail::atlasWidth * detail::atlasHeight>& detail::fontAtlas() {
    static const auto atlas=[] {
        constexpr std::uint8_t data[]{
#include "shell_font_atlas.inc"
        };
        std::array<std::uint8_t, atlasWidth * atlasHeight> pixels{};
        std::size_t source=0, destination=0;
        while (source<std::size(data)) {
            const auto packet=data[source++];
            const std::size_t count=(packet&127u)+1;
            if (packet&128u) {
                std::copy_n(data+source,count,pixels.data()+destination);
                source+=count;
            }
            destination+=count;
        }
        return pixels;
    }();
    return atlas;
}

namespace {
using namespace vl;
using Clock=std::chrono::steady_clock;
constexpr U8 windowClose=14;
constexpr int keypadEnter=88;
constexpr U32 mouseFocus=0x400;
constexpr auto viewportBinding=0x0BA2, programBinding=0x8B8D, vaoBinding=0x85B5, arrayBufferBinding=0x8894;
constexpr auto drawFramebuffer=0x8CA9, readFramebuffer=0x8CA8, drawFramebufferBinding=0x8CA6, readFramebufferBinding=0x8CAA;
constexpr auto activeTexture=0x84E0, textureBinding=0x8069, samplerBinding=0x8919;
constexpr auto colorWriteMask=0x0C23, blendSourceRgb=0x80C9, blendDestRgb=0x80C8;
constexpr auto blendSourceAlpha=0x80CB, blendDestAlpha=0x80CA;
constexpr auto blendEquationRgb=0x8009, blendEquationAlpha=0x883D, blendAdd=0x8006;
constexpr auto pixelUnpackBuffer=0x88EC, unpackBufferBinding=0x88EF;
constexpr std::array<GLenum,6> capabilities{0x0B71,0x0B44,GL_SCISSOR_TEST,0x8DB9,0x0B90,0x8C89};
using BlendEquation = void (*)(GLenum, GLenum);
using IndexedCapability = void (*)(GLenum, GLuint);

struct DrawFunctions {
    BlendEquation blendEquation{};
    IndexedCapability enable{}, disable{};
};

int integer(GL& gl, GLenum name) {
    int value=0;
    gl.GetIntegerv(name,&value);
    return value;
}

// Shader progress may interrupt the renderer with a program, framebuffer and
// textures already bound. The splash uses an independent pipeline on the backbuffer.
class DrawState {
    GL& gl;
    DrawFunctions functions;
    int viewport[4]{}, mask[4]{};
    int program, vao, buffer, draw, read, unit, texture, sampler;
    int srcRgb, dstRgb, srcAlpha, dstAlpha, opRgb, opAlpha, blend;
    std::array<int,capabilities.size()> enabled{};

  public:
    DrawState(GL& api, DrawFunctions callbacks) :
        gl(api), functions(callbacks), program(integer(gl,programBinding)), vao(integer(gl,vaoBinding)),
        buffer(integer(gl,arrayBufferBinding)), draw(integer(gl,drawFramebufferBinding)), read(integer(gl,readFramebufferBinding)),
        unit(integer(gl,activeTexture)), srcRgb(integer(gl,blendSourceRgb)), dstRgb(integer(gl,blendDestRgb)),
        srcAlpha(integer(gl,blendSourceAlpha)), dstAlpha(integer(gl,blendDestAlpha)),
        opRgb(integer(gl,blendEquationRgb)), opAlpha(integer(gl,blendEquationAlpha)), blend(integer(gl,GL_BLEND)) {
        gl.GetIntegerv(viewportBinding,viewport);
        gl.GetIntegerv(colorWriteMask,mask);
        gl.ActiveTexture(GL_TEXTURE0);
        texture=integer(gl,textureBinding);
        sampler=integer(gl,samplerBinding);
        for (std::size_t i=0;i<capabilities.size();++i)
            enabled[i]=integer(gl,capabilities[i]);
    }

    void prepare(int w, int h) {
        gl.BindFramebuffer(GL_FRAMEBUFFER,0);
        gl.Viewport(0,0,w,h);
        gl.ColorMaski(0,1,1,1,1);
        for (const auto cap : capabilities)
            gl.Disable(cap);
        functions.enable(GL_BLEND,0);
        gl.BlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        functions.blendEquation(blendAdd,blendAdd);
        gl.BindSampler(0,0);
    }

    ~DrawState() {
        gl.BindFramebuffer(drawFramebuffer,draw);
        gl.BindFramebuffer(readFramebuffer,read);
        gl.Viewport(viewport[0],viewport[1],viewport[2],viewport[3]);
        gl.ColorMaski(0,GLboolean(mask[0]),GLboolean(mask[1]),GLboolean(mask[2]),GLboolean(mask[3]));
        gl.UseProgram(program);
        gl.BindVertexArray(vao);
        gl.BindBuffer(GL_ARRAY_BUFFER,buffer);
        gl.BindSampler(0,sampler);
        gl.BindTexture(GL_TEXTURE_2D,texture);
        gl.ActiveTexture(unit);
        gl.BlendFuncSeparate(srcRgb,dstRgb,srcAlpha,dstAlpha);
        functions.blendEquation(opRgb,opAlpha);
        (blend ? functions.enable : functions.disable)(GL_BLEND,0);
        for (std::size_t i=0;i<capabilities.size();++i) {
            if (enabled[i]) gl.Enable(capabilities[i]);
            else gl.Disable(capabilities[i]);
        }
    }
};

struct Vertex {
    Point position, uv;
    std::array<std::uint8_t,4> color;
};
static_assert(sizeof(Vertex)==20);

auto pack(rf::Color color) {
    const auto byte=[](float v){return static_cast<std::uint8_t>(std::clamp(v,0.f,1.f)*255.f+.5f);};
    return std::array{byte(color.r),byte(color.g),byte(color.b),byte(color.a)};
}

bool closeEvent(const SDL_Event& event) {
    return event.type==sdl::quit || (event.type==sdl::window && event.window.event==windowClose);
}
} // namespace

struct Window::Impl final : Canvas {
    rf::Platform& platform;
    std::string title, lastPhase;
    GL gl;
    DrawFunctions drawFunctions;
    std::vector<Vertex> vertices;
    unsigned program{}, vao{}, vbo{}, atlas{};
    int viewportUniform{}, fontUniform{};
    const Clock::time_point began=Clock::now();
    Clock::time_point updated{};

    explicit Impl(rf::Platform& p, std::string_view t) : platform(p), title(t) {}
    ~Impl() override {
        if (program) gl.DeleteProgram(program);
        if (vao) gl.DeleteVertexArrays(1,&vao);
        if (vbo) gl.DeleteBuffers(1,&vbo);
        if (atlas) gl.DeleteTextures(1,&atlas);
    }

    unsigned compile(unsigned stage, const char* source) {
        const auto shader=gl.CreateShader(stage);
        gl.ShaderSource(shader,1,&source,nullptr);
        gl.CompileShader(shader);
        int ready=0;
        gl.GetShaderiv(shader,GL_COMPILE_STATUS,&ready);
        if (!ready) {
            std::array<char,1024> log{};
            gl.GetShaderInfoLog(shader,int(log.size()),nullptr,log.data());
            gl.DeleteShader(shader);
            throw std::runtime_error(std::string("Game shell shader compilation failed: ")+log.data());
        }
        return shader;
    }

    void initialize() {
        gl.load([&](const char* name){return platform.resolve(name);});
        drawFunctions.blendEquation=reinterpret_cast<BlendEquation>(platform.resolve("glBlendEquationSeparate"));
        drawFunctions.enable=reinterpret_cast<IndexedCapability>(platform.resolve("glEnablei"));
        drawFunctions.disable=reinterpret_cast<IndexedCapability>(platform.resolve("glDisablei"));
        if (!drawFunctions.blendEquation || !drawFunctions.enable || !drawFunctions.disable)
            throw std::runtime_error("Game shell requires OpenGL 3.3 core");

        const auto vs=compile(GL_VERTEX_SHADER,R"GLSL(#version 330 core
layout(location=0) in vec2 position;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 tint;
uniform vec2 viewport;
out vec2 vUV;
out vec4 vTint;
void main(){
    gl_Position=vec4(position.x/viewport.x*2.-1.,1.-position.y/viewport.y*2.,0,1);
    vUV=uv;
    vTint=tint;
})GLSL");
        unsigned fs=0;
        try {
            fs=compile(GL_FRAGMENT_SHADER,R"GLSL(#version 330 core
in vec2 vUV;
in vec4 vTint;
uniform sampler2D fontAtlas;
out vec4 color;
void main(){
    float coverage=1.;
    if(vUV.x>=0.){
        float d=texture(fontAtlas,vUV).r;
        float edge=clamp(fwidth(d)*.55,.017,.28);
        coverage=smoothstep(.49-edge,.49+edge,d);
    }
    color=vec4(vTint.rgb,vTint.a*coverage);
})GLSL");
        } catch (...) {
            gl.DeleteShader(vs);
            throw;
        }
        program=gl.CreateProgram();
        gl.AttachShader(program,vs);
        gl.AttachShader(program,fs);
        gl.LinkProgram(program);
        gl.DeleteShader(vs);
        gl.DeleteShader(fs);
        int linked=0;
        gl.GetProgramiv(program,GL_LINK_STATUS,&linked);
        if (!linked)
            throw std::runtime_error("Game shell shader linking failed");
        viewportUniform=gl.GetUniformLocation(program,"viewport");
        fontUniform=gl.GetUniformLocation(program,"fontAtlas");

        const DrawState state(gl,drawFunctions);
        gl.GenVertexArrays(1,&vao);
        gl.GenBuffers(1,&vbo);
        gl.BindVertexArray(vao);
        gl.BindBuffer(GL_ARRAY_BUFFER,vbo);
        for (unsigned i=0;i<3;++i) gl.EnableVertexAttribArray(i);
        gl.VertexAttribPointer(0,2,GL_FLOAT,0,sizeof(Vertex),nullptr);
        gl.VertexAttribPointer(1,2,GL_FLOAT,0,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,uv)));
        gl.VertexAttribPointer(2,4,GL_UNSIGNED_BYTE,1,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,color)));
        gl.GenTextures(1,&atlas);
        gl.BindTexture(GL_TEXTURE_2D,atlas);
        gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

        // A caller can create the shell while an unpack buffer is bound.
        const int unpack=integer(gl,unpackBufferBinding);
        constexpr std::array<GLenum,4> stores{0x0CF5,0x0CF2,0x0CF3,0x0CF4};
        std::array<int,stores.size()> previous{};
        for (std::size_t i=0;i<stores.size();++i) {
            previous[i]=integer(gl,stores[i]);
            gl.PixelStorei(stores[i],i==0?1:0);
        }
        gl.BindBuffer(pixelUnpackBuffer,0);
        gl.TexImage2D(GL_TEXTURE_2D,0,0x8229,detail::atlasWidth,detail::atlasHeight,0,0x1903,
                      GL_UNSIGNED_BYTE,detail::fontAtlas().data());
        gl.BindBuffer(pixelUnpackBuffer,unpack);
        for (std::size_t i=0;i<stores.size();++i)
            gl.PixelStorei(stores[i],previous[i]);
    }

    void quad(Rect r, rf::Color top, rf::Color bottom, Point uv0={-1,-1}, Point uv1={-1,-1}) {
        if (r.w<=0 || r.h<=0) return;
        const auto a=pack(top), b=pack(bottom);
        const Vertex tl{{r.x,r.y},uv0,a}, tr{{r.x+r.w,r.y},{uv1.x,uv0.y},a};
        const Vertex bl{{r.x,r.y+r.h},{uv0.x,uv1.y},b}, br{{r.x+r.w,r.y+r.h},uv1,b};
        vertices.insert(vertices.end(),{tl,tr,br,tl,br,bl});
    }

    void gradient(Rect r, rf::Color top, rf::Color bottom) override { quad(r,top,bottom); }

    void triangles(std::span<const rf::V3> positions, rf::Color color) override {
        const auto rgba=pack(color);
        for (const auto p : positions)
            vertices.push_back({{p.x,p.y},{-1,-1},rgba});
    }

    void line(Point a, Point b, rf::Color color, float width) override {
        const float dx=b.x-a.x, dy=b.y-a.y, length=std::hypot(dx,dy);
        if (length<=0 || width<=0) return;
        const float x=-dy*(width*.5f/length), y=dx*(width*.5f/length);
        const auto rgba=pack(color);
        const Vertex a0{{a.x-x,a.y-y},{-1,-1},rgba}, a1{{a.x+x,a.y+y},{-1,-1},rgba};
        const Vertex b0{{b.x-x,b.y-y},{-1,-1},rgba}, b1{{b.x+x,b.y+y},{-1,-1},rgba};
        vertices.insert(vertices.end(),{a0,b0,b1,a0,b1,a1});
    }

    void text(Point p, std::string_view text, float scale, rf::Color color) override {
        const auto start=p.x;
        for (unsigned char ch : text) {
            if (ch=='\n') {
                p={start,p.y+detail::lineAdvance*scale};
                continue;
            }
            if (ch!=' ') {
                const int glyph=(ch>=33 && ch<=126?ch:'?')-32;
                const float x=float(glyph%16*detail::glyphWidth);
                const float y=float(glyph/16*detail::glyphHeight);
                const Point uv0{(x+.5f)/detail::atlasWidth,(y+.5f)/detail::atlasHeight};
                const Point uv1{(x+detail::glyphWidth-.5f)/detail::atlasWidth,(y+detail::glyphHeight-.5f)/detail::atlasHeight};
                quad({p.x-.48f*scale,p.y-.34f*scale,6.4f*scale,8.f*scale},color,color,uv0,uv1);
            }
            p.x+=detail::glyphAdvance*scale;
        }
    }

    void circle(Point center, float radius, rf::Color color) {
        const auto rgba=pack(color);
        for (int i=0;i<64;++i) {
            const float a=2*rf::pi*i/64, b=2*rf::pi*(i+1)/64;
            vertices.push_back({center,{-1,-1},rgba});
            vertices.push_back({{center.x+std::cos(a)*radius,center.y+std::sin(a)*radius},{-1,-1},rgba});
            vertices.push_back({{center.x+std::cos(b)*radius,center.y+std::sin(b)*radius},{-1,-1},rgba});
        }
    }

    void flush(float w, float h) {
        DrawState state(gl,drawFunctions);
        state.prepare(platform.width,platform.height);
        gl.UseProgram(program);
        gl.Uniform2f(viewportUniform,w,h);
        gl.Uniform1i(fontUniform,0);
        gl.BindTexture(GL_TEXTURE_2D,atlas);
        gl.BindVertexArray(vao);
        gl.BindBuffer(GL_ARRAY_BUFFER,vbo);
        gl.BufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(vertices.size()*sizeof(Vertex)),vertices.data(),GL_STREAM_DRAW);
        gl.DrawArrays(GL_TRIANGLES,0,static_cast<int>(vertices.size()));
        platform.present();
    }
};

Window::Window(rf::Platform& platform, std::string_view title) : impl(std::make_unique<Impl>(platform,title)) {
    impl->initialize();
}
Window::~Window() = default;

void Window::showStartup(std::string_view phase, std::string_view detailText, bool force) {
    auto& p=*impl;
    const auto input=pollInput();
    if (input.quit || input.back)
        throw StartupCancelled{};
    const auto now=Clock::now();
    if (!force && phase==p.lastPhase && now-p.updated<std::chrono::milliseconds(33))
        return;
    if (p.platform.width<=0 || p.platform.height<=0)
        return;

    p.updated=now;
    p.lastPhase=phase;
    const float w=float(p.platform.width), h=float(p.platform.height);
    const float scale=std::min({w/1280.f,h/720.f,3.f}), cx=w*.5f, cy=h*.37f;
    p.vertices.clear();
    p.gradient({0,0,w,h},{.015f,.024f,.038f,1},{.035f,.055f,.075f,1});
    for (int i=0;i<90;++i) {
        const float x=float((i*193+31)%1279)/1280*w, y=float((i*317+47)%719)/720*h;
        const float size=(i%4==0?2.f:1.f)*scale;
        const rf::Color color{.42f,.54f,.61f,.65f};
        p.gradient({x,y,size,size},color,color);
    }
    p.circle({cx,cy},78*scale,{.12f,.22f,.27f,1});
    p.circle({cx-4*scale,cy-4*scale},72*scale,{.67f,.72f,.72f,1});
    for (const auto crater : std::array<rf::V3,5>{{{25,-23,13},{-24,-13,18},{8,29,16},{-33,26,8},{36,13,6}}})
        p.circle({cx+crater.x*scale,cy+crater.y*scale},crater.z*scale,{.49f,.56f,.57f,1});
    p.line({cx-105*scale,cy+35*scale},{cx+105*scale,cy-35*scale},{.35f,.85f,.79f,.8f},2*scale);
    const auto centered=[&](float y, std::string_view text, float font, rf::Color color) {
        if (text.empty()) return;
        const float width=float(text.size())*detail::glyphAdvance;
        const float size=std::min(font*scale,w*.92f/width);
        p.text({cx-width*size*.5f,y},text,size,color);
    };
    centered(cy+115*scale,p.title,4.2f,{.92f,.97f,.95f,1});
    centered(cy+169*scale,phase,2.3f,{.55f,.86f,.82f,1});
    centered(cy+201*scale,detailText,1.65f,{.57f,.65f,.71f,1});
    const float barW=380*scale, barY=cy+245*scale;
    const rf::Color track{.11f,.19f,.23f,1}, progress{.38f,.88f,.78f,1};
    p.gradient({cx-barW*.5f,barY,barW,3*scale},track,track);
    const float t=std::fmod(std::chrono::duration<float>(now-p.began).count()*.40f,1.f);
    p.gradient({cx-barW*.5f+t*(barW-70*scale),barY,70*scale,3*scale},progress,progress);
    p.flush(w,h);
}

void Window::showShaderProgress(const rf::gfx::ShaderBuildProgress& progress) {
    auto detailText=std::to_string(progress.completed)+" shaders ready";
    if (progress.caching)
        detailText+=" / "+std::to_string(progress.cached)+" cached";
    showStartup(progress.phase,detailText);
}

rf::gfx::RendererStartup Window::rendererStartup(bool cacheShaders) {
    return {cacheShaders,{},[this](const auto& progress){showShaderProgress(progress);}};
}

Input Window::pollInput() {
    auto& platform=impl->platform;
    platform.dimensions();
    Input input;
    input.pointer={-1,-1};
    if (!platform.window)
        return input;

    int x=0, y=0, width=0, height=0;
    SDL_GetWindowSize(platform.window,&width,&height);
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (closeEvent(event)) input.quit=true;
        if (event.type==sdl::keyDown && !event.key.repeat) {
            const auto key=event.key.keysym.scancode;
            input.select|=key==sdl::key::enter || key==keypadEnter;
            input.up|=key==sdl::key::up;
            input.down|=key==sdl::key::down;
            input.back|=key==sdl::key::esc;
        }
        if (event.type==sdl::mouseDown && event.button.button==sdl::buttonLeft) {
            input.click=true;
            x=event.button.x;
            y=event.button.y;
        }
    }
    if (!input.click)
        SDL_GetMouseState(&x,&y);
    if ((input.click || (SDL_GetWindowFlags(platform.window)&mouseFocus)) && width>0 && height>0)
        input.pointer={float(x)*float(platform.width)/float(width),float(y)*float(platform.height)/float(height)};
    return input;
}

std::optional<std::size_t> Window::present(Menu& menu, const Page& page, const Input& input, const Theme& theme) {
    auto& p=*impl;
    p.platform.dimensions();
    if (p.platform.width<=0 || p.platform.height<=0)
        return {};
    const float scale=std::clamp(std::min(p.platform.width/1280.f,p.platform.height/720.f),.65f,3.f);
    const float w=p.platform.width/scale, h=p.platform.height/scale;
    auto logical=input;
    logical.pointer={input.pointer.x/scale,input.pointer.y/scale};
    p.vertices.clear();
    auto selected=menu.draw(p,w,h,page,logical,theme);
    p.flush(w,h);
    return selected;
}

} // namespace jsd::shell
