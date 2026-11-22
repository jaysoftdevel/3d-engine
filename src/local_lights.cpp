#include "internal.hpp"
#include "frontier/detail/light_bounds.hpp"
namespace rf::gfx {
void Renderer::Impl::uploadLights() {
    lightGridWidth = (width + 15) / 16;
    int rows = (height + 15) / 16;
    lightRanges.assign(lightGridWidth * rows, {0, 0});
    lightRects.clear();
    lightUpload.clear();
    auto &data = lightUpload;
    data.reserve(lights.size() * 4);
    for (size_t i = 0; i < lights.size(); i++) {
        auto &l = lights[i];
        auto d = normalized(l.direction);
        data.push_back({l.position.x, l.position.y, l.position.z, l.radius});
        data.push_back({l.color.r, l.color.g, l.color.b, l.intensity});
        data.push_back({d.x, d.y, d.z, std::cos(l.outerCone)});
        data.push_back({std::cos(l.innerCone), float(l.type == LightType::Spot), float(l.falloff == LightFalloff::InverseSquare), l.sourceRadiusMetres});
        auto p = transformPoint(view, l.position);
        float z = -p.z;
        if (z + l.radius < camera.nearPlane || z - l.radius > settings().viewDistance)
            continue;
        int x0 = 0, y0 = 0, x1 = lightGridWidth - 1, y1 = rows - 1;
        std::array<float,4> bounds;
        if(!detail::lightBounds(p,l.radius,camera.nearPlane,projection.m[0],projection.m[5],bounds))continue;
        x0=std::clamp(int((bounds[0]+1)*.5f*width/16),0,x1);
        x1=std::clamp(int((bounds[2]+1)*.5f*width/16),0,x1);
        y0=std::clamp(int((bounds[1]+1)*.5f*height/16),0,y1);
        y1=std::clamp(int((bounds[3]+1)*.5f*height/16),0,y1);
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                auto &count = lightRanges[y * lightGridWidth + x][1];
                if (count < 128)
                    count++;
                else
                    stats.lightTileOverflows++;
            }
        lightRects.push_back({unsigned(i), x0, y0, x1, y1});
    }
    unsigned count = 0;
    for (auto &range : lightRanges) {
        range[0] = count;
        count += range[1];
        range[1] = 0;
    }
    lightIndices.resize(std::max(1u, count));
    for (const auto &rect : lightRects)
        for (int y = rect.y0; y <= rect.y1; y++)
            for (int x = rect.x0; x <= rect.x1; x++) {
                auto &range = lightRanges[y * lightGridWidth + x];
                if (range[1] < 128)
                    lightIndices[range[0] + range[1]++] = rect.light;
            }
    auto &ranges = lightRanges;
    auto &indices = lightIndices;
    if (data.empty())
        data.resize(4);
    auto upload = [&](unsigned buffer, unsigned texture, unsigned format, const void *p, size_t size) {
        gl.BindBuffer(0x8C2A, buffer);
        gl.BufferData(0x8C2A, size, p, vl::GL_STREAM_DRAW);
        gl.BindTexture(0x8C2A, texture);
        gl.TexBuffer(0x8C2A, format, buffer);
    };
    upload(lightDataBuffer, lightDataTexture, 0x8814, data.data(), data.size() * sizeof(data[0]));
    upload(lightGridBuffer, lightGridTexture, 0x823C, ranges.data(), ranges.size() * sizeof(ranges[0]));
    upload(lightIndexBuffer, lightIndexTexture, 0x8236, indices.data(), indices.size() * sizeof(indices[0]));
    stats.localLights = lights.size();
}
namespace {
Color photometricColor(Color color){
    for(float c:{color.r,color.g,color.b})if(!std::isfinite(c)||c<0)throw std::invalid_argument("Invalid photometric color");
    float y=.2126f*color.r+.7152f*color.g+.0722f*color.b;
    if(y<=0)throw std::invalid_argument("Photometric color must emit light");
    return {color.r/y,color.g/y,color.b/y,color.a};
}
}
Light pointLightLumens(V3 position,float lumens,float range,Color color){
    if(!std::isfinite(range)||range<=0)throw std::invalid_argument("Invalid light range");
    Light l;l.position=position;l.radius=range;l.intensity=float(pointCandela(lumens));
    l.color=photometricColor(color);l.falloff=LightFalloff::InverseSquare;return l;
}
Light spotLightLumens(V3 position,V3 direction,float lumens,float range,float inner,float outer,Color color){
    auto l=pointLightLumens(position,lumens,range,color);l.type=LightType::Spot;l.direction=direction;
    l.innerCone=inner;l.outerCone=outer;l.intensity=float(spotCandela(lumens,inner,outer));return l;
}
Color lightColorTemperature(float kelvin){
    if(!std::isfinite(kelvin)||kelvin<1000||kelvin>25000)throw std::invalid_argument("Light temperature must be 1000–25000 K");
    // Planck spectrum integrated with Wyman/Sloan/Shirley's piecewise Gaussian
    // CIE 1931 fits (JCGT 2(2), 2013), sampled every 5 nm.
    auto gaussian=[](double w,double center,double left,double right){double t=(w-center)*(w<center?left:right);return std::exp(-.5*t*t);};
    double X=0,Y=0,Z=0;
    for(int w=380;w<=780;w+=5){
        double energy=std::pow(550./w,5)/std::expm1(1.438776877e7/(w*double(kelvin)));
        X+=energy*(.362*gaussian(w,442,.0624,.0374)+1.056*gaussian(w,599.8,.0264,.0323)-.065*gaussian(w,501.1,.049,.0382));
        Y+=energy*(.821*gaussian(w,568.8,.0213,.0247)+.286*gaussian(w,530.9,.0613,.0322));
        Z+=energy*(1.217*gaussian(w,437,.0845,.0278)+.681*gaussian(w,459,.0385,.0725));
    }
    return photometricColor({float(std::max(0.,3.2406*X-1.5372*Y-.4986*Z)),float(std::max(0.,-.9689*X+1.8758*Y+.0415*Z)),float(std::max(0.,.0557*X-.204*Y+1.057*Z)),1});
}
} // namespace rf::gfx
