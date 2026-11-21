#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace rf {
constexpr float pi = 3.14159265359f;
struct V3 {
    float x = 0, y = 0, z = 0;
    V3 operator+(V3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    V3 operator-(V3 b) const { return {x - b.x, y - b.y, z - b.z}; }
    V3 operator*(float s) const { return {x * s, y * s, z * s}; }
    V3 operator/(float s) const { return *this * (1 / s); }
    V3 &operator+=(V3 b) {
        *this = *this + b;
        return *this;
    }
};
inline float dot(V3 a, V3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline V3 cross(V3 a, V3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(V3 a) {
    return std::sqrt(dot(a, a));
}
inline float distance(V3 a, V3 b) {
    return length(a - b);
}
inline float flatDistance(V3 a, V3 b) {
    a.y = b.y = 0;
    return distance(a, b);
}
inline V3 normalized(V3 a) {
    float l = length(a);
    return l > .00001f ? a / l : V3{};
}
inline float clamp(float a, float lo = 0, float hi = 1) {
    return std::clamp(a, lo, hi);
}
inline float mix(float a, float b, float t) {
    return a + (b - a) * t;
}
inline V3 mix(V3 a, V3 b, float t) {
    return a + (b - a) * t;
}
inline V3 forward(float yaw, float pitch = 0) {
    return {std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
}
inline V3 rotate(V3 a, float yaw) {
    return {a.x * std::cos(yaw) + a.z * std::sin(yaw), a.y, -a.x * std::sin(yaw) + a.z * std::cos(yaw)};
}
struct Color {
    float r = 1, g = 1, b = 1, a = 1;
};
struct Mat4 {
    std::array<float, 16> m{};
    static Mat4 identity() {
        Mat4 a;
        a.m = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        return a;
    }
};
inline Mat4 operator*(const Mat4 &a, const Mat4 &b) {
    Mat4 c;
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
            for (int k = 0; k < 4; k++)
                c.m[col * 4 + row] += a.m[k * 4 + row] * b.m[col * 4 + k];
    return c;
}
inline Mat4 perspective(float fov, float aspect, float nearPlane = .08f, float farPlane = 500) {
    Mat4 a;
    float f = 1 / std::tan(fov / 2);
    a.m[0] = f / aspect;
    a.m[5] = f;
    a.m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    a.m[11] = -1;
    a.m[14] = 2 * farPlane * nearPlane / (nearPlane - farPlane);
    return a;
}
inline Mat4 lookAt(V3 eye, V3 target) {
    V3 f = normalized(target - eye);
    V3 up = std::abs(f.y) > .999f ? V3{0, 0, 1} : V3{0, 1, 0};
    V3 s = normalized(cross(f, up)), u = cross(s, f);
    Mat4 a = Mat4::identity();
    a.m[0] = s.x;
    a.m[4] = s.y;
    a.m[8] = s.z;
    a.m[1] = u.x;
    a.m[5] = u.y;
    a.m[9] = u.z;
    a.m[2] = -f.x;
    a.m[6] = -f.y;
    a.m[10] = -f.z;
    a.m[12] = -dot(s, eye);
    a.m[13] = -dot(u, eye);
    a.m[14] = dot(f, eye);
    return a;
}
struct AABB {
    V3 lo, hi;
};
struct Quat {
    float x = 0, y = 0, z = 0, w = 1;
};
inline Quat normalize(Quat q) {
    float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return n > 1e-8f ? Quat{q.x / n, q.y / n, q.z / n, q.w / n} : Quat{};
}
inline Quat slerp(Quat a, Quat b, float t) {
    a = normalize(a);
    b = normalize(b);
    float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (d < 0) {
        b = {-b.x, -b.y, -b.z, -b.w};
        d = -d;
    }
    if (d > .9995f)
        return normalize({mix(a.x, b.x, t), mix(a.y, b.y, t), mix(a.z, b.z, t), mix(a.w, b.w, t)});
    float theta = std::acos(clamp(d, -1, 1)), s = std::sin(theta);
    float x = std::sin((1 - t) * theta) / s, y = std::sin(t * theta) / s;
    return {a.x * x + b.x * y, a.y * x + b.y * y, a.z * x + b.z * y, a.w * x + b.w * y};
}
inline Mat4 compose(V3 p, Quat q = {}, V3 s = {1, 1, 1}) {
    q = normalize(q);
    Mat4 m = Mat4::identity();
    float x = q.x, y = q.y, z = q.z, w = q.w;
    m.m[0] = (1 - 2 * y * y - 2 * z * z) * s.x;
    m.m[1] = (2 * x * y + 2 * z * w) * s.x;
    m.m[2] = (2 * x * z - 2 * y * w) * s.x;
    m.m[4] = (2 * x * y - 2 * z * w) * s.y;
    m.m[5] = (1 - 2 * x * x - 2 * z * z) * s.y;
    m.m[6] = (2 * y * z + 2 * x * w) * s.y;
    m.m[8] = (2 * x * z + 2 * y * w) * s.z;
    m.m[9] = (2 * y * z - 2 * x * w) * s.z;
    m.m[10] = (1 - 2 * x * x - 2 * y * y) * s.z;
    m.m[12] = p.x;
    m.m[13] = p.y;
    m.m[14] = p.z;
    return m;
}
inline V3 transformPoint(const Mat4 &m, V3 p) {
    float w = m.m[3] * p.x + m.m[7] * p.y + m.m[11] * p.z + m.m[15];
    if (std::abs(w) < 1e-8f)
        w = 1;
    return {(m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12]) / w,
            (m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13]) / w,
            (m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14]) / w};
}
inline V3 transformVector(const Mat4 &m, V3 p) {
    return {m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z, m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z,
            m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z};
}
inline Mat4 orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
    Mat4 m = Mat4::identity();
    m.m[0] = 2 / (right - left);
    m.m[5] = 2 / (top - bottom);
    m.m[10] = -2 / (farPlane - nearPlane);
    m.m[12] = -(right + left) / (right - left);
    m.m[13] = -(top + bottom) / (top - bottom);
    m.m[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    return m;
}
inline Mat4 inverse(const Mat4 &in) {
    double a[4][8]{};
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++)
            a[r][c] = in.m[c * 4 + r];
        a[r][r + 4] = 1;
    }
    for (int c = 0; c < 4; c++) {
        int pivot = c;
        for (int r = c + 1; r < 4; r++)
            if (std::abs(a[r][c]) > std::abs(a[pivot][c]))
                pivot = r;
        if (std::abs(a[pivot][c]) < 1e-12)
            return Mat4::identity();
        if (pivot != c)
            for (int k = 0; k < 8; k++)
                std::swap(a[pivot][k], a[c][k]);
        double d = a[c][c];
        for (double &v : a[c])
            v /= d;
        for (int r = 0; r < 4; r++)
            if (r != c) {
                double f = a[r][c];
                for (int k = 0; k < 8; k++)
                    a[r][k] -= f * a[c][k];
            }
    }
    Mat4 result;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            result.m[c * 4 + r] = static_cast<float>(a[r][c + 4]);
    return result;
}
inline bool contains(AABB a, V3 p) {
    return p.x >= a.lo.x && p.x <= a.hi.x && p.y >= a.lo.y && p.y <= a.hi.y && p.z >= a.lo.z && p.z <= a.hi.z;
}
inline float rayBox(V3 o, V3 d, AABB b) {
    float low = 0, high = 10000;
    const std::array origin{o.x, o.y, o.z}, direction{d.x, d.y, d.z};
    const std::array minimum{b.lo.x, b.lo.y, b.lo.z}, maximum{b.hi.x, b.hi.y, b.hi.z};
    for (int i = 0; i < 3; i++) {
        float p = origin[i], v = direction[i], lo = minimum[i], hi = maximum[i];
        if (std::abs(v) < 1e-6) {
            if (p < lo || p > hi)
                return -1;
        } else {
            float a = (lo - p) / v, c = (hi - p) / v;
            if (a > c)
                std::swap(a, c);
            low = std::max(low, a);
            high = std::min(high, c);
            if (high < low)
                return -1;
        }
    }
    return low;
}
inline uint32_t hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}
inline float random01(uint32_t x) {
    return (hash(x) & 0xffffff) / 16777215.f;
}
inline float terrainHeight(float x, float z, uint32_t seed = 101) {
    float r = std::sqrt(x * x + z * z), base = std::sin(x * .018f + seed * .01f) * 3.2f +
                                               std::cos(z * .021f) * 2.4f + std::sin((x + z) * .045f) * 1.2f;
    float crater = 0;
    float cx = x - 120, cz = z - 130, cr = std::sqrt(cx * cx + cz * cz);
    if (cr < 66)
        crater = static_cast<float>(-9 * std::pow(1 - cr / 66, 2));
    if (cr > 60 && cr < 77)
        crater += 1.5f * std::sin((cr - 60) / 17 * pi);
    return (base + crater) * clamp((r - 25) / 50);
}
inline float slope(float x, float z, uint32_t seed = 101) {
    float dx = (terrainHeight(x + .5f, z, seed) - terrainHeight(x - .5f, z, seed)),
          dz = (terrainHeight(x, z + .5f, seed) - terrainHeight(x, z - .5f, seed));
    return std::atan(std::sqrt(dx * dx + dz * dz)) * 180 / pi;
}
} // namespace rf
