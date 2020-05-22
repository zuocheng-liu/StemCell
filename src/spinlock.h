#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <atomic>
#include <utils/vlog/loghelper.h>

using namespace vlog;

class Spinlock {
public:
    Spinlock() : flag(ATOMIC_FLAG_INIT), id(0), wait_count(10000) {}

    void lock() {
        int64_t i = 0, j = 0;
        while(flag.test_and_set(std::memory_order_acquire)) {
            __asm__ ("pause");
            if (++i > wait_count) {
                ++j;
                if (j > 3) {
                    VLOG_APP(WARNING) << "spinlock "<< id << "! yield times:" << j;
                }
                sched_yield();
                i = 0;
            } 
        }
        // VLOG_APP(INFO) << "spinlocked !" << (int64_t)this;
    }

    bool try_lock() {
        if (flag.test_and_set(std::memory_order_acquire)) {
            return false;
        }
        return true;
    }

    void unlock() { 
        flag.clear(std::memory_order_release); 
        // VLOG_APP(INFO) << "spinlock released !" << (int64_t)this;
    }

    void setId(int64_t id) { this->id = id; }

private :
    std::atomic_flag flag;
    int64_t id;
    int32_t wait_count;
};
#endif
