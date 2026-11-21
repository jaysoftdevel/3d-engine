#pragma once
#include "project_runtime.hpp"
namespace jsd::project {
class ProjectCapture:public gfx::SceneObserver {
 struct Impl;std::unique_ptr<Impl>impl;
public:
 struct Part {gfx::MeshId mesh;gfx::Material material;Mat4 local;std::string name;};
 Document document;
 explicit ProjectCapture(const std::filesystem::path&);
 ~ProjectCapture();
 void meshCreated(gfx::MeshId,std::span<const gfx::Vertex>)override;
 void textureCreated(gfx::TextureId,int,int,std::span<const unsigned char>,bool)override;
 void submitted(const gfx::Draw&)override;void lightSubmitted(const gfx::Light&)override;
 void parts(gfx::MeshId,std::vector<Part>);void frame(float);void endFrame();
 Id sourceObject(uint64_t)const;
 std::string meshRef(gfx::MeshId)const;std::string texturePath(gfx::TextureId)const;
 unsigned material(const gfx::Material&);
 std::string copyAsset(const std::filesystem::path&);
 void finish();
};
}
