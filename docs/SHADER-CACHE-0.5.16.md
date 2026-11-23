# Compiled shader startup cache — JSD 0.5.16

`Renderer(Platform&, GraphicsSettings, RendererStartup)` and the Context overload
accept optional startup settings. Existing source calls remain valid.
`RendererStartup` controls `cacheShaders`, an optional `cacheDirectory` root and
an optional progress callback. `ShaderBuildProgress` reports phase, completed,
cached and compiled counts. Its callback is detached when constructor startup
finishes, so application splash objects may then be destroyed safely.
`Renderer::shaderCacheStatistics()` exposes cumulative supported/enabled status,
loaded/compiled/stored/rejected counts and load/compile timings.

OpenGL 4.1 or ARB_get_program_binary, all three optional binary entry points and a
nonzero binary-format list are required for cache use. The GL 3.3 source path
remains supported. Source linking requests PROGRAM_BINARY_RETRIEVABLE_HINT;
restoration verifies GL_LINK_STATUS. Exact source/stage and engine/driver/device
fingerprints isolate entries. Headers, lengths, formats and payload checksums
are validated before calling the driver. Rejected binaries compile from source.
Uniform values and bindings are initialized by normal rendering code after load.

Writes use closed temporary files and atomic rename. Concurrent app instances
may reuse an existing winner. Cache errors do not make renderer startup fail.
The driver namespace is bounded to 256 entries / 256 MiB; individual binaries
are bounded to 64 MiB. Cache location follows LOCALAPPDATA on Windows and
XDG_CACHE_HOME/HOME on Linux. JSD_SHADER_CACHE_DIR overrides the root and
JSD_SHADER_CACHE=0 disables it.

`Platform(headless, width, height, fullscreen, title)` can create a desktop window
in fullscreen before GL initialization. Existing constructor source calls retain
their windowed default. A loading callback must preserve GL state if it draws;
the colony splash does so, including framebuffer, viewport, texture/sampler,
program, VAO/buffer, blend and enable state. GL calls stay on the context thread.

Rebuild SDK consumers after this C++ API update. Program caches are local derived
data, not portable game assets. Reference: Khronos OpenGL program binary API,
https://registry.khronos.org/OpenGL-Refpages/gl4/html/glProgramBinary.xhtml
