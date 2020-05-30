#ifndef ASYNC_TASK_CONTEXT_H
#define ASYNC_TASK_CONTEXT_H

namespace StemCell {

struct AsyncTaskContext {
public:
    AsyncTaskContext() {}
    virtual ~AsyncTaskContext() {}
    virtual void reset() = 0;
};

#endif
