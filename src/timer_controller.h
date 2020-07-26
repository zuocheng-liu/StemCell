#ifndef TIMER_TASK_CONTROLLER_H
#define TIMER_TASK_CONTROLLER_H

#include <vector>
#include <queue>
#include <memory>
#include <mutex>
#include <thread>
#include <map>
#include <future>
#include <functional>
#include <stdexcept>

#include <time.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include "spinlock.h"

namespace StemCell {

struct TimerTask {
    std::function<void()> fun;
    struct itimerspec interval;
    struct itimerspec create_time;
    struct itimerspec expect_time;
};

typedef std::shared_ptr<TimerTask> TimerTaskPtr;
typedef ObjectPool<TimerTask> TimerTaskPool;


class TimerController {
public:
    TimerController() 
        : _epollfd(0), 
        _eventfd(0), 
        _timerfd(0), 
        _stop(false), 
        _initialized(false) {}

    bool init(); 

    auto delayProcess(int64_t delay_time, F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    //auto cycleProcess(int64_t term, F&& f, Args&&... args) 
    //    -> std::future<typename std::result_of<F(Args...)>::type>;

private:
    
    void loop(); 
    void custTimerTask();   
    void addTimerTask(TimerTaskPtr& task);
    void refreshTimer(TimerTaskPtr& task);
    void execEarliestTimerTask();
    TimerTaskPtr createTimerTask() { return _timer_task_pool.createObject(); }

    bool TimerTaskComp(TimerTaskPtr& a, TimerTaskPtr& b) {
        return (a->expect_time.it_value.tv_sec == b->expect_time.it_value.tv_sec ? 
                a->expect_time.it_value.tv_usec >= b->expect_time.it_value.tv_usec :
                a->expect_time.it_value.tv_sec >= b->expect_time.it_value.tv_sec);
    }

    bool TimerTaskEarlierComp(TimerTaskPtr& a, TimerTaskPtr& b) {
        return TimerTaskComp(a, b);
    }
    

    int32_t _epollfd;
    int32_t _eventfd;
    int32_t _timerfd;
    Spinlock _lock;
    std::queue<TimerTaskPtr> _timer_task_queue;
    std::vector<TimerTaskPtr> _timer_task_heap;
    bool _stop;
    bool _initialized;
    TimerTaskPool _timer_task_pool;
};

// add new work item to the pool
template<class F, class... Args>
auto TimerController::delayProcess(int64_t delay_time, F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<return_type> res = task->get_future();
    
    TimerTaskPtr timer_task = createTimerTask();
    timer_task->fun = [task](){ (*task)(); }
    addTimerTask(timer_task);
    return res;
}

} // end namespace StemCell
#endif
