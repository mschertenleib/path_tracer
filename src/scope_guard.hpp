#ifndef SCOPE_GUARD_HPP
#define SCOPE_GUARD_HPP

#include <stdexcept>
#include <utility>

template <typename F>
class Scope_exit
{
public:
    explicit Scope_exit(F &&f) : m_f {std::forward<F>(f)}
    {
    }

    ~Scope_exit()
    {
        m_f();
    }

    Scope_exit(const Scope_exit &) = delete;
    Scope_exit &operator=(const Scope_exit &) = delete;

    Scope_exit(Scope_exit &&) noexcept = default;
    Scope_exit &operator=(Scope_exit &&) noexcept = default;

private:
    F m_f;
};

template <typename F>
Scope_exit(F &&) -> Scope_exit<F>;

template <typename F>
class Scope_fail
{
public:
    explicit Scope_fail(F &&f)
        : m_f {std::forward<F>(f)},
          m_exception_count {std::uncaught_exceptions()}
    {
    }

    ~Scope_fail()
    {
        if (std::uncaught_exceptions() > m_exception_count)
        {
            m_f();
        }
    }

    Scope_fail(const Scope_fail &) = delete;
    Scope_fail &operator=(const Scope_fail &) = delete;

    Scope_fail(Scope_fail &&) noexcept = default;
    Scope_fail &operator=(Scope_fail &&) noexcept = default;

private:
    F m_f;
    int m_exception_count;
};

template <typename F>
Scope_fail(F &&) -> Scope_fail<F>;

#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2)      CONCATENATE_IMPL(s1, s2)

#ifdef __COUNTER__
#define ANONYMIZE(name) CONCATENATE(name, __COUNTER__)
#else
#define ANONYMIZE(name) CONCATENATE(name, __LINE__)
#endif

#define SCOPE_EXIT(func) auto ANONYMIZE(scope_exit_state_) = Scope_exit(func)
#define SCOPE_FAIL(func) auto ANONYMIZE(scope_fail_state_) = Scope_fail(func)

#endif // SCOPE_GUARD_HPP
