#ifndef OBJECT_POOL_HPP
#define OBJECT_POOL_HPP

#include <memory>
#include <queue>
#include <map>
#include <cassert>
#include <butil/logging.h>
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
            VLOG(1) << "create object! " << (int64_t)ptr;
            if (_ptr_map.size() > 1000) {
                LOG(WARNING) << "object pool is full! size:" << _ptr_map.size()
                   << " pool:" << (int64_t)this << " class_name:" << typeid(T).name();
            }
            return instance;
        } else {
            std::shared_ptr<T> instance = _recycle_pool.front();
            _recycle_pool.pop();
            // T* ptr = &(*instance);
            // VLOG(1) << "use crycling object! " << (int64_t)ptr;
            return instance;
        }
    }
    
    T& createInstance() {
        return *getSharedPtr();
    } 

    std::shared_ptr<T> createObject() { return getSharedPtr(); }

    void recycle(std::shared_ptr<T> ptr) {
        if (!ptr) {
            LOG(ERROR) << "shared_ptr is nullptr!";
        }
        T* object = &(*ptr);
        recycle(object);
    }

    void recycle(T *object) {
        std::lock_guard<Spinlock> locker(_lock);
        if (nullptr == object) {
            LOG(ERROR) << "object is nullptr!";
            return;
        }
        auto it = _ptr_map.find(object);
        if (_ptr_map.end() == it) {
            LOG(ERROR) << "object is not created by object pool!" << (int64_t)object;
            abort();
            return;
        }
        // LOG(ERROR) << "recrycle object! " << (int64_t)object;
        _recycle_pool.push(it->second);
    }

private:
    std::queue<std::shared_ptr<T> >  _recycle_pool;
    std::map<T*, std::shared_ptr<T> >  _ptr_map;
    Spinlock _lock;
};

} // end namespace StemCell
#endif
