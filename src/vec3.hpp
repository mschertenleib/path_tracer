#ifndef VEC3_HPP
#define VEC3_HPP

#include <cmath>

struct vec3
{
    float x;
    float y;
    float z;

    [[nodiscard]] constexpr bool
    operator==(const vec3 &other) const noexcept = default;
    [[nodiscard]] constexpr bool
    operator!=(const vec3 &other) const noexcept = default;
};

[[nodiscard]] constexpr vec3 operator+(const vec3 &v) noexcept
{
    return v;
}

[[nodiscard]] constexpr vec3 operator-(const vec3 &v) noexcept
{
    return {-v.x, -v.y, -v.z};
}

[[nodiscard]] constexpr vec3 operator+(const vec3 &u, const vec3 &v) noexcept
{
    return {u.x + v.x, u.y + v.y, u.z + v.z};
}

[[nodiscard]] constexpr vec3 operator-(const vec3 &u, const vec3 &v) noexcept
{
    return {u.x - v.x, u.y - v.y, u.z - v.z};
}

[[nodiscard]] constexpr vec3 operator*(const vec3 &u, const vec3 &v) noexcept
{
    return {u.x * v.x, u.y * v.y, u.z * v.z};
}

[[nodiscard]] constexpr vec3 operator/(const vec3 &u, const vec3 &v) noexcept
{
    return {u.x / v.x, u.y / v.y, u.z / v.z};
}

[[nodiscard]] constexpr vec3 operator+(const vec3 &v, float f) noexcept
{
    return {v.x + f, v.y + f, v.z + f};
}

[[nodiscard]] constexpr vec3 operator-(const vec3 &v, float f) noexcept
{
    return {v.x - f, v.y - f, v.z - f};
}

[[nodiscard]] constexpr vec3 operator*(const vec3 &v, float f) noexcept
{
    return {v.x * f, v.y * f, v.z * f};
}

[[nodiscard]] constexpr vec3 operator/(const vec3 &v, float f) noexcept
{
    return {v.x / f, v.y / f, v.z / f};
}

[[nodiscard]] constexpr vec3 operator+(float f, const vec3 &v) noexcept
{
    return {f + v.x, f + v.y, f + v.z};
}

[[nodiscard]] constexpr vec3 operator-(float f, const vec3 &v) noexcept
{
    return {f - v.x, f - v.y, f - v.z};
}

[[nodiscard]] constexpr vec3 operator*(float f, const vec3 &v) noexcept
{
    return {f * v.x, f * v.y, f * v.z};
}

[[nodiscard]] constexpr vec3 operator/(float f, const vec3 &v) noexcept
{
    return {f / v.x, f / v.y, f / v.z};
}

constexpr vec3 &operator+=(vec3 &u, const vec3 &v) noexcept
{
    u = u + v;
    return u;
}

constexpr vec3 &operator-=(vec3 &u, const vec3 &v) noexcept
{
    u = u - v;
    return u;
}

constexpr vec3 &operator*=(vec3 &u, const vec3 &v) noexcept
{
    u = u * v;
    return u;
}

constexpr vec3 &operator/=(vec3 &u, const vec3 &v) noexcept
{
    u = u / v;
    return u;
}

constexpr vec3 &operator+=(vec3 &v, float f) noexcept
{
    v = v + f;
    return v;
}

constexpr vec3 &operator-=(vec3 &v, float f) noexcept
{
    v = v - f;
    return v;
}

constexpr vec3 &operator*=(vec3 &v, float f) noexcept
{
    v = v * f;
    return v;
}

constexpr vec3 &operator/=(vec3 &v, float f) noexcept
{
    v = v / f;
    return v;
}

[[nodiscard]] constexpr float dot(const vec3 &u, const vec3 &v) noexcept
{
    return u.x * v.x + u.y * v.y + u.z * v.z;
}

[[nodiscard]] constexpr vec3 cross(const vec3 &u, const vec3 &v) noexcept
{
    return {
        u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
}

[[nodiscard]] inline float norm(const vec3 &v) noexcept
{
    return std::sqrt(dot(v, v));
}

[[nodiscard]] inline vec3 normalize(const vec3 &v) noexcept
{
    return v * (1.0f / norm(v));
}

#endif // VEC3_HPP
