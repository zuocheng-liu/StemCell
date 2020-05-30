#ifndef __SINGLETON_H__
#define __SINGLETON_H__

#include <pthread.h>
#include <cassert>
#include <mutex>

namespace StemCell {

template<class T>
class Singleton {
public:
    static T& GetInstance() {
        if (nullptr == instance) {
            pthread_once(&g_once_control, InitOnce);
        }
        assert(instance != nullptr);
        return *instance;
    }
private:
    static void InitOnce() { instance = new T(); }
    static pthread_once_t g_once_control;
    static T *instance;
};

template<class T>
T* Singleton<T>::instance = nullptr;

template<class T>
pthread_once_t Singleton<T>::g_once_control = PTHREAD_ONCE_INIT;


template<class T>
class ThreadSpecificSingleton {
public:
    static T& GetInstance() {
        pthread_once(&g_once_control, InitOnce);
        T *thread_specific_instance = (T*)pthread_getspecific(g_thread_data_key);
        if (nullptr == thread_specific_instance) {
            thread_specific_instance = new T();
            pthread_setspecific(g_thread_data_key, (void*)thread_specific_instance);
        }
        return *thread_specific_instance;
    }

private:
    static void InitOnce(void) {
        pthread_key_create(&g_thread_data_key, NULL);
    }

    static pthread_once_t g_once_control;
    static pthread_key_t g_thread_data_key;
};

template<class T>
pthread_once_t ThreadSpecificSingleton<T>::g_once_control = PTHREAD_ONCE_INIT;

template<class T>
pthread_key_t  ThreadSpecificSingleton<T>::g_thread_data_key;



template<class T>
class ThreadLocalSingleton {
public:
    static T& GetInstance() {
        if (nullptr == instance) {
            std::call_once(s_flag, [&]() { instance = new T();});
        }
        assert(instance != nullptr);
        return *instance;
    }

private:
    static thread_local std::once_flag s_flag;;
    static thread_local T* instance;
};

template<class T>
thread_local T* ThreadLocalSingleton<T>::instance = nullptr;

} // end namespace StemCell
#endif
