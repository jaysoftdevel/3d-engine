#include "shader_cache.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif
namespace rf::gfx::detail {
namespace {
constexpr std::uint64_t maxBinaryBytes=64u*1024u*1024u;
constexpr std::array<char,8> magic{'J','S','D','G','L','B','0','2'};
struct Hash {
    std::array<std::uint64_t,2> value{1469598103934665603ull,7809847782465536322ull};
    void bytes(const void* data,std::size_t size){
        const auto* p=static_cast<const unsigned char*>(data);
        for(std::size_t i=0;i<size;++i){value[0]=(value[0]^p[i])*1099511628211ull;value[1]=(value[1]^p[i])*14029467366897019727ull;}
    }
    void part(std::string_view text){const std::uint64_t size=text.size();bytes(&size,sizeof(size));bytes(text.data(),text.size());}
    std::string name() const {std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto v:value)out<<std::setw(16)<<v;return out.str();}
};
struct Header {std::array<char,8> tag;std::array<std::uint64_t,2> key;std::uint32_t format,size;std::uint64_t checksum;};
static_assert(sizeof(Header)==40);
std::filesystem::path cacheRoot(){
#ifdef _WIN32
    if(const auto* value=_wgetenv(L"JSD_SHADER_CACHE_DIR");value&&*value)return value;
    if(const auto* value=_wgetenv(L"LOCALAPPDATA");value&&*value)return std::filesystem::path(value)/L"JSD"/L"shader-cache";
#else
    if(const auto* value=std::getenv("JSD_SHADER_CACHE_DIR");value&&*value)return std::filesystem::u8path(value);
    if(const auto* value=std::getenv("XDG_CACHE_HOME");value&&*value&&std::filesystem::path(value).is_absolute())return std::filesystem::path(value)/"jsd"/"shader-cache";
    if(const auto* value=std::getenv("HOME");value&&*value)return std::filesystem::path(value)/".cache"/"jsd"/"shader-cache";
#endif
    return {};
}
std::string glString(vl::GL& gl,unsigned name){const auto* text=gl.GetString(name);return text?reinterpret_cast<const char*>(text):"";}
}
ProgramCache::ProgramCache(vl::GL& api,RendererStartup options):gl(api),startup(std::move(options)){
    int major=0,minor=0;gl.GetIntegerv(0x821B,&major);gl.GetIntegerv(0x821C,&minor);
    bool extension=major>4||(major==4&&minor>=1);
    if(!extension){int count=0;gl.GetIntegerv(0x821D,&count);for(int i=0;i<count;++i){const auto* e=gl.GetStringi(0x1F03,i);if(e&&std::strcmp(reinterpret_cast<const char*>(e),"GL_ARB_get_program_binary")==0)extension=true;}}
    if(extension&&gl.ProgramParameteri&&gl.GetProgramBinary&&gl.ProgramBinary){
        int count=0;gl.GetIntegerv(0x87FE,&count);
        if(count>0&&count<=256){formats.resize(count);gl.GetIntegerv(0x87FF,formats.data());counters.supported=true;}
    }
    bool enabled=startup.cacheShaders;
    if(const char* setting=std::getenv("JSD_SHADER_CACHE");setting&&std::strcmp(setting,"0")==0)enabled=false;
    if(enabled&&counters.supported){
        Hash driver;
        driver.part("jsd-program-cache-2");driver.part(Version);
#ifdef _WIN32
        driver.part("windows-x64");
#else
        driver.part("linux");
#endif
        for(unsigned name:{0x1F00u,0x1F01u,0x1F02u,0x8B8Cu})driver.part(glString(gl,name));
        for(int format:formats)driver.bytes(&format,sizeof(format));
        identity=driver.name();
        auto root=startup.cacheDirectory.empty()?cacheRoot():startup.cacheDirectory;
        if(!root.empty()){
            directory=root/identity;std::error_code ec;std::filesystem::create_directories(directory,ec);
            counters.enabled=!ec&&std::filesystem::is_directory(directory,ec);
        }
    }
    report("Preparing graphics");
}
void ProgramCache::report(std::string_view phase){
    if(startup.progress)startup.progress({std::string(phase),counters.loaded+counters.compiled,counters.loaded,counters.compiled,counters.enabled});
}
unsigned ProgramCache::get(const std::string& vs,const std::string& fs,const char* tcs,const char* tes,const std::function<unsigned()>& build){
    Hash key;key.part(identity);key.part("vertex");key.part(vs);key.part("fragment");key.part(fs);
    key.part(tcs?"tess-control-present":"tess-control-absent");if(tcs)key.part(tcs);
    key.part(tes?"tess-evaluation-present":"tess-evaluation-absent");if(tes)key.part(tes);
    const auto file=directory/(key.name()+".bin");
    if(counters.enabled){
        const auto began=std::chrono::steady_clock::now();
        std::ifstream in(file,std::ios::binary|std::ios::ate);
        if(in){
            report("Restoring compiled shaders");
            const auto length=in.tellg();Header header{};in.seekg(0);in.read(reinterpret_cast<char*>(&header),sizeof(header));
            bool valid=bool(in)&&header.tag==magic&&header.key==key.value&&header.size>0&&header.size<=maxBinaryBytes&&
                length==std::streamoff(sizeof(header))+header.size&&std::find(formats.begin(),formats.end(),int(header.format))!=formats.end();
            std::vector<unsigned char> bytes;
            if(valid){bytes.resize(header.size);in.read(reinterpret_cast<char*>(bytes.data()),bytes.size());Hash check;check.bytes(bytes.data(),bytes.size());valid=bool(in)&&check.value[0]==header.checksum;}
            in.close();
            if(valid){
                const auto program=gl.CreateProgram();gl.ProgramBinary(program,header.format,bytes.data(),static_cast<int>(bytes.size()));
                int linked=0;gl.GetProgramiv(program,vl::GL_LINK_STATUS,&linked);
                bool error=false;for(int i=0;i<16;++i){if(gl.GetError()==vl::GL_NO_ERROR)break;error=true;}
                if(linked&&!error){++counters.loaded;counters.loadMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();report("Restoring compiled shaders");return program;}
                gl.DeleteProgram(program);
            }
            ++counters.rejected;std::error_code ec;std::filesystem::remove(file,ec);
        }
    }
    report(counters.enabled?"Compiling and caching shaders":"Compiling shaders");
    const auto began=std::chrono::steady_clock::now();const auto program=build();++counters.compiled;
    counters.compileMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
    if(counters.enabled){
        int size=0;gl.GetProgramiv(program,0x8741,&size);
        if(size>0&&std::uint64_t(size)<=maxBinaryBytes){
            std::vector<unsigned char> bytes(size);int actual=0;unsigned format=0;
            gl.GetProgramBinary(program,size,&actual,&format,bytes.data());
            if(actual>0&&actual<=size&&std::find(formats.begin(),formats.end(),int(format))!=formats.end()){
                bytes.resize(actual);Hash check;check.bytes(bytes.data(),bytes.size());
                Header header{magic,key.value,format,static_cast<std::uint32_t>(actual),check.value[0]};
                static std::atomic<std::uint64_t> serial{};
#ifdef _WIN32
                const auto pid=_getpid();
#else
                const auto pid=getpid();
#endif
                auto temp=file;temp+=std::string(".tmp-")+std::to_string(pid)+"-"+std::to_string(++serial);
                std::ofstream out(temp,std::ios::binary|std::ios::trunc);
                if(out){out.write(reinterpret_cast<const char*>(&header),sizeof(header));out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());out.close();
                    std::error_code ec;if(out){std::filesystem::rename(temp,file,ec);if(!ec)++counters.stored;}
                }
                std::error_code ec;std::filesystem::remove(temp,ec);trim(file);
            }
        }
    }
    report(counters.enabled?"Compiling and caching shaders":"Compiling shaders");return program;
}
void ProgramCache::trim(const std::filesystem::path& keep) noexcept{
    try{
        struct File {std::filesystem::path path;std::filesystem::file_time_type time;std::uintmax_t bytes;};
        std::vector<File> files;std::uintmax_t bytes=0;std::error_code ec;
        for(std::filesystem::directory_iterator it(directory,ec),end;!ec&&it!=end;it.increment(ec)){
            const auto name=it->path().filename().string();
            if(name.size()!=36||it->path().extension()!=".bin"||!std::all_of(name.begin(),name.begin()+32,[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))continue;
            if(!it->is_regular_file(ec)||it->is_symlink(ec))continue;
            const auto size=it->file_size(ec);const auto time=it->last_write_time(ec);if(ec)continue;
            files.push_back({it->path(),time,size});bytes+=size;
        }
        std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return a.time<b.time;});
        std::size_t count=files.size();
        for(const auto& file:files){if(bytes<=256u*1024u*1024u&&count<=256)break;if(file.path==keep)continue;
            if(std::filesystem::remove(file.path,ec)){bytes-=file.bytes;--count;}}
    }catch(const std::exception&){} // Cache maintenance must never prevent startup.
}
void ProgramCache::complete(){report("Graphics ready");startup.progress={};}
}
