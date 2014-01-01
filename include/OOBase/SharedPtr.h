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

#ifndef OOBASE_SHAREDPTR_H_INCLUDED_
#define OOBASE_SHAREDPTR_H_INCLUDED_

#include "Atomic.h"
#include "Memory.h"

namespace OOBase
{
	template <typename T> class SharedPtr;
	template <typename T> class WeakPtr;
	template <typename T> class EnableSharedFromThis;

	namespace detail
	{
		class SharedCountBase : public NonCopyable
		{
		public:
			SharedCountBase() : m_ref_count(1), m_weak_count(1)
			{}

			virtual ~SharedCountBase()
			{}

			void addref()
			{
				++m_ref_count;
			}

			bool addref_lock()
			{
				// Loop trying to increase m_ref_count unless it is 0
				for (;;)
				{
					size_t t = m_ref_count;
					if (t == 0)
						return false;

					if (m_ref_count.CompareAndSwap(t+1,t) == t)
						return true;
				}
			}

			void release()
			{
				if (--m_ref_count == 0)
				{
					dispose();
					weak_release();
				}
			}

			void weak_addref()
			{
				++m_weak_count;
			}

			void weak_release()
			{
				if (--m_weak_count == 0)
					destroy();
			}

			size_t use_count() const
			{
				return m_ref_count;
			}

		protected:
			virtual void dispose() = 0;
			virtual void destroy() = 0;

		private:
			Atomic<size_t> m_ref_count;  ///< Number of hard references
			Atomic<size_t> m_weak_count; ///< Number of weak references + (m_ref_count > 0)
		};

		template <typename T, typename Allocator>
		class SharedCountAlloc : public SharedCountBase
		{
		public:
			explicit SharedCountAlloc(T* p) : m_ptr(p)
			{}

			virtual void dispose()
			{
				Allocator::delete_free(m_ptr);
				m_ptr = NULL;
			}

			virtual void destroy()
			{
				Allocator::delete_free(this);
			}

		private:
			T* m_ptr;
		};

		template <typename T>
		class SharedCountAllocInstance : public SharedCountBase
		{
		public:
			SharedCountAllocInstance(AllocatorInstance& alloc, T* p) : m_alloc(alloc), m_ptr(p)
			{}

			virtual void dispose()
			{
				m_alloc.delete_free(m_ptr);
				m_ptr = NULL;
			}

			virtual void destroy()
			{
				AllocatorInstance& a = m_alloc;
				a.delete_free(this);
			}

		private:
			AllocatorInstance& m_alloc;
			T*                 m_ptr;
		};

		class WeakCount;

		class SharedCount
		{
			friend class WeakCount;

		public:
			SharedCount() : m_impl(NULL)
			{}

			template <typename T, typename Allocator>
			SharedCount(const Allocator*, T* p) : m_impl(NULL)
			{
				SharedCountAlloc<T,Allocator>* i = NULL;
				if (!Allocator::allocate_new(i,p))
					OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
				m_impl = i;
			}

			template <typename T>
			SharedCount(AllocatorInstance& alloc, T* p) : m_impl(NULL)
			{
				SharedCountAllocInstance<T>* i = NULL;
				if (!alloc.allocate_new(i,alloc,p))
					OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
				m_impl = i;
			}

			SharedCount(const SharedCount& rhs) : m_impl(rhs.m_impl)
			{
				if (m_impl)
					m_impl->addref();
			}

			SharedCount(const WeakCount& rhs);

			SharedCount& operator = (const SharedCount& rhs)
			{
				SharedCountBase* rhs_impl = rhs.m_impl;
				if (rhs_impl != m_impl)
				{
					if (rhs_impl)
						rhs_impl->addref();

					if (m_impl)
						m_impl->release();

					m_impl = rhs_impl;
				}
				return *this;
			}

			~SharedCount()
			{
				if (m_impl)
					m_impl->release();
			}

			void swap(SharedCount& other)
			{
				SharedCountBase* t = other.m_impl;
				other.m_impl = m_impl;
				m_impl = t;
			}

			size_t use_count() const
			{
				return (m_impl ? m_impl->use_count() : 0);
			}

		private:
			SharedCountBase* m_impl;
		};

		class WeakCount
		{
			friend class SharedCount;

		public:
			WeakCount() : m_impl(NULL)
			{}

			WeakCount(const WeakCount& rhs) : m_impl(rhs.m_impl)
			{
				if (m_impl)
					m_impl->weak_addref();
			}

			WeakCount(const SharedCount& rhs) : m_impl(rhs.m_impl)
			{
				if (m_impl)
					m_impl->weak_addref();
			}

			WeakCount& operator = (const WeakCount& rhs)
			{
				SharedCountBase* rhs_impl = rhs.m_impl;
				if (rhs_impl != m_impl)
				{
					if (rhs_impl)
						rhs_impl->weak_addref();

					if (m_impl)
						m_impl->weak_release();

					m_impl = rhs_impl;
				}
				return *this;
			}

			WeakCount& operator = (const SharedCount& rhs)
			{
				SharedCountBase* rhs_impl = rhs.m_impl;
				if (rhs_impl != m_impl)
				{
					if (rhs_impl)
						rhs_impl->weak_addref();

					if (m_impl)
						m_impl->weak_release();

					m_impl = rhs_impl;
				}
				return *this;
			}

			~WeakCount()
			{
				if (m_impl)
					m_impl->weak_release();
			}

			void swap(WeakCount& other)
			{
				SharedCountBase* t = other.m_impl;
				other.m_impl = m_impl;
				m_impl = t;
			}

