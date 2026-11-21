/* The bundled SDL source tree excludes its optional Direct3D renderers.
 * Keep their public device-query functions available for SDL's dynamic API;
 * an OpenGL-only build has no Direct3D device to return. */
#include "../third_party/SDL2/src/SDL_internal.h"
#include "SDL_system.h"
#if defined(__WIN32__)
#if !SDL_VIDEO_RENDER_D3D
IDirect3DDevice9 *SDLCALL SDL_RenderGetD3D9Device(SDL_Renderer *renderer)
{
    (void)renderer;
    SDL_SetError("Direct3D 9 renderer is not included in this OpenGL build");
    return NULL;
}
#endif
#if !SDL_VIDEO_RENDER_D3D11
ID3D11Device *SDLCALL SDL_RenderGetD3D11Device(SDL_Renderer *renderer)
{
    (void)renderer;
    SDL_SetError("Direct3D 11 renderer is not included in this OpenGL build");
    return NULL;
}
#endif
#if !SDL_VIDEO_RENDER_D3D12
ID3D12Device *SDLCALL SDL_RenderGetD3D12Device(SDL_Renderer *renderer)
{
    (void)renderer;
    SDL_SetError("Direct3D 12 renderer is not included in this OpenGL build");
    return NULL;
}
#endif
#endif
