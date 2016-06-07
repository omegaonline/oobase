///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2015 Rick Taylor
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

#ifndef OOBASE_DELEGATE_H_INCLUDED_
#define OOBASE_DELEGATE_H_INCLUDED_

#include "SharedPtr.h"

namespace OOBase
{
	template <typename R, typename Allocator = CrtAllocator>
	class Delegate0 : public SafeBoolean
	{
	public:
		Delegate0(R (*fn)() = NULL) : m_static(fn)
		{
		}

		template<typename T>
		Delegate0(T* p, R (T::*fn)()) : m_static(NULL)
		{
			m_ptr = OOBase::allocate_shared<Thunk<T>,Allocator>(p,fn);
		}

		bool operator == (const Delegate0& rhs) const
		{
			if (this == &rhs)
				return true;
			return m_ptr == rhs.m_ptr && m_static == rhs.m_static;
		}

		operator bool_type() const
		{
			return SafeBoolean::safe_bool(m_ptr || m_static != NULL);
		}

		R invoke() const
		{
			if (m_ptr)
				return m_ptr->thunk();
			return (*m_static)();
		}

		void swap(Delegate0& rhs)
		{
			m_ptr.swap(rhs.m_ptr);
			OOBase::swap(m_static,rhs.m_static);
		}

#if defined(OOBASE_CDR_STREAM_H_INCLUDED_)
		bool read(CDRStream& stream)
		{
			return (m_ptr.read(stream) && stream.read(m_static));
		}

		bool write(CDRStream& stream) const
		{
			return (m_ptr.write(stream) && stream.write(m_static));
		}
#endif

	private:
		struct ThunkBase
		{
			virtual R thunk() = 0;
		};

		template<typename T>
		struct Thunk : public ThunkBase
		{
			Thunk(T* p, R (T::*fn)()) : m_p(p), m_fn(fn)
			{}

			R thunk()
			{
				return (m_p->*m_fn)();
			}

			T* m_p;
			R (T::*m_fn)();
		};

		SharedPtr<ThunkBase> m_ptr;
		R (*m_static)();
	};

	template <typename Allocator, typename R>
	Delegate0<R,Allocator> make_delegate(R (*fn)())
	{
		return Delegate0<R,Allocator>(fn);
	}

	template <typename R>
	Delegate0<R,CrtAllocator> make_delegate(R (*fn)())
	{
		return make_delegate<CrtAllocator,R>(fn);
	}

	template <typename Allocator, typename T, typename R>
	Delegate0<R,Allocator> make_delegate(T* p, R (T::*fn)())
	{
		return Delegate0<R,Allocator>(p,fn);
	}

	template <typename T, typename R>
	Delegate0<R,CrtAllocator> make_delegate(T* p, R (T::*fn)())
	{
		return make_delegate<CrtAllocator,T,R>(p,fn);
	}

	template <typename R, typename P1, typename Allocator = CrtAllocator>
	class Delegate1 : public SafeBoolean
	{
	public:
		Delegate1(R (*fn)(P1) = NULL) : m_static(fn)
		{
		}

		template<typename T>
		Delegate1(T* p, R (T::*fn)(P1)) : m_static(NULL)
		{
			m_ptr = OOBase::allocate_shared<Thunk<T>,Allocator>(p,fn);
		}

		bool operator == (const Delegate1& rhs) const
		{
			if (this == &rhs)
				return true;
			return m_ptr == rhs.m_ptr && m_static == rhs.m_static;
		}

		operator bool_type() const
		{
			return SafeBoolean::safe_bool(m_ptr || m_static != NULL);
		}

		R invoke(typename call_traits<P1>::param_type p1) const
		{
			if (m_ptr)
				return m_ptr->thunk(p1);
			return (*m_static)(p1);
		}

		void swap(Delegate1& rhs)
		{
			m_ptr.swap(rhs.m_ptr);
			OOBase::swap(m_static,rhs.m_static);
		}

#if defined(OOBASE_CDR_STREAM_H_INCLUDED_)
		bool read(CDRStream& stream)
		{
			return (m_ptr.read(stream) && stream.read(m_static));
		}

