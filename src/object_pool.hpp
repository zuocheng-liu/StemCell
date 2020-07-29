#ifndef OBJECT_POOL_HPP
#define OBJECT_POOL_HPP

#include <memory>
#include <queue>
#include <map>
#include <cassert>
// #include <butil/logging.h>
#include "spinlock.h"

namespace StemCell {

template<class T>
class ObjectPool {
public: 
    std::shared_ptr<T> getSharedPtr() {
        std::lock_guard<Spinlock> locker(_lock);
        if (_recycle_pool.empty()) {
            std::shared_ptr<T> instance = std::make_shared<T>();
            T* ptr = &(*instance);
            _ptr_map.insert(std::make_pair(ptr, instance));
            return instance;
        } else {
            std::shared_ptr<T> instance = _recycle_pool.front();
            _recycle_pool.pop();
            return instance;
        }
    }
    
    T& createInstance() {
        return *getSharedPtr();
    } 

    std::shared_ptr<T> createObject() { return getSharedPtr(); }

    void recycle(std::shared_ptr<T> ptr) {
        if (!ptr) {
            return;
        }
        T* object = &(*ptr);
        recycle(object);
    }

    void recycle(T *object) {
        std::lock_guard<Spinlock> locker(_lock);
        if (nullptr == object) {
            return;
        }
        auto it = _ptr_map.find(object);
        if (_ptr_map.end() == it) {
            abort();
            return;
        }
        _recycle_pool.push(it->second);
    }

private:
    std::queue<std::shared_ptr<T> >  _recycle_pool;
    std::map<T*, std::shared_ptr<T> >  _ptr_map;
    Spinlock _lock;
};

} // end namespace StemCell
#endif
