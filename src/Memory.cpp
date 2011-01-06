///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2011 Rick Taylor
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

#include "../include/config-base.h"
#include "../include/OOBase/Destructor.h"

namespace
{
	class CrtAllocator
	{
	public:
		static void* allocate(size_t len, int /*flags*/, const char* /*file*/, unsigned int /*line*/)
		{
			return ::malloc(len);
		}

		static void free(void* ptr, int /*flags*/)
		{
			::free(ptr);
		}	
	};

	template <typename T>
	class MemoryManager
	{
	public:
		static void* allocate(size_t len, int flags, const char* file, unsigned int line)
		{
			Instance& i = instance();

			void* p = i.inst.allocate(len,flags,file,line);
			if (p)
				++i.refcount;
			return p;
		}
			
		static void free(void* ptr, int flags)
		{
			if (ptr)
			{
				Instance& i = instance();

				i.inst.free(ptr,flags);
				if (--i.refcount == 0)
					term(const_cast<Instance*>(s_instance));
			}
		}

	private:
		struct Instance
		{
			T                         inst;
			OOBase::AtomicInt<size_t> refcount;
		};

		static volatile Instance* s_instance;

		static Instance& instance()
		{
			if (!s_instance)
			{
				static OOBase::Once::once_t key = ONCE_T_INIT;
				OOBase::Once::Run_Internal(&key,&init);
			}

			assert(s_instance != reinterpret_cast<Instance*>((uintptr_t)0xdeadbeef));
			return *const_cast<Instance*>(s_instance);
		}

		static void init()
		{
			// Use crt malloc here...
			Instance* instance = new (std::nothrow) Instance();
			instance->refcount = 1;

			s_instance = instance;

			OOBase::DLLDestructor<OOBase::Module>::add_destructor(deref,0);
		}

		static void term(Instance* i)
		{
			s_instance = reinterpret_cast<Instance*>((uintptr_t)0xdeadbeef);
			delete i;
		}

		static void deref(void*)
		{
			assert(s_instance != reinterpret_cast<Instance*>((uintptr_t)0xdeadbeef));

			Instance* i = const_cast<Instance*>(s_instance);

			if (i && --i->refcount == 0)
				term(i);
		}
	};

	template <typename T>
	volatile typename MemoryManager<T>::Instance* MemoryManager<T>::s_instance = 0;
}


#if defined(_DEBUG)

#include <set>
#include <string>

namespace
{
	// This class is temp!!
	class MemWatcher
	{
	public:
		void* allocate(size_t len, int flags, const char* file, unsigned int line)
		{
			/*if (flags == 2)
			{
		#if defined(_MSC_VER)
				return _malloca(len);
		#else
				flags = 1;
		#endif
			}*/
			
			//std::ostringstream os;
			//os << "Alloc(" << flags << ") " << p << " " << len << " " << file << " " << line << std::endl;
			//OutputDebugString(os.str().c_str());

			void* p = CrtAllocator::allocate(len,flags,file,line);

			OOBase::Guard<OOBase::SpinLock> guard(m_lock);

			m_setEntries.insert(p);

			return p;
		}

		void free(void* ptr, int flags)
		{
			//std::ostringstream os;
			//os << "Free(" << flags << ")  " << mem << std::endl;
			//OutputDebugString(os.str().c_str());
			/*if (flags == 2)
			{
		#if defined(_MSC_VER)
				_freea(mem);
				return;
		#else
				flags = 1;
		#endif
			}*/

			OOBase::Guard<OOBase::SpinLock> guard(m_lock);

			size_t c = m_setEntries.erase(ptr);
			assert(c == 1);

			CrtAllocator::free(ptr,flags);
		}

	private:
		OOBase::SpinLock   m_lock;
		std::set<void*>    m_setEntries;
	};
}

#endif // _DEBUG



// flags: 0 - C++ object - align to size, no reallocation
//        1 - Buffer - align 32, reallocation
//        2 - Stack-local buffer - align 32, no reallocation
void* OOBase::Allocate(size_t len, int flags, const char* file, unsigned int line)
{
#if defined(_DEBUG)
	return MemoryManager<MemWatcher>::allocate(len,flags,file,line);
#else
	return MemoryManager<CrtAllocator>::allocate(len,flags,file,line);
#endif
}

void OOBase::Free(void* ptr, int flags)
{
#if defined(_DEBUG)
	MemoryManager<MemWatcher>::free(ptr,flags);
#else
	MemoryManager<CrtAllocator>::free(ptr,flags);
#endif	
}
