#pragma once

#include <array>
#include <cstddef>

#ifndef __cpp_lib_to_array
	#define __cpp_lib_to_array 201907L

namespace std {
namespace detail {

template <class T, std::size_t N, std::size_t... I>
constexpr std::array<std::remove_cv_t<T>, N>
to_array_impl(T (&&a)[N], std::index_sequence<I...>) {
	return {{std::move(a[I])...}};
} // namespace detail

} // namespace detail

template <class T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&&a)[N]) {
	return detail::to_array_impl(std::move(a), std::make_index_sequence<N> {});
}

} // namespace std

#endif
