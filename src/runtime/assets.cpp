#include "jsd/assets.hpp"
#define CGLTF_IMPLEMENTATION
#include "../../third_party/cgltf/cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 16384
#include "../../third_party/stb/stb_image.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
namespace jsd {
namespace {
std::vector<unsigned char> read(const std::filesystem::path &p) {
    auto size = std::filesystem::file_size(p);
    if (size > 128 * 1024 * 1024)
        throw std::runtime_error("asset exceeds 128 MiB limit");
    std::ifstream f(p, std::ios::binary);
    std::vector<unsigned char> data(size);
    f.read(reinterpret_cast<char *>(data.data()), size);
    if (!f)
        throw std::runtime_error("cannot read asset");
    return data;
}
Image decode(std::span<const unsigned char> data) {
    int w = 0, h = 0, n = 0;
    stbi_uc *p = stbi_load_from_memory(data.data(), data.size(), &w, &h, &n, 4);
    if (!p)
        throw std::runtime_error(std::string("image decode: ") + stbi_failure_reason());
    if (size_t(w) * h * 4 > 256 * 1024 * 1024) {
        stbi_image_free(p);
        throw std::runtime_error("decoded image too large");
    }
    Image image{w, h, 4, {p, p + size_t(w) * h * 4}};
    stbi_image_free(p);
    return image;
}
std::filesystem::path safePath(const std::filesystem::path &path, const std::filesystem::path &root) {
    auto p = std::filesystem::weakly_canonical(path), r = std::filesystem::weakly_canonical(root);
    auto a = p.begin(), b = r.begin();
    for (; b != r.end(); ++a, ++b)
        if (a == p.end() || *a != *b)
            throw std::runtime_error("glTF resource escapes allowed root");
    return p;
}
std::vector<unsigned char> base64(std::string_view text) {
    static const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<unsigned char> out;
    unsigned value = 0, bits = 0;
    for (unsigned char c : text) {
        if (c == '=')
            break;
        auto index = alphabet.find(c);
        if (index == std::string::npos) {
            if (std::isspace(c))
                continue;
            throw std::runtime_error("invalid embedded image encoding");
        }
        value = (value << 6) | unsigned(index);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((value >> bits) & 255);
        }
    }
    return out;
}
std::vector<Mat4> worldMatrices(const SceneAsset &a, std::vector<Mat4> local) {
    std::vector<int> visited(a.nodes.size());
    std::function<void(int)> visit = [&](int i) {
        if (visited[i] == 2)
            return;
        if (visited[i] == 1)
            throw std::runtime_error("glTF hierarchy cycle");
        visited[i] = 1;
        int p = a.nodes[i].parent;
        if (p >= 0) {
            visit(p);
            local[i] = local[p] * local[i];
        }
        visited[i] = 2;
    };
    for (int i = 0; i < int(a.nodes.size()); i++)
        visit(i);
    return local;
}
} // namespace
Image loadImage(const std::filesystem::path &p) {
    return decode(read(p));
}
HdrImage loadHdr(const std::filesystem::path &p) {
    auto data = read(p);
    int w, h, n;
    float *image = stbi_loadf_from_memory(data.data(), data.size(), &w, &h, &n, 3);
    if (!image)
        throw std::runtime_error("cannot decode HDR image");
    if (size_t(w) * h * 3 > 64 * 1024 * 1024) {
        stbi_image_free(image);
        throw std::runtime_error("HDR image too large");
    }
    HdrImage result{w, h, {image, image + size_t(w) * h * 3}};
    stbi_image_free(image);
    return result;
}
SceneAsset loadGltf(const std::filesystem::path &path, const std::filesystem::path &allowedRoot) {
    auto root = allowedRoot.empty() ? std::filesystem::absolute(path).parent_path()
                                    : std::filesystem::absolute(allowedRoot);
    auto full = safePath(std::filesystem::absolute(path), root);
    cgltf_options options{};
    options.file.user_data = &root;
    options.file.read = [](const cgltf_memory_options *, const cgltf_file_options *file, const char *name,
                           cgltf_size *size, void **data) -> cgltf_result {
        try {
            auto bytes = read(safePath(name, *static_cast<std::filesystem::path *>(file->user_data)));
            size_t n = *size ? *size : bytes.size();
            if (n > bytes.size())
                return cgltf_result_data_too_short;
            *data = std::malloc(n);
            if (!*data)
                return cgltf_result_out_of_memory;
            std::memcpy(*data, bytes.data(), n);
            *size = n;
            return cgltf_result_success;
        } catch (...) {
            return cgltf_result_io_error;
        }
    };
    options.file.release = [](const cgltf_memory_options *, const cgltf_file_options *, void *data,
                              cgltf_size) { std::free(data); };
    cgltf_data *raw = nullptr;
    if (cgltf_parse_file(&options, full.c_str(), &raw) != cgltf_result_success)
        throw std::runtime_error("cannot parse glTF");
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> guard(raw, cgltf_free);
    if (cgltf_load_buffers(&options, raw, full.c_str()) != cgltf_result_success ||
        cgltf_validate(raw) != cgltf_result_success)
        throw std::runtime_error("invalid glTF buffers/accessors");
    if (raw->nodes_count > 100000 || raw->meshes_count > 10000 || raw->images_count > 1024)
        throw std::runtime_error("glTF limits exceeded");
    SceneAsset out;
    for (size_t i = 0; i < raw->images_count; i++) {
        auto &image = raw->images[i];
        std::vector<unsigned char> data;
        if (image.uri) {
            std::string uri = image.uri;
            if (uri.starts_with("data:")) {
                auto comma = uri.find(',');
                if (comma == std::string::npos || uri.substr(0, comma).find(";base64") == std::string::npos)
                    throw std::runtime_error("embedded images must use base64");
                data = base64(uri.substr(comma + 1));
            } else
                data = read(safePath(full.parent_path() / uri, root));
        } else if (image.buffer_view) {
            auto *p = cgltf_buffer_view_data(image.buffer_view);
            if (!p)
                throw std::runtime_error("missing image data");
            data.assign(p, p + image.buffer_view->size);
        } else
            throw std::runtime_error("empty glTF image");
        out.images.push_back(decode(data));
    }
    auto imageIndex = [&](const cgltf_texture_view &v) {
        return v.texture && v.texture->image ? int(v.texture->image - raw->images) : -1;
    };
    for (size_t i = 0; i < raw->materials_count; i++) {
        auto &m = raw->materials[i];
        AssetMaterial material;
        auto &p = m.pbr_metallic_roughness;
        material.material.albedo = {p.base_color_factor[0], p.base_color_factor[1], p.base_color_factor[2],
                                    p.base_color_factor[3]};
        material.material.roughness = p.roughness_factor;
        material.material.metallic = p.metallic_factor;
        material.material.emissive = {m.emissive_factor[0], m.emissive_factor[1], m.emissive_factor[2], 1};
        if (m.has_emissive_strength) {
            material.material.emissive.r *= m.emissive_strength.emissive_strength;
            material.material.emissive.g *= m.emissive_strength.emissive_strength;
            material.material.emissive.b *= m.emissive_strength.emissive_strength;
        }
        if (m.has_clearcoat) {
            material.material.clearcoat = m.clearcoat.clearcoat_factor;
            material.material.clearcoatRoughness = m.clearcoat.clearcoat_roughness_factor;
        }
        material.material.normalScale = m.normal_texture.scale;
        material.material.alphaCutoff = m.alpha_cutoff;
        material.material.alphaMode = m.alpha_mode == cgltf_alpha_mode_blend  ? gfx::AlphaMode::Blend
                                      : m.alpha_mode == cgltf_alpha_mode_mask ? gfx::AlphaMode::Mask
                                                                              : gfx::AlphaMode::Opaque;
        material.material.doubleSided = m.double_sided;
        material.albedo = imageIndex(p.base_color_texture);
        material.normal = imageIndex(m.normal_texture);
        material.orm = imageIndex(p.metallic_roughness_texture);
        material.emissive = imageIndex(m.emissive_texture);
        out.materials.push_back(material);
    }
    std::vector<std::vector<int>> meshIndices(raw->meshes_count);
    for (size_t mi = 0; mi < raw->meshes_count; mi++)
        for (size_t pi = 0; pi < raw->meshes[mi].primitives_count; pi++) {
            auto &p = raw->meshes[mi].primitives[pi];
            if (p.type != cgltf_primitive_type_triangles || p.has_draco_mesh_compression)
                throw std::runtime_error("requires uncompressed triangle glTF primitives");
            const cgltf_accessor *position = nullptr, *normal = nullptr, *uv = nullptr, *color = nullptr,
                                 *joints = nullptr, *weights = nullptr;
            for (size_t ai = 0; ai < p.attributes_count; ai++) {
                auto &a = p.attributes[ai];
                if (a.type == cgltf_attribute_type_position)
                    position = a.data;
                if (a.type == cgltf_attribute_type_normal)
                    normal = a.data;
                if (a.type == cgltf_attribute_type_texcoord && a.index == 0)
                    uv = a.data;
                if (a.type == cgltf_attribute_type_color && a.index == 0)
                    color = a.data;
                if (a.type == cgltf_attribute_type_joints && a.index == 0)
                    joints = a.data;
                if (a.type == cgltf_attribute_type_weights && a.index == 0)
                    weights = a.data;
            }
            if (!position)
                throw std::runtime_error("glTF mesh has no position");
            size_t count = p.indices ? p.indices->count : position->count;
            if (count % 3 || count > 3000000)
                throw std::runtime_error("invalid triangle budget");
            MeshAsset mesh;
            mesh.material = p.material ? int(p.material - raw->materials) : -1;
            mesh.vertices.reserve(count);
            for (size_t i = 0; i < count; i++) {
                size_t index = p.indices ? cgltf_accessor_read_index(p.indices, i) : i;
                float v[4] = {0, 0, 0, 1};
                if (!cgltf_accessor_read_float(position, index, v, 3))
                    throw std::runtime_error("invalid position accessor");
                gfx::Vertex vertex;
                vertex.p = {v[0], v[1], v[2]};
                if (normal) {
                    cgltf_accessor_read_float(normal, index, v, 3);
                    vertex.n = {v[0], v[1], v[2]};
                }
                if (uv) {
                    cgltf_accessor_read_float(uv, index, v, 2);
                    vertex.u = v[0];
                    vertex.v = v[1];
                }
                if (color) {
                    v[3] = 1;
                    cgltf_accessor_read_float(color, index, v, cgltf_num_components(color->type));
                    vertex.c = {v[0], v[1], v[2], v[3]};
                }
                mesh.vertices.push_back(vertex);
                if (joints && weights) {
                    cgltf_uint indices[4]{};
                    cgltf_accessor_read_uint(joints, index, indices, 4);
                    cgltf_accessor_read_float(weights, index, v, 4);
                    SkinWeight weight;
                    float total = v[0] + v[1] + v[2] + v[3];
                    for (int k = 0; k < 4; k++) {
                        weight.joints[k] = indices[k];
                        weight.weights[k] = total > 0 ? v[k] / total : float(k == 0);
                    }
                    mesh.weights.push_back(weight);
                }
            }
            if (!normal)
                for (size_t i = 0; i < count; i += 3) {
                    V3 n = rf::normalized(rf::cross(mesh.vertices[i + 1].p - mesh.vertices[i].p,
                                                    mesh.vertices[i + 2].p - mesh.vertices[i].p));
                    for (int j = 0; j < 3; j++)
                        mesh.vertices[i + j].n = n;
                }
            meshIndices[mi].push_back(out.meshes.size());
            out.meshes.push_back(std::move(mesh));
        }
    for (size_t i = 0; i < raw->nodes_count; i++) {
        auto &n = raw->nodes[i];
        AssetNode node;
        node.parent = n.parent ? int(n.parent - raw->nodes) : -1;
        node.skin = n.skin ? int(n.skin - raw->skins) : -1;
        node.translation = {n.translation[0], n.translation[1], n.translation[2]};
        node.scale = {n.scale[0], n.scale[1], n.scale[2]};
        node.rotation = {n.rotation[0], n.rotation[1], n.rotation[2], n.rotation[3]};
        node.matrixTransform = n.has_matrix;
        cgltf_node_transform_local(&n, node.local.m.data());
        if (n.mesh)
            node.meshes = meshIndices.at(n.mesh - raw->meshes);
        out.nodes.push_back(node);
    }
    for (size_t i = 0; i < raw->skins_count; i++) {
        auto &s = raw->skins[i];
        AssetSkin skin;
        for (size_t j = 0; j < s.joints_count; j++) {
            skin.joints.push_back(s.joints[j] - raw->nodes);
            Mat4 m = Mat4::identity();
            if (s.inverse_bind_matrices &&
                !cgltf_accessor_read_float(s.inverse_bind_matrices, j, m.m.data(), 16))
                throw std::runtime_error("invalid bind matrices");
            skin.inverseBind.push_back(m);
        }
        out.skins.push_back(std::move(skin));
    }
    for (size_t i = 0; i < raw->animations_count; i++) {
        auto &a = raw->animations[i];
        AnimationClip clip;
        clip.name = a.name ? a.name : "animation";
        for (size_t j = 0; j < a.channels_count; j++) {
            auto &c = a.channels[j];
            if (!c.target_node || c.target_path == cgltf_animation_path_type_weights)
                continue;
            auto &s = *c.sampler;
            AnimationTrack track;
            track.node = c.target_node - raw->nodes;
            track.property = c.target_path == cgltf_animation_path_type_translation
                                 ? TrackProperty::Translation
                             : c.target_path == cgltf_animation_path_type_rotation ? TrackProperty::Rotation
                                                                                   : TrackProperty::Scale;
            track.interpolation = s.interpolation == cgltf_interpolation_type_step ? Interpolation::Step
                                  : s.interpolation == cgltf_interpolation_type_cubic_spline
                                      ? Interpolation::Cubic
                                      : Interpolation::Linear;
            for (size_t k = 0; k < s.input->count; k++) {
                float t;
                cgltf_accessor_read_float(s.input, k, &t, 1);
                if (!std::isfinite(t) || (k && t <= track.times.back()))
                    throw std::runtime_error("invalid animation times");
                track.times.push_back(t);
                clip.duration = std::max(clip.duration, t);
            }
            for (size_t k = 0; k < s.output->count; k++) {
                std::array<float, 4> v{0, 0, 0, 1};
                cgltf_accessor_read_float(s.output, k, v.data(),
                                          track.property == TrackProperty::Rotation ? 4 : 3);
                track.values.push_back(v);
            }
            if (track.values.size() !=
                track.times.size() * (track.interpolation == Interpolation::Cubic ? 3 : 1))
                throw std::runtime_error("invalid animation track");
            clip.tracks.push_back(std::move(track));
        }
        out.animations.push_back(std::move(clip));
    }
    return out;
}
std::vector<Mat4> sampleAnimation(const SceneAsset &a, int clipIndex, float seconds, bool loop) {
    std::vector<V3> positions, scales;
    std::vector<Quat> rotations;
    for (auto &n : a.nodes) {
        positions.push_back(n.translation);
        scales.push_back(n.scale);
        rotations.push_back(n.rotation);
    }
    std::vector<bool> animated(a.nodes.size());
    if (clipIndex >= 0) {
        auto &clip = a.animations.at(clipIndex);
        float t = loop && clip.duration > 0 ? std::fmod(std::max(0.f, seconds), clip.duration)
                                            : rf::clamp(seconds, 0, clip.duration);
        for (auto &track : clip.tracks) {
            if (track.times.empty())
                continue;
            size_t b = std::upper_bound(track.times.begin(), track.times.end(), t) - track.times.begin();
            size_t i = b ? b - 1 : 0, j = std::min(i + 1, track.times.size() - 1);
            float dt = track.times[j] - track.times[i], u = dt > 0 ? rf::clamp((t - track.times[i]) / dt) : 0;
            auto A = track.values[track.interpolation == Interpolation::Cubic ? i * 3 + 1 : i],
                 B = track.values[track.interpolation == Interpolation::Cubic ? j * 3 + 1 : j], value = A;
            if (track.interpolation == Interpolation::Cubic) {
                for (int k = 0; k < 4; k++)
                    value[k] = (2 * u * u * u - 3 * u * u + 1) * A[k] +
                               (u * u * u - 2 * u * u + u) * dt * track.values[i * 3 + 2][k] +
                               (-2 * u * u * u + 3 * u * u) * B[k] +
                               (u * u * u - u * u) * dt * track.values[j * 3][k];
            } else if (track.interpolation == Interpolation::Linear) {
                for (int k = 0; k < 4; k++)
                    value[k] = rf::mix(A[k], B[k], u);
                if (track.property == TrackProperty::Rotation) {
                    auto q = rf::slerp({A[0], A[1], A[2], A[3]}, {B[0], B[1], B[2], B[3]}, u);
                    value = {q.x, q.y, q.z, q.w};
                }
            }
            if (track.property == TrackProperty::Translation)
                positions[track.node] = {value[0], value[1], value[2]};
            else if (track.property == TrackProperty::Scale)
                scales[track.node] = {value[0], value[1], value[2]};
            else
                rotations[track.node] = rf::normalize({value[0], value[1], value[2], value[3]});
            animated[track.node] = true;
        }
    }
    std::vector<Mat4> local;
    for (size_t i = 0; i < a.nodes.size(); i++)
        local.push_back(a.nodes[i].matrixTransform && !animated[i]
                            ? a.nodes[i].local
                            : rf::compose(positions[i], rotations[i], scales[i]));
    return worldMatrices(a, std::move(local));
}
std::vector<gfx::Vertex> skinMesh(const MeshAsset &m, std::span<const Mat4> palette) {
    if (m.weights.size() != m.vertices.size())
        throw std::invalid_argument("missing skin weights");
    std::vector<Mat4> normalMatrices;
    for (auto &matrix : palette)
        normalMatrices.push_back(rf::inverse(matrix));
    auto result = m.vertices;
    for (size_t i = 0; i < result.size(); i++) {
        V3 position{}, normal{};
        for (int k = 0; k < 4; k++) {
            auto &w = m.weights[i];
            if (w.weights[k] == 0)
                continue;
            if (w.joints[k] >= palette.size())
                throw std::invalid_argument("skin joint outside palette");
            position += rf::transformPoint(palette[w.joints[k]], m.vertices[i].p) * w.weights[k];
            auto &nm = normalMatrices[w.joints[k]].m;
            auto n = m.vertices[i].n;
            normal += V3{nm[0] * n.x + nm[1] * n.y + nm[2] * n.z, nm[4] * n.x + nm[5] * n.y + nm[6] * n.z,
                         nm[8] * n.x + nm[9] * n.y + nm[10] * n.z} *
                      w.weights[k];
        }
        result[i].p = position;
        result[i].n = rf::normalized(normal);
    }
    return result;
}
GpuScene::GpuScene(gfx::Renderer &r, SceneAsset a) : asset(std::move(a)), renderer(&r) {
    try {
        std::map<std::pair<int, bool>, gfx::TextureId> uploaded;
        auto tex = [&](int index, bool srgb) {
            if (index < 0)
                return gfx::TextureId{};
            auto key = std::pair{index, srgb};
            auto it = uploaded.find(key);
            if (it != uploaded.end())
                return it->second;
            auto &image = asset.images.at(index);
            auto id = r.createTexture(image.width, image.height, image.rgba, srgb);
            textures.push_back(id);
            uploaded[key] = id;
            return id;
        };
        for (auto &m : asset.materials) {
            auto value = m.material;
            value.albedoTexture = tex(m.albedo, true);
            value.normalTexture = tex(m.normal, false);
            value.ormTexture = tex(m.orm, false);
            value.emissiveTexture = tex(m.emissive, true);
            materials.push_back(value);
        }
        for (size_t node = 0; node < asset.nodes.size(); node++)
            for (auto mesh : asset.nodes[node].meshes) {
                auto id = r.createMesh(asset.meshes.at(mesh).vertices);
                meshes.push_back(id);
                records.push_back({int(node), mesh, id});
            }
    } catch (...) {
        for (auto id : meshes)
            r.destroyMesh(id);
        for (auto id : textures)
            r.destroyTexture(id);
        throw;
    }
}
GpuScene::~GpuScene() {
    for (auto m : meshes)
        renderer->destroyMesh(m);
    for (auto t : textures)
        renderer->destroyTexture(t);
}
void GpuScene::draw(float seconds, int animation, const Mat4 &root, uint64_t idBase) {
    auto pose = sampleAnimation(asset, animation, seconds);
    for (auto &record : records) {
        auto &node = asset.nodes[record.node];
        auto &m = asset.meshes[record.mesh];
        if (node.skin >= 0 && !m.weights.empty()) {
            auto &s = asset.skins.at(node.skin);
            std::vector<Mat4> palette;
            auto inv = rf::inverse(pose[record.node]);
            for (size_t j = 0; j < s.joints.size(); j++)
                palette.push_back(inv * pose.at(s.joints[j]) * s.inverseBind[j]);
            renderer->updateMesh(record.gpu, skinMesh(m, palette));
        }
        gfx::Draw draw;
        draw.mesh = record.gpu;
        draw.material = m.material >= 0 ? materials.at(m.material) : gfx::Material{};
        draw.material.temporalReactive = node.skin >= 0;
        draw.objectId = idBase + record.node * 1000 + record.mesh;
        draw.modelMatrix = root * pose[record.node];
        renderer->submit(draw);
    }
}
} // namespace jsd