		bool write(CDRStream& stream) const
		{
			return (m_ptr.write(stream) && stream.write(m_static));
		}
#endif

	private:
		struct ThunkBase
		{
			virtual R thunk(typename call_traits<P1>::param_type) = 0;
		};

		template<typename T>
		struct Thunk : public ThunkBase
		{
			Thunk(T* p, R (T::*fn)(P1)) : m_p(p), m_fn(fn)
			{}

			R thunk(typename call_traits<P1>::param_type p1)
			{
				return (m_p->*m_fn)(p1);
			}

			T* m_p;
			R (T::*m_fn)(P1);
		};

		SharedPtr<ThunkBase> m_ptr;
		R (*m_static)(P1);
	};

	template <typename Allocator, typename R, typename P1>
	Delegate1<R,P1,Allocator> make_delegate(R (*fn)(P1))
	{
		return Delegate1<R,P1,Allocator>(fn);
	}

	template <typename R, typename P1>
	Delegate1<R,P1,CrtAllocator> make_delegate(R (*fn)(P1))
	{
		return make_delegate<CrtAllocator,R,P1>(fn);
	}

	template <typename Allocator, typename T, typename R, typename P1>
	Delegate1<R,P1,Allocator> make_delegate(T* p, R (T::*fn)(P1))
	{
		return Delegate1<R,P1,Allocator>(p,fn);
	}

	template <typename T, typename R, typename P1>
	Delegate1<R,P1,CrtAllocator> make_delegate(T* p, R (T::*fn)(P1))
	{
		return make_delegate<CrtAllocator,T,R,P1>(p,fn);
	}

	template <typename R, typename P1, typename P2, typename Allocator = CrtAllocator>
	class Delegate2 : public SafeBoolean
	{
	public:
		Delegate2(R (*fn)(P1,P2) = NULL) : m_static(fn)
		{
		}

		template<typename T>
		Delegate2(T* p, R (T::*fn)(P1,P2)) : m_static(NULL)
		{
			m_ptr = OOBase::allocate_shared<Thunk<T>,Allocator>(p,fn);
		}

		bool operator == (const Delegate2& rhs) const
		{
			if (this == &rhs)
				return true;
			return m_ptr == rhs.m_ptr && m_static == rhs.m_static;
		}

		operator bool_type() const
		{
			return SafeBoolean::safe_bool(m_ptr || m_static != NULL);
		}

		R invoke(typename call_traits<P1>::param_type p1, typename call_traits<P2>::param_type p2) const
		{
			if (m_ptr)
				return m_ptr->thunk(p1,p2);
			return (*m_static)(p1,p2);
		}

		void swap(Delegate2& rhs)
		{
			m_ptr.swap(rhs.m_ptr);
			OOBase::swap(m_static,rhs.m_static);
		}

#if defined(OOBASE_CDR_STREAM_H_INCLUDED_)
		bool read(CDRStream& stream)
		{
			return (m_ptr.read(stream) && stream.read(m_static));
		}

		bool write(CDRStream& stream) const
		{
			return (m_ptr.write(stream) && stream.write(m_static));
		}
#endif

	private:
		struct ThunkBase
		{
			virtual R thunk(typename call_traits<P1>::param_type, typename call_traits<P2>::param_type) = 0;
		};

		template<typename T>
		struct Thunk : public ThunkBase
		{
			Thunk(T* p, R (T::*fn)(P1,P2)) : m_p(p), m_fn(fn)
			{}

			R thunk(typename call_traits<P1>::param_type p1, typename call_traits<P2>::param_type p2)
			{
				return (m_p->*m_fn)(p1,p2);
			}

			T* m_p;
			R (T::*m_fn)(P1,P2);
		};

		SharedPtr<ThunkBase> m_ptr;
		R (*m_static)(P1,P2);
	};

	template <typename Allocator, typename R, typename P1, typename P2>
	Delegate2<R,P1,P2,Allocator> make_delegate(R (*fn)(P1,P2))
	{
		return Delegate2<R,P1,P2,Allocator>(fn);
	}

	template <typename R, typename P1, typename P2>
	Delegate2<R,P1,P2,CrtAllocator> make_delegate(R (*fn)(P1,P2))
	{
		return make_delegate<CrtAllocator,R,P1,P2>(fn);
	}

