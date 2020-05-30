#ifndef __DICT_H__
#define __DICT_H__

#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <string>
#include <fstream>
#include <unordered_map>
#include <sstream>
#include <butil/logging.h>

#define MAP_DICT_SEP '\t'
#define SET_DICT_SEP ','
#define VEC_DICT_SEP ','
#define MAP_DICT_FIELD_COUNT 2
#define MAP_DICT_KEY_INDEX 0
#define MAP_DICT_VALUE_INDEX 1
#define REVERSE_MAP_DICT_KEY_INDEX 1
#define REVERSE_MAP_DICT_VALUE_INDEX 0

namespace StemCell {

class Dict {
public:
    typedef std::map<std::string, std::map<int64_t, double> > SIDMapDict;
    typedef std::map<int64_t,std::vector<int32_t> > LIVecMapDict;
    typedef std::map<std::string, int32_t> SIMapDict;
    typedef std::unordered_map<int64_t, int32_t> LIHashMapDict;
    //typedef std::map<int64_t, int64_t> LLMapDict;
    //typedef std::map<int64_t, int32_t> LIMapDict;
    //typedef std::unordered_map<int64_t, int64_t> LLHashMapDict;
    //typedef std::map<uint64_t, double> IDMapDict;
    //typedef std::map<std::string, double> SDMapDict;
    //typedef std::map<std::string, std::set<uint32_t> > SIMapSetDict;
    //typedef std::map<std::string, std::string> SSMapDict;
    //typedef std::map<std::string, std::vector<std::string> > SSVecMapDict;
    //typedef std::map<std::string, std::set<std::string> > SSMapSetDict;
    //typedef std::map<int64_t,std::vector<int64_t> > LLVecMapDict;
    //typedef std::map<std::string, std::set<int64_t> > USISetMapDict;
    //typedef std::map<int64_t, std::set<int64_t> > IISetMapDict;
    //typedef std::vector<std::pair<uint32_t,uint32_t> > IIVectorDict;
    
    static const std::string strip(std::string &source) {
        static const std::string trims = " \t\r\n";
        std::string::size_type pos1, pos2;

        pos1 = source.find_first_not_of(trims);
        pos2 = source.find_last_not_of(trims);

        if (pos1 != std::string::npos)
            return source.substr(pos1, pos2 - pos1 + 1);
        return "";
    }

    static void split(char delimiter, const std::string &source,
            std::vector<std::string> &result, int32_t reqSize = 0) {
        if (source.empty()) {
            return;
        }
        size_t prev_pos = 0, pos = 0;

        result.resize(0);
        pos = source.find(delimiter, pos);
        int32_t index  = 0;
        while (pos != std::string::npos)
        {
            result.push_back(source.substr(prev_pos, pos - prev_pos));
            if( reqSize != 0 && ++index >= reqSize ) {
                return;
            }
            prev_pos = ++ pos;
            pos = source.find(delimiter, pos);
        }
        result.push_back(source.substr(prev_pos));
    }

