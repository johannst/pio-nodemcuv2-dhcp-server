// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022, Johannes Stoelp <dev@memzero.de>

#ifndef UTILS_H
#define UTILS_H

#include <type_traits>

// Convert from an underlying enum type into an enum variant.
template<typename E>
constexpr E from_raw(std::underlying_type_t<E> u) {
    static_assert(std::is_enum_v<E>);
    return static_cast<E>(u);
}

// Convert from an enum variant into an underlying enum type.
template<typename E>
constexpr std::underlying_type_t<E> into_raw(E e) {
    static_assert(std::is_enum_v<E>);
    return static_cast<std::underlying_type_t<E>>(e);
}

// Simple cyclic rotation hash function.
constexpr u32 hash(const u8* data, usize len) {
    u32 hash = 0xa5a55a5a /* seed */;
    for (usize i = 0; i < len; ++i) {
        hash += ((hash << 25) | (hash >> 7) /* rrot(7) */) ^ data[i];
    }
    return hash;
}

#endif
