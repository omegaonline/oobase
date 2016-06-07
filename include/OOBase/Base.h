///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2013 Rick Taylor
//
// This file is part of OOBase, the Omega Online Base library.
//
// OOBase is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// OOBase is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with OOBase.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////////

#ifndef OOBASE_BASE_H_INCLUDED_
#define OOBASE_BASE_H_INCLUDED_

#if defined(HAVE_CONFIG_H)
#include <oobase-platform.h>
#elif defined(_MSC_VER)

#if (_MSC_VER < 1310)
	#error OOBase will not compile with a pre Visual C++ .NET 2003 compiler
#endif

#ifndef _MT
#error You must enable multithreaded library use /MT, /MTd, /MD or /MDd
#endif

#if defined(_WIN32) || defined(_WIN32_WCE)
	/* Define to 1 if you have the <windows.h> header file. */
	#define HAVE_WINDOWS_H 1
#else
	#error What else can MSVC compile?
#endif

#endif

////////////////////////////////////////
// Try to work out what's going on with MS Windows
#if defined(HAVE_WINDOWS_H)
	#if !defined(_WIN32)
	#error No _WIN32 ?!?
	#endif

	// Prevent inclusion of old winsock
	#define _WINSOCKAPI_

	// Reduce the amount of windows we include
	#define WIN32_LEAN_AND_MEAN
	#if !defined(STRICT)
	#define STRICT
	#endif

	// We support Vista API's
	#if !defined(_WIN32_WINNT)
	#define _WIN32_WINNT 0x0600
	#elif _WIN32_WINNT < 0x0500
	#error OOBase requires _WIN32_WINNT >= 0x0500!
	#endif

	// We require IE 5 or later
	#if !defined(_WIN32_IE)
	#define _WIN32_IE 0x0500
	#elif _WIN32_IE < 0x0500
	#error OOBase requires _WIN32_IE >= 0x0500!
	#endif

	#if !defined(WINVER)
	#define WINVER _WIN32_WINNT
	#endif

	#include <windows.h>

	// Check for obsolete windows versions
	#if defined(_WIN32_WINDOWS)
	#error You cannot build OOBase for Windows 95/98/Me!
	#endif

	// Remove the unistd include - we are windows
	#if defined(HAVE_UNISTD_H)
	#undef HAVE_UNISTD_H
	#endif
#endif

////////////////////////////////////////
// Bring in POSIX if possible
#if defined(HAVE_UNISTD_H)
	#include <unistd.h>

	// check for POSIX.1 IEEE 1003.1
	#if !defined(_POSIX_VERSION)
	#error <unistd.h> is not POSIX compliant?
	#endif

	// Check pthreads
	#if defined (HAVE_PTHREAD)
	#include <pthread.h>
	#if !defined(_POSIX_THREADS)
	#error Your pthreads does not appears to be POSIX compliant?
	#endif
	#endif
#endif

#if defined(_MSC_VER)
namespace OOBase
{
	typedef __int8 int8_t;
	typedef unsigned __int8 uint8_t;
	typedef __int16 int16_t;
	typedef unsigned __int16 uint16_t;
	typedef __int32 int32_t;
	typedef unsigned __int32 uint32_t;
	typedef __int64 int64_t;
	typedef unsigned __int64 uint64_t;
}
#elif (HAVE_STDINT_H)
#include <stdint.h>

#if defined(__cplusplus)
namespace OOBase
{
	using ::int8_t;
	using ::uint8_t;
	using ::int16_t;
	using ::uint16_t;
	using ::int32_t;
	using ::uint32_t;
	using ::int64_t;
	using ::uint64_t;
}
#endif

#else
#error Define uint64_t
#endif

////////////////////////////////////////
// Some standard headers.
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>

////////////////////////////////////////
// Define some standard errors to avoid lots of #ifdefs.

#if !defined(ERROR_OUTOFMEMORY) && defined(ENOMEM) && !defined(DOXYGEN)
#define ERROR_OUTOFMEMORY ENOMEM
#endif

#if defined(_MSC_VER)
#define OOBASE_NORETURN __declspec(noreturn)
#else
#define OOBASE_NORETURN
#endif

////////////////////////////////////////
// Forward declare custom error handling

#ifdef __cplusplus

/// Call OOBase::CallCriticalFailure passing __FILE__ and __LINE__
#define OOBase_CallCriticalFailure(expr) \
	OOBase::CallCriticalFailure(__FILE__,__LINE__,expr)

#if defined(_MSC_VER) && defined(_CPPUNWIND)
#define OOBASE_HAVE_EXCEPTIONS 1
#elif defined(__clang__)
#if __has_extension(__EXCEPTIONS)
#define OOBASE_HAVE_EXCEPTIONS 1
#endif
#elif defined(__GNUC__) && defined(__EXCEPTIONS)
#define OOBASE_HAVE_EXCEPTIONS 1
#endif

