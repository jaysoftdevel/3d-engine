#pragma once
#include "frontier/jobs.hpp"
#include "frontier/renderer.hpp"
#include "gl.hpp"
#include <future>
#include <map>
#include <optional>
#include <tuple>
#include <unordered_map>

namespace rf::gfx {
struct GpuMesh {
    unsigned vao = 0, vbo = 0, ibo = 0;
    int count = 0;
    unsigned uniqueVertices = 0;
    V3 center;
    float radius = 0;
};
struct GpuTexture {
    unsigned object = 0;
    int width = 0, height = 0, uploadedWidth = 0, uploadedHeight = 0;
    bool srgb = true, screenImage = false, averageReady = false;
    std::array<float,4> average{1,1,1,1};
    std::vector<unsigned char> pixels;
};
struct GpuInstance {
    Mat4 model = Mat4::identity(), previous = Mat4::identity();
    Color color;
    // Inverse-transpose computed once per instance rather than once per vertex.
    std::array<float, 12> normal{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    GpuInstance() = default;
    GpuInstance(const Mat4 &m, const Mat4 &old, Color c) : model(m), previous(old), color(c) {
        V3 x{m.m[0], m.m[1], m.m[2]}, y{m.m[4], m.m[5], m.m[6]}, z{m.m[8], m.m[9], m.m[10]};
        V3 a = cross(y, z), b = cross(z, x), d = cross(x, y);
        float determinant = dot(x, a);
        if (std::isfinite(determinant) && std::abs(determinant) > std::numeric_limits<float>::min()) {
            a = a / determinant;
            b = b / determinant;
            d = d / determinant;
            normal = {a.x, a.y, a.z, 0, b.x, b.y, b.z, 0, d.x, d.y, d.z, 0};
        }
    }
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
    SceneObserver *observer=nullptr;
    unsigned outputFramebuffer() const { return context.outputFramebuffer ? context.outputFramebuffer() : 0; }
    vl::GL gl;
    Jobs jobs;
    Capabilities caps;
    SettingsReport report;
    Statistics stats;
    std::map<unsigned, GpuMesh> meshes;
    std::map<unsigned, GpuTexture> textures;
    unsigned nextMesh = 1, nextTexture = 1;
    std::array<MeshId, 3> primitives{};
    unsigned minimalProgram = 0;
    bool compactScene = false;
    unsigned sceneProgram = 0, shadowProgram = 0, tessProgram = 0, tessShadowProgram = 0;
    unsigned aoProgram = 0, aoBlurProgram = 0, lightingProgram = 0, dofProgram = 0, tonemapProgram = 0,
             copyProgram = 0, fxaaProgram = 0, uiProgram = 0;
    unsigned smaaEdgeProgram = 0, smaaWeightProgram = 0, smaaBlendProgram = 0, smaaArea = 0, smaaSearch = 0;
    unsigned instanceBuffer = 0, fullscreenVao = 0, uiVao = 0, uiVbo = 0, shadowSampler = 0;
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
    std::unordered_map<uint64_t, Mat4> previousModels;
    struct StringHash {
        using is_transparent = void;
        size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
    };
    struct UniformState {
        int location = -1;
        unsigned type = 0;
        std::array<unsigned char, 16> bytes{};
        std::unique_ptr<std::array<unsigned char, 64>> matrix;
    };
    std::unordered_map<unsigned, std::unordered_map<std::string, UniformState, StringHash, std::equal_to<>>> uniforms;
    UniformState &uniformState(unsigned, std::string_view);
    bool updateUniform(unsigned, std::string_view, unsigned, const void *, size_t, int &);
    int uniform(unsigned program, std::string_view name);
    std::vector<Light> lights;
    bool physicalLighting = false;
    unsigned sceneMotion = 0, transparentFbo = 0;
    unsigned skyProgram = 0, fogProgram = 0, motionProgram = 0, reactiveProgram = 0, taaProgram = 0,
             bloomProgram = 0, bloomBlurProgram = 0, exposureProgram = 0, exposureAdaptProgram = 0,
             upscaleProgram = 0;
    unsigned iblProgram = 0, brdfProgram = 0, iblFbo = 0;
    unsigned lightDataBuffer = 0, lightGridBuffer = 0, lightIndexBuffer = 0, lightDataTexture = 0,
             lightGridTexture = 0, lightIndexTexture = 0;
    int lightGridWidth = 0;
    struct LightRect {
        unsigned light;
        int x0, y0, x1, y1;
    };
    std::vector<LightRect> lightRects;
    std::vector<std::array<float, 4>> lightUpload;
    std::vector<std::array<unsigned, 2>> lightRanges;
    std::vector<unsigned> lightIndices;
    struct EnvMap {
        unsigned specular = 0, irradiance = 0, brdf = 0, source = 0;
    };
    std::map<unsigned, EnvMap> environments;
    unsigned nextEnvironment = 1;
    std::array<TargetBuffer, 2> temporal, exposure;
    std::array<TargetBuffer, 2> focus;
    unsigned focusProgram = 0, focusIndex = 0;
    bool focusValid = false;
    TargetBuffer luminance;
    TargetBuffer fogTarget, motionTarget;
    std::vector<TargetBuffer> bloomPyramid, bloomScratch;
    unsigned temporalIndex = 0, frameIndex = 0, exposureIndex = 0;
    bool historyValid = false;
    V3 lastEye{}, lastDirection{0, 0, 1};
    std::array<unsigned, 3> gpuQueries{};
    std::array<bool, 3> queryActive{};
    unsigned queryIndex = 0;
    double lastGpuMilliseconds = 0;
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
    using BatchKey = std::array<unsigned, 23>;
    struct PreparedDraw {
        const Draw *source;
        const GpuMesh *mesh;
        GpuInstance instance;
        BatchKey key;
        V3 center;
        float radius, distanceSquared, sortDistanceSquared;
    };
    std::vector<PreparedDraw> prepared;
    std::vector<size_t> opaqueOrder, transparentOrder;
    std::vector<GpuInstance> instanceScratch;
    std::vector<UiVertex> ui;
    Terrain terrainSpec;
    bool terrainRequested = false, rendered = false;
    unsigned terrainSeed = ~0u, terrainRevision = ~0u;
    Quality terrainQuality = Quality::Off;
    bool terrainHardware = false;
    int terrainTileSize = 32;
    std::map<TileKey, Tile> tiles;
    std::map<TileKey, PendingTile> pending;
    std::string messages;
    explicit Impl(Context, GraphicsSettings);
    ~Impl();
    const GraphicsSettings &settings() const { return report.effective; }
    bool minimalFrame()const{const auto&s=settings();return !customMaterialShader&&s.textureQuality==TextureQuality::Lowest&&s.antiAliasing==AntiAliasing::Off&&s.shadows==Quality::Off&&s.ambientOcclusion==Quality::Off&&s.depthOfField==Quality::Off&&s.globalIllumination==GlobalIllumination::Off&&s.reflections==Quality::Off&&s.edgeFusion==Quality::Off&&s.terrainTessellation==Quality::Off&&s.bloom==Quality::Off&&!s.autoExposure&&s.motionBlur==Quality::Off&&s.volumetricFog==Quality::Off;}
    unsigned surfaceProgram()const{return compactScene?minimalProgram:sceneProgram;}
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
    void drawCommands(unsigned program, const Mat4 &matrix, bool shadow, bool transparent = false,
                      bool depthOnly = false);
    void prepareCommands();
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
    void drawTerrain(bool shadow, bool depthOnly = false);
    void projectionTerms(unsigned program, bool xy = true);
    bool depthPrepassed = false, countDraws = true, customMaterialShader = false;
    void pipeline();
    void drawUi();
    void initializeSmaa();
    unsigned antialias(unsigned texture);
};
} // namespace rf::gfx
