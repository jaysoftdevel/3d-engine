#pragma once
#include <frontier/math.hpp>
#include <cmath>
#include <cstdint>
namespace jsd {
inline float showcaseTerrainHeight(float x, float z) {
  auto smooth=[](float a,float b,float t){t=rf::clamp((t-a)/(b-a));return t*t*(3-2*t);};
  auto field=[](float x,float z){
    int ix=int(std::floor(x)),iz=int(std::floor(z));float u=x-ix,v=z-iz;
    u=u*u*(3-2*u);v=v*v*(3-2*v);
    auto at=[](int a,int b){return rf::random01(rf::hash(uint32_t(a)*73856093u^uint32_t(b)*19349663u^38004u))*2-1;};
    return rf::mix(rf::mix(at(ix,iz),at(ix+1,iz),u),rf::mix(at(ix,iz+1),at(ix+1,iz+1),u),v);
  };
  float local = rf::clamp((std::sqrt(x * x + (z - 3) * (z - 3)) - 23) / 18);
  float warp=field(x*.018f,z*.018f)*9;
  float base=(field((x+warp)*.027f,z*.027f)*1.6f+
              field(x*.08f,(z-warp)*.08f)*.45f+field(x*.22f,z*.22f)*.13f)*local;
  struct Mesa {
    float x, z, r, h;
  };
  static const Mesa mesas[] = {{-47, 40, 25, 29}, {-23, 92, 30, 42},
                               {30, 113, 33, 47}, {72, 64, 28, 38},
                               {-92, 86, 34, 39}, {105, 133, 35, 49},
                               {-69, 142, 34, 48}};
  for (auto m : mesas) {
    float dx = x - m.x, dz = z - m.z;
    float d = std::sqrt(dx * dx + dz * dz);
    float edge =
        m.r * (1 + .045f * std::sin(x * .4f) + .035f * std::sin(z * .3f));
    base += m.h * .12f * (1 - smooth(edge * .58f, edge, d));
  }
  float road = (1 - smooth(2.2f, 6.0f, std::abs(x + 1))) *
               (1 - smooth(45.f, 60.f, std::abs(z + 7)));
  base *= 1 - road;
  float rut = std::exp(-std::pow((x + 2.48f) / .23f, 2)) +
              std::exp(-std::pow((x - .48f) / .23f, 2));
  base -= rut * .028f * road;
  // A broad deposited apron joins the cut mine floor to the outpost elevation.
  float mine = (1-smooth(35,58,std::abs(x-128))) * smooth(62,88,z) * (1-smooth(187,214,z));
  base=rf::mix(base,7.95f,mine);
  float center=128*smooth(16,88,z),grade=7.95f*smooth(18,88,z);
  float service=(1-smooth(4.0f,7.5f,std::abs(x-center)))*smooth(16,25,z)*(1-smooth(87,93,z));
  base=rf::mix(base,grade,service);
  // Long alluvial ridges have small-scale variation, instead of a featureless wave plane.
  float desert=(1-road)*local*(1-mine)*(1-service);
  base+=desert*(.5f*(1-std::abs(field((x+warp)*.12f,z*.035f)))+
                  .12f*field(x*.37f,z*.19f));
  return base;
}
}
