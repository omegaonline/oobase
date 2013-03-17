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

#ifndef OOBASE_ASYNC_RESPONSE_H_INCLUDED_
#define OOBASE_ASYNC_RESPONSE_H_INCLUDED_

#include "CDRStream.h"

namespace OOBase
{
	namespace detail
	{
		template <typename T>
		struct non_ref
		{
			typedef T value;
		};

		template <typename T>
		struct non_ref<T&>
		{
			typedef T value;
		};
	}

	template <typename H, typename Allocator = CrtAllocator>
	class AsyncResponseDispatcher : public Allocating<Allocator>
	{
		typedef Allocating<Allocator> baseClass;
	public:
		AsyncResponseDispatcher() : baseClass()
		{}

		AsyncResponseDispatcher(AllocatorInstance& instance) : baseClass(instance)
		{}

		~AsyncResponseDispatcher()
		{
			OOBase::Guard<OOBase::SpinLock> guard(m_lock);

			DelegateV* resp = NULL;
			H handle;
			while (m_response_table.pop(&handle,&resp))
			{
				if (resp)
				{
					guard.release();

					OOBase::CDRStream stream(static_cast<OOBase::Buffer*>(NULL));
					resp->call(stream);

					resp->destroy(this);

					guard.acquire();
				}
			}
		}

		template <typename T>
		int add_response(T* pThis, bool (T::*callback)(OOBase::CDRStream&), H& handle)
		{
			Delegate0<T>* d = NULL;
			if (!baseClass::allocate_new(d,pThis,callback))
				return ERROR_OUTOFMEMORY;

			int err = add_response(d,handle);
			if (err)
				baseClass::delete_free(d);
			return err;
		}

		template <typename T, typename P1, typename PP1>
		int add_response(T* pThis, bool (T::*callback)(OOBase::CDRStream&, P1 p1), PP1 p1, H& handle)
		{
			Delegate1<T,P1,PP1>* d = NULL;
			if (!baseClass::allocate_new(d,pThis,callback,p1))
				return ERROR_OUTOFMEMORY;

			int err = add_response(d,handle);
			if (err)
				baseClass::delete_free(d);
			return err;
		}

		template <typename T, typename P1, typename P2, typename PP1, typename PP2>
		int add_response(T* pThis, bool (T::*callback)(OOBase::CDRStream&, P1 p1, P2 p2), PP1 p1, PP2 p2, H& handle)
		{
			Delegate2<T,P1,P2,PP1,PP2>* d = NULL;
			if (!baseClass::allocate_new(d,pThis,callback,p1,p2))
				return ERROR_OUTOFMEMORY;

			int err = add_response(d,handle);
			if (err)
				baseClass::delete_free(d);
			return err;
		}

		template <typename T, typename P1, typename P2, typename P3, typename PP1, typename PP2, typename PP3>
		int add_response(T* pThis, bool (T::*callback)(OOBase::CDRStream&, P1 p1, P2 p2, P3 p3), PP1 p1, PP2 p2, PP3 p3, H& handle)
		{
			Delegate3<T,P1,P2,P3,PP1,PP2,PP3>* d = NULL;
			if (!baseClass::allocate_new(d,pThis,callback,p1,p2,p3))
				return ERROR_OUTOFMEMORY;

			int err = add_response(d,handle);
			if (err)
				baseClass::delete_free(d);
			return err;
		}

		void drop_response(H handle)
		{
			OOBase::Guard<OOBase::SpinLock> guard(m_lock);

			DelegateV* resp = NULL;
			if (m_response_table.remove(handle,&resp) && resp)
			{
				guard.release();

				OOBase::CDRStream stream(static_cast<OOBase::Buffer*>(NULL));
				resp->call(stream);

				resp->destroy(this);
			}
		}

		bool handle_response(OOBase::CDRStream& stream)
		{
			H handle = 0;
			if (!stream.read(handle))
				return false;

			OOBase::Guard<OOBase::SpinLock> guard(m_lock);

			DelegateV* resp;
			if (!m_response_table.remove(handle,&resp) || !resp)
				return false;

			guard.release();

			bool ret = resp->call(stream);

			resp->destroy(this);

			return ret;
		}

	private:
		struct DelegateV
		{
			virtual bool call(OOBase::CDRStream& stream) = 0;

			void destroy(Allocating<Allocator>* allocator)
			{
				(void)allocator;
				allocator->delete_free(this);
			}

			virtual ~DelegateV() {}
		};

		template <typename T>
		struct Delegate0 : public DelegateV
		{
			typedef bool (T::*callback_t)(OOBase::CDRStream&);
			T* m_this;
			callback_t m_callback;

			Delegate0(T* pThis, callback_t callback) : m_this(pThis), m_callback(callback)
			{}

			Delegate0(const Delegate0& rhs) : m_this(rhs.m_this), m_callback(rhs.m_callback)
			{}