	template <typename Allocator, typename T, typename R, typename P1, typename P2>
	Delegate2<R,P1,P2,Allocator> make_delegate(T* p, R (T::*fn)(P1,P2))
	{
		return Delegate2<R,P1,P2,Allocator>(p,fn);
	}

	template <typename T, typename R, typename P1, typename P2>
	Delegate2<R,P1,P2,CrtAllocator> make_delegate(T* p, R (T::*fn)(P1,P2))
	{
		return make_delegate<CrtAllocator,T,R,P1,P2>(p,fn);
	}

	template <typename R, typename P1, typename P2, typename P3, typename Allocator = CrtAllocator>
	class Delegate3 : public SafeBoolean
	{
	public:
		Delegate3(R (*fn)(P1,P2,P3) = NULL) : m_static(fn)
		{
		}

		template<typename T>
		Delegate3(T* p, R (T::*fn)(P1,P2,P3)) : m_static(NULL)
		{
			m_ptr = OOBase::allocate_shared<Thunk<T>,Allocator>(p,fn);
		}

		bool operator == (const Delegate3& rhs) const
		{
			if (this == &rhs)
				return true;
			return m_ptr == rhs.m_ptr && m_static == rhs.m_static;
		}

		operator bool_type() const
		{
			return SafeBoolean::safe_bool(m_ptr || m_static != NULL);
		}

		R invoke(typename call_traits<P1>::param_type p1, typename call_traits<P2>::param_type p2, typename call_traits<P3>::param_type p3) const
		{
			if (m_ptr)
				return m_ptr->thunk(p1,p2,p3);
			return (*m_static)(p1,p2,p3);
		}

		void swap(Delegate3& rhs)
		{
			m_ptr.swap(rhs.m_ptr);
			OOBase::swap(m_static,rhs.m_static);
		}

#if defined(OOBASE_CDR_STREAM_H_INCLUDED_)
		bool read(CDRStream& stream)
		{
			return (m_ptr.read(stream) && stream.read(m_static));
		}

		bool write(CDRStream& stream) const
		{
			return (m_ptr.write(stream) && stream.write(m_static));
		}
#endif

	private:
		struct ThunkBase
		{
			virtual R thunk(typename call_traits<P1>::param_type, typename call_traits<P2>::param_type, typename call_traits<P3>::param_type) = 0;
		};

		template<typename T>
		struct Thunk : public ThunkBase
		{
			Thunk(T* p, R (T::*fn)(P1,P2,P3)) : m_p(p), m_fn(fn)
			{}

			R thunk(typename call_traits<P1>::param_type p1, typename call_traits<P2>::param_type p2, typename call_traits<P3>::param_type p3)
			{
				return (m_p->*m_fn)(p1,p2,p3);
			}

			T* m_p;
			R (T::*m_fn)(P1,P2,P3);
		};

		SharedPtr<ThunkBase> m_ptr;
		R (*m_static)(P1,P2,P3);
	};

	template <typename Allocator, typename R, typename P1, typename P2, typename P3>
	Delegate3<R,P1,P2,P3,Allocator> make_delegate(R (*fn)(P1,P2,P3))
	{
		return Delegate3<R,P1,P2,P3,Allocator>(fn);
	}

	template <typename R, typename P1, typename P2, typename P3>
	Delegate3<R,P1,P2,P3,CrtAllocator> make_delegate(R (*fn)(P1,P2,P3))
	{
		return make_delegate<CrtAllocator,R,P1,P2,P3>(fn);
	}

	template <typename Allocator, typename T, typename R, typename P1, typename P2, typename P3>
	Delegate3<R,P1,P2,P3,Allocator> make_delegate(T* p, R (T::*fn)(P1,P2,P3))
	{
		return Delegate3<R,P1,P2,P3,Allocator>(p,fn);
	}

	template <typename T, typename R, typename P1, typename P2, typename P3>
	Delegate3<R,P1,P2,P3,CrtAllocator> make_delegate(T* p, R (T::*fn)(P1,P2,P3))
	{
		return make_delegate<CrtAllocator,T,R,P1,P2,P3>(p,fn);
	}

