#ifndef LOCK_FREE_OBJECT_POOL_HPP
#define LOCK_FREE_OBJECT_POOL_HPP

#include "lock_free_queue.hpp"
namespace StemCell {

template<class T>
class LockFreeObjectPool {
public: 
    T* createInstance() {
        T* object = pool.pop();
        return (nullptr == object) ? (new T()) : object;
    }

    bool recycle(T& *object) {
        for (;;) {
            T *tmp = object;
            if (nullptr == tmp) { return false; }
            if(!__sync_bool_compare_and_swap(&object, tmp, nullptr)) {
                continue;
            } else {
                pool.push(tmp);
                return true;
            }
        }
    }

private:
    LockFreeQueue<T> pool;
};

} // end namespace StemCell
#endif
