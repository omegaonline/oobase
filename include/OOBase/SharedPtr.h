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
						break;

					m_ref_count.CompareAndSwap(t,t+1);
					if (m_ref_count == t+1)
						return true;
				}
				return false;
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
			SharedCountAlloc(T* p) : m_ptr(p)
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
			SharedCountAllocInstance(AllocatorInstance* alloc, T* p) : m_alloc(*alloc), m_ptr(p)
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
			SharedCount(SharedCountBase* impl = NULL) : m_impl(impl)
			{}

			template <typename T, typename Allocator>
			SharedCount(const Allocator*, T* p) : m_impl(NULL)
			{
				SharedCountAlloc<T,Allocator>* i = NULL;
				if (!Allocator::allocate_new_ref(i,p))
					OOBase_CallCriticalFailure(system_error());
				m_impl = i;
			}

			template <typename T>
			SharedCount(AllocatorInstance& alloc, T* p) : m_impl(NULL)
			{
				SharedCountAllocInstance<T>* i = alloc.allocate_new<SharedCountAllocInstance<T> >(&alloc,p);
				if (!i)
					OOBase_CallCriticalFailure(system_error());
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
				SharedCount(rhs).swap(*this);
				return *this;
			}

			~SharedCount()
			{
				if (m_impl)
					m_impl->release();
			}

			void swap(SharedCount& other)
			{
				OOBase::swap(m_impl,other.m_impl);
			}

			bool empty() const
			{
				return (m_impl == NULL);
			}

			size_t use_count() const
			{
				return (m_impl ? m_impl->use_count() : 0);
			}

#if defined(OOBASE_CDR_STREAM_H_INCLUDED_)
			bool read(CDRStream& stream)
			{
				SharedCountBase* impl = NULL;
				if (!stream.read(impl))
					return false;

				SharedCount(impl).swap(*this);
				return true;
			}

			bool write(CDRStream& stream) const
			{
				if (!stream.write(m_impl))
					return false;

				if (m_impl)
					m_impl->addref();
				return true;
			}
