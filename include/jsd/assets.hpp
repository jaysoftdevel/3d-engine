#pragma once
#include "runtime.hpp"
namespace jsd {
struct Image {
    int width = 0, height = 0, channels = 4;
    std::vector<unsigned char> rgba;
};
struct HdrImage {
    int width = 0, height = 0;
    std::vector<float> rgb;
};
Image loadImage(const std::filesystem::path &);
HdrImage loadHdr(const std::filesystem::path &);
struct SkinWeight {
    std::array<unsigned, 4> joints{};
    std::array<float, 4> weights{1, 0, 0, 0};
};
struct AssetMaterial {
    gfx::Material material;
    int albedo = -1, normal = -1, orm = -1, emissive = -1;
};
struct MeshAsset {
    std::vector<gfx::Vertex> vertices;
    std::vector<SkinWeight> weights;
    int material = -1;
};
struct AssetNode {
    int parent = -1, skin = -1;
    std::vector<int> meshes;
    V3 translation{}, scale{1, 1, 1};
    Quat rotation{};
    Mat4 local = Mat4::identity();
    bool matrixTransform = false;
};
struct AssetSkin {
    std::vector<int> joints;
    std::vector<Mat4> inverseBind;
};
enum class TrackProperty { Translation, Rotation, Scale };
enum class Interpolation { Linear, Step, Cubic };
struct AnimationTrack {
    int node = -1;
    TrackProperty property;
    Interpolation interpolation;
    std::vector<float> times;
    std::vector<std::array<float, 4>> values;
};
struct AnimationClip {
    std::string name;
    float duration = 0;
    std::vector<AnimationTrack> tracks;
};
struct SceneAsset {
    std::vector<MeshAsset> meshes;
    std::vector<Image> images;
    std::vector<AssetMaterial> materials;
    std::vector<AssetNode> nodes;
    std::vector<AssetSkin> skins;
    std::vector<AnimationClip> animations;
};
SceneAsset loadGltf(const std::filesystem::path &, const std::filesystem::path &allowedRoot = {});
std::vector<Mat4> sampleAnimation(const SceneAsset &, int clip, float seconds, bool loop = true);
std::vector<gfx::Vertex> skinMesh(const MeshAsset &, std::span<const Mat4> palette);
class GpuScene {
    SceneAsset asset;
    std::vector<gfx::MeshId> meshes;
    std::vector<gfx::TextureId> textures;
    std::vector<gfx::Material> materials;
    gfx::Renderer *renderer = nullptr;
    struct Record {
        int node, mesh;
        gfx::MeshId gpu;
    };
    std::vector<Record> records;

  public:
    GpuScene(gfx::Renderer &, SceneAsset);
    ~GpuScene();
    GpuScene(const GpuScene &) = delete;
    GpuScene &operator=(const GpuScene &) = delete;
    void draw(float seconds = 0, int animation = -1, const Mat4 &root = Mat4::identity(),
              uint64_t idBase = 10000);
    const SceneAsset &source() const { return asset; }
};
} // namespace jsd
