#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <concepts>
#include <iosfwd>
#include <stdexcept>
#include <utility>
#include <vector>

#include <cassert>
#include <cstdint>

template <typename F>
class Scope_exit
{
public:
    explicit Scope_exit(F &&f) : m_f(std::move(f))
    {
    }

    explicit Scope_exit(F &f) : m_f(f)
    {
    }

    ~Scope_exit() noexcept
    {
        m_f();
    }

    Scope_exit(const Scope_exit &) = delete;
    Scope_exit(Scope_exit &&) = delete;
    Scope_exit &operator=(const Scope_exit &) = delete;
    Scope_exit &operator=(Scope_exit &&) = delete;

private:
    F m_f;
};

template <typename F>
class Scope_fail
{
public:
    explicit Scope_fail(F &&f)
        : m_exception_count(std::uncaught_exceptions()), m_f(std::move(f))
    {
    }

    explicit Scope_fail(F &f)
        : m_exception_count(std::uncaught_exceptions()), m_f(f)
    {
    }

    ~Scope_fail() noexcept
    {
        if (std::uncaught_exceptions() > m_exception_count)
        {
            m_f();
        }
    }

    Scope_fail(const Scope_fail &) = delete;
    Scope_fail(Scope_fail &&) = delete;
    Scope_fail &operator=(const Scope_fail &) = delete;
    Scope_fail &operator=(Scope_fail &&) = delete;

private:
    int m_exception_count;
    F m_f;
};

#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2)      CONCATENATE_IMPL(s1, s2)

#define SCOPE_EXIT(f) const Scope_exit CONCATENATE(scope_exit_, __LINE__)(f)
#define SCOPE_FAIL(f) const Scope_fail CONCATENATE(scope_fail_, __LINE__)(f)

// alignment MUST be a power of 2
template <std::unsigned_integral U>
[[nodiscard]] constexpr U align_up(U value, U alignment) noexcept
{
    assert((alignment & (alignment - 1)) == 0);
    return (value + (alignment - 1)) & ~(alignment - 1);
}

[[nodiscard]] std::vector<std::uint32_t>
read_binary_file(const char *file_name);

enum struct Text_color
{
    reset,
    red,
    yellow,
    green,
    blue
};

std::ostream &operator<<(std::ostream &os, Text_color color);

#endif // UTILITY_HPP