	template <typename R, typename P1, typename P2, typename P3, typename P4, typename Allocator = CrtAllocator>
	class Delegate4 : public SafeBoolean
	{
	public:
		Delegate4(R (*fn)(P1,P2,P3,P4) = NULL) : m_static(fn)
		{
		}

		template<typename T>
		Delegate4(T* p, R (T::*fn)(P1,P2,P3,P4)) : m_static(NULL)
		{
			m_ptr = OOBase::allocate_shared<Thunk<T>,Allocator>(p,fn);
		}

		bool operator == (const Delegate4& rhs) const
		{
			if (this == &rhs)
				return true;
			return m_ptr == rhs.m_ptr && m_static == rhs.m_static;
		}

		operator bool_type() const
		{
			return SafeBoolean::safe_bool(m_ptr || m_static != NULL);
		}

		R invoke(typename call_traits<P1>::param_type p1, typename call_traits<P2>::param_type p2, typename call_traits<P3>::param_type p3, typename call_traits<P4>::param_type p4) const
		{
			if (m_ptr)
				return m_ptr->thunk(p1,p2,p3,p4);
			return (*m_static)(p1,p2,p3,p4);
		}

		void swap(Delegate4& rhs)
		{
			m_ptr.swap(rhs.m_ptr);
			OOBase::swap(m_static,rhs.m_static);
		}

#if defined(OOBASE_CDR_STREAM_H_INCLUDED_)
		bool read(CDRStream& stream)
		{
			return (m_ptr.read(stream) && stream.read(m_static));
		}

		bool write(CDRStream& stream) const
		{
			return (m_ptr.write(stream) && stream.write(m_static));
		}
#endif

	private:
		struct ThunkBase
		{
			virtual R thunk(typename call_traits<P1>::param_type, typename call_traits<P2>::param_type, typename call_traits<P3>::param_type, typename call_traits<P4>::param_type) = 0;
		};

		template<typename T>
		struct Thunk : public ThunkBase
		{
			Thunk(T* p, R (T::*fn)(P1,P2,P3,P4)) : m_p(p), m_fn(fn)
			{}

			R thunk(typename call_traits<P1>::param_type p1, typename call_traits<P2>::param_type p2, typename call_traits<P3>::param_type p3, typename call_traits<P4>::param_type p4)
			{
				return (m_p->*m_fn)(p1,p2,p3,p4);
			}

			T* m_p;
			R (T::*m_fn)(P1,P2,P3,P4);
		};

		SharedPtr<ThunkBase> m_ptr;
		R (*m_static)(P1,P2,P3,P4);
	};

	template <typename Allocator, typename R, typename P1, typename P2, typename P3, typename P4>
	Delegate4<R,P1,P2,P3,P4,Allocator> make_delegate(R (*fn)(P1,P2,P3,P4))
	{
		return Delegate4<R,P1,P2,P3,P4,Allocator>(fn);
	}

	template <typename R, typename P1, typename P2, typename P3, typename P4>
	Delegate4<R,P1,P2,P3,P4,CrtAllocator> make_delegate(R (*fn)(P1,P2,P3,P4))
	{
		return make_delegate<CrtAllocator,R,P1,P2,P3,P4>(fn);
	}

	template <typename Allocator, typename T, typename R, typename P1, typename P2, typename P3, typename P4>
	Delegate4<R,P1,P2,P3,P4,Allocator> make_delegate(T* p, R (T::*fn)(P1,P2,P3,P4))
	{
		return Delegate4<R,P1,P2,P3,P4,Allocator>(p,fn);
	}

	template <typename T, typename R, typename P1, typename P2, typename P3, typename P4>
	Delegate4<R,P1,P2,P3,P4,CrtAllocator> make_delegate(T* p, R (T::*fn)(P1,P2,P3,P4))
	{
		return make_delegate<CrtAllocator,T,R,P1,P2,P3,P4>(p,fn);
	}

	template <typename R, typename P1, typename P2, typename P3, typename P4, typename P5, typename Allocator = CrtAllocator>
	class Delegate5 : public SafeBoolean
	{
	public:
		Delegate5(R (*fn)(P1,P2,P3,P4,P5) = NULL) : m_static(fn)
		{
		}

