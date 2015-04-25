#ifndef THROW_H
#define THROW_H

#include <utility>

// Reduce code size by letting the compiler break out throwing
// details into a function.
template <typename E, typename... Args>
[[noreturn]] void throw_(Args... args) {
    throw E(std::forward<Args>(args)...);
}

#endif
