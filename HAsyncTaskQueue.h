#pragma once 
#ifndef __C_HALF_TASK_QUEUE_H__
#define __C_HALF_TASK_QUEUE_H__

#include <cmath>
#include <queue>
#include <list>
#include <mutex>
#include <atomic>
#include <memory>
#include <thread>
#include <future>
#include <vector>
#include <stdexcept>
#include <functional>
#include <type_traits>
#include <condition_variable>


class CHAsyncTaskQueue
{
public:
  explicit CHAsyncTaskQueue(unsigned short nCount = std::thread::hardware_concurrency()) :
    m_bIsWorking(true),
    m_nThreadWaitCount(0)
  {
    IncreaseThread(nCount);
  }
  virtual ~CHAsyncTaskQueue()
  {
    std::call_once(m_stTerminate, [this]
    {
      m_bIsWorking = false;

      m_cvTaskSignal.notify_all();

      for (std::shared_ptr<std::thread>& itemThread : m_vecThreads)
        if (itemThread->joinable())
          itemThread->join();

      m_vecThreads.clear();

      std::lock_guard<std::mutex> lockGuard(m_mutexLocker);
      while (!m_queueTasks.empty())
        m_queueTasks.pop();
    });

    std::this_thread::sleep_for(std::chrono::microseconds(1000));
  }

public:
  //Enqueue(F);
  //Enqueue(O());
  //Enqueue(I.F);
  //Enqueue(C::F);
  //Enqueue([]()->RT{});
  //Enqueue(std::bind(&C::F, &CI));
  //Enqueue(std::mem_fn(&C::F), &CI);
  //
  //std::vector<std::future<int> > R;
  //R.emplace_back(Enqueue([int]{})
  template<typename F, typename... Args>
  auto Enqueue(F&& pfn, Args&&... args) ->std::future<decltype(pfn(args...))>
  {
    if (!m_bIsWorking)
      throw std::runtime_error("ThreadPool is stopped.");

    using RT = decltype(pfn(args...)); // typename std::result_of<F(Args...)>::type;  //pfn's return type

    auto itemTask = std::make_shared<std::packaged_task<RT()>>(std::bind(std::forward<F>(pfn), std::forward<Args>(args)...));
    std::future<RT> autoResult = itemTask->get_future();

    std::lock_guard<std::mutex> lockGuard(m_mutexLocker);
    //if (/*task max*/)m_cvThreadSignal.wait(m_mutexLocker);
    m_queueTasks.emplace([itemTask]()
    {
      (*itemTask)();
    });

    if (m_nThreadWaitCount < 1 && m_vecThreads.size() < std::pow(std::thread::hardware_concurrency(), std::thread::hardware_concurrency()))
      IncreaseThread(1);

    m_cvTaskSignal.notify_one();

    return autoResult;
  }

  template<typename F, typename... Args>
  void Enqueue(const F& pfn, Args... args)
  {
    if (!m_bIsWorking)
      throw std::runtime_error("ThreadPool is stopped.");

    TheTask itemTask = [&pfn, args...]{ return pfn(args...); };

    std::lock_guard<std::mutex> lockGuard(m_mutexLocker);
    //if (/*task max*/)m_cvThreadSignal.wait(m_mutexLocker);
    m_queueTasks.emplace(std::move(itemTask));

    if (m_nThreadWaitCount < 1 && m_vecThreads.size() < std::pow(std::thread::hardware_concurrency(), std::thread::hardware_concurrency()))
      IncreaseThread(1);

    m_cvTaskSignal.notify_one();
  }