		template<typename T>
		Delegate5(T* p, R (T::*fn)(P1,P2,P3,P4,P5)) : m_static(NULL)
		{
			m_ptr = OOBase::allocate_shared<Thunk<T>,Allocator>(p,fn);
		}

		bool operator == (const Delegate5& rhs) const
		{
			if (this == &rhs)
				return true;
			return m_ptr == rhs.m_ptr && m_static == rhs.m_static;
		}

		operator bool_type() const
		{
			return SafeBoolean::safe_bool(m_ptr || m_static != NULL);
		}

		R invoke(typename call_traits<P1>::param_type p1, typename call_traits<P2>::param_type p2, typename call_traits<P3>::param_type p3, typename call_traits<P4>::param_type p4, typename call_traits<P5>::param_type p5) const
		{
			if (m_ptr)
				return m_ptr->thunk(p1,p2,p3,p4,p5);
			return (*m_static)(p1,p2,p3,p4,p5);
		}

		void swap(Delegate5& rhs)
		{
			m_ptr.swap(rhs.m_ptr);
			OOBase::swap(m_static,rhs.m_static);
		}

#if defined(OOBASE_CDR_STREAM_H_INCLUDED_)
		bool read(CDRStream& stream)
		{
			return (m_ptr.read(stream) && stream.read(m_static));
		}

		bool write(CDRStream& stream) const
		{
			return (m_ptr.write(stream) && stream.write(m_static));
		}
#endif

	private:
		struct ThunkBase
		{
			virtual R thunk(typename call_traits<P1>::param_type, typename call_traits<P2>::param_type, typename call_traits<P3>::param_type, typename call_traits<P4>::param_type, typename call_traits<P5>::param_type) = 0;
		};

		template<typename T>
		struct Thunk : public ThunkBase
		{
			Thunk(T* p, R (T::*fn)(P1,P2,P3,P4,P5)) : m_p(p), m_fn(fn)
			{}

			R thunk(typename call_traits<P1>::param_type p1, typename call_traits<P2>::param_type p2, typename call_traits<P3>::param_type p3, typename call_traits<P4>::param_type p4, typename call_traits<P5>::param_type p5)
			{
				return (m_p->*m_fn)(p1,p2,p3,p4,p5);
			}

			T* m_p;
			R (T::*m_fn)(P1,P2,P3,P4,P5);
		};

		SharedPtr<ThunkBase> m_ptr;
		R (*m_static)(P1,P2,P3,P4,P5);
	};

	template <typename Allocator, typename R, typename P1, typename P2, typename P3, typename P4, typename P5>
	Delegate5<R,P1,P2,P3,P4,P5,Allocator> make_delegate(R (*fn)(P1,P2,P3,P4,P5))
	{
		return Delegate5<R,P1,P2,P3,P4,P5,Allocator>(fn);
	}

	template <typename R, typename P1, typename P2, typename P3, typename P4, typename P5>
	Delegate5<R,P1,P2,P3,P4,P5,CrtAllocator> make_delegate(R (*fn)(P1,P2,P3,P4,P5))
	{
		return make_delegate<CrtAllocator,R,P1,P2,P3,P4,P5>(fn);
	}

	template <typename Allocator, typename T, typename R, typename P1, typename P2, typename P3, typename P4, typename P5>
	Delegate5<R,P1,P2,P3,P4,P5,Allocator> make_delegate(T* p, R (T::*fn)(P1,P2,P3,P4,P5))
	{
		return Delegate5<R,P1,P2,P3,P4,P5,Allocator>(p,fn);
	}

	template <typename T, typename R, typename P1, typename P2, typename P3, typename P4, typename P5>
	Delegate5<R,P1,P2,P3,P4,P5,CrtAllocator> make_delegate(T* p, R (T::*fn)(P1,P2,P3,P4,P5))
	{
		return make_delegate<CrtAllocator,T,R,P1,P2,P3,P4,P5>(p,fn);
	}
}

#endif // OOBASE_DELEGATE_H_INCLUDED_