    static void split(const std::string &delimiters, const std::string &source, std::vector<std::string> &result) {
        if (source.empty()) {
            return;
        }
        size_t prev_pos = 0, pos = 0;

        result.resize(0);
        pos = source.find_first_of(delimiters, pos);
        while (pos != std::string::npos) {
            result.push_back(source.substr(prev_pos, pos - prev_pos));
            prev_pos = ++ pos;
            pos = source.find_first_of(delimiters, pos);
        }
        result.push_back(source.substr(prev_pos));
    }

// set类型的dict加载
template<class T>
inline std::set<T>* buildSetDict(const std::string& fileName) {
    std::ifstream fin(fileName.c_str());
    if(!fin.is_open()) {
        return nullptr;
    }
    std::set<T>* _s_set = new std::set<T>();
    std::string line;
    while (getline(fin, line)) {
        line = strip(line);
        if(line.empty()) {
            continue;
        }
        std::stringstream s_key;
        T key;
        s_key << line;
        s_key >> key;
        _s_set->insert(key);
    }
    return _s_set;
}

// HashMap类型的dict加载,  T -> line number 
template<class T>
static inline std::unordered_map<T, int32_t>* buildLineNoHashMapDict(const std::string& fileName) {
    std::ifstream fin(fileName.c_str());
    if(!fin.is_open()) {
        return nullptr;
    }
    std::unordered_map<T, int32_t>* _s_map = new std::unordered_map<T, int32_t>();
    std::string line;
    int index = 0;
    while (getline(fin, line)) {
        line = strip(line);
        if(line.empty()) {
            continue;
        }
        std::stringstream s_key;
        T key;
        s_key << line;
        s_key >> key;
        (*_s_map)[key] = index;
        ++index;
    }
    return _s_map;
}

//所有map类型的dict加载
template<class T1,class T2>
static inline std::map<T1,T2> *buildMapDict(const std::string &fileName) {
    std::ifstream fin(fileName.c_str());
    if( !fin.is_open() ) {
        return nullptr;
    }

    std::map<T1, T2> * _s_map = new std::map<T1,T2>();
    std::string line;
    while( getline(fin, line) ) {
        line = strip(line);
        if( line.empty() ) continue;
        std::vector<std::string> fields;
        split(MAP_DICT_SEP, line, fields);
        if( fields.size() < MAP_DICT_FIELD_COUNT ) {
            continue;
        }
        std::stringstream s_key;
        std::stringstream s_value;
        T1 key;
        T2 value;

        s_key<<fields[MAP_DICT_KEY_INDEX];
        s_value<<fields[MAP_DICT_VALUE_INDEX];

        s_key>>key;
        s_value>>value;

        (*_s_map)[key] = value;
    }
    return _s_map;
}

//所有map类型的dict加载，buildMapDict不支持value含有空格的情况
template<class T1,class T2>
inline std::map<T1,T2> *buildMapDictSpaceSupport(const std::string &fileName) {
    std::ifstream fin(fileName.c_str());
    if( !fin.is_open() )
    {
        return nullptr;
    }

    std::map<T1, T2> * _s_map = new std::map<T1,T2>();
    std::string line;
    while( getline(fin, line) )
    {
        line = strip(line);
        if( line.empty() ) continue;
        std::vector<std::string> fields;
        split(MAP_DICT_SEP, line, fields);
        if( fields.size() < MAP_DICT_FIELD_COUNT )
        {
            continue;
        }
        std::stringstream s_key;
        std::stringstream s_value;
        T1 key;
        T2 value;

        s_key<<fields[MAP_DICT_KEY_INDEX];
        s_value<<fields[MAP_DICT_VALUE_INDEX];

        std::getline(s_key, key);
        std::getline(s_value, value);

        (*_s_map)[key] = value;
    }
    //MV_DEBUG((L_INFO, "===print m_ctripGoodsDict==="));
    //printMapDict(_s_map);
    return _s_map;
}

//二维关联map类型的dict加载
template<class T1, class T2, class T3>
static std::map<T1, std::map<T2, T3> > *buildMapDict(const std::string &fileName) {
    std::ifstream fin(fileName.c_str());
    if( !fin.is_open() ) {
        return nullptr;
    }
    std::map<T1, std::map<T2, T3> > * _s_map = new std::map<T1, std::map<T2, T3> >();
    std::string line;
    while( getline(fin, line) ) {
        line = strip(line);
        if ( line.empty() ) {
            continue;
        }
        std::vector<std::string> fields;
        split(MAP_DICT_SEP, line, fields);
        if( fields.size() < 3 ) {
            continue;
        }
        std::stringstream s_key1;
        std::stringstream s_key2;
        std::stringstream s_value;
        T1 key1;
        T2 key2;
        T3 value;

        s_key1 << fields[0];
        s_key2 << fields[1];
        s_value << fields[2];
        
        s_key1 >> key1;
        s_key2 >> key2;
        s_value >> value;
        
        (*_s_map)[key1][key2] = value;
    }
    LOG(INFO) << "new_dict_ size:" << _s_map->size(); 
    /*
    auto& dict = *_s_map;
    for (auto& v : dict) {
        for (auto& m : v.second) {
            LOG(INFO) << v.first << "\t" << m.first << "\t" << m.second;
        }

    }
    */
    return _s_map;
}

template<class T1,class T2>
inline std::map<T1,std::set<T2> > *buildMapSetDict(const std::string &fileName) {
    std::ifstream fin(fileName.c_str());
    if( !fin.is_open() )
    {
        return nullptr;
    }

    std::map<T1, std::set<T2> > * _s_map = new std::map<T1,std::set<T2> >();
    std::string line;
    while( getline(fin, line) )
    {
        line = strip(line);
        if( line.empty() ) continue;
        std::vector<std::string> fields;
        split(MAP_DICT_SEP, line, fields);
        if( fields.size() < MAP_DICT_FIELD_COUNT )
        {
            continue;
        }


        std::stringstream s_key;
        std::stringstream s_value;
        T1 key;
        T2 value;

        s_key<<fields[MAP_DICT_KEY_INDEX];
        s_value<<fields[MAP_DICT_VALUE_INDEX];

        s_key>>key;
        s_value>>value;

        std::set<T2> valuelist;
        if(_s_map->find(key) != _s_map->end())
        {
            valuelist = (*_s_map)[key];
        }

        valuelist.insert(value);

        (*_s_map)[key] = valuelist;
    }
    LOG(INFO) << "new_dict_ size:" << _s_map->size(); 
    return _s_map;
}

template<class T1,class T2>
static inline std::vector<std::pair<T1,T2> > *buildVectorDict(const std::string &fileName) {
    std::ifstream fin(fileName.c_str());
    if( !fin.is_open() )
    {
        return nullptr;
    }

    std::vector<std::pair<T1,T2> > *s_vec = new std::vector<std::pair<T1,T2> >();
    std::string line;
    while( getline(fin, line) )
    {
        line = strip(line);
        if( line.empty() ) continue;
        std::vector<std::string> fields;
        split(MAP_DICT_SEP, line, fields);
        if( fields.size() < MAP_DICT_FIELD_COUNT )
        {
            continue;
        }


        std::stringstream s_key;
        std::stringstream s_value;
        T1 key;
        T2 value;

        s_key<<fields[MAP_DICT_KEY_INDEX];
        s_value<<fields[MAP_DICT_VALUE_INDEX];

        s_key>>key;
        s_value>>value;
        s_vec->push_back(make_pair(key,value));
    }
    return s_vec;
}

/*
 *以字典文件的第二列为key，第一列为value，建立key ==> value词典
 *字典格式为 value1	key1
 *           value2	key2
 *           value3	key1
 *注意，这个函数对于多对多的映射建立词典有局限性
 *因为key1对应的value1会被value3覆盖
 */
template<class T1,class T2>
static inline std::map<T1,T2> *buildReverseMapDict(const std::string &fileName) {
    std::ifstream fin(fileName.c_str());
    if( !fin.is_open() ) 
    {
        return nullptr;
    }

    std::map<T1, T2> * _s_map = new std::map<T1,T2>();
    std::string line;
    while( getline(fin, line) ) 
    {
        line = strip(line);
        if( line.empty() ) continue;
        std::vector<std::string> fields;
        split(MAP_DICT_SEP, line, fields);
        if( fields.size() < MAP_DICT_FIELD_COUNT ) 
        {
            continue;
        }


        std::stringstream s_key;
        std::stringstream s_value;
        T1 key;
        T2 value;

        s_key<<fields[REVERSE_MAP_DICT_KEY_INDEX];
        s_value<<fields[REVERSE_MAP_DICT_VALUE_INDEX];

        s_key>>key;
        s_value>>value;

        (*_s_map)[key] = value;
    }
    return _s_map;

}

template<class T1,class T2>
static inline std::map<T1,std::vector<T2> > *buildVectorValueTypeMapDict(const std::string &fileName) {
    std::ifstream fin(fileName.c_str());
    if( !fin.is_open() )
    {
        return nullptr;
    }

    std::map<T1, std::vector<T2> > * _s_map = new std::map<T1,std::vector<T2> >();
    std::string line;
    while( getline(fin, line) )
    {
        line = strip(line);
        if( line.empty() ) continue;
        std::vector<std::string> fields;
        split(MAP_DICT_SEP, line, fields);
        if( fields.size() < MAP_DICT_FIELD_COUNT )
        {
            continue;
        }


        std::stringstream s_key;
        T1 key;
        s_key << fields[MAP_DICT_KEY_INDEX];
        s_key >> key;

        std::string valueListWithComma = fields[MAP_DICT_VALUE_INDEX];
        std::stringstream s_value(valueListWithComma);

        std::string::iterator itr = valueListWithComma.begin();
        T2 tmp;
        std::vector<T2> valueVec;
        while (s_value >> tmp)
        {
            valueVec.push_back(tmp);
            if (s_value.peek() == VEC_DICT_SEP)
                s_value.ignore();
        }

        (*_s_map)[key] = valueVec;
    }
    LOG(INFO) << "new_dict_ size:" << _s_map->size(); 
    return _s_map;
}
}; // class Dict
} // end namespace StemCell
#endif
