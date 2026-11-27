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
 // A fan emits the original two triangles from four shared corners.
 int corner=gl_VertexID;bool atEnd=corner==1||corner==2;
 float signSide=corner<2?1.:-1.;
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
    for(unsigned i=0;i<4;++i){gl.EnableVertexAttribArray(i);gl.VertexAttribDivisor(i,1);}
    bindLineAttributes(0);
    gl.BindVertexArray(0);
}
void Renderer::Impl::bindLineAttributes(std::size_t first){
    const int sizes[]{3,3,4,1};
    const std::size_t offsets[]{offsetof(LineSegment,a),offsetof(LineSegment,b),offsetof(LineSegment,color),offsetof(LineSegment,width)};
    for(unsigned i=0;i<4;++i)
        gl.VertexAttribPointer(i,sizes[i],GL_FLOAT,0,sizeof(LineSegment),reinterpret_cast<void*>(first*sizeof(LineSegment)+offsets[i]));
    lineAttributeStart=first;
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
    constexpr std::size_t blockSize=256;
    if(uploadedLines.size()!=lineCommands.size() || std::memcmp(uploadedLines.data(),lineCommands.data(),lineCommands.size()*sizeof(LineSegment))){
        gl.BufferData(GL_ARRAY_BUFFER,lineCommands.size()*sizeof(LineSegment),lineCommands.data(),GL_STREAM_DRAW);
        lineBounds.clear();
        widestGuide=0;
        if(lineCommands.size()>=4096){
            for(std::size_t first=0;first<lineCommands.size();first+=blockSize){
                GuideBounds box{lineCommands[first].a,lineCommands[first].a};
                const auto end=std::min(first+blockSize,lineCommands.size());
                for(auto i=first;i<end;++i){
                    const auto& line=lineCommands[i];
                    widestGuide=std::max(widestGuide,line.width);
                    for(const auto& p:{line.a,line.b}){
                        box.low={std::min(box.low.x,p.x),std::min(box.low.y,p.y),std::min(box.low.z,p.z)};
                        box.high={std::max(box.high.x,p.x),std::max(box.high.y,p.y),std::max(box.high.z,p.z)};
                    }
                }
                lineBounds.push_back(box);
            }
        }
        uploadedLines=lineCommands;stats.lineUploadBytes=lineCommands.size()*sizeof(LineSegment);
    }
    auto stableProjection=perspective(camera.fovDegrees*pi/180,float(outputWidth)/outputHeight,camera.nearPlane,settings().viewDistance);
    const auto lineVP=stableProjection*view;
    matrix(lineProgram,"lineVP",lineVP);
    gl.Uniform2f(uniform(lineProgram,"viewport"),float(outputWidth),float(outputHeight));
    gl.Uniform2f(uniform(lineProgram,"depthOffset"),-(projection.m[8]-stableProjection.m[8])*.5f,-(projection.m[9]-stableProjection.m[9])*.5f);
    sampler(lineProgram,"depthMap",sceneDepth,0);
    const auto drawRange=[&](std::size_t first,std::size_t end){
        if(first==end)return;
        if(first!=lineAttributeStart)bindLineAttributes(first);
        gl.DrawArraysInstanced(0x0006 /* GL_TRIANGLE_FAN */,0,4,static_cast<int>(end-first));
        stats.lineSegments+=static_cast<unsigned>(end-first);
        ++stats.drawCalls;
    };
    std::size_t first=0;
    if(!lineBounds.empty()){
        // A guide is expanded in screen space after near clipping. Pad the
        // four side planes for the widest stroke, its AA fringe and roundoff.
        std::array<std::array<double,4>,4> planes{};
        for(int side=0;side<4;++side){
            const int row=side/2;
            const double pad=1.+(std::max(1.f,widestGuide)+4.)/(row==0?outputWidth:outputHeight);
            const double sign=side%2==0?1.:-1.;
            for(int c=0;c<4;++c)planes[side][c]=pad*lineVP.m[4*c+3]+sign*lineVP.m[4*c+row];
        }
        for(std::size_t block=0;block<lineBounds.size();++block){
            const auto& box=lineBounds[block];
            const bool outside=std::any_of(planes.begin(),planes.end(),[&](const auto& p){
                const double x=p[0]*(p[0]>=0?box.high.x:box.low.x);
                const double y=p[1]*(p[1]>=0?box.high.y:box.low.y);
                const double z=p[2]*(p[2]>=0?box.high.z:box.low.z);
                const double tolerance=1.e-5*(1.+std::abs(x)+std::abs(y)+std::abs(z)+std::abs(p[3]));
                return x+y+z+p[3]<-tolerance;
            });
            if(outside){
                drawRange(first,block*blockSize);
                first=std::min((block+1)*blockSize,lineCommands.size());
            }
        }
    }
    drawRange(first,lineCommands.size());
    gl.BindVertexArray(0);gl.Disable(GL_BLEND);
}
}