#endif

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
				WeakCount(rhs).swap(*this);
				return *this;
			}

			WeakCount& operator = (const SharedCount& rhs)
			{
				if (rhs.m_impl != m_impl)
				{
					if (rhs.m_impl)
						rhs.m_impl->weak_addref();

					if (m_impl)
						m_impl->weak_release();

					m_impl = rhs.m_impl;
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
				OOBase::swap(m_impl,other.m_impl);
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
			class template_friend;

			// SFINAE
			template <typename X, typename Y, typename T>
			inline void enable_shared_from_this(const SharedPtr<X>* px, const Y* y, const EnableSharedFromThis<T>* pe);
			inline void enable_shared_from_this(...) { }

			template <typename A, typename B>
			inline void assert_convertible()
			{
				(void)static_cast<A*>(static_cast<B*>(NULL));
			}
		}
	}

	template <typename T>
	class SharedPtr : public SafeBoolean
	{
		friend class detail::shared::template_friend;

		template <typename T1> friend class SharedPtr;
		template <typename T1> friend class WeakPtr;
		friend class EnableSharedFromThis<T>;

	public:
		SharedPtr() : m_ptr(NULL), m_sc()
		{}

		SharedPtr(const SharedPtr& rhs) : m_ptr(rhs.m_ptr), m_sc(rhs.m_sc)
		{}

		template <typename T1>
		SharedPtr(const SharedPtr<T1>& rhs) : m_ptr(rhs.m_ptr), m_sc(rhs.m_sc)
		{
			detail::shared::assert_convertible<T,T1>();
		}

		template <typename T1>
		explicit SharedPtr(const WeakPtr<T1>& rhs) : m_ptr(NULL), m_sc(rhs.m_wc)
		{
			detail::shared::assert_convertible<T,T1>();

			if (!m_sc.empty())
				m_ptr = rhs.m_ptr;
		}

		SharedPtr& operator =(const SharedPtr& rhs)
		{
			SharedPtr<T>(rhs).swap(*this);
			return *this;
		}

		template <typename T1>
		SharedPtr& operator =(const SharedPtr<T1>& rhs)
		{
			detail::shared::assert_convertible<T,T1>();
			SharedPtr<T>(rhs).swap(*this);
			return *this;
		}

		void reset()
		{
			SharedPtr().swap(*this);
		}

		template <typename T1>
		void reset(T1* p, AllocatorInstance& alloc)
		{
			SharedPtr(p,alloc).swap(*this);
		}

		T* get() const
		{
			return m_ptr;
		}

		operator bool_type() const
		{
			return SafeBoolean::safe_bool(m_ptr != NULL);
		}

		T* operator ->() const
		{
			return m_ptr;
		}

		T& operator *() const
		{
			return *m_ptr;
		}

		size_t use_count() const
		{
			return m_sc.use_count();
		}

		bool unique() const
		{
			return m_sc.use_count() == 1;
		}

		void swap(SharedPtr& other)
		{
			OOBase::swap(m_ptr,other.m_ptr);
			m_sc.swap(other.m_sc);
		}

#if defined(OOBASE_CDR_STREAM_H_INCLUDED_)
		bool read(CDRStream& stream)
		{
			return stream.read(m_ptr) && m_sc.read(stream);
		}

		bool write(CDRStream& stream) const
		{
			return stream.write(m_ptr) && m_sc.write(stream);
		}
#endif

	private:
		T*                  m_ptr;
		detail::SharedCount m_sc;

		template <typename T1, typename Allocator>
		SharedPtr(T1* p, const Allocator* a) : m_ptr(p), m_sc()
		{
			detail::shared::assert_convertible<T,T1>();
			if (p)
			{
				detail::SharedCount(a,p).swap(m_sc);
				detail::shared::enable_shared_from_this(this,p,p);
			}
		}

		template <typename T1>
		SharedPtr(T1* p, AllocatorInstance& alloc) : m_ptr(p), m_sc()
		{
			if (p)
			{
				detail::SharedCount(alloc,p).swap(m_sc);
				detail::shared::enable_shared_from_this(this,p,p);
			}
		}

		template <typename T1>
		SharedPtr(const SharedPtr<T1>& rhs, T* p) : m_ptr(p), m_sc(rhs.m_sc)
		{}

		template <typename T1>
		SharedPtr(T1* p, detail::SharedCountBase* b) : m_ptr(p), m_sc(b)
		{
			if (p)
				detail::shared::enable_shared_from_this(this,p,p);
		}
	};

	template <typename T>
	class WeakPtr : public SafeBoolean
	{
		friend class detail::shared::template_friend;

		template <typename T1> friend class SharedPtr;
		template <typename T1> friend class WeakPtr;

	public:
		WeakPtr() : m_ptr(NULL), m_wc()
		{}

		// Default copy constructor, assignment and destructor are OK

		template<class T1>
		WeakPtr(const WeakPtr<T1>& rhs) : m_ptr(rhs.lock().get()), m_wc(rhs.m_wc)
		{
			detail::shared::assert_convertible<T,T1>();
		}

		template <class T1>
		WeakPtr(const SharedPtr<T1>& rhs) : m_ptr(rhs.m_ptr), m_wc(rhs.m_sc)
		{
			detail::shared::assert_convertible<T,T1>();
		}

		template<class T1>
		WeakPtr& operator = (const WeakPtr<T1>& rhs)
		{
			detail::shared::assert_convertible<T,T1>();
			m_ptr = rhs.lock().get();
			m_wc = rhs.m_wc;
			return *this;
		}

		template<class T1>
		WeakPtr& operator = (const SharedPtr<T1>& rhs)
		{
			detail::shared::assert_convertible<T,T1>();
			m_ptr = rhs.m_ptr;
			m_wc = rhs.m_sc;
			return *this;
		}

		SharedPtr<T> lock() const
		{
			return SharedPtr<T>(*this);
		}

		bool expired() const
		{
			return m_wc.use_count() == 0;
		}

		void reset()
		{
			WeakPtr().swap(*this);
		}

		void swap(WeakPtr& rhs)
		{
			OOBase::swap(m_ptr,rhs.m_ptr);
			m_wc.swap(rhs.m_wc);
		}

		size_t use_count() const
		{
			return m_wc.use_count();
		}

		operator bool_type() const
		{
			return SafeBoolean::safe_bool(m_ptr != NULL);
		}

	private:
		T*                m_ptr;
		detail::WeakCount m_wc;
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

				template <typename T>
				static SharedPtr<T> make_shared(T* p, AllocatorInstance& a)
				{
					return SharedPtr<T>(p,a);
				}

				template <typename T>
				static SharedPtr<T> make_shared(T* p, detail::SharedCountBase* b)
				{
					return SharedPtr<T>(p,b);
				}

				template <typename X, typename Y, typename T>
				static void enable_shared_from_this(const SharedPtr<X>& px, const Y* y, const EnableSharedFromThis<T>* pe)
				{
					pe->internal_accept_owner(px,const_cast<Y*>(y));
				}

				template <typename T, typename T1>
				static SharedPtr<T> staticcast(const SharedPtr<T1>& rhs)
				{
					(void)static_cast<T*>(static_cast<T1*>(NULL));
					return SharedPtr<T>(rhs,static_cast<T*>(rhs.get()));
				}

				template <typename T, typename T1>
				static SharedPtr<T> constcast(const SharedPtr<T1>& rhs)
				{
					(void)const_cast<T*>(static_cast<T1*>(NULL));
					return SharedPtr<T>(rhs,const_cast<T*>(rhs.get()));
				}

				template <typename T, typename T1>
				static SharedPtr<T> reinterpretcast(const SharedPtr<T1>& rhs)
				{
					(void)reinterpret_cast<T*>(static_cast<T1*>(NULL));
					return SharedPtr<T>(rhs,reinterpret_cast<T*>(rhs.get()));
				}
			};

			template <typename X, typename Y, typename T>
			void enable_shared_from_this(const SharedPtr<X>* px, const Y* y, const EnableSharedFromThis<T>* pe)
			{
				template_friend::enable_shared_from_this(*px,y,pe);
			}
		}
	}

	template <typename T, typename T1>
	inline SharedPtr<T> static_pointer_cast(const SharedPtr<T1>& rhs)
	{
		return detail::shared::template_friend::staticcast<T>(rhs);
	}

	template <typename T, typename T1>
	inline SharedPtr<T> const_pointer_cast(const SharedPtr<T1>& rhs)
	{
		return detail::shared::template_friend::constcast<T>(rhs);
	}

	template <typename T, typename T1>
	inline SharedPtr<T> reinterpret_pointer_cast(const SharedPtr<T1>& rhs)
	{
		return detail::shared::template_friend::reinterpretcast<T>(rhs);
	}

	template<class T1, class T2>
	inline bool operator == (const SharedPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.get() == s2.get();
	}

	template<class T1, class T2>
	inline bool operator != (const SharedPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.get() != s2.get();
	}

	template<class T1, class T2>
	inline bool operator < (const SharedPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.get() < s2.get();
	}

	template<class T1, class T2>
	inline bool operator <= (const SharedPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.get() <= s2.get();
	}

	template<class T1, class T2>
	inline bool operator > (const SharedPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.get() > s2.get();
	}

	template<class T1, class T2>
	inline bool operator >= (const SharedPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.get() >= s2.get();
	}

	template<class T1, class T2>
	inline bool operator == (const SharedPtr<T1>& s1, const WeakPtr<T2>& s2)
	{
		return s1.get() == s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator != (const SharedPtr<T1>& s1, const WeakPtr<T2>& s2)
	{
		return s1.get() != s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator < (const SharedPtr<T1>& s1, const WeakPtr<T2>& s2)
	{
		return s1.get() < s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator <= (const SharedPtr<T1>& s1, const WeakPtr<T2>& s2)
	{
		return s1.get() <= s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator > (const SharedPtr<T1>& s1, const WeakPtr<T2>& s2)
	{
		return s1.get() > s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator >= (const SharedPtr<T1>& s1, const WeakPtr<T2>& s2)
	{
		return s1.get() >= s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator == (const WeakPtr<T1>& s1, const WeakPtr<T2>& s2)
	{
		return s1.lock().get() == s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator != (const WeakPtr<T1>& s1, const WeakPtr<T2>& s2)
	{
		return s1.lock().get() != s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator < (const WeakPtr<T1>& s1, const WeakPtr<T2>& s2)
	{
		return s1.lock().get() < s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator <= (const WeakPtr<T1>& s1, const WeakPtr<T2>& s2)
	{
		return s1.lock().get() <= s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator > (const WeakPtr<T1>& s1, const WeakPtr<T2>& s2)
	{
		return s1.lock().get() > s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator >= (const WeakPtr<T1>& s1, const WeakPtr<T2>& s2)
	{
		return s1.lock().get() >= s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator == (const WeakPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.lock().get() == s2.get();
	}

	template<class T1, class T2>
	inline bool operator != (const WeakPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.lock().get() != s2.get();
	}

	template<class T1, class T2>
	inline bool operator < (const WeakPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.lock().get() < s2.get();
	}

	template<class T1, class T2>
	inline bool operator <= (const WeakPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.lock().get() <= s2.get();
	}

	template<class T1, class T2>
	inline bool operator > (const WeakPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.lock().get() > s2.get();
	}

	template<class T1, class T2>
	inline bool operator >= (const WeakPtr<T1>& s1, const SharedPtr<T2>& s2)
	{
		return s1.lock().get() >= s2.get();
	}

	template<class T1, class T2>
	inline bool operator == (const SharedPtr<T1>& s1, T2* const s2)
	{
		return s1.get() == s2;
	}

	template<class T1, class T2>
	inline bool operator != (const SharedPtr<T1>& s1, T2* const s2)
	{
		return s1.get() != s2;
	}

	template<class T1, class T2>
	inline bool operator < (const SharedPtr<T1>& s1, T2* const s2)
	{
		return s1.get() < s2;
	}

	template<class T1, class T2>
	inline bool operator <= (const SharedPtr<T1>& s1, T2* const s2)
	{
		return s1.get() <= s2;
	}

	template<class T1, class T2>
	inline bool operator > (const SharedPtr<T1>& s1, T2* const s2)
	{
		return s1.get() > s2;
	}

	template<class T1, class T2>
	inline bool operator >= (const SharedPtr<T1>& s1, T2* const s2)
	{
		return s1.get() >= s2;
	}

	template<class T1, class T2>
	inline bool operator == (T1* const s1, const SharedPtr<T2>& s2)
	{
		return s1 == s2.get();
	}

	template<class T1, class T2>
	inline bool operator != (T1* const s1, const SharedPtr<T2>& s2)
	{
		return s1 != s2.get();
	}

	template<class T1, class T2>
	inline bool operator < (T1* const s1, const SharedPtr<T2>& s2)
	{
		return s1 < s2.get();
	}

	template<class T1, class T2>
	inline bool operator <= (T1* const s1, const SharedPtr<T2>& s2)
	{
		return s1 <= s2.get();
	}

	template<class T1, class T2>
	inline bool operator > (T1* const s1, const SharedPtr<T2>& s2)
	{
		return s1 > s2.get();
	}

	template<class T1, class T2>
	inline bool operator >= (T1* const s1, const SharedPtr<T2>& s2)
	{
		return s1 >= s2.get();
	}

	template<class T1, class T2>
	inline bool operator == (const WeakPtr<T1>& s1, T2* const s2)
	{
		return s1.lock().get() == s2;
	}

	template<class T1, class T2>
	inline bool operator != (const WeakPtr<T1>& s1, T2* const s2)
	{
		return s1.lock().get() != s2;
	}

	template<class T1, class T2>
	inline bool operator < (const WeakPtr<T1>& s1, T2* const s2)
	{
		return s1.lock().get() < s2;
	}

	template<class T1, class T2>
	inline bool operator <= (const WeakPtr<T1>& s1, T2* const s2)
	{
		return s1.lock().get() <= s2;
	}

	template<class T1, class T2>
	inline bool operator > (const WeakPtr<T1>& s1, T2* const s2)
	{
		return s1.lock().get() > s2;
	}

	template<class T1, class T2>
	inline bool operator >= (const WeakPtr<T1>& s1, T2* const s2)
	{
		return s1.lock().get() >= s2;
	}

	template<class T1, class T2>
	inline bool operator == (T1* const s1, const WeakPtr<T2>& s2)
	{
		return s1 == s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator != (T1* const s1, const WeakPtr<T2>& s2)
	{
		return s1 != s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator < (T1* const s1, const WeakPtr<T2>& s2)
	{
		return s1 < s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator <= (T1* const s1, const WeakPtr<T2>& s2)
	{
		return s1 <= s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator > (T1* const s1, const WeakPtr<T2>& s2)
	{
		return s1 > s2.lock().get();
	}

	template<class T1, class T2>
	inline bool operator >= (T1* const s1, const WeakPtr<T2>& s2)
	{
		return s1 >= s2.lock().get();
	}

	template <typename T, typename Allocator>
	inline SharedPtr<T> make_shared(T* p)
	{
		return detail::shared::template_friend::make_shared(p,static_cast<const Allocator*>(NULL));
	}

	template <typename T>
	inline SharedPtr<T> make_shared(T* p)
	{
		return make_shared<T,CrtAllocator>(p);
	}

	template <typename T>
	inline SharedPtr<T> make_shared(T* p, AllocatorInstance& a)
	{
		return detail::shared::template_friend::make_shared(p,a);
	}

	template <typename T>
	inline SharedPtr<T> make_shared(T* p, detail::SharedCountBase* b)
	{
		return detail::shared::template_friend::make_shared(p,b);
	}

	template <typename T, typename Allocator>
	inline SharedPtr<T> allocate_shared()
	{
		T* p = NULL;
		Allocator::allocate_new_ref(p);
		return make_shared<T,Allocator>(p);
	}

	template <typename T>
	inline SharedPtr<T> allocate_shared()
	{
		return allocate_shared<T,CrtAllocator>();
	}

	template <typename T, typename Allocator, typename P1>
	inline SharedPtr<T> allocate_shared(P1 p1)
	{
		T* p = NULL;
		Allocator::allocate_new_ref(p,p1);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename P1>
	inline SharedPtr<T> allocate_shared(P1 p1)
	{
		return allocate_shared<T,CrtAllocator>(p1);
	}

	template <typename T, typename Allocator, typename P1, typename P2>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2)
	{
		T* p = NULL;
		Allocator::allocate_new_ref(p,p1,p2);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename P1, typename P2>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2)
	{
		return allocate_shared<T,CrtAllocator>(p1,p2);
	}

	template <typename T, typename Allocator, typename P1, typename P2, typename P3>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3)
	{
		T* p = NULL;
		Allocator::allocate_new_ref(p,p1,p2,p3);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename P1, typename P2, typename P3>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3)
	{
		return allocate_shared<T,CrtAllocator>(p1,p2,p3);
	}

	template <typename T, typename Allocator, typename P1, typename P2, typename P3, typename P4>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4)
	{
		T* p = NULL;
		Allocator::allocate_new_ref(p,p1,p2,p3,p4);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename P1, typename P2, typename P3, typename P4>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4)
	{
		return allocate_shared<T,CrtAllocator>(p1,p2,p3,p4);
	}

	template <typename T, typename Allocator, typename P1, typename P2, typename P3, typename P4, typename P5>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5)
	{
		T* p = NULL;
		Allocator::allocate_new_ref(p,p1,p2,p3,p4,p5);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5)
	{
		return allocate_shared<T,CrtAllocator>(p1,p2,p3,p4,p5);
	}

	template <typename T, typename Allocator, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6)
	{
		T* p = NULL;
		Allocator::allocate_new_ref(p,p1,p2,p3,p4,p5,p6);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6)
	{
		return allocate_shared<T,CrtAllocator>(p1,p2,p3,p4,p5,p6);
	}

	template <typename T, typename Allocator, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7)
	{
		T* p = NULL;
		Allocator::allocate_new_ref(p,p1,p2,p3,p4,p5,p6,p7);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7)
	{
		return allocate_shared<T,CrtAllocator>(p1,p2,p3,p4,p5,p6,p7);
	}

	template <typename T, typename Allocator, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8)
	{
		T* p = NULL;
		Allocator::allocate_new_ref(p,p1,p2,p3,p4,p5,p6,p7,p8);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8)
	{
		return allocate_shared<T,CrtAllocator>(p1,p2,p3,p4,p5,p6,p7,p8);
	}

	template <typename T, typename Allocator, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9)
	{
		T* p = NULL;
		Allocator::allocate_new_ref(p,p1,p2,p3,p4,p5,p6,p7,p8,p9);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9)
	{
		return allocate_shared<T,CrtAllocator>(p1,p2,p3,p4,p5,p6,p7,p8,p9);
	}

	template <typename T, typename Allocator, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9, P10 p10)
	{
		T* p = NULL;
		Allocator::allocate_new_ref(p,p1,p2,p3,p4,p5,p6,p7,p8,p9,p10);
		return make_shared<T,Allocator>(p);
	}

	template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
	inline SharedPtr<T> allocate_shared(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9, P10 p10)
	{
		return allocate_shared<T,CrtAllocator>(p1,p2,p3,p4,p5,p6,p7,p8,p9,p10);
	}

	template <typename T>
	class EnableSharedFromThis
	{
		friend class detail::shared::template_friend;

	public:
		SharedPtr<T> shared_from_this()
		{
			return SharedPtr<T>(m_weak_this);
		}

		SharedPtr<const T> shared_from_this() const
		{
			return SharedPtr<const T>(m_weak_this);
		}

	private:
		mutable WeakPtr<T> m_weak_this;

		template <typename X, typename Y>
		void internal_accept_owner(const SharedPtr<X>& px, Y* y) const
		{
			detail::shared::assert_convertible<T,X>();
			detail::shared::assert_convertible<X,Y>();

			if (m_weak_this.expired())
				m_weak_this = SharedPtr<T>(px,y);
		}
	};
}

#endif // OOBASE_SHAREDPTR_H_INCLUDED_
