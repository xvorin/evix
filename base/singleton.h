#pragma once

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

#include <array>
#include <cinttypes>
#include <mutex>

namespace xvorin::evix {

namespace detail {

template <typename T>
struct has_no_destroy {
    template <typename C>
    static char test(decltype(&C::no_destroy));

    template <typename C>
    static int32_t test(...);

    const static bool value = sizeof(test<T>(0)) == 1;
};

} // namespace xvorin::evix::detail
template <typename T, size_t N = 1>
class Singleton {
public:
    Singleton(const Singleton&) = delete;

    void operator=(const Singleton&) = delete;

    template <size_t M = 0, typename... Args>
    static T& instance(Args&&... args)
    {
        static_assert(M >= 0 && M < N, "Instance Number Illegal!");
        call_once(once_array_[M], [&]() {
            instance_array_[M] = new T(std::forward<Args>(args)...);
            if (!detail::has_no_destroy<T>::value) {
                ::atexit(destroy<M>);
            }
            instance_num_++;
        });
        assert(instance_array_[M] != NULL);
        return *instance_array_[M];
    }

    using element_type = T;

    static size_t instanceNum()
    {
        return instance_num_;
    }

    static constexpr size_t instanceSlots()
    {
        return N;
    }

private:
    Singleton() = delete;

    ~Singleton() = delete;

    template <size_t M>
    static void destroy()
    {
        typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
        T_must_be_complete_type dummy;
        (void)dummy;
        delete instance_array_[M];
        instance_array_[M] = NULL;
        instance_num_--;
    }

    static_assert(N > 0 && N < 256, "Instance Number Must be Bigger than 1, Lesser than 256!");

private:
    static size_t instance_num_;
    static std::array<T*, N> instance_array_;
    static std::array<std::once_flag, N> once_array_;
};

template <typename T, size_t N>
std::array<T*, N> Singleton<T, N>::instance_array_;
template <typename T, size_t N>
std::array<std::once_flag, N> Singleton<T, N>::once_array_;
template <typename T, size_t N>
size_t Singleton<T, N>::instance_num_ = 0;

} // namespace xvorin::evix
