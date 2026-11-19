#pragma once
#include "frontier/jobs.hpp"
#include "frontier/renderer.hpp"
#include "gl.hpp"
#include <future>
#include <map>
#include <optional>
#include <tuple>

namespace rf::gfx {
struct GpuMesh {
    unsigned vao = 0, vbo = 0;
    int count = 0;
    V3 center;
    float radius = 0;
};
struct GpuTexture {
    unsigned object = 0;
    int width = 0, height = 0, uploadedWidth = 0, uploadedHeight = 0;
    bool srgb = true;
    std::vector<unsigned char> pixels;
};
struct GpuInstance {
    Mat4 model = Mat4::identity(), previous = Mat4::identity();
    Color color;
};
struct UiVertex {
    float x, y;
    Color c;
};
struct TargetBuffer {
    unsigned fbo = 0, texture = 0;
};
struct TileData {
    std::vector<Vertex> vertices;
    std::vector<float> heights;
    int divisions = 8;
};
struct Tile {
    GpuMesh mesh;
    unsigned heightMap = 0;
    int divisions = 8;
};
using TileKey = std::pair<int, int>;
struct PendingTile {
    std::future<TileData> future;
    int divisions = 8;
};

struct Renderer::Impl {
    Context context;
    vl::GL gl;
    Jobs jobs;
    Capabilities caps;
    SettingsReport report;
    Statistics stats;
    std::map<unsigned, GpuMesh> meshes;
    std::map<unsigned, GpuTexture> textures;
    unsigned nextMesh = 1, nextTexture = 1;
    std::array<MeshId, 3> primitives{};
    unsigned sceneProgram = 0, shadowProgram = 0, tessProgram = 0, tessShadowProgram = 0;
    unsigned aoProgram = 0, aoBlurProgram = 0, lightingProgram = 0, dofProgram = 0, tonemapProgram = 0,
             copyProgram = 0, fxaaProgram = 0, uiProgram = 0;
    unsigned smaaEdgeProgram = 0, smaaWeightProgram = 0, smaaBlendProgram = 0, smaaArea = 0, smaaSearch = 0;
    unsigned instanceBuffer = 0, fullscreenVao = 0, uiVao = 0, uiVbo = 0;
    unsigned sceneFbo = 0, sceneColor = 0, sceneNormal = 0, sceneSurface = 0, sceneDepth = 0, msaaFbo = 0;
    std::array<unsigned, 5> msaaBuffers{};
    unsigned shadowFbo = 0, shadowDepth = 0;
    int shadowSize = 0;
    TargetBuffer aoRaw, aoA, aoB, lit, dof, display, edges, weights;
    int width = 0, height = 0, allocatedSamples = 0;
    int outputWidth = 0, outputHeight = 0;
    bool buffersDirty = true;
    Camera camera;
    Environment environment;
    Mat4 view, projection, vp, invProjection, lightVP;
    Mat4 previousVP = Mat4::identity(), previousView = Mat4::identity();
    std::map<uint64_t, Mat4> previousModels;
    std::vector<Light> lights;
    unsigned sceneMotion = 0, transparentFbo = 0;
    unsigned skyProgram = 0, fogProgram = 0, motionProgram = 0, reactiveProgram = 0, taaProgram = 0,
             bloomProgram = 0, bloomBlurProgram = 0, exposureProgram = 0, exposureAdaptProgram = 0,
             upscaleProgram = 0;
    unsigned iblProgram = 0, brdfProgram = 0, iblFbo = 0;
    unsigned lightDataBuffer = 0, lightGridBuffer = 0, lightIndexBuffer = 0, lightDataTexture = 0,
             lightGridTexture = 0, lightIndexTexture = 0;
    int lightGridWidth = 0;
    struct EnvMap {
        unsigned specular = 0, irradiance = 0, brdf = 0, source = 0;
    };
    std::map<unsigned, EnvMap> environments;
    unsigned nextEnvironment = 1;
    std::array<TargetBuffer, 2> temporal, exposure;
    TargetBuffer luminance;
    TargetBuffer fogTarget, motionTarget;
    std::vector<TargetBuffer> bloomPyramid, bloomScratch;
    unsigned temporalIndex = 0, frameIndex = 0, exposureIndex = 0;
    bool historyValid = false;
    V3 lastEye{}, lastDirection{0, 0, 1};
    std::array<unsigned, 3> gpuQueries{};
    std::array<bool, 3> queryActive{};
    unsigned queryIndex = 0;
    unsigned particleProgram = 0, particleVao = 0, particleVbo = 0, noiseVolume = 0;
    unsigned particleDepth = 0, particleDepthFbo = 0, imageProgram = 0;
    std::vector<ParticleSprite> particleCommands;
    struct UiImage {
        TextureId texture;
        float x, y, w, h;
        Color tint;
    };
    std::vector<UiImage> uiImages;
    void initializeCinematic();
    void releaseCinematic();
    void drawParticles();
    void drawImages();
    std::vector<Draw> commands;
    std::vector<UiVertex> ui;
    Terrain terrainSpec;
    bool terrainRequested = false, rendered = false;
    unsigned terrainSeed = ~0u, terrainRevision = ~0u;
    Quality terrainQuality = Quality::Off;
    bool terrainHardware = false;
    std::map<TileKey, Tile> tiles;
    std::map<TileKey, PendingTile> pending;
    std::string messages;
    explicit Impl(Context, GraphicsSettings);
    ~Impl();
    const GraphicsSettings &settings() const { return report.effective; }
    unsigned program(const std::string &vs, const std::string &fs, const char *tcs = nullptr,
                     const char *tes = nullptr);
    unsigned texture(int w, int h, unsigned internal, unsigned format, unsigned type,
                     const void *data = nullptr);
    TargetBuffer target(unsigned internal = vl::GL_RGBA16F, unsigned format = vl::GL_RGBA, int w = 0,
                        int h = 0);
    void destroy(TargetBuffer &);
    void destroy(GpuMesh &);
    void destroy(Tile &);
    GpuMesh upload(std::span<const Vertex>);
    void upload(GpuTexture &);
    void allocate();
    void releaseTargets();
    void sampler(unsigned program, const char *name, unsigned tex, int unit);
    void matrix(unsigned program, const char *name, const Mat4 &);
    void scalar(unsigned program, const char *name, float);
    void integer(unsigned program, const char *name, int);
    void vector(unsigned program, const char *name, V3);
    void common(unsigned program, const Mat4 &matrix);
    void material(unsigned program, const Material &);
    void mesh(const GpuMesh &, std::span<const GpuInstance>, bool patches = false);
    void fullscreen(unsigned program, unsigned fbo);
    void drawCommands(unsigned program, const Mat4 &matrix, bool shadow, bool transparent = false);
    bool visible(const GpuMesh &, const Transform &, const Mat4 &, const Mat4 *model = nullptr) const;
    Mat4 model(const Transform &) const;
    void initializeAdvanced();
    void releaseAdvanced();
    void allocateAdvanced();
    void uploadLights();
    void bindAdvanced(unsigned program);
    unsigned postAdvanced(unsigned source);
    void temporalResolve(unsigned source);
    void bind(unsigned program, const char *name, unsigned texture, int unit, unsigned target);
    void streamTerrain();
    void drawTerrain(bool shadow);
    void pipeline();
    void drawUi();
    void initializeSmaa();
    unsigned antialias(unsigned texture);
};
} // namespace rf::gfx
