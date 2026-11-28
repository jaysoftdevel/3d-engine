#include "internal.hpp"

#include <stdexcept>

namespace rf::gfx {
using namespace vl;
void validateMesh(std::span<const Vertex>);

namespace {
using RGB=std::array<int,3>;
constexpr std::size_t meshBudget=3u*1024u*1024u;
constexpr std::size_t meshLimit=96u*1024u*1024u;
constexpr std::size_t mipBudget=512u*1024u;
constexpr unsigned retainFrames=160;

unsigned rgb565(RGB rgb) {
    return (unsigned(rgb[0]*31+127)/255u)<<11 |
           (unsigned(rgb[1]*63+127)/255u)<<5 |
           (unsigned(rgb[2]*31+127)/255u);
}
RGB expand565(unsigned value) {
    return {int(value>>11)*255/31,int((value>>5)&63)*255/63,int(value&31)*255/31};
}
std::vector<unsigned char> bc1Blocks(int width,int height,std::span<const unsigned char> pixels) {
    std::vector<unsigned char> output;
    output.reserve(std::size_t((width+3)/4)*((height+3)/4)*8);
    const auto luma=[](RGB rgb) { return rgb[0]*3+rgb[1]*6+rgb[2]; };
    for (int y=0;y<height;y+=4) {
        for (int x=0;x<width;x+=4) {
            std::array<RGB,16> block;
            for (int i=0;i<16;++i) {
                const auto at=4*(std::size_t(std::min(height-1,y+i/4))*width+std::min(width-1,x+i%4));
                block[i]={pixels[at],pixels[at+1],pixels[at+2]};
            }
            const auto [dark,light]=std::minmax_element(block.begin(),block.end(),
                [&](RGB a,RGB b){return luma(a)<luma(b);});
            unsigned high=rgb565(*light),low=rgb565(*dark);
            if (high<low) std::swap(high,low);
            if (high==low) {
                if (low==0) ++high;
                else --low;
            }
            std::array<RGB,4> palette{expand565(high),expand565(low),{},{}};
            for (int c=0;c<3;++c) {
                palette[2][c]=(2*palette[0][c]+palette[1][c])/3;
                palette[3][c]=(palette[0][c]+2*palette[1][c])/3;
            }
            std::uint32_t selectors=0;
            for (unsigned i=0;i<block.size();++i) {
                int best=std::numeric_limits<int>::max();
                unsigned choice=0;
                for (unsigned j=0;j<palette.size();++j) {
                    const int r=block[i][0]-palette[j][0],g=block[i][1]-palette[j][1],b=block[i][2]-palette[j][2];
                    const int error=2*r*r+4*g*g+b*b;
                    if (error<best) { best=error; choice=j; }
                }
                selectors|=choice<<(2*i);
            }
            for (unsigned value:{high,low}) {
                output.push_back(static_cast<unsigned char>(value));
                output.push_back(static_cast<unsigned char>(value>>8));
            }
            for (int byte=0;byte<4;++byte)
                output.push_back(static_cast<unsigned char>(selectors>>(byte*8)));
        }
    }
    return output;
}

std::vector<unsigned char> halve(int w,int h,std::span<const unsigned char> pixels,bool srgb) {
    const int width=std::max(1,w/2),height=std::max(1,h/2);
    std::vector<unsigned char> reduced(std::size_t(width)*height*4,255);
    for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
        const int left=x*w/width,right=(x+1)*w/width,top=y*h/height,bottom=(y+1)*h/height;
        const auto to=(std::size_t(y)*width+x)*4;
        for (int c=0;c<3;++c) {
            unsigned sum=0;
            for (int sy=top;sy<bottom;++sy) for (int sx=left;sx<right;++sx) {
                const unsigned p=pixels[(std::size_t(sy)*w+sx)*4+c];
                sum+=srgb?p*p:p;
            }
            const double mean=double(sum)/((bottom-top)*(right-left));
            reduced[to+c]=static_cast<unsigned char>(std::lround(srgb?std::sqrt(mean):mean));
        }
    }
    return reduced;
}
}

