#ifndef PROFILER_H
#define PROFILER_H

#include <string>
#include <chrono>
#include <sstream>
#include <unordered_map>
#include <exception>

namespace StemCell {

class ProfilerItem {
public:
    enum ProfilerItemType {
        INFO_ITEM,
        TIME_ITEM,
        COUNT_ITEM,
    };
    
    ProfilerItem() {}

    ProfilerItem(const std::string& name,const std::string& message) :
        name(name), message(message), type(INFO_ITEM), 
        start_time(0), end_time(0), latency(0), count(0) {}

    ProfilerItem(const std::string& name) :
        name(name), message(""), type(TIME_ITEM), 
        start_time(0), end_time(0), latency(0), count(0) {}
    
    void start() {
        std::chrono::steady_clock::duration now = std::chrono::steady_clock::now().time_since_epoch();
        std::chrono::microseconds m = std::chrono::duration_cast<std::chrono::microseconds>(now);
        start_time = m.count();
        VLOG(1) << name << "|start_time:" << start_time;
    }
    
    void stop() {
        std::chrono::steady_clock::duration now = std::chrono::steady_clock::now().time_since_epoch();
        std::chrono::microseconds m = std::chrono::duration_cast<std::chrono::microseconds>(now);
        end_time = m.count();
        latency = end_time - start_time;
        VLOG(1)<< name << "|start_time:" << start_time  
            << "|end_time:" << end_time 
            << "|latency:" << latency;
    }
    
    void increase(int64_t inc) {
        type = COUNT_ITEM;
        count += inc;
    }

    void cancel() {
        latency = 0;
        message = "-";
        count = 0;
        type = INFO_ITEM;
    }
    
    enum ProfilerItemType getType() { return type; }
    std::string& getName() { return name; }
    std::string& getMessage() { return message; }
    int64_t getCount() { return count; }
    uint64_t getStartTime() { return start_time; }
    uint64_t getEndTime() { return end_time; }
    uint64_t getLatency() { return latency; }

private:
    std::string name;
    std::string message;
    enum ProfilerItemType type;
    int64_t start_time;
    int64_t end_time;
    int64_t latency; 
    int64_t count;
};

class Profiler {

public :
    void setInfoItem(const int64_t id, const std::string& name,const std::string& message) {
        ProfilerItem item(name, message);
        _info.insert(std::make_pair(id, item));
    }

    bool isItemExisted(const int64_t id) {
        return (_info.find(id) != _info.end());
    }

    ProfilerItem& getItem(const int64_t id) {
        auto it = _info.find(id);
        if (it == _info.end()) { 
            throw std::exception();
        }
        return it->second;
    }
        
    ProfilerItem& createTimeItem(const int64_t id, const std::string& name) {
        ProfilerItem item(name);
        auto it = _info.insert(std::make_pair(id, item));
        return it.first->second;
        // return _info[name];
    }

    void exportToString(std::string& output, char seg = '|', char con = '=') {
        std::stringstream ss;
        if (_info.size() <= 0) {
            return;
        }
        for (auto it = _info.begin(); it != _info.end(); ++it) {
            ProfilerItem& item = it->second;
            switch (item.getType()) {
                case ProfilerItem::INFO_ITEM :
                    if (item.getMessage().empty()) {
                        ss << seg << item.getName() << con << "-";
                        break;
                    }
                    ss << seg << item.getName() << con << item.getMessage();
                    break;
                case ProfilerItem::TIME_ITEM :
                    if (item.getEndTime() <= 0) {
                        item.stop();
                    }
                    ss << seg << item.getName() << con << (item.getLatency() / 1000);
                    break;
                case ProfilerItem::COUNT_ITEM :
                    ss << seg << item.getName() << con << (item.getCount());
                    break;
            }
        }   
        output = ss.str();
    }

    void reset() {
        _info.clear();
    }

private :
    std::map<int64_t, ProfilerItem> _info;
};

} // end namespace StemCell
#endif
