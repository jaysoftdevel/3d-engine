#include "internal.hpp"
#include <cstring>
#include <stdexcept>

namespace rf::gfx {
using namespace vl;
void Renderer::Impl::initializeLines(){
    // Each segment is one instance. Expand in clip space on the GPU; no CPU
    // triangle extrusion, normals, shadow passes or PBR work for guide lines.
    lineProgram=program(R"GLSL(#version 330 core
layout(location=0) in vec3 startPoint;layout(location=1) in vec3 endPoint;
layout(location=2) in vec4 segmentColor;layout(location=3) in float segmentWidth;
uniform mat4 lineVP;uniform vec2 viewport;
out vec4 tint;noperspective out float edge;flat out float extent;
void main(){
 vec4 a=lineVP*vec4(startPoint,1),b=lineVP*vec4(endPoint,1);
 float da=a.z+a.w,db=b.z+b.w;
 if(da<=0.&&db<=0.){gl_Position=vec4(2,2,2,1);edge=0.;extent=1.;tint=vec4(0);return;}
 if(da<0.)a=mix(a,b,da/(da-db));else if(db<0.)b=mix(b,a,db/(db-da));
 a.w=max(a.w,.000001);b.w=max(b.w,.000001);
 vec2 direction=(b.xy/b.w-a.xy/a.w)*viewport;
 float len=length(direction);vec2 normal=len>.0001?vec2(-direction.y,direction.x)/len:vec2(0,1);
 int corner=gl_VertexID%6;bool atEnd=corner==1||corner==2||corner==4;
 float signSide=(corner==0||corner==1||corner==3)?1.:-1.;
 vec4 p=atEnd?b:a;float halfWidth=max(.5,segmentWidth*.5);
 p.xy+=normal*(halfWidth+.75)*2./viewport*p.w*signSide;
 gl_Position=p;tint=segmentColor;extent=halfWidth+.75;edge=signSide*extent;
})GLSL",R"GLSL(#version 330 core
in vec4 tint;noperspective in float edge;flat in float extent;out vec4 color;
uniform sampler2D depthMap;uniform vec2 viewport,depthOffset;
void main(){
 float scene=texture(depthMap,clamp(gl_FragCoord.xy/viewport+depthOffset,vec2(0),vec2(1))).r;
 if(gl_FragCoord.z>scene+.000002)discard;
 float coverage=clamp((extent-abs(edge))/max(fwidth(edge),.0001),0.,1.);
 color=vec4(tint.rgb,tint.a*coverage);
})GLSL");
    gl.GenVertexArrays(1,&lineVao);gl.GenBuffers(1,&lineVbo);
    gl.BindVertexArray(lineVao);gl.BindBuffer(GL_ARRAY_BUFFER,lineVbo);
    const int sizes[]{3,3,4,1};
    const std::size_t offsets[]{offsetof(LineSegment,a),offsetof(LineSegment,b),offsetof(LineSegment,color),offsetof(LineSegment,width)};
    for(unsigned i=0;i<4;++i){gl.EnableVertexAttribArray(i);gl.VertexAttribPointer(i,sizes[i],GL_FLOAT,0,sizeof(LineSegment),reinterpret_cast<void*>(offsets[i]));gl.VertexAttribDivisor(i,1);}
    gl.BindVertexArray(0);
}
void Renderer::Impl::releaseLines(){
    gl.DeleteProgram(lineProgram);gl.DeleteBuffers(1,&lineVbo);gl.DeleteVertexArrays(1,&lineVao);
}
void Renderer::lines(std::span<const LineSegment> segments,const Mat4& transform){
    auto& i=*impl;
    if(segments.size()>2000000 || i.lineCommands.size()+segments.size()>2000000)throw std::invalid_argument("Too many guide segments");
    const auto first=i.lineCommands.size();i.lineCommands.resize(first+segments.size());
    i.stats.linePreparationWorkers=i.jobs.parallelFor(segments.size(),8192,[&](std::size_t lo,std::size_t hi){
      for(auto index=lo;index<hi;++index){
        const auto& s=segments[index];
        for(float f:{s.a.x,s.a.y,s.a.z,s.b.x,s.b.y,s.b.z,s.color.r,s.color.g,s.color.b,s.color.a,s.width})
            if(!std::isfinite(f))throw std::invalid_argument("Nonfinite guide segment");
        if(s.width<=0 || s.width>128)throw std::invalid_argument("Guide width outside 0..128 pixels");
        auto line=s;line.a=transformPoint(transform,line.a);line.b=transformPoint(transform,line.b);
        if(!std::isfinite(line.a.x)||!std::isfinite(line.a.y)||!std::isfinite(line.a.z)||
           !std::isfinite(line.b.x)||!std::isfinite(line.b.y)||!std::isfinite(line.b.z))throw std::invalid_argument("Nonfinite guide transform");
        i.lineCommands[first+index]=line;
      }
    });
}
void Renderer::Impl::drawLines(){
    if(lineCommands.empty())return;
    gl.BindFramebuffer(GL_FRAMEBUFFER,outputFramebuffer());gl.Viewport(0,0,outputWidth,outputHeight);
    gl.Disable(0x0B71);gl.Disable(0x0B44);gl.Disable(GL_SCISSOR_TEST);
    gl.Enable(GL_BLEND);gl.BlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    gl.UseProgram(lineProgram);gl.BindVertexArray(lineVao);gl.BindBuffer(GL_ARRAY_BUFFER,lineVbo);
    if(uploadedLines.size()!=lineCommands.size() || std::memcmp(uploadedLines.data(),lineCommands.data(),lineCommands.size()*sizeof(LineSegment))){
        gl.BufferData(GL_ARRAY_BUFFER,lineCommands.size()*sizeof(LineSegment),lineCommands.data(),GL_STREAM_DRAW);
        uploadedLines=lineCommands;stats.lineUploadBytes=lineCommands.size()*sizeof(LineSegment);
    }
    auto stableProjection=perspective(camera.fovDegrees*pi/180,float(outputWidth)/outputHeight,camera.nearPlane,settings().viewDistance);
    matrix(lineProgram,"lineVP",stableProjection*view);
    gl.Uniform2f(uniform(lineProgram,"viewport"),float(outputWidth),float(outputHeight));
    gl.Uniform2f(uniform(lineProgram,"depthOffset"),-(projection.m[8]-stableProjection.m[8])*.5f,-(projection.m[9]-stableProjection.m[9])*.5f);
    sampler(lineProgram,"depthMap",sceneDepth,0);
    gl.DrawArraysInstanced(GL_TRIANGLES,0,6,static_cast<int>(lineCommands.size()));
    stats.lineSegments=static_cast<unsigned>(lineCommands.size());++stats.drawCalls;
    gl.BindVertexArray(0);gl.Disable(GL_BLEND);
}
}
