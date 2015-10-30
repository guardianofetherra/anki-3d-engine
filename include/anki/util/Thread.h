// Copyright (C) 2009-2015, Panagiotis Christopoulos Charitos.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <anki/util/StdTypes.h>
#include <anki/util/Array.h>
#include <anki/util/NonCopyable.h>
#include <atomic>

#define ANKI_DISABLE_THREADPOOL_THREADING 0

namespace anki {

// Forward
class ThreadPool;

/// @addtogroup util_thread
/// @{

/// Thread implementation
class Thread: public NonCopyable
{
public:
	using Id = U64;

	/// It holds some information to be passed to the thread's callback
	class Info
	{
	public:
		void* m_userData;
		const char* m_threadName;
	};

	/// The type of the tread callback
	using Callback = Error(*)(Info&);

	/// Create a thread with or without a name
	/// @param[in] name The name of the new thread. Can be nullptr
	Thread(const char* name);

	~Thread();

	/// Start the thread
	/// @param userData The user data of the thread callback
	/// @param callback The thread callback that will be executed
	void start(void* userData, Callback callback);

	/// Wait for the thread to finish
	/// @return The error code of the thread's callback
	ANKI_USE_RESULT Error join();

	/// Identify the current thread
	static Id getCurrentThreadId();

	/// @privatesection
	/// @{
	const char* _getName() const
	{
		return &m_name[0];
	}

	void* _getUserData() const
	{
		return m_userData;
	}

	Callback _getCallback() const
	{
		return m_callback;
	}
	/// @}

private:
	void* m_impl = nullptr; ///< The system native type
	Array<char, 32> m_name; ///< The name of the thread
	Callback m_callback = nullptr; ///< The callback
	void* m_userData = nullptr; ///< The user date to pass to the callback

#if ANKI_ASSERTIONS
	Bool8 m_started = false;
#endif
};

/// Mutex
class Mutex: public NonCopyable
{
	friend class ConditionVariable;

public:
	Mutex();

	~Mutex();

	/// Lock
	void lock();

	/// Try lock
	/// @return True if it was locked successfully
	Bool tryLock();

	/// Unlock
	void unlock();

private:
	void* m_impl = nullptr; ///< The system native type
};

/// Condition variable
class ConditionVariable: public NonCopyable
{
public:
	ConditionVariable();

	~ConditionVariable();

	/// Signal one thread
	void notifyOne();

	/// Signal all threads
	void notifyAll();

	/// Bock until signaled.
	/// @param mtx The mutex.
	void wait(Mutex& mtx);

private:
	void* m_impl = nullptr; ///< The system native type
};

/// Spin lock. Good if the critical section will be executed in a short period
/// of time
class SpinLock: public NonCopyable
{
public:
	/// Lock
	void lock()
	{
		while(m_lock.test_and_set(std::memory_order_acquire))
		{}
	}

	/// Unlock
	void unlock()
	{
		m_lock.clear(std::memory_order_release);
	}

private:
	std::atomic_flag m_lock = ATOMIC_FLAG_INIT;
};

/// Lock guard. When constructed it locks a TMutex and unlocks it when it gets
/// destroyed.
template<typename TMutex>
class LockGuard
{
public:
	LockGuard(TMutex& mtx)
		: m_mtx(&mtx)
	{
		m_mtx->lock();
	}

	~LockGuard()
	{
		m_mtx->unlock();
	}

private:
	TMutex* m_mtx;
};

/// A barrier for thread synchronization. It works almost like boost::barrier
class Barrier: public NonCopyable
{
public:
	Barrier(U32 count);

	~Barrier();

	/// Wait until all threads call wait().
	Bool wait();

private:
	void* m_impl = nullptr;
};

// Forward
namespace detail {
class ThreadPoolThread;
}

/// Parallel task dispatcher. You feed it with tasks and sends them for
/// execution in parallel and then waits for all to finish
class ThreadPool: public NonCopyable
{
	friend class detail::ThreadPoolThread;

public:
	static constexpr U MAX_THREADS = 32; ///< An absolute limit

	/// A task assignment for a ThreadPool
	class Task
	{
	public:
		virtual ~Task()
		{}

		virtual Error operator()(U32 taskId, PtrSize threadsCount) = 0;

		/// Chose a starting and end index
		static void choseStartEnd(U32 taskId, PtrSize threadsCount,
			PtrSize elementsCount, PtrSize& start, PtrSize& end)
		{
			F32 tid = taskId;
			F32 div = F32(elementsCount) / threadsCount;
			start = PtrSize(tid * div);
			end = PtrSize((tid + 1.0) * div);
		}
	};

	/// Constructor
	ThreadPool(U32 threadsCount);

	~ThreadPool();

	/// Assign a task to a working thread
	/// @param slot The slot of the task
	/// @param task The task. If it's nullptr then a dummy task will be assigned
	void assignNewTask(U32 slot, Task* task);

	/// Wait for all tasks to finish.
	/// @return The error code in one of the worker threads.
	ANKI_USE_RESULT Error waitForAllThreadsToFinish()
	{
#if !ANKI_DISABLE_THREADPOOL_THREADING
		m_barrier.wait();
#endif
		Error err = m_err;
		m_err = ErrorCode::NONE;
		return err;
	}

	PtrSize getThreadsCount() const
	{
		return m_threadsCount;
	}

private:
	/// A dummy task for a ThreadPool
	class DummyTask: public Task
	{
	public:
		Error operator()(U32 taskId, PtrSize threadsCount)
		{
			(void)taskId;
			(void)threadsCount;
			return ErrorCode::NONE;
		}
	};

#if !ANKI_DISABLE_THREADPOOL_THREADING
	Barrier m_barrier; ///< Synchronization barrier
	detail::ThreadPoolThread* m_threads = nullptr; ///< Threads array
#endif
	U8 m_threadsCount = 0;
	Error m_err = ErrorCode::NONE;
	static DummyTask m_dummyTask;
};

/// @}

} // end namespace anki

