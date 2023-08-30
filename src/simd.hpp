#ifndef SIMD_HPP
#define SIMD_HPP

#if !defined(__AVX2__) || !defined(__FMA__)
#error "Support for AVX2 and FMA3 is required"
#endif

#include <immintrin.h>

#if defined(__GNUC__)
#define ALWAYS_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define ALWAYS_INLINE __forceinline
#else
#define ALWAYS_INLINE inline
#endif

struct pfloat
{
    __m256 v;
    static constexpr std::size_t width {8};
};

struct pmask
{
    __m256 v;
};

[[nodiscard]] ALWAYS_INLINE pfloat set1(float a)
{
    return {_mm256_set1_ps(a)};
}

[[nodiscard]] ALWAYS_INLINE pfloat load_aligned(const float *p)
{
    return {_mm256_load_ps(p)};
}

[[nodiscard]] ALWAYS_INLINE pfloat load_unaligned(const float *p)
{
    return {_mm256_loadu_ps(p)};
}

ALWAYS_INLINE void store_aligned(float *p, pfloat a)
{
    _mm256_store_ps(p, a.v);
}

ALWAYS_INLINE void store_unaligned(float *p, pfloat a)
{
    _mm256_storeu_ps(p, a.v);
}

[[nodiscard]] ALWAYS_INLINE pfloat operator+(pfloat a)
{
    return a;
}

[[nodiscard]] ALWAYS_INLINE pfloat operator-(pfloat a)
{
    return {_mm256_sub_ps(_mm256_setzero_ps(), a.v)};
}

[[nodiscard]] ALWAYS_INLINE pfloat operator+(pfloat a, pfloat b)
{
    return {_mm256_add_ps(a.v, b.v)};
}

[[nodiscard]] ALWAYS_INLINE pfloat operator-(pfloat a, pfloat b)
{
    return {_mm256_sub_ps(a.v, b.v)};
}

[[nodiscard]] ALWAYS_INLINE pfloat operator*(pfloat a, pfloat b)
{
    return {_mm256_mul_ps(a.v, b.v)};
}

[[nodiscard]] ALWAYS_INLINE pfloat operator/(pfloat a, pfloat b)
{
    return {_mm256_div_ps(a.v, b.v)};
}

ALWAYS_INLINE pfloat &operator+=(pfloat &a, pfloat b)
{
    a = a + b;
    return a;
}

ALWAYS_INLINE pfloat &operator-=(pfloat &a, pfloat b)
{
    a = a - b;
    return a;
}

ALWAYS_INLINE pfloat &operator*=(pfloat &a, pfloat b)
{
    a = a * b;
    return a;
}

ALWAYS_INLINE pfloat &operator/=(pfloat &a, pfloat b)
{
    a = a / b;
    return a;
}

[[nodiscard]] ALWAYS_INLINE pfloat fmadd(pfloat a, pfloat b, pfloat c)
{
    return {_mm256_fmadd_ps(a.v, b.v, c.v)};
}

[[nodiscard]] ALWAYS_INLINE pfloat fnmadd(pfloat a, pfloat b, pfloat c)
{
    return {_mm256_fnmadd_ps(a.v, b.v, c.v)};
}

[[nodiscard]] ALWAYS_INLINE pfloat fmsub(pfloat a, pfloat b, pfloat c)
{
    return {_mm256_fmsub_ps(a.v, b.v, c.v)};
}

[[nodiscard]] ALWAYS_INLINE pfloat fnmsub(pfloat a, pfloat b, pfloat c)
{
    return {_mm256_fnmsub_ps(a.v, b.v, c.v)};
}

[[nodiscard]] ALWAYS_INLINE pfloat sqrt(pfloat a)
{
    return {_mm256_sqrt_ps(a.v)};
}

[[nodiscard]] ALWAYS_INLINE pmask operator>(pfloat a, pfloat b)
{
    return {_mm256_cmp_ps(a.v, b.v, _CMP_GT_OQ)};
}

[[nodiscard]] ALWAYS_INLINE pmask operator>=(pfloat a, pfloat b)
{
    return {_mm256_cmp_ps(a.v, b.v, _CMP_GE_OQ)};
}

[[nodiscard]] ALWAYS_INLINE pmask operator<(pfloat a, pfloat b)
{
    return {_mm256_cmp_ps(a.v, b.v, _CMP_LT_OQ)};
}

[[nodiscard]] ALWAYS_INLINE pmask operator<=(pfloat a, pfloat b)
{
    return {_mm256_cmp_ps(a.v, b.v, _CMP_LE_OQ)};
}

[[nodiscard]] ALWAYS_INLINE pmask operator==(pfloat a, pfloat b)
{
    return {_mm256_cmp_ps(a.v, b.v, _CMP_EQ_OQ)};
}

[[nodiscard]] ALWAYS_INLINE pmask operator!=(pfloat a, pfloat b)
{
    return {_mm256_cmp_ps(a.v, b.v, _CMP_NEQ_OQ)};
}

[[nodiscard]] ALWAYS_INLINE pmask operator&(pmask a, pmask b)
{
    return {_mm256_and_ps(a.v, b.v)};
}

[[nodiscard]] ALWAYS_INLINE pmask operator|(pmask a, pmask b)
{
    return {_mm256_or_ps(a.v, b.v)};
}

[[nodiscard]] ALWAYS_INLINE pmask operator^(pmask a, pmask b)
{
    return {_mm256_xor_ps(a.v, b.v)};
}

ALWAYS_INLINE pmask &operator&=(pmask &a, pmask b)
{
    a = a & b;
    return a;
}

ALWAYS_INLINE pmask &operator|=(pmask &a, pmask b)
{
    a = a | b;
    return a;
}

ALWAYS_INLINE pmask &operator^=(pmask &a, pmask b)
{
    a = a ^ b;
    return a;
}

[[nodiscard]] ALWAYS_INLINE pfloat select(pfloat a, pfloat b, pmask m)
{
    return {_mm256_blendv_ps(a.v, b.v, m.v)};
}

[[nodiscard]] ALWAYS_INLINE pfloat select(pfloat a, pmask m)
{
    return {_mm256_and_ps(a.v, m.v)};
}

[[nodiscard]] ALWAYS_INLINE bool none_of(pmask m)
{
    return _mm256_movemask_ps(m.v) == 0;
}

[[nodiscard]] ALWAYS_INLINE bool all_of(pmask m)
{
    return _mm256_movemask_ps(m.v) == 0xff;
}

[[nodiscard]] ALWAYS_INLINE bool all_positive(pfloat a)
{
    return _mm256_movemask_ps(a.v) == 0;
}

[[nodiscard]] ALWAYS_INLINE bool all_negative(pfloat a)
{
    return _mm256_movemask_ps(a.v) == 0xff;
}

[[nodiscard]] ALWAYS_INLINE bool all_positive(pfloat a, pmask m)
{
    return static_cast<bool>(_mm256_testz_ps(a.v, m.v));
}

[[nodiscard]] ALWAYS_INLINE bool all_negative(pfloat a, pmask m)
{
    return static_cast<bool>(_mm256_testc_ps(a.v, m.v));
}

#endif // SIMD_HPP
