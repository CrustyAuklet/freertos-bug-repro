#pragma once
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <memory>
#include <atomic>
#include <larid/inplace_function.hpp>


namespace larid {

    /**
     * @brief A unique pointer with a type erased deleter
     *
     * The deleter of a unique_ptr is usually part of it's type. This means
     * that pointers with different deleters are incompatible and change the type
     * of any function accepting them as an argument. This variant of unique_ptr
     * uses an `inplace_function` to hold the deleter. This makes all `erased_unique_ptr<T>`
     * compatible at the cost of a slightly larger size and a pointer indirection on
     * destruction.
     *
     * @note The deleter has storage for a single pointer, allowing a closure to hold context to it's source
     * @tparam T
     */
    template <typename T>
    using erased_unique_ptr = std::unique_ptr<T, larid::inplace_function<void(void*), sizeof(void*)>>;

    // ensure that erased_unique_ptr is always the size of 3 pointers
    static_assert(sizeof(larid::erased_unique_ptr<int>) == sizeof(void*)*3);

} // namespace larid