MeshId Renderer::stageMesh(std::vector<Vertex> vertices) {
    validateMesh(vertices);
    if (vertices.size()*sizeof(Vertex)>meshBudget)
        throw std::invalid_argument("Divide staged meshes into batches of at most 3 MiB");
    const MeshId id{impl->nextMesh++};
    if (impl->observer) impl->observer->meshCreated(id,vertices);
    impl->stagedMeshes.emplace(id.value,Impl::StreamMesh{std::move(vertices)});
    return id;
}

bool Renderer::requestMesh(MeshId id,float distanceMetres) {
    auto& i=*impl;
    if (auto it=i.stagedMeshes.find(id.value);it!=i.stagedMeshes.end()) {
        if (!std::isfinite(distanceMetres) || distanceMetres<0.f)
            throw std::invalid_argument("Invalid mesh streaming distance");
        auto& source=it->second;
        if (!source.requested || source.requestedFrame!=i.frameIndex) {
            source.requested=true;
            source.requestedFrame=i.frameIndex;
            source.distance=distanceMetres;
            i.meshRequests.push_back(id.value);
        } else source.distance=std::min(distanceMetres,source.distance);
    }
    return i.meshes.contains(id.value);
}

TextureId Renderer::stageTexture(int w,int h,std::span<const unsigned char> rgba,bool srgb) {
    if (w<=0 || h<=0 || w>4096 || h>4096 || std::size_t(w)*h*4!=rgba.size())
        throw std::invalid_argument("Invalid staged RGB texture dimensions");
    for (std::size_t i=3;i<rgba.size();i+=4)
        if (rgba[i]!=255) throw std::invalid_argument("Staged textures require opaque texels");
    auto& i=*impl;
    GpuTexture texture;
    texture.width=w;
    texture.height=h;
    texture.srgb=srgb;
    if (i.bc1 && (!srgb || i.bc1Srgb)) texture.compression=srgb?0x8C4C:0x83F0;
    std::vector<unsigned char> pixels(rgba.begin(),rgba.end());
    for (;;) {
        auto lower=w>1 || h>1?halve(w,h,pixels,srgb):std::vector<unsigned char>{};
        auto data=texture.compression?bc1Blocks(w,h,pixels):std::move(pixels);
        texture.mips.push_back({w,h,std::move(data)});
        if (lower.empty()) break;
        pixels=std::move(lower);
        w=std::max(1,w/2);
        h=std::max(1,h/2);
    }
    i.refreshStreamTexture(texture);
    const TextureId id{i.nextTexture++};
    if (i.observer) i.observer->textureCreated(id,texture.width,texture.height,rgba,srgb);
    i.textures.emplace(id.value,std::move(texture));
    return id;
}

int Renderer::Impl::textureMip(const GpuTexture& t,int edge) const {
    edge=std::clamp(edge,1,std::min(caps.maxTextureSize,textureEdgeLimit(settings().textureQuality)));
    int level=0;
    for (const auto& mip:t.mips) {
        if (std::max(mip.width,mip.height)<=edge) return level;
        ++level;
    }
    return level-1;
}

void Renderer::requestTexture(TextureId id,int edgePixels) {
    auto& i=*impl;
    const auto found=i.textures.find(id.value);
    if (found==i.textures.end() || found->second.mips.empty()) return;
    auto& t=found->second;
    const int level=i.textureMip(t,std::max(edgePixels,32));
    if (t.requestedMip<0) i.textureRequests.push_back(id.value);
    t.lastRequested=i.frameIndex;
    t.requestedMip=t.requestedMip<0?level:std::min(level,t.requestedMip);
}

