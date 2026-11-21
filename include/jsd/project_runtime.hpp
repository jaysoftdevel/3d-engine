#pragma once
#include "project.hpp"
#include "project_simulation.hpp"
namespace jsd::project {
struct RenderOptions {bool geometry=true,terrain=true,particles=true,overlays=true,audio=false;int subdivisionCap=3,style=0;std::function<void()> beforeEnd;};
class ProjectRenderer {
 struct Impl;std::unique_ptr<Impl> impl;
public:
 explicit ProjectRenderer(gfx::Renderer&);~ProjectRenderer();
 void render(const Document&,const gfx::Camera&,float seconds,float delta,const std::map<Id,Mat4>&,const RenderOptions &options={});
 gfx::Material material(const Surface&);void reloadTextures();void invalidate();
 std::string warning()const;
};
gfx::Camera camera(const Document&,float seconds);
Json emitterJson(const VfxSettings&);VfxSettings emitterSettings(const Json&);
float scalarKeys(const Json&,float,float fallback);V3 vectorKeys(const Json&,float,V3 fallback);
}
