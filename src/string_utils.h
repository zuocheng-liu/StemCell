#ifndef STRING_UTILS_H
#define STRING_UTILS_H

namespace StemCell {
class StringUtils {
public:
    /*
     *  函数说明：对字符串中所有指定的子串进行替换
     *   参数：
     *   string resource_str           //源字符串
     *   string sub_str                //被替换子串
     *   string new_str                //替换子串
     *   返回值: string
     */
    static inline std::string subreplace(std::string& resource_str,
            const std::string sub_str, const std::string new_str) {
        std::string::size_type pos = 0;
        while((pos = resource_str.find(sub_str)) != std::string::npos) {
            resource_str.replace(pos, sub_str.length(), new_str);
        }
        return resource_str;
    }
};

} // end namespace StemCell
#endif