void Renderer::Impl::uploadMip(GpuTexture& t,int level) {
    const auto& mip=t.mips[level];
    gl.BindTexture(GL_TEXTURE_2D,t.object);
    if (t.compression)
        gl.CompressedTexImage2D(GL_TEXTURE_2D,level,t.compression,mip.width,mip.height,0,
            static_cast<int>(mip.bytes.size()),mip.bytes.data());
    else
        gl.TexImage2D(GL_TEXTURE_2D,level,t.srgb?0x8C43:GL_RGBA8,mip.width,mip.height,0,
            GL_RGBA,GL_UNSIGNED_BYTE,mip.bytes.data());
    gl.TexParameteri(GL_TEXTURE_2D,0x813C,level);
    t.residentMip=level;
    t.uploadedWidth=mip.width;
    t.uploadedHeight=mip.height;
    t.residentBytes+=mip.bytes.size();
    stats.mipUploadBytes+=mip.bytes.size();
}

void Renderer::Impl::refreshStreamTexture(GpuTexture& t) {
    const int cap=textureMip(t,caps.maxTextureSize);
    const int coarse=textureMip(t,32);
    if (!t.object || t.residentMip<cap) {
        if (t.object) gl.DeleteTextures(1,&t.object);
        gl.GenTextures(1,&t.object);
        t.residentBytes=0;
        gl.BindTexture(GL_TEXTURE_2D,t.object);
        gl.TexParameteri(GL_TEXTURE_2D,0x813D,static_cast<int>(t.mips.size())-1);
        for (int level=static_cast<int>(t.mips.size())-1;level>=coarse;--level) uploadMip(t,level);
    }
    gl.BindTexture(GL_TEXTURE_2D,t.object);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,0x2703);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    if (caps.anisotropicFiltering)
        gl.TexParameterf(GL_TEXTURE_2D,0x84FE,settings().anisotropy);
}

void Renderer::Impl::streamResources() {
    if (!stagedMeshes.empty()) {
        // Eviction never removes a mesh requested or submitted in this frame.
        for (const auto& [id,source]:stagedMeshes) {
            const auto gpu=meshes.find(id);
            if (gpu==meshes.end()) continue;
            if (frameIndex-source.requestedFrame>retainFrames) {
                destroy(gpu->second);
                meshes.erase(gpu);
                streamedMeshBytes-=source.vertices.size()*sizeof(Vertex);
            } else ++stats.streamedMeshes;
        }
        std::erase_if(meshRequests,[&](unsigned id){return !stagedMeshes.contains(id) || meshes.contains(id);});
        std::sort(meshRequests.begin(),meshRequests.end(),[&](unsigned a,unsigned b) {
            const float da=stagedMeshes.at(a).distance,db=stagedMeshes.at(b).distance;
            return da==db?a<b:da<db;
        });
        stats.pendingStreamedMeshes=static_cast<unsigned>(meshRequests.size());
        for (const auto id:meshRequests) {
            const auto& source=stagedMeshes.at(id).vertices;
            const auto bytes=source.size()*sizeof(Vertex);
            if (streamedMeshBytes+bytes>meshLimit || (stats.meshUploadBytes && stats.meshUploadBytes+bytes>meshBudget)) break;
            meshes.emplace(id,upload(source));
            streamedMeshBytes+=bytes;
            stats.meshUploadBytes+=bytes;
            ++stats.streamedMeshes;
            --stats.pendingStreamedMeshes;
        }
        stats.streamedMeshBytes=streamedMeshBytes;
    }
    for (const auto id:textureRequests) {
        const auto found=textures.find(id);
        if (found==textures.end()) continue;
        auto& t=found->second;
        const int desired=std::max(t.requestedMip,textureMip(t,caps.maxTextureSize));
        for (int level=t.residentMip-1;level>=desired;--level) {
            if (stats.mipUploadBytes && stats.mipUploadBytes+t.mips[level].bytes.size()>mipBudget) break;
            uploadMip(t,level);
            sunShadowsValid=false;
        }
    }
    if (frameIndex%32u==0) for (auto& [id,t]:textures) {
        (void)id;
        if (t.mips.empty() || frameIndex-t.lastRequested<=retainFrames || t.residentMip>=textureMip(t,32)) continue;
        gl.DeleteTextures(1,&t.object);
        t.object=0;
        refreshStreamTexture(t);
    }
}
}
