#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <set>
#include <boost/algorithm/string.hpp>


namespace utils {

inline std::vector<std::string> NormalizeTags(const std::string& input) {
    std::set<std::string> tags;
    std::stringstream ss(input);
    std::string tag;
    while (std::getline(ss, tag, ',')) {
        boost::algorithm::trim(tag);
        if (tag.empty()) {
            continue;
        }
        std::string result;
        bool space= false;
        for(char c : tag) {
            if (std::isspace(c)) {
                if (!space)  {
                    result += ' ';
                    space = true;
                }
            }
            else {
                result += c;
                space = false;
            }
        }
        if(!result.empty()) {
            tags.insert(result);
        }
    }
    return {tags.begin(), tags.end()};
}

inline std::string JoinTags(const std::vector<std::string>& tags, const std::string& separator = ", ") {
    std::string result;
    for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) {
            result += separator;
        }
        result += tags[i];
    }
    return result;
}

}
