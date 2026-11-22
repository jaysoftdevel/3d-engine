#include "advanced_shaders.hpp"
#include "internal.hpp"
#include "shaders.hpp"
#include "fog_shader.hpp"
#include <stdexcept>
#include <numeric>
namespace rf::gfx {
using namespace vl;
void Renderer::Impl::initializeAdvanced() {
    focusProgram = program(shader::fullscreen, R"GLSL(#version 330 core
in vec2 texcoord;out vec4 outputColor;
uniform sampler2D depthMap,previous;
uniform vec2 pixel,depthTerms;
uniform float farDistance,dt,speed;
uniform int valid;
void main(){
    float values[9];int index=0;
    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
        float d=texture(depthMap,vec2(.5)+vec2(x,y)*pixel*3.).r;
        values[index++]=d>=.999999?farDistance:clamp(depthTerms.y/(d*2.-1.+depthTerms.x),.2,farDistance);
    }
    for(int i=1;i<9;i++)for(int j=i;j>0;j--)
        if(values[j]<values[j-1]){float temp=values[j];values[j]=values[j-1];values[j-1]=temp;}
    float target=values[4],old=texture(previous,vec2(.5)).r;
    float focus=target;
    if(valid!=0){
        float blend=1.-exp(-speed*dt);
        focus=abs(target-old)<max(.01,old*.002)?old:mix(old,target,blend);
    }
    outputColor=vec4(focus,0,0,1);
})GLSL");
    skyProgram = program(shader::fullscreen, advanced::sky);
    fogProgram = program(shader::fullscreen, lighting::volumeFog);
    fogCompositeProgram=program(shader::fullscreen,lighting::fogComposite);
    motionProgram = program(shader::fullscreen, advanced::motionBlur);
    taaProgram = program(shader::fullscreen, advanced::taa);
    upscaleProgram = program(shader::fullscreen, advanced::upscale);
    bloomProgram = program(shader::fullscreen, advanced::bloom);
    bloomBlurProgram = program(shader::fullscreen, advanced::blur);
    exposureProgram = program(shader::fullscreen, advanced::luminance);
    exposureAdaptProgram = program(shader::fullscreen, advanced::exposure);
    iblProgram = program(shader::fullscreen, advanced::environment);
    brdfProgram = program(shader::fullscreen, advanced::brdf);
    reactiveProgram = program(shader::meshVertex, R"GLSL(#version 330 core
in vec2 texcoord;in vec4 tint;uniform sampler2D albedoMap;uniform int textured;layout(location=2)out vec4 reactive;void main(){float alpha=tint.a*(textured!=0?texture(albedoMap,texcoord).a:1.);if(alpha<.001)discard;reactive=vec4(0,0,0,1);})GLSL");
    gl.GenQueries(3, gpuQueries.data());
    gl.GenFramebuffers(1, &iblFbo);
    gl.GenBuffers(1, &lightDataBuffer);
    gl.GenBuffers(1, &lightGridBuffer);
    gl.GenBuffers(1, &lightIndexBuffer);
    gl.GenTextures(1, &lightDataTexture);
    gl.GenTextures(1, &lightGridTexture);
    gl.GenTextures(1, &lightIndexTexture);
}
void Renderer::Impl::releaseAdvanced() {
    for (unsigned p :
         {fogCompositeProgram,focusProgram, skyProgram, fogProgram, motionProgram, reactiveProgram, taaProgram, upscaleProgram, bloomProgram,
          bloomBlurProgram, exposureProgram, exposureAdaptProgram, iblProgram, brdfProgram})
        if (p)
            gl.DeleteProgram(p);
    gl.DeleteQueries(3, gpuQueries.data());
    gl.DeleteFramebuffers(1, &iblFbo);
    for (unsigned b : {lightDataBuffer, lightGridBuffer, lightIndexBuffer})
        gl.DeleteBuffers(1, &b);
    for (unsigned t : {lightDataTexture, lightGridTexture, lightIndexTexture})
        gl.DeleteTextures(1, &t);
    for (auto &[id, e] : environments) {
        (void)id;
        for (auto t : {e.specular, e.irradiance, e.brdf, e.source})
            gl.DeleteTextures(1, &t);
    }
}
void Renderer::Impl::allocateAdvanced() {
    if(settings().stochasticLighting)for(auto &t:directHistory)t=target();
    if (settings().depthOfField != Quality::Off)
        for (auto &buffer : focus) buffer = target(0x822E, 0x1903, 1, 1);
    focusValid = false;
    if (settings().volumetricFog != Quality::Off){
        fogTarget = target();fogScattering=target(GL_RGBA16F,GL_RGBA,std::max(1,width/2),std::max(1,height/2));
    }
    if (settings().motionBlur != Quality::Off)
        motionTarget = target();
    if (settings().antiAliasing == AntiAliasing::TAA)
        for (auto &t : temporal) {
            t = target(GL_RGBA16F, GL_RGBA, outputWidth, outputHeight);
            gl.BindFramebuffer(GL_FRAMEBUFFER, t.fbo);
            gl.ClearColor(0, 0, 0, 0);
            gl.Clear(GL_COLOR_BUFFER_BIT);
        }
    for (auto &t : exposure) {
        t = target(GL_RGBA16F, GL_RGBA, 1, 1);
        gl.BindFramebuffer(GL_FRAMEBUFFER, t.fbo);
        gl.ClearColor(1, 0, 0, 1);
        gl.Clear(GL_COLOR_BUFFER_BIT);
    }
    luminance = target(GL_RGBA16F, GL_RGBA, 128, 128);
    if (settings().bloom != Quality::Off) {
        int w = std::max(1, width / 2), h = std::max(1, height / 2);
        int levels = 2 + int(settings().bloom);
        for (int i = 0; i < levels; i++) {
            bloomPyramid.push_back(target(GL_RGBA16F, GL_RGBA, w, h));
            bloomScratch.push_back(target(GL_RGBA16F, GL_RGBA, w, h));
            if (w <= 8 || h <= 8)
                break;
            w = std::max(1, w / 2);
            h = std::max(1, h / 2);
        }
    }
}
void Renderer::Impl::bind(unsigned p, const char *name, unsigned texture, int unit, unsigned target) {
    int location;
    bool changed = updateUniform(p, name, 3, &unit, sizeof(unit), location);
    if (location < 0) return;
    gl.ActiveTexture(GL_TEXTURE0 + unit);
    gl.BindTexture(target, texture);
    if (changed) gl.Uniform1i(location, unit);
}
void Renderer::Impl::bindAdvanced(unsigned p) {
    scalar(p, "environmentRotation", environment.imageRotation);
    bind(p, "materialNoise", noiseVolume, 12, 0x806F);
    bind(p, "lightData", lightDataTexture, 6, 0x8C2A);
    bind(p, "lightGrid", lightGridTexture, 7, 0x8C2A);
    bind(p, "lightIndices", lightIndexTexture, 8, 0x8C2A);
    integer(p, "localLightCount", lights.size());
    integer(p, "lightGridWidth", lightGridWidth);
    auto env = environments.find(environment.image.value);
    bool active = env != environments.end();
    integer(p, "iblEnabled", active);
    bind(p, "environmentMap", active ? env->second.specular : 0, 9, 0x8513);
    bind(p, "irradianceMap", active ? env->second.irradiance : 0, 10, 0x8513);
    sampler(p, "brdfMap", active ? env->second.brdf : 0, 11);
}
unsigned Renderer::Impl::postAdvanced(unsigned source) {
    if (fogTarget.fbo) {
        gl.UseProgram(fogProgram);
        sampler(fogProgram, "depthMap", sceneDepth, 1);
        sampler(fogProgram, "shadowMap", shadowDepth, 2);
        matrix(fogProgram, "invVP", inverse(vp));
        for(int c=0;c<4;c++)matrix(fogProgram,("shadowMatrices["+std::to_string(c)+"]").c_str(),shadowMatrices[c]);
        vector(fogProgram, "eye", camera.position);vector(fogProgram, "sun", environment.sunDirection);
        vector(fogProgram, "sunColor",{environment.sunColor.r,environment.sunColor.g,environment.sunColor.b});
        vector(fogProgram,"ambientColor",{environment.skyColor.r*environment.ambientIntensity*.35f,environment.skyColor.g*environment.ambientIntensity*.35f,environment.skyColor.b*environment.ambientIntensity*.35f});
        scalar(fogProgram,"density",settings().fogDensity);scalar(fogProgram,"falloff",settings().fogHeightFalloff);
        scalar(fogProgram,"intensity",environment.sunIntensity);scalar(fogProgram,"time",environment.atmosphere.time);
        integer(fogProgram,"steps",std::array{0,16,24,40,64}[int(settings().volumetricFog)]);
        integer(fogProgram,"shadows",shadowDepth!=0);integer(fogProgram,"volumeCount",int(fogVolumes.size()));stats.fogVolumes=fogVolumes.size();
        auto vec4=[&](const char*name,int j,float x,float y,float z,float w){auto key=std::string(name)+"["+std::to_string(j)+"]";gl.Uniform4f(uniform(fogProgram,key.c_str()),x,y,z,w);};
        for(size_t j=0;j<fogVolumes.size();j++){auto&v=fogVolumes[j];
            vec4("volumePosition",j,v.position.x,v.position.y,v.position.z,0);
            vec4("volumeExtent",j,v.halfExtent.x,v.halfExtent.y,v.halfExtent.z,v.density);
            vec4("volumeColor",j,v.albedo.r,v.albedo.g,v.albedo.b,v.anisotropy);
            vec4("volumeEmission",j,v.emission.r,v.emission.g,v.emission.b,v.noiseScale);
            vec4("volumeVelocity",j,v.velocity.x,v.velocity.y,v.velocity.z,0);
        }
        std::vector<size_t> order(lights.size());std::iota(order.begin(),order.end(),0);
        std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){auto weight=[&](size_t j){auto&l=lights[j];return l.intensity*(l.color.r+l.color.g+l.color.b)/(1+dot(l.position-camera.position,l.position-camera.position));};return weight(a)>weight(b);});
        int count=std::min(size_t(12),order.size());integer(fogProgram,"lightCount",count);
        for(int j=0;j<4;j++)matrix(fogProgram,("localShadowMatrices["+std::to_string(j)+"]").c_str(),localShadows[j].matrix);
        for(int j=0;j<count;j++){auto&l=lights[order[j]];auto d=normalized(l.direction);
            int slot=-1;for(int k=0;k<4;k++)if(localShadows[k].ready&&localShadows[k].light==int(order[j]))slot=k;
            integer(fogProgram,("fogLightShadow["+std::to_string(j)+"]").c_str(),slot);
            vec4("fogLightPosition",j,l.position.x,l.position.y,l.position.z,l.radius);
            vec4("fogLightColor",j,l.color.r,l.color.g,l.color.b,l.intensity);
            vec4("fogLightDirection",j,d.x,d.y,d.z,std::cos(l.outerCone));
            vec4("fogLightCone",j,std::cos(l.innerCone),float(l.type==LightType::Spot),float(l.falloff==LightFalloff::InverseSquare),l.sourceRadiusMetres);
        }
        gl.BindFramebuffer(GL_FRAMEBUFFER,fogScattering.fbo);gl.Viewport(0,0,std::max(1,width/2),std::max(1,height/2));
        gl.BindVertexArray(fullscreenVao);gl.DrawArrays(GL_TRIANGLES,0,3);stats.drawCalls++;stats.postPasses++;
        gl.Viewport(0,0,width,height);gl.UseProgram(fogCompositeProgram);
        sampler(fogCompositeProgram,"source",source,0);sampler(fogCompositeProgram,"scattering",fogScattering.texture,1);sampler(fogCompositeProgram,"depthMap",sceneDepth,2);
        projectionTerms(fogCompositeProgram,false);fullscreen(fogCompositeProgram,fogTarget.fbo);
        source = fogTarget.texture;
    }
    if (motionTarget.fbo) {
        gl.UseProgram(motionProgram);
        sampler(motionProgram, "source", source, 0);
        sampler(motionProgram, "motionMap", sceneMotion, 1);
        sampler(motionProgram, "depthMap", sceneDepth, 2);
        sampler(motionProgram, "surfaceMap", sceneSurface, 3);
        integer(motionProgram, "samples", std::array{0, 5, 9, 13, 17}[int(settings().motionBlur)]);
        scalar(motionProgram, "strength", camera.optics ? std::clamp(camera.optics->shutterSeconds / std::max(.0001f, environment.deltaSeconds), 0.f, 2.f) : settings().motionBlurStrength);
        gl.Uniform2f(uniform(motionProgram, "size"), width, height);
        fullscreen(motionProgram, motionTarget.fbo);
        source = motionTarget.texture;
    }

    if ((settings().autoExposure && !(camera.optics && camera.optics->physicalExposure))||settings().localExposure>0) {
        gl.UseProgram(exposureProgram);
        sampler(exposureProgram, "source", source, 0);
        sampler(exposureProgram,"depthMap",sceneDepth,1);
        gl.BindFramebuffer(GL_FRAMEBUFFER, luminance.fbo);
        gl.Viewport(0, 0, 128, 128);
        gl.BindVertexArray(fullscreenVao);
        gl.DrawArrays(GL_TRIANGLES, 0, 3);
        stats.drawCalls++;
        stats.postPasses++;
        gl.BindTexture(GL_TEXTURE_2D, luminance.texture);
        gl.GenerateMipmap(GL_TEXTURE_2D);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 0x2703);
        unsigned next = 1 - exposureIndex;
        gl.UseProgram(exposureAdaptProgram);
        sampler(exposureAdaptProgram, "source", luminance.texture, 0);
        sampler(exposureAdaptProgram, "previous", exposure[exposureIndex].texture, 1);
        integer(exposureAdaptProgram, "valid", historyValid);
        scalar(exposureAdaptProgram, "level", 7);
        scalar(exposureAdaptProgram, "dt", std::clamp(environment.deltaSeconds, .0001f, .25f));
        scalar(exposureAdaptProgram, "speed", settings().exposureAdaptation);
        scalar(exposureAdaptProgram,"minEV",settings().minExposureEV);scalar(exposureAdaptProgram,"maxEV",settings().maxExposureEV);
        gl.BindFramebuffer(GL_FRAMEBUFFER, exposure[next].fbo);
        gl.Viewport(0, 0, 1, 1);
        gl.BindVertexArray(fullscreenVao);
        gl.DrawArrays(GL_TRIANGLES, 0, 3);
        stats.drawCalls++;
        stats.postPasses++;
        exposureIndex = next;
    }
    unsigned previous = source;
    for (size_t i = 0; i < bloomPyramid.size(); i++) {
        int w = std::max(1, width / (2 << i)), h = std::max(1, height / (2 << i));
        gl.UseProgram(bloomProgram);
        sampler(bloomProgram, "source", previous, 0);
        scalar(bloomProgram, "threshold", settings().bloomThreshold);
        sampler(bloomProgram,"exposureMap",exposure[exposureIndex].texture,1);
        scalar(bloomProgram,"exposureEV",settings().exposureCompensation+(camera.optics&&camera.optics->physicalExposure?float(std::log2(camera.optics->exposureMultiplier())):0));
        integer(bloomProgram,"autoExposure",settings().autoExposure&&!(camera.optics&&camera.optics->physicalExposure));
        integer(bloomProgram, "first", i == 0);
        gl.BindFramebuffer(GL_FRAMEBUFFER, bloomPyramid[i].fbo);
        gl.Viewport(0, 0, w, h);
        gl.BindVertexArray(fullscreenVao);
        gl.DrawArrays(GL_TRIANGLES, 0, 3);
        stats.drawCalls++;
        stats.postPasses++;
        previous = bloomPyramid[i].texture;
    }
    for (int i = int(bloomPyramid.size()) - 1; i >= 0; i--) {
        int w = std::max(1, width / (2 << i)), h = std::max(1, height / (2 << i));
        gl.UseProgram(bloomBlurProgram);
        sampler(bloomBlurProgram, "source", bloomPyramid[i].texture, 0);
        sampler(bloomBlurProgram, "smaller",
                i + 1 < int(bloomPyramid.size()) ? bloomPyramid[i + 1].texture : 0, 1);
        integer(bloomBlurProgram, "combine", i + 1 < int(bloomPyramid.size()));
        gl.Uniform2f(uniform(bloomBlurProgram, "direction"), 1.f / w, 0);
        gl.BindFramebuffer(GL_FRAMEBUFFER, bloomScratch[i].fbo);
        gl.Viewport(0, 0, w, h);
        gl.DrawArrays(GL_TRIANGLES, 0, 3);
        sampler(bloomBlurProgram, "source", bloomScratch[i].texture, 0);
        integer(bloomBlurProgram, "combine", 0);
        gl.Uniform2f(uniform(bloomBlurProgram, "direction"), 0, 1.f / h);
        gl.BindFramebuffer(GL_FRAMEBUFFER, bloomPyramid[i].fbo);
        gl.DrawArrays(GL_TRIANGLES, 0, 3);
        stats.drawCalls += 2;
        stats.postPasses += 2;
    }
    gl.Viewport(0, 0, width, height);
    return source;
}
void Renderer::Impl::temporalResolve(unsigned source) {
    unsigned next = 1 - temporalIndex;
    gl.UseProgram(taaProgram);
    sampler(taaProgram, "source", source, 0);
    sampler(taaProgram, "history", temporal[temporalIndex].texture, 1);
    sampler(taaProgram, "motionMap", sceneMotion, 2);
    sampler(taaProgram, "surfaceMap", sceneSurface, 3);
    gl.Uniform2f(uniform(taaProgram, "pixel"), 1.f / width, 1.f / height);
    scalar(taaProgram, "weight", settings().temporalWeight);
    integer(taaProgram, "valid", historyValid);
    gl.BindFramebuffer(GL_FRAMEBUFFER, temporal[next].fbo);
    gl.Viewport(0, 0, outputWidth, outputHeight);
    gl.BindVertexArray(fullscreenVao);
    gl.DrawArrays(GL_TRIANGLES, 0, 3);
    gl.UseProgram(upscaleProgram);
    sampler(upscaleProgram, "source", temporal[next].texture, 0);
    gl.Uniform2f(uniform(upscaleProgram, "pixel"), 1.f / outputWidth, 1.f / outputHeight);
    scalar(upscaleProgram, "sharpening", settings().sharpening);
    integer(upscaleProgram, "sameSize", 1);
    gl.BindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer());
    gl.DrawArrays(GL_TRIANGLES, 0, 3);
    stats.drawCalls += 2;
    stats.postPasses += 2;
    temporalIndex = next;
}
void Renderer::resetHistory() {
    impl->historyValid = false;
    impl->focusValid = false;impl->directHistoryValid=false;
    for(auto &[id,p]:impl->probeCache){(void)id;p.ready=false;p.nextFace=0;p.filterTask=0;p.refreshing=true;}
}
void Renderer::light(const Light &light) {
    if (impl->lights.size() >= 1024)
        throw std::runtime_error("local light budget exceeded");
    for (float f :
         {light.position.x, light.position.y, light.position.z, light.radius, light.intensity,
          light.direction.x, light.direction.y, light.direction.z, light.innerCone, light.outerCone,
          light.color.r, light.color.g, light.color.b})
        if (!std::isfinite(f))
            throw std::invalid_argument("nonfinite light");
    if (light.radius <= 0 || light.intensity < 0 || !std::isfinite(light.sourceRadiusMetres) || light.sourceRadiusMetres <= 0 || light.innerCone < 0 ||
        light.outerCone <= light.innerCone || light.outerCone >= pi ||
        (light.falloff != LightFalloff::Legacy && light.falloff != LightFalloff::InverseSquare) ||
        light.color.r < 0 || light.color.g < 0 || light.color.b < 0 ||
        (light.type == LightType::Spot && length(light.direction) < 1e-8f))
        throw std::invalid_argument("invalid local light");
    if(light.castShadow){
        if(light.type!=LightType::Spot||!light.shadowId||light.outerCone>=pi*.5f-.001f||light.radius<=.1f)
            throw std::invalid_argument("Shadowed spotlights require a persistent ID, range > 0.1 m and outer half angle < 90 degrees");
        for(auto &existing:impl->lights)if(existing.castShadow&&existing.shadowId==light.shadowId)
            throw std::invalid_argument("Duplicate spotlight shadow ID");
    }
    if(impl->observer)impl->observer->lightSubmitted(light);
    impl->lights.push_back(light);
    impl->physicalLighting |= light.falloff == LightFalloff::InverseSquare;
}
void Renderer::updateMesh(MeshId id, std::span<const Vertex> v) {
    auto it = impl->meshes.find(id.value);
    if (it == impl->meshes.end() || v.empty() || v.size() % 3 || v.size() > 3000000)
        throw std::invalid_argument("invalid mesh update");
    for (auto &vertex : v)
        for (float f : {vertex.p.x, vertex.p.y, vertex.p.z, vertex.n.x, vertex.n.y, vertex.n.z, vertex.c.r,
                        vertex.c.g, vertex.c.b, vertex.c.a, vertex.u, vertex.v})
            if (!std::isfinite(f))
                throw std::invalid_argument("nonfinite mesh update");
    for (auto &vertex : v)
        if (dot(vertex.n, vertex.n) < 1e-12f)
            throw std::invalid_argument("zero-length mesh normal");
    // Streaming/deforming meshes keep their VAO/VBO. Do not run static-mesh
    // deduplication or allocate new GL objects on every animation frame.
    auto &mesh = it->second;
    mesh.revision++;
    for(auto &lod:mesh.lods)impl->destroy(lod);mesh.lods.clear();
    auto &gl = impl->gl;
    gl.BindVertexArray(mesh.vao);
    gl.BindBuffer(0x8893, 0);
    if (mesh.ibo)
        gl.DeleteBuffers(1, &mesh.ibo);
    mesh.ibo = 0;
    gl.BindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    gl.BufferData(GL_ARRAY_BUFFER, v.size_bytes(), v.data(), GL_STREAM_DRAW);
    mesh.count = v.size();
    mesh.uniqueVertices = v.size();
    V3 low = v[0].p, high = low;
    for (const auto &vertex : v) {
        low = {std::min(low.x, vertex.p.x), std::min(low.y, vertex.p.y), std::min(low.z, vertex.p.z)};
        high = {std::max(high.x, vertex.p.x), std::max(high.y, vertex.p.y), std::max(high.z, vertex.p.z)};
    }
    mesh.center = (low + high) * .5f;
    mesh.radius = 0;
    for (const auto &vertex : v)
        mesh.radius = std::max(mesh.radius, distance(vertex.p, mesh.center));
}
bool Renderer::reloadMaterialShaders(const std::string &vs, const std::string &fs, std::string &error) {
    try {
        unsigned next = impl->program(vs, fs);
        impl->uniforms.erase(impl->sceneProgram);
        impl->gl.DeleteProgram(impl->sceneProgram);
        impl->sceneProgram = next;
        // Replacement vertex stages can move geometry, so their depth cannot be prepassed with the built-in shader.
        impl->customMaterialShader = true;
        impl->historyValid = false;
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
} // namespace rf::gfx
