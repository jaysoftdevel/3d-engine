#pragma once
#include "assets.hpp"
#include "audio.hpp"
#include "cinematic.hpp"
#include "game_shell.hpp"
#include "particles.hpp"
#include "physics.hpp"
#include "vehicle.hpp"
#include "runtime.hpp"
#include "timing.hpp"
#include <frontier/frontier.hpp>
namespace jsd {
using rf::Jobs;
using rf::Platform;
inline constexpr const char *Version = "0.5.25";
} // namespace jsd
extern "C" const char *jsd_game_engine_version();
