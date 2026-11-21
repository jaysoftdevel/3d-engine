# Embedded engine preview

The optional sibling `jsd-editor` is an independent native application. Build it in its own directory with `./build.sh release`; it links the public engine target and does not require a game/showcase build. Its README lists object authoring, polygon/material editing, animation, interchange and physics-preview controls.

A custom application can now render into its own framebuffer through the appended `Context::outputFramebuffer` callback:

```cpp
rf::gfx::Context context;
context.resolve = [&](const char *name) { return platform.resolve(name); };
context.dimensions = [&] { return std::pair{previewPixelWidth, previewPixelHeight}; };
context.outputFramebuffer = [&] { return previewFbo; };
rf::gfx::Renderer renderer(context);
```

The application creates a complete color-attached GL framebuffer, keeps it live in the same GL context for the renderer's lifetime, and resizes its storage when panel dimensions change. The engine renders its full pipeline into that target and restores it after diagnostic readbacks/environment preprocessing. Composite its texture into the application's interface afterward. Leave the callback empty to retain the previous window/offscreen framebuffer-zero behavior. All GL calls must use the context's owning thread. Rebuild existing consumers because the public Context struct grew.
