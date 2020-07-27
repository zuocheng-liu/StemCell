#include "timer_controller.h"

#include <stdexcept>
#include <sstream>
#include <iostream>
#include <errno.h>
#include <cstring>
#include <time.h>

using namespace StemCell;
using namespace std;

static bool TimerTaskComp(TimerTaskPtr a, TimerTaskPtr b) {
    return (a->expect_time.it_value.tv_sec == b->expect_time.it_value.tv_sec ? 
            a->expect_time.it_value.tv_nsec > b->expect_time.it_value.tv_nsec :
            a->expect_time.it_value.tv_sec > b->expect_time.it_value.tv_sec);
}

static bool TimerTaskEarlierComp(TimerTaskPtr a, TimerTaskPtr b) {
    return !TimerTaskComp(a, b);
}

bool TimerController::init() {
    if (_initialized) {
        return true;
    }

    _stop = false;

    // init _eventfd
    _eventfd = eventfd(0, EFD_NONBLOCK);
    if (_eventfd < 0) {
        throw std::runtime_error("Failed to init eventfd");
    }

    // init _timerfd
    _timerfd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
    if (_timerfd < 0) {
        throw std::runtime_error("failed to create timerfd");
    }

    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) < 0) {
        throw std::runtime_error("failed to clock_gettime");
    }
    struct itimerspec new_value;
    new_value.it_value.tv_sec = 1;
    new_value.it_value.tv_nsec = now.tv_nsec;
    new_value.it_interval.tv_sec = 1;
    new_value.it_interval.tv_nsec = 0;

    if (timerfd_settime(_timerfd, 0, &new_value, NULL) < 0) {
        stringstream msg;
        msg << "failed to timerfd_settime when init.  errno: " << errno 
           << " err:" << strerror(errno);
        throw std::runtime_error(msg.str());
    }

    // init _epollfd
    _epollfd = epoll_create1(0);
    if (_epollfd < 0) {
        throw std::runtime_error("epoll_create");
    }

    // add _timerfd and _eventfd to _epollfd
    {
        epoll_event evnt = {0};
        evnt.data.fd = _eventfd;
        evnt.events = EPOLLIN | EPOLLET;
        if (epoll_ctl(_epollfd, EPOLL_CTL_ADD, _eventfd, &evnt) < 0) {
            throw std::runtime_error("failed to epoll_ctl EPOLL_CTL_ADD");
        }
    }
    {
        epoll_event evnt = {0};
        evnt.data.fd = _timerfd;
        evnt.events = EPOLLIN | EPOLLET;
        if (epoll_ctl(_epollfd, EPOLL_CTL_ADD, _timerfd, &evnt) < 0) {
            throw std::runtime_error("failed to epoll_ctl EPOLL_CTL_ADD");
        }
    }

    // make timer task heap
    make_heap(_timer_task_heap.begin(), _timer_task_heap.end(), TimerTaskComp);
    
    // init loop thread
    TimerController *tc = this;
    _loop_thread = make_shared<thread>([tc]() mutable { tc->loop(); });
    
    _initialized = true;
    return true;
}

void TimerController::loop() {
    static const int EVENTS = 20;
    struct epoll_event evnts[EVENTS];
    int count = 0;
    for(;;) {
        if (_stop) {
            return; 
        }
        count = epoll_wait(_epollfd, evnts, EVENTS, -1); 
        if (count < 0 && errno != EINTR) {
            throw std::runtime_error("epoll_wait");
        }
        for (int i = 0; i < count; ++i) {
            struct epoll_event *e = evnts + i;
            if (e->data.fd == _eventfd) {
                eventfd_t val;
                eventfd_read(_eventfd, &val);
                if (val >= EVENT_STOP) {
                    close();
                    return;
                } else {
                    custTimerTask();
                }
            }
            if (e->data.fd == _timerfd) {
                execEarliestTimerTask();
            }
        }
    }
}

