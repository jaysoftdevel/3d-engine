#pragma once
#include "frontier/renderer.hpp"
#include "gl.hpp"
#include <chrono>
#include <filesystem>
#include <functional>
#include <string_view>

namespace rf::gfx::detail {
class ProgramCache {
    vl::GL& gl;
    RendererStartup startup;
    ShaderCacheStatistics counters;
    std::filesystem::path directory;
    std::string identity;
    std::vector<int> formats;
    void report(std::string_view phase);
    void trim(const std::filesystem::path& keep) noexcept;
public:
    ProgramCache(vl::GL&,RendererStartup);
    unsigned get(const std::string&,const std::string&,const char*,const char*,const std::function<unsigned()>&);
    bool retrievable() const {return counters.enabled;}
    void complete();
    const ShaderCacheStatistics& statistics() const {return counters;}
};
}
