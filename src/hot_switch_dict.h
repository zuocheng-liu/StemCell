/**
 * @brief: 用来加载动态管理的词典
 */
#ifndef _HOT_SWITCH_DICT_H_
#define _HOT_SWITCH_DICT_H_
#include <string>
#include <pthread.h>
#include <unistd.h>
#include <fstream>
#include <sys/stat.h>
#include <iostream>
#include <time.h>
#include <utils/vlog/loghelper.h>

using namespace vlog;


/**
 * @brief 获取当前时间
 */
inline std::string _getCurrentTime() {
    time_t t = time(NULL);
    /*本地时间：日期，时间 年月日，星期，时分秒*/
    struct tm* c_t = localtime(&t);
    char buf[20];
    sprintf(buf, "%d-%d-%d %d:%d:%d", c_t->tm_year+1900, c_t->tm_mon, 
            c_t->tm_mday, c_t->tm_hour, c_t->tm_min, c_t->tm_sec);
    return std::string(buf);
}

template <class DictType> class HotSwitchDict;

template <class DictType> 
void* monitorThread(void *arg) {
    HotSwitchDict<DictType> * pHotDict = (HotSwitchDict<DictType>*)arg;
    bool needSleep = false;
    uint32_t curSleepTime = 0;

    // 检测是否需要停止检测
    while( !pHotDict->m_stopMonitor )
    {
        if( needSleep ) {
            VLOG(1) << "if need sleep " << curSleepTime << " " << pHotDict->m_sleepTime ;
            if( curSleepTime++ < pHotDict->m_sleepTime ) {
                sleep(1);
                continue;
            }
            else {
                needSleep = false;
                curSleepTime = 0;
            }
        }
        // 检测pHotDict->m_pSwitchDict是否不为空 2012-08-12
        // 不为空，判断是否到底备用词典的延迟生命周期
        if( pHotDict->m_pSwitchDict != NULL ) {
            delete pHotDict->m_pSwitchDict;
            pHotDict->m_pSwitchDict = NULL;
        }
        // 检测新文件是否到来
        if(pHotDict->isNewDictArrived())
        {
            // 构造新的DictType类型指针
            pHotDict->m_pSwitchDict = 
                pHotDict->newDictFunc(pHotDict->getNewDictFileName());
            // 判断词典是否加载成功
            if(pHotDict->m_pSwitchDict == NULL) {
                VLOG_APP(WARNING) << "[" << _getCurrentTime() << 
                    "] Error when load new Dictionary!";
            }
            else {
                // 加读锁后，交换m_pCurDict和m_pSwitchDict指针
                DictType * pDictTemp = pHotDict->m_pCurDict;
                pthread_rwlock_wrlock(&pHotDict->m_rwLock);
                pHotDict->m_pCurDict = pHotDict->m_pSwitchDict;
                // 不在删除，直接将m_pSwitchDict指向原来的内存空间
                // 保证指针不删除原来m_pCurDict的指针
                pHotDict->m_pSwitchDict = pDictTemp;
                pthread_rwlock_unlock(&pHotDict->m_rwLock);
                //  delete pDictTemp;
                // 修改当前内存中的词典名字
                pHotDict->switchFileName();
                VLOG_APP(INFO) << "[" << _getCurrentTime() 
                    << "] New Dictionary Switched!";
            }
            needSleep = true;
        }
        else { // 继续监控
            needSleep = true;
        }
    }
    return NULL;
}

// 默认的词典构建对象函数
template <class DictType>
inline DictType * DefaultNewDictFunc(const std::string & fileName) {
    DictType * tmpDict;
    try {
        tmpDict = new DictType(fileName.c_str());
    } catch (int e) {
        return NULL;
    }
    return tmpDict;
}

