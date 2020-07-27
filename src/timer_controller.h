#ifndef TIMER_CONTROLLER_H
#define TIMER_CONTROLLER_H

#include <vector>
#include <queue>
#include <memory>
#include <map>
#include <future>
#include <functional>
#include <thread>
#include <cstring>

#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include "spinlock.h"
#include "object_pool.hpp"

namespace StemCell {

struct TimerTask {
    long interval;
    struct itimerspec create_time;
    struct itimerspec expect_time;
    std::function<void()> fun;

    void reset() {
        interval = 0;
        bzero(&create_time, sizeof(struct itimerspec));
        bzero(&expect_time, sizeof(struct itimerspec));
    }
};

typedef TimerTask* TimerTaskPtr;

class TimerController {
public:
    typedef ObjectPool<TimerTask> TimerTaskPool;
    
    enum EventType { 
        // 0 is an invalid val in eventfd 
        EVENT_ADD_TASK = 1, 
        EVENT_STOP = 0x100000000 
    }; 

    TimerController() 
        : _epollfd(0), 
        _eventfd(0), 
        _timerfd(0), 
        _stop(true), 
        _initialized(false), 
        _loop_thread(nullptr) {}

    ~TimerController() { 
        stop();
        close();
        if (_loop_thread)  _loop_thread->join();
    }

    bool init(); 
    void stop() { 
        if (_stop) return;
        eventfd_t wdata = EVENT_STOP;
        if(eventfd_write(_eventfd, wdata) < 0) {
            throw std::runtime_error("failed to writer eventfd");
        }
        _stop = true; 
    }

    template<class F, class... Args>
    auto delayProcess(int64_t delay_time, F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;
    template<class F, class... Args>
    auto cycleProcess(int64_t interval, F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;

private:
    
    void loop(); 
    void custTimerTask();   
    void addTimerTask(TimerTaskPtr task);
    void refreshTimer(TimerTaskPtr task);
    void execEarliestTimerTask();
    
    void close() {
        if (!_initialized) return;
        _stop = true;
        ::close(_eventfd);
        _eventfd = 0;
        ::close(_timerfd);
        _timerfd = 0;
        ::close(_epollfd);
        _epollfd = 0;
        _initialized = false;
    }

    TimerTaskPtr createTimerTask() { 
        return &(_timer_task_pool.createInstance());
    }
    
    int32_t _epollfd;
    int32_t _eventfd;
    int32_t _timerfd;
    volatile bool _stop;
    bool _initialized;
    Spinlock _lock;
    std::queue<TimerTaskPtr> _timer_task_queue;
    std::vector<TimerTaskPtr> _timer_task_heap;
    TimerTaskPool _timer_task_pool;
    std::shared_ptr<std::thread> _loop_thread;
};

// add new work item to the timer heap
template<class F, class... Args>
auto TimerController::delayProcess(int64_t delay_time, F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
    if(_stop) { 
        throw std::runtime_error("TimerController is stoped!");
    }
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<return_type> res = task->get_future();
    TimerTaskPtr timer_task = createTimerTask();
    timer_task->interval = delay_time;
    timer_task->fun = [task](){ (*task)(); };
    addTimerTask(timer_task);
    return res;
}

template<class F, class... Args>
auto TimerController::cycleProcess(int64_t interval, F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    if(_stop) { 
        throw std::runtime_error("TimerController is stoped!");
    }
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<return_type> res = task->get_future();
    
    TimerTaskPtr timer_task = createTimerTask();
    timer_task->interval = interval;
    timer_task->fun = [task](){ (*task)(); };
    addTimerTask(timer_task);
    return res;
}

} // end namespace StemCell
#endif
