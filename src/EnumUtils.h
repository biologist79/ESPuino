#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

/*
helper for converting to/from enum classes
Example:

	enum class ExampleEnum : int {
		a = 1,
		b = 3,
		c = 5
	};

	constexpr int a = EnumUtils::underlying_value(ExampleEnum::c); // a = 5
	constexpr ExampleEnum b = EnumUtils::to_enum<ExampleEnum>(a); // b = ExampleEnum::c
	constexpr int c = EnumUtils::underlying_value(b); // c = 5
*/
namespace EnumUtils {
namespace details {
template <typename E>
using enable_enum_t = typename std::enable_if<std::is_enum<E>::value, typename std::underlying_type<E>::type>::type;
}

template <typename E>
constexpr inline details::enable_enum_t<E> underlying_value(E e) noexcept {
	return static_cast<typename std::underlying_type<E>::type>(e);
}

template <typename E, typename T>
constexpr inline typename std::enable_if<std::is_enum<E>::value && std::is_integral<T>::value, E>::type to_enum(T value) noexcept {
	return static_cast<E>(value);
}
} // namespace EnumUtils