  template<typename F, typename O, typename... Args>
  void Enqueue(const F& pfn, O* obj, Args... args)
  {
    if (!m_bIsWorking)
      throw std::runtime_error("ThreadPool is stopped.");

    TheTask itemTask = [&pfn, &obj, args...]{ return (*obj.*pfn)(args...); };

    std::lock_guard<std::mutex> lockGuard(m_mutexLocker);
    //if (/*task max*/)m_cvThreadSignal.wait(m_mutexLocker);
    m_queueTasks.emplace(std::move(itemTask));

    if (m_nThreadWaitCount < 1 && m_vecThreads.size() < std::pow(std::thread::hardware_concurrency(), std::thread::hardware_concurrency()))
      IncreaseThread(1);

    m_cvTaskSignal.notify_one();
  }

  template<class F, typename... Args>
  typename std::enable_if<std::is_class<F>::value>::type Enqueue(F& pfn, Args... args)
  {
    if (!m_bIsWorking)
      throw std::runtime_error("ThreadPool is stopped.");

    TheTask itemTask = [&pfn, args...]{ return pfn(args...); };

    std::lock_guard<std::mutex> lockGuard(m_mutexLocker);
    //if (/*task max*/)m_cvThreadSignal.wait(m_mutexLocker);
    m_queueTasks.emplace(std::move(itemTask));

    if (m_nThreadWaitCount < 1 && m_vecThreads.size() < std::pow(std::thread::hardware_concurrency(), std::thread::hardware_concurrency()))
      IncreaseThread(1);

    m_cvTaskSignal.notify_one();
  }


private:
  void IncreaseThread(unsigned short nCount)
  {
    for (; nCount > 0 && m_vecThreads.size() < std::pow(std::thread::hardware_concurrency(), std::thread::hardware_concurrency()); --nCount)
    {
      /*
      std::shared_ptr<std::thread> theThread =
        std::make_shared<std::thread>(std::bind(&CHAsyncTaskQueue::WorkPool, this));
      //std::shared_ptr<std::thread>(new std::thread(&CHAsyncTaskQueue::WorkPool, this));;
      m_vecThreads.emplace_back(theThread);
*/

/**/
      m_vecThreads.emplace_back(std::make_shared<std::thread>([this]
      {
        while (m_bIsWorking)
        {
          std::function<void()> itemTask = nullptr;

          std::unique_lock<std::mutex> uniqueLock(m_mutexLocker);

          m_cvTaskSignal.wait(uniqueLock, [this]
          {
            return (!m_bIsWorking || !m_queueTasks.empty()); //true break wait-status
          });

          if (!m_bIsWorking && m_queueTasks.empty())
            return;

          if (!m_queueTasks.empty())
          {
            itemTask = std::move(m_queueTasks.front());
            m_queueTasks.pop();
          }

          if (nullptr != itemTask)
          {
            --m_nThreadWaitCount;
            itemTask();
            ++m_nThreadWaitCount;
          }
        }
      }));


      ++m_nThreadWaitCount;
    }
  }

  void WorkPool()
  {
    while (m_bIsWorking)
    {
      TheTask itemTask = nullptr;

      std::unique_lock<std::mutex> uniqueLock(m_mutexLocker);

      while (m_bIsWorking && m_queueTasks.empty())
        m_cvTaskSignal.wait(uniqueLock);

      if (!m_bIsWorking && m_queueTasks.empty())
        break;

      if (!m_queueTasks.empty())
      {
        itemTask = std::move(m_queueTasks.front());
        m_queueTasks.pop();
      }

      if (nullptr != itemTask)
      {
        --m_nThreadWaitCount;
        itemTask();
        ++m_nThreadWaitCount;
        //m_cvThreadSignal.notify_one();
      }
    }
  }

private:
  std::mutex m_mutexLocker;
  std::once_flag m_stTerminate;
  std::atomic<bool> m_bIsWorking;
  std::atomic<int> m_nThreadWaitCount;
  using TheTask = std::function<void()>;
  std::condition_variable m_cvTaskSignal;
  std::condition_variable m_cvThreadSignal;//for limit task
  std::queue<std::function<void()>> m_queueTasks;
  std::vector<std::shared_ptr<std::thread> > m_vecThreads;
};

#endif // !__C_HALF_TASK_QUEUE_H__
