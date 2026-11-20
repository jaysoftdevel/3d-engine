#include <frontier/frontier.hpp>
#include <chrono>
#include <cstring>
#include <iostream>
using namespace rf;
using namespace rf::gfx;
namespace {
struct Calls { unsigned uniforms=0, uploads=0, framebuffers=0; } calls;
void (*f1)(int,float); void (*i1)(int,int); void (*f3)(int,float,float,float);
void (*m4)(int,int,unsigned char,const float *);
void (*tex)(unsigned,int,int,int,int,int,unsigned,unsigned,const void *);
void (*fb)(int,unsigned *);
void one(int p,float v){++calls.uniforms;f1(p,v);} void integer(int p,int v){++calls.uniforms;i1(p,v);}
void three(int p,float x,float y,float z){++calls.uniforms;f3(p,x,y,z);}
void matrix(int p,int n,unsigned char t,const float *v){++calls.uniforms;m4(p,n,t,v);}
void texture(unsigned a,int b,int c,int d,int e,int f,unsigned g,unsigned h,const void *i){++calls.uploads;tex(a,b,c,d,e,f,g,h,i);}
void framebuffer(int n,unsigned *p){calls.framebuffers+=n;fb(n,p);}
}
int main(int argc,char **argv){
    if (argc != 2) { std::cerr << "Usage: processing-benchmark output.ppm\n"; return 2; }
    Platform platform(true,320,180);
    Context context{[&](const char *name){
        void *p=platform.resolve(name);
#define WRAP(text,real,fn) if(std::strcmp(name,text)==0){real=reinterpret_cast<decltype(real)>(p);return reinterpret_cast<void *>(fn);}
        WRAP("glUniform1f",f1,one) WRAP("glUniform1i",i1,integer) WRAP("glUniform3f",f3,three)
        WRAP("glUniformMatrix4fv",m4,matrix) WRAP("glTexImage2D",tex,texture) WRAP("glGenFramebuffers",fb,framebuffer)
#undef WRAP
        return p;
    },[]{return std::pair{320,180};},true};
    auto settings=GraphicsSettings::legacy(); settings.viewDistance=80;settings.anisotropy=1;
    Renderer renderer(context,settings);
    std::vector<unsigned char> pixels(512*512*4,180);
    auto checker=renderer.createTexture(512,512,pixels);
    auto draw=[&]{
        renderer.begin({{0,4,-12},normalized(V3{0,-.1f,1}),65,.08f});
        for(int i=0;i<1600;i++){
            Material m;m.albedo={.5f,.6f,.7f,1};m.roughness=.05f+(i%200)/230.f;m.albedoTexture=checker;
            renderer.primitive(Primitive::Box,{{float(i%40-20)*.6f,float(i%5)*.1f,float(i/40)*.6f},{.45f,.4f,.45f},0},m);
        }
        renderer.end();renderer.check();platform.present();
    };
    for(int n=0;n<4;n++)draw();
    calls={};
    auto start=std::chrono::steady_clock::now();
    for(int n=0;n<16;n++)draw();
    double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/16;
    auto warmed=calls;
    renderer.screenshot(argv[1]);
    calls={}; settings.exposureCompensation=.5f;settings.anisotropy=4;
    renderer.setSettings(settings);draw();
    std::cout<<"{\"frame_ms\":"<<ms<<",\"uniform_calls_per_frame\":"<<warmed.uniforms/16.
             <<",\"setting_edit_texture_uploads\":"<<calls.uploads<<",\"setting_edit_framebuffers\":"<<calls.framebuffers<<"}\n";
}
