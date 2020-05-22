#include "timer_event.h"

int64_t TimerEvent::CurrUniqueId = 0;

TimerEvent::TimerEvent(int32_t timeout, void (*func)(void*), void *args, Timer *timer) 
    :timeout(timeout), callback(func), args(args), timer(timer) {
    id = ++CurrUniqueId;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    VLOG(1) << "event add second:" << tv.tv_sec <<"| usec:"<< tv.tv_usec;   
    ev = event_new(timer->getBase(), -1, 0, Timer::Callback, this);
}

void TimerEvent::assign(int32_t timeout, void (*func)(void*), void *args, Timer *timer) {
    timeout = timeout;
    callback = func;
    // VLOG_APP(INFO) << "event assign! old taskid:" << (int32_t)this->args 
    // << " new taskid:" << (int32_t)args;   
    this->args = args;
    timer = timer;
    // id = ++CurrUniqueId; reused event, can not assign new id
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    VLOG(1) << "event add second:" << tv.tv_sec <<"| usec:"<< tv.tv_usec;   
    // event_assign(ev, timer->getBase(), -1, 0, Timer::Callback, this);
}