template <class DictType>
class HotSwitchDict {
    public:
        typedef DictType* (*NewDictFunc)(const std::string & fileName);
        // 友元函数，需要操作HotSwitchDict的private变量
        friend void * monitorThread<DictType>(void * arg);
        /**
         * @brief HotSwitchDict 构造函数
         * @param fileName 词典的文件名
         * @param flagFile 词典的监控文件，保存新词典的名字和md5码
         * @param sleepTime 每次词典监控的扫描时间间隔
         */
        HotSwitchDict(const std::string & fileName, const std::string & flagFile, 
                uint32_t sleepTime = 1)
            : m_pCurDict(NULL), m_pSwitchDict(NULL), m_fileName(fileName),
            m_newFileName(""), m_flagFile(flagFile), m_sleepTime(sleepTime), 
            m_stopMonitor(false), m_inited(false) {
                pthread_rwlock_init(&m_rwLock, NULL);
                m_lastUpdateTime = 0;
                if( sleepTime < 10 || sleepTime > 3600 ) {
                    m_sleepTime = 3600;
                }
            }
        /**
         * @brief 初始化函数，用来初始化词典文件以及监控线程
         * @ndf 初始化词典的函数指针
         * @return 如果初始化不成功，返回false 
         */
        bool init(NewDictFunc ndf, const std::string& logName = "HotSwitch" )
        {
            m_logName = logName;
            this->setNewDictFunc(ndf);
            m_pCurDict = newDictFunc(m_fileName);
            if( m_pCurDict == NULL ) {
                return false;
            }
            int err = pthread_create(&m_monitorThread, NULL, 
                    monitorThread<DictType>, (void*)this);
            if( err != 0 ) return false;
            m_inited = true;
            return true;
        }
        /**
         * @brief 销毁对象，等待监控线程结束，销毁dict指针和读写锁
         * todo 屏蔽继承
         */
        ~HotSwitchDict() {
            if( m_pCurDict != NULL )
            {
                m_stopMonitor = true;
                // 取消线程，避免出现sleep时间过长，导致join需要大量时间
                //pthread_cancel(m_monitorThread);
                // 等待监控线程结束
                if( m_inited )
                    pthread_join(m_monitorThread, NULL);
                // 销毁对象
                delete m_pCurDict;
            }
            m_pCurDict = NULL;
            if( m_pSwitchDict != NULL ) {
                delete m_pSwitchDict;
                m_pSwitchDict = NULL;
            }
            // 销毁锁
            pthread_rwlock_destroy(&m_rwLock);
        }
        /**
         * @brief 获取当前dict的指针
         */
        DictType * getCurDict() { return m_pCurDict; }
        /**
         * @brief 加读锁
         */
        int32_t rwlock_read_lock() { return pthread_rwlock_rdlock(&m_rwLock); }
        /**
         * @brief 加写锁
         */
        int32_t rwlock_write_lock() { return pthread_rwlock_wrlock(&m_rwLock); }
        /**
         * @brief 解开锁 
         */
        int32_t rwlock_unlock() { return pthread_rwlock_unlock(&m_rwLock); }
    private:
        HotSwitchDict() {}
        /**
         * @brief 根据词典文件创建新的dict指针
         */
        DictType * newDictFunc(const std::string & fileName) {
            return m_newDictFunc(fileName.c_str());
        }
        /**
         * @brief 判断新词典文件是否存在
         */
        bool isNewDictArrived() {
            // 读取finish文件，得到新词典对应的文件名
            if( access(m_flagFile.c_str(), F_OK|R_OK) < 0 ) {
                VLOG_APP(ERROR) << "flagFile not exist or not readable :" << m_flagFile;
                return false;
            }
            // 如果文件修改时间比m_lastUpdateTime大，则读取文件
            struct stat buf;
            if( stat(m_flagFile.c_str(), &buf) < 0 ) {
                return false;
            }
            time_t flagFileTime = buf.st_ctime;
            if( flagFileTime <= m_lastUpdateTime ) {
                VLOG(1) << "HotSwitch: Time is smaller than m_lastUpdateTime" << m_flagFile;
                return false;
            }
            std::ifstream in(m_flagFile.c_str());
            if( !in.is_open() ) {
                VLOG_APP(ERROR) << "| opened failed " <<  m_flagFile;
                return false;
            }
            std::string newfileName;
            in >> newfileName;
            in.close();
            // 判断新文件是否存在，使用绝对路径
            if( access(newfileName.c_str(), F_OK|R_OK) < 0 )
            {
                VLOG(1) << " not exist or not readable " << newfileName;
                return false;
            }
            m_newFileName = newfileName;
            m_lastUpdateTime = flagFileTime;
            VLOG(1) << "change to new dict." << m_newFileName;
            return true;
        }
        std::string getNewDictFileName()
        {
            return m_newFileName;
        }
        /**
         * @brief 当前词典的文件名修改
         */
        void switchFileName()
        {
            m_fileName = m_newFileName;
            m_newFileName = "";
        }
        /**
         * @brief 设置从一个文件初始化DictType*类型的函数
         */
        void setNewDictFunc(NewDictFunc ndf) {
            m_newDictFunc = ndf;
        }
    
    private:	
        DictType * m_pCurDict;
        DictType * m_pSwitchDict;
        std::string m_fileName;
        std::string m_newFileName;
        std::string m_flagFile;
        uint32_t m_sleepTime;
        pthread_rwlock_t m_rwLock;
        pthread_t m_monitorThread;
        bool m_stopMonitor;
        NewDictFunc m_newDictFunc;
        time_t m_lastUpdateTime;
        bool   m_inited;
        std::string m_logName;
};
#endif // _HOT_SWITCH_DICT_H_
