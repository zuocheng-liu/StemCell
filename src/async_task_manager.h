#ifndef ASYNC_TASK_MANAGER_H
#define ASYNC_TASK_MANAGER_H

#include <memory>
#include <map>
#include <atomic>
#include <mutex>
#include <cassert>
#include <utils/vlog/loghelper.h>
#include "async_task.h"
#include "async_task_context.h"
#include "timer_event.h"
#include "singleton.hpp"
#include "spinlock.h"
#include "ThreadPool.h"
#include "profiler.h"

using namespace vlog;

class AsyncTaskManager {
public:
    typedef std::map<int64_t, std::shared_ptr<AsyncTask> > TaskMap;

    AsyncTaskManager() {
        _curr_unique_id = 0;
        // init thread pool
        VLOG_APP(INFO) << "create thread pool, size:" << THREAD_NUM;   
        _thread_pool = new ThreadPool(THREAD_NUM);
        
        Timer& te = _timer;
        _thread_pool->enqueue([&te]() mutable { te.loop(); });
        _lock.setId(-2);
    }

    template <class T, class C>
    std::shared_ptr<T> createAsyncTask() {
        static_assert(std::is_base_of<AsyncTask, T>::value, 
                "T is not derived from AsyncTask");
        static_assert(std::is_base_of<AsyncTaskContext, C>::value, 
                "C is not derived from AsyncTaskContext");
        std::lock_guard<Spinlock> locker(_lock);
        if (_task_recycle_pool.empty()) {
            std::shared_ptr<T> task = std::make_shared<T>();
            std::shared_ptr<C> context = std::make_shared<C>();
            task->setContext(context);
            task->setId(generateUniqueId());
            VLOG(1) << "create new task!" << task->getId();
            return task;
        } else {
            auto task = std::dynamic_pointer_cast<T>(_task_recycle_pool.front()); 
            _task_recycle_pool.pop();
            int64_t old_task_id = task->getId();
            task->reset();
            task->setId(generateUniqueId());
            VLOG(1) << "reuse task from cycle pool! " << task->getId() << "|" << old_task_id;
            VLOG(1) << "cycle pool size: " << _task_recycle_pool.size();
            return task;
        }
        return nullptr;
    }
    
    uint64_t getQueueLength() {
        std::lock_guard<Spinlock> locker(_lock);
        return _active_task_map.size();
    }

    // thread safe
    void enqueue(std::shared_ptr<AsyncTask> task) {
        std::lock_guard<Spinlock> locker(_lock);
        if (!task) {
            return;
        }
        _active_task_map.insert(std::make_pair(task->getId(), task));
        if (_active_task_map.size() > 1000) {
            VLOG_APP(ERROR) << "task queue length :" 
                << _active_task_map.size() << "|task id:" << task->getId();
        }
        _thread_pool->enqueue([task]() mutable { task->process(); });
        _timer.addTimerEvent(task->getTimeoutThreshold(), TimeoutCallback,(void *)(task->getId()));
    }
    
    void safeReleaseTask(int64_t task_id) {
        std::lock_guard<Spinlock> locker(_lock);
        releaseTask(task_id);
    }

    static void TimeoutCallback(void *args) {
        int64_t task_id =  (int64_t)args;
        AsyncTaskManager& manager = Singleton<AsyncTaskManager>::GetInstance();
        manager.timeoutTask(task_id);
    }
    
    std::shared_ptr<AsyncTask> getSafeTask(int64_t taskId) {
        std::lock_guard<Spinlock> locker(_lock);
        return getTask(taskId);  
    }
  
    static void TimerLoop(void *arg) {
        Timer *te = (Timer *)arg;
        te->loop();
    }
    
    int64_t generateUniqueId() {
        return ++_curr_unique_id;    
    }
    
    static int32_t THREAD_NUM;

private:
    // all privated functions are not thread safe.
    std::shared_ptr<AsyncTask> getTask(int64_t taskId) {
        TaskMap::iterator it = _active_task_map.find(taskId);
        if (_active_task_map.end() != it) {
            return it->second;
        }
        VLOG(1) << "get task failed! " << taskId;   
        return std::shared_ptr<AsyncTask>(nullptr);
    }
    
    void releaseTask(int64_t task_id) {
        auto it = _active_task_map.find(task_id);
        if (it != _active_task_map.end()) {
            std::shared_ptr<AsyncTask> task = it->second;
            _task_recycle_pool.push(task);
            _active_task_map.erase(it);
        }
        VLOG(1) << "release task : " << task_id;   
        VLOG(1) << "task map size: " << _active_task_map.size();   
    }

    void timeoutTask(int64_t task_id) {
        VLOG(1) << "task timeout! task id:" << task_id;   
        std::lock_guard<Spinlock> locker(_lock);
        std::shared_ptr<AsyncTask> task = getTask(task_id);
        if (!task) {
            VLOG(1) << "invalid task : " << task_id;   
            return;
        }
        if (task->isTimeout() || task->isFinished()) {
            // relase task
            releaseTask(task_id);
            return;
        }
        AsyncTaskManager& manager = *this;
        _thread_pool->enqueue([task, &manager]() mutable { 
                task->timeout();
                manager.safeReleaseTask(task->getId());
                });
    }

    Spinlock _lock;
    ThreadPool *_thread_pool;
    Timer _timer;
    TaskMap _active_task_map;
    std::atomic<int64_t> _curr_unique_id;
    std::queue<std::shared_ptr<AsyncTask> >  _task_recycle_pool;
};

#endif
