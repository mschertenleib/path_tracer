#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <concepts>
#include <type_traits>
#include <utility>
#include <vector>

#include <cstdint>

template <typename EF>
class Scope_exit
{
public:
    template <typename F>
        requires(std::is_constructible_v<EF, F> &&
                 !std::is_lvalue_reference_v<F>)
    explicit Scope_exit(F &&f) : m_f(std::forward<F>(f))
    {
    }

    template <typename F>
        requires(std::is_constructible_v<EF, F> &&
                 std::is_lvalue_reference_v<F>)
    explicit Scope_exit(F &&f) : m_f(f)
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
    EF m_f;
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
