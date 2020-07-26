#include "timer_task_controller.h"

using namespace StemCell;
using namespace std;

bool TimerController::init() {
    if (initialized) {
        return true;
    }
    _stop = false;
    _latest_task_id = 0;

    // init _eventfd
    _eventfd = eventfd(0, EFD_NONBLOCK);
    if (_eventfd < 0) {
        throw std::runtime_error("Failed in eventfd\n");
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

    if (timerfd_settime(_timefd, 0, &new_value, NULL) < 0) {
        throw std::runtime_error("failed to timerfd_settime");
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

    initialized = true;
    return true;
}

void TimerController::loop() {
    if (!initialized) {
        init();
    }
    const int EVENTS = 20;
    struct epoll_event evnts[EVENTS];
    for(;;) {
        if (epoll_wait(_epollfd, evnts, EVENTS, -1) < 0)
            if (errno != EINTR) {
                throw std::runtime_error("epoll_wait");
            }
    }
    for (int i = 0; i < count; ++i) {
        struct epoll_event *e = evnts + i;
        if (e->data.fd == _eventfd) {
            eventfd_t val;
            eventfd_read(_eventfd, &val);
            custTimerTask();
        }
        if (e->data.fd == _timerfd) {
            execEarliestTimerTask();
        }
    }
}

void TimerController::addTimerTask(TimerTaskPtr& task) {
    if (clock_gettime(CLOCK_REALTIME, &(timer_task->create_time)) < 0) {
        throw std::runtime_error("failed to clock_gettime");
    }

    long second = delay_time / 1000;
    long narosecond = (delay_time % 1000) * 1000000;
    timer_task->interval.it_value.tv_sec = second;
    timer_task->interval.it_value.tv_nsec = narosecond;
    timer_task->expect_time.it_value.tv_nsec = (narosecond + timer_task->create_time.it_value.tv_nsec) % 1000000000;
    timer_task->expect_time.it_value.tv_sec = timer_task->create_time.it_value.tv_sec;
    {
        std::lock_guard<Spinlock> locker(_lock);
        _timer_task_queue.push(timer_task);
    }
    uint64_t wdata = 1;
    if(write(_eventfd, &wdata , sizeof(uint64_t)) < 0) {
        throw std::runtime_error("failed to writer eventfd");
    }
}

void TimerController::refreshTimer(TimerTaskPtr& task) {
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) < 0) {
        throw std::runtime_error("failed to clock_gettime");
    }
    struct itimerspec& expect_time = task->expect_time;
    struct itimerspec new_value;
    if (expect_time.it_value.tv_sec < now.it_value.tv_sec || 
            (expect_time.it_value.tv_sec == now.it_value.tv_sec 
             && expect_time.it_value.tv_nsec <= now.it_value.tv_nsec)) {
        new_value.it_value.tv_sec = 0;
        new_value.it_value.tv_nsec = 1;
    } else {
        long sec = expect_time.it_value.tv_sec - now.it_value.tv_sec;
        long nsec = expect_time.it_value.tv_nsec - now.it_value.tv_nsec;
        if (nsec < 0) {
            --sec;
            nsec = 1000000000 + nsec;
        }
        new_value.it_value.tv_sec = sec;
        new_value.it_value.tv_nsec = nsec;
    }
    
    if (timerfd_settime(_timefd, 0, &new_value, NULL) < 0) {
        throw std::runtime_error("failed to timerfd_settime");
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
        std::lock_guard<Spinlock> locker(_lock);
        if (_timer_task_queue.empty()) {
            return;
        }
        TimerTaskPtr task = _timer_task_queue.front();
        _timer_task_queue.pop();

        if (!_timer_task_heap.empty()) {
            TimerTaskPtr earliestTimerTask = *(_timer_task_heap.begin());
            if (!TimerTaskEarlierComp(task, earliestTimerTask)) {
                refreshTimer(task);           
            } 
        }
        _timer_task_heap.emplace_back(task);
        push_heap(_timer_task_heap.begin(), _timer_task_heap.end(), TimerTaskComp);
    }
}

void TimerController::execEarliestTimerTask() {
    for(;;) {
        // std::lock_guard<Spinlock> locker(_lock);
        if (_timer_task_heap.empty()) {
            return;
        }
        TimerTaskPtr earliestTimerTask = *(_timer_task_heap.begin());
        pop_heap(_timer_task_heap.begin(), _timer_task_heap.end(), TimerTaskComp);
        _timer_task_heap.pop_back();
        earliestTimerTask->fun();
        if (!_timer_task_heap.empty()) { 
            earliestTimerTask = *(_timer_task_heap.begin());
            refreshTimer(earliestTimerTask);
        }        
    }
}
