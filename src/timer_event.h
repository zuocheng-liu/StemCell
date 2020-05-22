#ifndef TIMER_EVENT_H
#define TIMER_EVENT_H

#include <event2/event.h>
#include <event2/dns.h>
#include <utils/vlog/loghelper.h>
#include <mutex>
#include <queue>
#include <map>
#include <memory>
#include <atomic>
#include "spinlock.h"

using namespace vlog;
class Timer;

struct TimerEvent {
    TimerEvent(int32_t timeout, void (*func)(void*), void *args, Timer *timer); 
    void assign(int32_t timeout, void (*func)(void*), void *args, Timer *timer);
    int32_t timeout;
    void (*callback)(void*);
    void *args;
    Timer *timer;
    
    struct event *ev;
    struct timeval tv;
    int64_t id;
    // libevent 在单线程下运行，这里是安全的
    static int64_t CurrUniqueId;
};

class Timer {
public:
    Timer() : keep_ev(nullptr) {
        event_base_ptr_ = event_base_new();
        // evdns_base_ptr_的作用为阻止基于event_base_ptr_的事件循环退出,
        // 是非常必要的
        evdns_base_ptr_ = evdns_base_new(event_base_ptr_, EVDNS_BASE_INITIALIZE_NAMESERVERS);
        KeepTimer(0, 0, this);
        _lock.setId(-1);
    }

    ~Timer() {
        if (event_base_ptr_ != NULL) {
            event_base_free(event_base_ptr_);
            event_base_ptr_ = NULL;
        }
        if (evdns_base_ptr_ != NULL) {
            evdns_base_free(evdns_base_ptr_, 0);
            evdns_base_ptr_ = NULL;
        }
    }

    void addTimerEvent(int32_t timeout_threshold, 
            void (*callback_func)(void*), void *args) {
        // std::lock_guard<std::mutex> locker(_lock);
        std::lock_guard<Spinlock> locker(_lock);
        // VLOG_APP(INFO) << "spinlock:" << (int64_t)&_lock;
        if (event_recycle_pool.empty()) {
            std::shared_ptr<TimerEvent> te = std::make_shared<TimerEvent>(timeout_threshold, callback_func, args, this);
            event_map.insert(std::make_pair(te->id, te));
            event_queue.push(te);
            VLOG(1) << "event_recycle_pool is empty, new one: " << te->id;
        } else {
            std::shared_ptr<TimerEvent> te = event_recycle_pool.front();
            te->assign(timeout_threshold, callback_func, args, this); 
            VLOG(1) << "get one ev in recycle_pool : " << te->id;
            event_queue.push(te);
            event_recycle_pool.pop();
        }
    }
    
    void recycleTimerEvent(int64_t te_id) {
        // std::lock_guard<std::mutex> locker(_lock);
        std::lock_guard<Spinlock> locker(_lock);
        // VLOG_APP(INFO) << "spinlock:" << (int64_t)&_lock;
        auto it = event_map.find(te_id);
        if (it == event_map.end()) {
            VLOG_APP(ERROR) << "TimerEvent is not existed! " << te_id;
            return;
        }
        event_recycle_pool.push(it->second);
    }

    void loop() {
        VLOG(1) << "start loop";   
        event_base_dispatch(event_base_ptr_);
    }

    static void Callback(int fd, short event, void *args) {
        TimerEvent *te =  (TimerEvent *)args;
        VLOG(1) << "Timer callback! taskid:" << te->args;   
        te->callback(te->args);
        te->timer->recycleTimerEvent(te->id);
        te->timer->custTimerEvent();
    }
    
    event_base *getBase() { return  event_base_ptr_; }

private:
    void custTimerEvent() {
        std::lock_guard<Spinlock> locker(_lock);
        while(!event_queue.empty()) {
            std::shared_ptr<TimerEvent> te = event_queue.front();
            event_add(te->ev, &(te->tv));
            event_queue.pop();
        }
    }
    
    static void KeepTimer(int fd, short event, void *args) {
        Timer *te = (Timer *)args;
        VLOG(1) << "keep timer!repeat it!";   
        if (!te->keep_ev) {
            VLOG_APP(INFO) << "keep timer ev is null, new it!";   
           te->keep_ev = event_new(te->event_base_ptr_, -1, 0, KeepTimer, te);
        }
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10 * 1000;
        event_add(te->keep_ev, &tv);
        te->custTimerEvent();
    }
    
    event_base * event_base_ptr_;
    evdns_base * evdns_base_ptr_;
    
    Spinlock _lock;  // for event_queue
    std::map<int64_t, std::shared_ptr<TimerEvent> > event_map;
    std::queue<std::shared_ptr<TimerEvent> > event_queue;
    std::queue<std::shared_ptr<TimerEvent> > event_recycle_pool;
    struct event *keep_ev; 
};

#endif