void TimerController::addTimerTask(TimerTaskPtr timer_task) {
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) < 0) {
        throw std::runtime_error("failed to clock_gettime");
    }
    timer_task->create_time.it_value.tv_sec = now.tv_nsec;
    timer_task->create_time.it_value.tv_nsec = now.tv_nsec;
    
    int64_t delay_time = timer_task->interval;
    int64_t second = delay_time / 1000;
    int64_t narosecond = (delay_time % 1000) * 1000000;
    struct itimerspec& expect_time = timer_task->expect_time;
    int64_t delta = (narosecond + now.tv_nsec) / 1000000000;
    expect_time.it_value.tv_sec = now.tv_sec + second + delta;
    expect_time.it_value.tv_nsec = (narosecond + now.tv_nsec) % 1000000000;
    {
        std::lock_guard<Spinlock> locker(_lock);
        _timer_task_queue.push(timer_task);
    }
    eventfd_t wdata = EVENT_ADD_TASK;
    if(eventfd_write(_eventfd, wdata) < 0) {
        throw std::runtime_error("failed to writer eventfd");
    }
}

void TimerController::refreshTimer(TimerTaskPtr task) {
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) < 0) {
        throw std::runtime_error("failed to clock_gettime");
    }
    struct itimerspec& expect_time = task->expect_time;
    struct itimerspec new_value;
    if (expect_time.it_value.tv_sec < now.tv_sec || 
            (expect_time.it_value.tv_sec == now.tv_sec 
             && expect_time.it_value.tv_nsec <= now.tv_nsec)) {
        new_value.it_value.tv_sec = 0;
        new_value.it_value.tv_nsec = 1;
    } else {
        int64_t sec = expect_time.it_value.tv_sec - now.tv_sec;
        int64_t nsec = expect_time.it_value.tv_nsec - now.tv_nsec;
        if (nsec < 0) {
            --sec;
            nsec = 1000000000 + nsec;
        }
        
        int64_t delay_time = task->interval;
        int64_t second = delay_time / 1000;
        int64_t narosecond = (delay_time % 1000) * 1000000;
        new_value.it_value.tv_sec = sec;
        new_value.it_value.tv_nsec = nsec;
    }
    
    new_value.it_interval.tv_sec = 1;
    new_value.it_interval.tv_nsec = 0;
    if (timerfd_settime(_timerfd, 0, &new_value, NULL) < 0) {
        throw std::runtime_error("failed to timerfd_settime when refresh");
    }
    
    epoll_event evnt = {0};
    evnt.data.fd = _timerfd;
    evnt.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(_epollfd, EPOLL_CTL_MOD, _timerfd, &evnt) < 0) {
        throw std::runtime_error("failed to epoll_ctl EPOLL_CTL_ADD");
    }
}

void TimerController::custTimerTask() {
    for(;;) {
        TimerTaskPtr task;
        {
            std::lock_guard<Spinlock> locker(_lock);
            if (_timer_task_queue.empty()) {
                return;
            }
            task = _timer_task_queue.front();
            _timer_task_queue.pop();
        }
        if (!_timer_task_heap.empty()) {
            TimerTaskPtr earliestTimerTask = _timer_task_heap.front();
            if (TimerTaskEarlierComp(task, earliestTimerTask)) {
                refreshTimer(task);           
            } 
        } else {
            refreshTimer(task);           
        }
        _timer_task_heap.emplace_back(task);
        push_heap(_timer_task_heap.begin(), _timer_task_heap.end(), TimerTaskComp);
    }
}

void TimerController::execEarliestTimerTask() {
    if (_timer_task_heap.empty()) {
        return;
    }
    TimerTaskPtr earliestTimerTask = _timer_task_heap.front();
    pop_heap(_timer_task_heap.begin(), _timer_task_heap.end(), TimerTaskComp);
    _timer_task_heap.pop_back();
    earliestTimerTask->fun();
    if (!_timer_task_heap.empty()) { 
        earliestTimerTask = _timer_task_heap.front();
        refreshTimer(earliestTimerTask);
    }
    earliestTimerTask->reset();
    _timer_task_pool.recycle(earliestTimerTask);
}
