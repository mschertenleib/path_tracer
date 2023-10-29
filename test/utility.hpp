#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <concepts>
#include <type_traits>
#include <utility>
#include <vector>

#include <cstdint>

template <typename EF>
class Scope_guard
{
public:
    template <typename F>
        requires(std::is_constructible_v<EF, F> &&
                 !std::is_lvalue_reference_v<F>)
    explicit Scope_guard(F &&f) : m_exit_fun(std::forward<F>(f))
    {
    }

    template <typename F>
        requires(std::is_constructible_v<EF, F> &&
                 std::is_lvalue_reference_v<F>)
    explicit Scope_guard(F &&f) : m_exit_fun(f)
    {
    }

    ~Scope_guard() noexcept
    {
        m_exit_fun();
    }

    Scope_guard(const Scope_guard &) = delete;
    Scope_guard(Scope_guard &&) = delete;
    Scope_guard &operator=(const Scope_guard &) = delete;
    Scope_guard &operator=(Scope_guard &&) = delete;

private:
    EF m_exit_fun;
};

// alignment MUST be a power of 2
template <std::unsigned_integral U>
[[nodiscard]] constexpr U align_up(U value, U alignment) noexcept
{
    return (value + (alignment - 1)) & ~(alignment - 1);
}

[[nodiscard]] std::vector<std::uint32_t>
read_binary_file(const char *file_name);

#endif // UTILITY_HPP