			Delegate0& operator = (const Delegate0& rhs)
			{
				if (this != &rhs)
				{
					m_this = rhs.m_this;
					m_callback = rhs.m_callback;
				}
				return *this;
			}

			bool call(OOBase::CDRStream& stream)
			{
				return (m_this->*m_callback)(stream);
			}
		};

		template <typename T, typename P1, typename PP1>
		struct Delegate1 : public DelegateV
		{
			typedef bool (T::*callback_t)(OOBase::CDRStream&, P1 p1);
			T* m_this;
			callback_t m_callback;
			typename detail::non_ref<P1>::value m_p1;

			Delegate1(T* pThis, callback_t callback, PP1 p1) : m_this(pThis), m_callback(callback), m_p1(p1)
			{}

			Delegate1(const Delegate1& rhs) : m_this(rhs.m_this), m_callback(rhs.m_callback), m_p1(rhs.m_p1)
			{}

			Delegate1& operator = (const Delegate1& rhs)
			{
				if (this != &rhs)
				{
					m_this = rhs.m_this;
					m_callback = rhs.m_callback;
					m_p1 = rhs.m_p1;
				}
				return *this;
			}

			bool call(OOBase::CDRStream& stream)
			{
				return (m_this->*m_callback)(stream,m_p1);
			}
		};

		template <typename T, typename P1, typename P2, typename PP1, typename PP2>
		struct Delegate2 : public DelegateV
		{
			typedef bool (T::*callback_t)(OOBase::CDRStream&, P1 p1, P2 p2);
			T* m_this;
			callback_t m_callback;
			typename detail::non_ref<P1>::value m_p1;
			typename detail::non_ref<P2>::value m_p2;

			Delegate2(T* pThis, callback_t callback, PP1 p1, PP2 p2) : m_this(pThis), m_callback(callback), m_p1(p1), m_p2(p2)
			{}

			Delegate2(const Delegate2& rhs) : m_this(rhs.m_this), m_callback(rhs.m_callback), m_p1(rhs.m_p1), m_p2(rhs.m_p2)
			{}

			Delegate2& operator = (const Delegate2& rhs)
			{
				if (this != &rhs)
				{
					m_this = rhs.m_this;
					m_callback = rhs.m_callback;
					m_p1 = rhs.m_p1;
					m_p2 = rhs.m_p2;
				}
				return *this;
			}

			bool call(OOBase::CDRStream& stream)
			{
				return (m_this->*m_callback)(stream,m_p1,m_p2);
			}
		};

		template <typename T, typename P1, typename P2, typename P3, typename PP1, typename PP2, typename PP3>
		struct Delegate3 : public DelegateV
		{
			typedef bool (T::*callback_t)(OOBase::CDRStream&, P1 p1, P2 p2, P3 p3);
			T* m_this;
			callback_t m_callback;
			typename detail::non_ref<P1>::value m_p1;
			typename detail::non_ref<P2>::value m_p2;
			typename detail::non_ref<P3>::value m_p3;

			Delegate3(T* pThis, callback_t callback, PP1 p1, PP2 p2, PP3 p3) : m_this(pThis), m_callback(callback), m_p1(p1), m_p2(p2), m_p3(p3)
			{}

			Delegate3(const Delegate3& rhs) : m_this(rhs.m_this), m_callback(rhs.m_callback), m_p1(rhs.m_p1), m_p2(rhs.m_p2), m_p3(rhs.m_p3)
			{}

			Delegate3& operator = (const Delegate3& rhs)
			{
				if (this != &rhs)
				{
					m_this = rhs.m_this;
					m_callback = rhs.m_callback;
					m_p1 = rhs.m_p1;
					m_p2 = rhs.m_p2;
					m_p3 = rhs.m_p3;
				}
				return *this;
			}

			bool call(OOBase::CDRStream& stream)
			{
				return (m_this->*m_callback)(stream,m_p1,m_p2,m_p3);
			}
		};

		OOBase::SpinLock                    m_lock;
		HandleTable<H,DelegateV*,Allocator> m_response_table;

		int add_response(DelegateV* delegate, H& handle)
		{
			OOBase::Guard<OOBase::SpinLock> guard(m_lock);

			return m_response_table.insert(delegate,handle);
		}

	public:
		class AutoDrop : public NonCopyable
		{
		public:
			AutoDrop(AsyncResponseDispatcher& disp, H handle = 0) : m_disp(disp), m_handle(handle)
			{}

			~AutoDrop()
			{
				if (m_handle)
					m_disp.drop_response(m_handle);
			}

			bool write(CDRStream& stream)
			{
				return stream.write(m_handle);
			}

			operator H& ()
			{
				return m_handle;
			}

			H detach()
			{
				H r = m_handle;
				m_handle = 0;
				return r;
			}

		private:
			AsyncResponseDispatcher& m_disp;
			H                        m_handle;
		};
	};
}

#endif // OOBASE_ASYNC_RESPONSE_H_INCLUDED_