namespace OOBase
{
	void OOBASE_NORETURN CallCriticalFailure(const char* pszFile, unsigned int nLine, const char*);
	void OOBASE_NORETURN CallCriticalFailure(const char* pszFile, unsigned int nLine, int);

	int system_error();

	/// Return the system supplied error string from error code 'err'.
	const char* system_error_text(int err = system_error());

	int stderr_write(const char* sz, size_t len = size_t(-1));
	int stdout_write(const char* sz, size_t len = size_t(-1));

#if defined(_WIN32)
	int stderr_write(const wchar_t* wsz, size_t len = size_t(-1));
	int stdout_write(const wchar_t* wsz, size_t len = size_t(-1));
#endif

	typedef bool (*OnCriticalFailure)(const char*);

	/// Override the default critical failure behaviour
	OnCriticalFailure SetCriticalFailure(OnCriticalFailure pfn);
}

#if defined(DOXYGEN)
/// Compile time assertion, assert that expr == true
#define static_assert(expr,msg)
#elif (_MSC_VER >= 1500)
// Builtin static_assert
#elif (__cplusplus <= 199711L)
#define static_assert(expr,msg) \
	{ struct oobase_static_assert { char static_check[expr ? 1 : -1]; }; }
#endif

#if defined(__GNUC__)
#define OOBASE_FORMAT(f,a,b) __attribute__((format(f,a,b)))
#else
#define OOBASE_FORMAT(f,a,b)
#endif

#if defined(_MSC_VER)
#define HAVE__IS_POD 1
#define HAVE__ALIGNOF 1
#define ALIGNOF(X) __alignof(X)
#define IS_POD(X) (!__is_class(X) || __is_pod(X))
#elif defined(__GNUC__) && (__GNUC__ >= 3)
#define HAVE__ALIGNOF 1
#define ALIGNOF(X) __alignof__(X)
#endif

#if defined(__clang__)
#if __has_extension(is_pod)
#define HAVE__IS_POD 1
#define IS_POD(X) __is_pod(X)
#endif
#elif defined(__GNUG__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#define HAVE__IS_POD 1
#define IS_POD(X) __is_pod(X)
#endif

#if !defined(HAVE__IS_POD)
#if defined(min)
#undef min
#endif
#if defined(max)
#undef max
#endif
#include <limits>
#endif

namespace OOBase
{
	template <typename T>
	struct add_const
	{
		typedef T const type;
	};

	template <typename T>
	struct add_const<T const>
	{
		typedef T const type;
	};

	template <typename T>
	struct remove_const
	{
		typedef T type;
	};

	template <typename T>
	struct remove_const<T const>
	{
		typedef T type;
	};

	template <typename T>
	struct add_reference
	{
		typedef T& type;
	};

	template <typename T>
	struct add_reference<T&>
	{
		typedef T& type;
	};

	template <typename T>
	struct remove_reference
	{
		typedef T type;
	};

	template <typename T>
	struct remove_reference<T&>
	{
		typedef T type;
	};

	template <typename T>
	struct add_const_ref
	{
		typedef T const& type;
	};

	template <typename T>
	struct add_const_ref<T const>
	{
		typedef T const& type;
	};

	template <typename T>
	struct add_const_ref<T&>
	{
		typedef T const& type;
	};

	template <typename T>
	struct add_const_ref<T const&>
	{
		typedef T const& type;
	};

	template <typename T>
	struct remove_const_ref
	{
		typedef T type;
	};

	template <typename T>
	struct remove_const_ref<T const>
	{
		typedef T type;
	};

	template <typename T>
	struct remove_const_ref<T&>
	{
		typedef T type;
	};

	template <typename T>
	struct remove_const_ref<T const&>
	{
		typedef T type;
	};

	template <typename T>
	struct add_pointer
	{
		typedef T* type;
	};

	template <typename T>
	struct add_pointer<T&>
	{
		typedef T* type;
	};

	template <typename T>
	struct alignment_of
	{
#if defined(HAVE__ALIGNOF)
		static const size_t value = ALIGNOF(T);
#else
		static const size_t value = sizeof(T) > 16 ? 16 : sizeof(T);
#endif
	};

	namespace detail
	{
		template <typename T>
		struct is_pod
		{
#if defined(HAVE__IS_POD)
			static const bool value = IS_POD(T);
#else
			static const bool value = std::numeric_limits<T>::is_specialized;
#endif
		};

		template <>
		struct is_pod<void>
		{
			static const bool value = true;
		};

		template <typename T>
		struct is_pod<T&>
		{
			static const bool value = false;
		};

		template <typename T>
		struct is_pod<T*>
		{
			static const bool value = true;
		};

		template <typename T>
		struct is_pod<T const>
		{
			static const bool value = is_pod<T>::value;
		};

