#ifndef TIMER_CONTROLLER_H
#define TIMER_CONTROLLER_H

#include <vector>
#include <queue>
#include <memory>
#include <map>
#include <future>
#include <functional>
#include <thread>
#include <iostream>

#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include "spinlock.h"
#include "object_pool.hpp"

namespace StemCell {

struct TimerTask {
    bool is_cycle;
    long interval;
    struct timespec create_time;
    struct timespec expect_time;
    std::function<void()> fun;
    
    TimerTask() : 
        is_cycle(false), 
        interval(0), 
        create_time({0}), 
        expect_time({0}) {}

    void reset() {
        is_cycle = false;
        interval = 0;
        create_time = {0};
        expect_time = {0};
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
        _initialized(false) {}

    ~TimerController() { 
        stop();
        close();
        if (_loop_thread.joinable()) _loop_thread.join();
    }

    bool init(); 
    void stop() { 
        if (_stop) return;
        _stop = true; 
        eventfd_t wdata = EVENT_STOP;
        if(eventfd_write(_eventfd, wdata) < 0) {
            throw std::runtime_error("failed to writer eventfd");
        }
        if (_loop_thread.joinable()) _loop_thread.join();
    }

    template<class F, class... Args>
    void delayProcess(int64_t delay_time, F&& f, Args&&... args);
    template<class F, class... Args>
    void  cycleProcess(int64_t interval, F&& f, Args&&... args);

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
    std::thread _loop_thread;
};

// add new work item to the timer heap
template<class F, class... Args>
void TimerController::delayProcess(int64_t delay_time, F&& f, Args&&... args) {
    if(_stop) { 
        throw std::runtime_error("TimerController is stoped!");
    }
    if (delay_time < 0) {
        throw std::runtime_error("delay_time is less than zero!");
    }
    using return_type = typename std::result_of<F(Args...)>::type;
    std::function<return_type()> task = 
        std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    TimerTaskPtr timer_task = createTimerTask();
    timer_task->interval = delay_time;
    timer_task->create_time = {0};
    timer_task->fun = [task](){ task(); };
    addTimerTask(timer_task);
}

template<class F, class... Args>
void TimerController::cycleProcess(int64_t interval, F&& f, Args&&... args) {
    if(_stop) { 
        throw std::runtime_error("TimerController is stoped!");
    }
    if (interval < 0) {
        throw std::runtime_error("interval is less than zero!");
    }
    using return_type = typename std::result_of<F(Args...)>::type;
    std::function<return_type()> task = 
        std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    TimerTaskPtr timer_task = createTimerTask();
    timer_task->is_cycle = true;
    timer_task->interval = interval;
    timer_task->create_time = {0};
    timer_task->fun = [task](){ task(); };
    addTimerTask(timer_task);
}

} // end namespace StemCell
#endif