			size_t use_count() const
			{
				return (m_impl ? m_impl->use_count() : 0);
			}

		private:
			SharedCountBase* m_impl;
		};

		inline SharedCount::SharedCount(const WeakCount& rhs) : m_impl(rhs.m_impl)
		{
			if (m_impl && !m_impl->addref_lock())
				m_impl = NULL;
		}

		namespace shared
		{
			// SFINAE
			template <typename X, typename Y, typename T>
			inline void enable_shared_from_this(const SharedPtr<X>& px, const Y* y, const EnableSharedFromThis<T>* pe)
			{
				pe->internal_accept_owner(px,const_cast<Y*>(y));
			}

			inline void enable_shared_from_this(...)
			{
			}

			class template_friend;
		}
	}

	template <typename T>
	class SharedPtr : public SafeBoolean
	{
		friend class detail::shared::template_friend;

	public:
		SharedPtr() : m_ptr(NULL), m_sc()
		{}

		template <typename T1>
		SharedPtr(T1* p, AllocatorInstance& alloc) : m_ptr(p), m_sc()
		{
			if (p)
			{
				detail::SharedCount(alloc,p).swap(m_sc);
				detail::shared::enable_shared_from_this(this,p,p);
			}
		}

		template <typename Y>
		SharedPtr& operator =(const SharedPtr<Y>& rhs)
		{
			SharedPtr<T>(rhs).swap(*this);
			return *this;
		}

		T* get() const
		{
			return m_ptr;
		}

		operator bool_type() const
		{
			return m_ptr != NULL ? &SafeBoolean::this_type_does_not_support_comparisons : NULL;
		}

		T* operator ->() const
		{
			return m_ptr;
		}

		T& operator *() const
		{
			return *m_ptr;
		}

		void swap(SharedPtr& other)
		{
			OOBase::swap(m_ptr,other.m_ptr);
			m_sc.swap(other.m_sc);
		}

		template <typename Allocator>
		static SharedPtr make(T* p)
		{
			return SharedPtr(p,static_cast<const Allocator*>(NULL));
		}

	private:
		T*                  m_ptr;
		detail::SharedCount m_sc;

		template <typename T1, typename Allocator>
		SharedPtr(T1* p, const Allocator* a) : m_ptr(p)
		{
			if (p)
			{
				detail::SharedCount(a,p).swap(m_sc);
				detail::shared::enable_shared_from_this(this,p,p);
			}
		}
	};

	namespace detail
	{
		namespace shared
		{
			class template_friend
			{
			public:
				template <typename T, typename Allocator>
				static SharedPtr<T> make_shared(T* p, const Allocator* a)
				{
					return SharedPtr<T>(p,a);
				}
			};
		}
	}

	template<class T1, class T2>
	bool operator == (const SharedPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.get() == s2.get();
	}

	template<class T1, class T2>
	bool operator != (const SharedPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.get() != s2.get();
	}

	template<class T1, class T2>
	bool operator < (const SharedPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.get() < s2.get();
	}

	template<class T1, class T2>
	bool operator <= (const SharedPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.get() <= s2.get();
	}

	template<class T1, class T2>
	bool operator > (const SharedPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.get() > s2.get();
	}

	template<class T1, class T2>
	bool operator >= (const SharedPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.get() >= s2.get();
	}

	template <typename T, typename Allocator>
	inline SharedPtr<T> make_shared(T* p)
	{
		return detail::shared::template_friend::make_shared(p,static_cast<const Allocator*>(NULL));
	}

	template <typename T, typename Allocator>
	inline SharedPtr<T> allocate_shared()
	{
		T* p = NULL;
		Allocator::allocate_new(p);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename Allocator, typename P1>
	inline SharedPtr<T> allocate_shared(P1 p1)
	{
		T* p = NULL;
		Allocator::allocate_new(p,p1);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename Allocator, typename P1, typename P2>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2)
	{
		T* p = NULL;
		Allocator::allocate_new(p,p1,p2);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename Allocator, typename P1, typename P2, typename P3>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3)
	{
		T* p = NULL;
		Allocator::allocate_new(p,p1,p2,p3);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename Allocator, typename P1, typename P2, typename P3, typename P4>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4)
	{
		T* p = NULL;
		Allocator::allocate_new(p,p1,p2,p3,p4);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename Allocator, typename P1, typename P2, typename P3, typename P4, typename P5>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5)
	{
		T* p = NULL;
		Allocator::allocate_new(p,p1,p2,p3,p4,p5);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename Allocator, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6)
	{
		T* p = NULL;
		Allocator::allocate_new(p,p1,p2,p3,p4,p5,p6);
		return make_shared<T,Allocator>(p);
	}

	template <typename T>
	class EnableSharedFromThis
	{
	protected:
		EnableSharedFromThis()
		{}

		EnableSharedFromThis(const EnableSharedFromThis&)
		{}

		~EnableSharedFromThis()
		{}

	public:
		SharedPtr<T> shared_from_this()
		{
			return SharedPtr<T>(m_weak_this);
		}

		SharedPtr<const T> shared_from_this() const
		{
			return SharedPtr<const T>(m_weak_this);
		}

	public:
		template <typename X, typename Y>
		void internal_accept_owner(const SharedPtr<X>& px, Y* y) const
		{
			if (m_weak_this.expired())
				m_weak_this = SharedPtr<T>(px,y);
		}

	private:
		mutable WeakPtr<T> m_weak_this;
	};
}

#endif // OOBASE_SHAREDPTR_H_INCLUDED_
