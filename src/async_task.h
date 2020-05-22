#ifndef ASYNC_TASK_H
#define ASYNC_TASK_H

#include <cstdint>
#include <mutex>
#include <memory>
#include <butil/logging.h>
#include "async_task_context.h"
#include "spinlock.h"

class AsyncTask {
public:
    enum Status {
        UNSCHEDULED = 0,
        PROCESSING,
        TIMEOUT,
        FINISHED
    };

    AsyncTask():id(0),parent_id(0),timeout_threshold(0), status(UNSCHEDULED) {};
    virtual ~AsyncTask() {};
    virtual void process() = 0;
    virtual void timeout() = 0;
    virtual void close() = 0;
    virtual void reset() {
        id = 0;
        parent_id = 0;
        timeout_threshold = 0;
        status = UNSCHEDULED;
        context->reset();
    }
    
    void setId(int64_t id) { this->id = id; _lock.setId(id); }
    int64_t getId() { return id; }
    void setParentId(int64_t id) { parent_id = id; }
    int64_t getParentId() { return parent_id; }
    void setTimeoutThreshold(uint32_t timeout_threshold) { 
        this->timeout_threshold = timeout_threshold; 
    }
    uint32_t getTimeoutThreshold() { return timeout_threshold; }
    bool isUnscheduled() { return (status == UNSCHEDULED); }
    bool isTimeout() { return (status == TIMEOUT); }
    void setTimeout() { status = TIMEOUT; }
    bool isFinished() { return (status == FINISHED); }
    void setFinished() { status = FINISHED; }
    void setStatus(Status status) { this->status = status; }
    Spinlock& getLock() { return _lock; }
    std::shared_ptr<AsyncTaskContext> getContext() { return context; }
    template <class T> 
    std::shared_ptr<T> getContext() {
        static_assert(std::is_base_of<AsyncTaskContext, T>::value, 
                "Derived not derived from BaseClass");
        return std::dynamic_pointer_cast<T>(context); 
    }
    
    void setContext(std::shared_ptr<AsyncTaskContext> context) { 
        this->context = context; 
    }

private:
    int64_t id;
    int64_t parent_id;
    uint32_t timeout_threshold; // millisecond;
    Status status;
    Spinlock _lock;
    std::shared_ptr<AsyncTaskContext> context;
};
#endif