		template <typename T>
		struct is_pod<T volatile>
		{
			static const bool value = is_pod<T>::value;
		};

		template <typename T, bool pod = true>
		struct call_traits_impl
		{
			typedef T const param_type;
		};

		template <typename T>
		struct call_traits_impl<T,false>
		{
			typedef T const& param_type;
		};

		namespace swap
		{
			template <typename Type>
			struct has_swap
			{
			private:
				// SFINAE
				typedef char (&yes)[2];

				// MSVC way
				template <typename T,void (T::*)(T&)> struct ptmf_helper {};
				template <typename T> static yes test(ptmf_helper<T,&T::swap>*);

				template <typename T> static char test(...);

			public:
				static const bool value = (sizeof(test<Type>(NULL)) == sizeof(yes));
			};

			template <bool s = true>
			struct swap_impl
			{
				template <typename T>
				static void swap(T& lhs, T& rhs)
				{
					lhs.swap(rhs);
				}
			};

			template <>
			struct swap_impl<false>
			{
				template <typename T>
				static void swap(T& lhs, T& rhs)
				{
					T temp(lhs);
					lhs = rhs;
					rhs = temp;
				}
			};
		}
	}

	template <typename T>
	struct call_traits
	{
		typedef typename detail::call_traits_impl<T,detail::is_pod<T>::value && sizeof(T) <= sizeof(const T&)>::param_type param_type;
	};

	template <typename T>
	struct call_traits<T&>
	{
		typedef T& param_type;
	};

	template <typename T>
	struct call_traits<const T&>
	{
		typedef const T& param_type;
	};

	template <typename T>
	inline void swap(T& lhs, T& rhs)
	{
		detail::swap::swap_impl<detail::swap::has_swap<T>::value>::swap(lhs,rhs);
	}

	struct NonCopyable
	{
	protected:
		NonCopyable() {}

	private:
		NonCopyable(const NonCopyable&);
		NonCopyable& operator = (const NonCopyable&);
	};

	struct SafeBoolean
	{
	public:
		typedef void (SafeBoolean::*bool_type)() const;
		void this_type_does_not_support_comparisons() const {}

		bool_type safe_bool(bool v) const
		{
			return (v ? &SafeBoolean::this_type_does_not_support_comparisons : NULL);
		}
	};

	template <typename T>
	struct Less
	{
		template <typename T1>
		bool operator() (const T& lhs, const T1& rhs) const
		{
			return lhs < rhs;
		}
	};

	template <typename T>
	struct Greater
	{
		template <typename T1>
		bool operator() (const T& lhs, const T1& rhs) const
		{
			return lhs > rhs;
		}
	};

	template <typename T1, typename T2>
	class Pair
	{
	public:
		typedef T1 first_type;
		typedef T2 second_type;

		Pair() : first(), second()
		{}

		template <typename U1, typename U2>
		Pair(const Pair<U1,U2>& n) : first(n.first), second(n.second)
		{}

		Pair(const first_type& f, const second_type& s) : first(f), second(s)
		{}

		Pair& operator = (const Pair& rhs)
		{
			Pair(rhs).swap(*this);
			return *this;
		}

		void swap(Pair& rhs)
		{
			OOBase::swap(first,rhs.first);
			OOBase::swap(second,rhs.second);
		}

		T1 first;
		T2 second;
	};

	template <class T1,class T2>
	Pair<T1,T2> make_pair(T1 x, T2 y)
	{
		return Pair<T1,T2>(x,y);
	}

	template <class T1, class T2>
	inline bool operator == (const Pair<T1,T2>& lhs, const Pair<T1,T2>& rhs)
	{
		return lhs.first==rhs.first && lhs.second==rhs.second;
	}

	template <class T1, class T2>
	inline bool operator != (const Pair<T1,T2>& lhs, const Pair<T1,T2>& rhs)
	{
		return !(lhs == rhs);
	}

	template <class T1, class T2>
	inline bool operator < (const Pair<T1,T2>& lhs, const Pair<T1,T2>& rhs)
	{
		return lhs.first<rhs.first || (!(rhs.first<lhs.first) && lhs.second<rhs.second);
	}

	template <class T1, class T2>
	inline bool operator <= (const Pair<T1,T2>& lhs, const Pair<T1,T2>& rhs)
	{
		return !(rhs < lhs);
	}

	template <class T1, class T2>
	inline bool operator > (const Pair<T1,T2>& lhs, const Pair<T1,T2>& rhs)
	{
		return rhs < lhs;
	}

	template <class T1, class T2>
	inline bool operator >= (const Pair<T1,T2>& lhs, const Pair<T1,T2>& rhs)
	{
		return !(lhs < rhs);
	}

	// The discrimination type for singleton scoping for this module
	struct Module
	{
		int unused;
	};
}

#endif // __cplusplus

#endif // OOBASE_BASE_H_INCLUDED_
