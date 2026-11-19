#include "internal.hpp"
namespace rf::gfx {
void Renderer::Impl::uploadLights() {
    lightGridWidth = (width + 15) / 16;
    int rows = (height + 15) / 16;
    std::vector<std::vector<unsigned>> grid(lightGridWidth * rows);
    std::vector<std::array<float, 4>> data;
    for (size_t i = 0; i < lights.size(); i++) {
        auto &l = lights[i];
        auto d = normalized(l.direction);
        data.push_back({l.position.x, l.position.y, l.position.z, l.radius});
        data.push_back({l.color.r, l.color.g, l.color.b, l.intensity});
        data.push_back({d.x, d.y, d.z, std::cos(l.outerCone)});
        data.push_back({std::cos(l.innerCone), float(l.type == LightType::Spot), 0, 0});
        auto p = transformPoint(view, l.position);
        float z = -p.z;
        if (z + l.radius < camera.nearPlane || z - l.radius > settings().viewDistance)
            continue;
        int x0 = 0, y0 = 0, x1 = lightGridWidth - 1, y1 = rows - 1;
        if (z > l.radius + camera.nearPlane) {
            auto c = transformPoint(vp, l.position);
            float rx = projection.m[0] * l.radius / (z - l.radius),
                  ry = projection.m[5] * l.radius / (z - l.radius);
            x0 = std::clamp(int((c.x - rx + 1) * .5f * width / 16), 0, x1);
            x1 = std::clamp(int((c.x + rx + 1) * .5f * width / 16), 0, x1);
            y0 = std::clamp(int((c.y - ry + 1) * .5f * height / 16), 0, y1);
            y1 = std::clamp(int((c.y + ry + 1) * .5f * height / 16), 0, y1);
        }
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                auto &tile = grid[y * lightGridWidth + x];
                if (tile.size() < 128)
                    tile.push_back(i);
                else
                    stats.lightTileOverflows++;
            }
    }
    std::vector<std::array<unsigned, 2>> ranges;
    std::vector<unsigned> indices;
    for (auto &tile : grid) {
        ranges.push_back({unsigned(indices.size()), unsigned(tile.size())});
        indices.insert(indices.end(), tile.begin(), tile.end());
    }
    if (indices.empty())
        indices.push_back(0);
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
    upload(lightIndexBuffer, lightIndexTexture, 0x8236, indices.data(), indices.size() * sizeof(unsigned));
    stats.localLights = lights.size();
}
} // namespace rf::gfx
