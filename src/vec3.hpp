#ifndef VEC3_HPP
#define VEC3_HPP

#include "simd.hpp"

#include <cmath>

template <typename T>
struct vec3
{
    T x;
    T y;
    T z;
};

using float3 = vec3<float>;
using pfloat3 = vec3<pfloat>;

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr vec3<T> operator+(const vec3<T> &a)
{
    return a;
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr vec3<T> operator-(const vec3<T> &a)
{
    return {-a.x, -a.y, -a.z};
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr vec3<T> operator+(const vec3<T> &a,
                                                        const vec3<T> &b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr vec3<T> operator-(const vec3<T> &a,
                                                        const vec3<T> &b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr vec3<T> operator*(const vec3<T> &a,
                                                        const vec3<T> &b)
{
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr vec3<T> operator/(const vec3<T> &a,
                                                        const vec3<T> &b)
{
    return {a.x / b.x, a.y / b.y, a.z / b.z};
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr vec3<T> operator+(const vec3<T> &a, T b)
{
    return {a.x + b, a.y + b, a.z + b};
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr vec3<T> operator+(T a, const vec3<T> &b)
{
    return {a + b.x, a + b.y, a + b.z};
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr vec3<T> operator-(const vec3<T> &a, T b)
{
    return {a.x - b, a.y - b, a.z - b};
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr vec3<T> operator-(T a, const vec3<T> &b)
{
    return {a - b.x, a - b.y, a - b.z};
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr vec3<T> operator*(const vec3<T> &a, T b)
{
    return {a.x * b, a.y * b, a.z * b};
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr vec3<T> operator*(T a, const vec3<T> &b)
{
    return {a * b.x, a * b.y, a * b.z};
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr vec3<T> operator/(const vec3<T> &a, T b)
{
    return {a.x / b, a.y / b, a.z / b};
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr vec3<T> operator/(T a, const vec3<T> &b)
{
    return {a / b.x, a / b.y, a / b.z};
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr T dot(const vec3<T> &a, const vec3<T> &b)
{
    if constexpr (std::is_same_v<T, pfloat>)
    {
        return fmadd(a.z, b.z, fmadd(a.y, b.y, a.x * b.x));
    }
    else
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE constexpr vec3<T> cross(const vec3<T> &a,
                                                    const vec3<T> &b)
{
    if constexpr (std::is_same_v<T, pfloat>)
    {
        return {fmsub(a.y, b.z, a.z * b.y),
                fmsub(a.z, b.x, a.x * b.z),
                fmsub(a.x, b.y, a.y * b.x)};
    }
    else
    {
        return {a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
    }
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE T norm(const vec3<T> &a)
{
    using std::sqrt;
    return sqrt(dot(a, a));
}

template <typename T>
[[nodiscard]] ALWAYS_INLINE vec3<T> normalized(const vec3<T> &a)
{
    return a / norm(a);
}

#endif // VEC3_HPP
