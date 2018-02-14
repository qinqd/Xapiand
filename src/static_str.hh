/*
 * Copyright (C) 2017 Andrzej Krzemienski.
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#pragma once

#include <type_traits>
#include <cassert>
#include <cstddef>
#include <string>

namespace static_str {

// # Implementation of a subset of C++14 std::integer_sequence and std::make_integer_sequence

namespace detail
{
	template <int... I>
	struct int_sequence
	{};

	template <int i, typename T>
	struct cat
	{
		static_assert (sizeof(T) < 0, "bad use of cat");
	};

	template <int i, int... I>
	struct cat<i, int_sequence<I...>>
	{
		using type = int_sequence<I..., i>;
	};

	template <int I>
	struct make_int_sequence_
	{
		static_assert (I >= 0, "bad use of make_int_sequence: negative size");
		using type = typename cat<I - 1, typename make_int_sequence_<I - 1>::type>::type;
	};

	template <>
	struct make_int_sequence_<0>
	{
		using type = int_sequence<>;
	};

	template <int I>
	using make_int_sequence = typename make_int_sequence_<I>::type;
}


// # Implementation of a constexpr-compatible assertion
#ifdef NDEBUG
#   define AK_TOOLKIT_ASSERT(CHECK) void(0)
#else
#   define AK_TOOLKIT_ASSERT(CHECK) ((CHECK) ? void(0) : []{assert(!#CHECK);}())
#endif


struct literal_ref {};
struct char_array {};

template <int N, typename T = literal_ref>
class string
{
	static_assert (N > 0 && N < 0, "Invalid specialization of string");
};

// # A wraper over a string literal with alternate interface. No ownership management

template <int N>
class string<N, literal_ref>
{
	const char (&_lit)[N + 1];

public:
	constexpr string(const char (&lit)[N + 1]) : _lit((AK_TOOLKIT_ASSERT(lit[N] == 0), lit)) {}
	constexpr char operator[](int i) const { return AK_TOOLKIT_ASSERT(i >= 0 && i < N), _lit[i]; }
	constexpr std::size_t size() const { return N; };
	constexpr const char* c_str() const { return _lit; }
	constexpr operator const char * () const { return c_str(); }

	operator std::string () const { return std::string(c_str(), N); }
};

template <int N>
using string_literal = string<N, literal_ref>;


// # A function that converts raw string literal into string_literal and deduces the size.

template <int N_PLUS_1>
constexpr string_literal<N_PLUS_1 - 1> literal(const char (&lit)[N_PLUS_1])
{
	return string_literal<N_PLUS_1 - 1>(lit);
}


// # This implements a null-terminated array that stores elements on stack.

template <int N>
class string<N, char_array>
{
	char _array[N + 1];
	struct private_ctor {};

	template <int M, int... Il, int... Ir, typename TL, typename TR>
	constexpr explicit string(private_ctor, string<M, TL> const& l, string<N - M, TR> const& r, detail::int_sequence<Il...>, detail::int_sequence<Ir...>)
	  : _array{l[Il]..., r[Ir]..., 0}
	{
	}

	template <int... Il, typename T>
	constexpr explicit string(private_ctor, string<N, T> const& l, detail::int_sequence<Il...>)
	  : _array{l[Il]..., 0}
	{
	}

public:
	template <int M, typename TL, typename TR, typename std::enable_if<(M < N), bool>::type = true>
	constexpr explicit string(string<M, TL> l, string<N - M, TR> r)
	: string(private_ctor{}, l, r, detail::make_int_sequence<M>{}, detail::make_int_sequence<N - M>{})
	{
	}

	constexpr string(string_literal<N> l) // converting
	: string(private_ctor{}, l, detail::make_int_sequence<N>{})
	{
	}

	constexpr std::size_t size() const { return N; }

	constexpr const char* c_str() const { return _array; }
	constexpr operator const char * () const { return c_str(); }
	constexpr char operator[] (int i) const { return AK_TOOLKIT_ASSERT(i >= 0 && i < N), _array[i]; }

	operator std::string () const { return std::string(c_str(), N); }
};

template <int N>
using array_string = string<N, char_array>;

// # A set of concatenating operators, for different combinations of raw literals, string_literal<>, and array_string<>

template <int NL, typename TL, int NR, typename TR>
constexpr string<NL + NR, char_array> operator+(string<NL, TL> const& l, string<NR, TR> const& r)
{
	return string<NL + NR, char_array>(l, r);
}

template <int NL_PLUS_1, int NR, typename TR>
constexpr string<NL_PLUS_1 - 1 + NR, char_array> operator+(const char (&l)[NL_PLUS_1], string<NR, TR> const& r)
{
	return string<NL_PLUS_1 - 1 + NR, char_array>(string_literal<NL_PLUS_1 - 1>(l), r);
}

template <int NL, typename TL, int NR_PLUS_1>
constexpr string<NL + NR_PLUS_1 - 1, char_array> operator+(string<NL, TL> const& l, const char (&r)[NR_PLUS_1])
{
	return string<NL + NR_PLUS_1 - 1, char_array>(l, string_literal<NR_PLUS_1 - 1>(r));
}

}