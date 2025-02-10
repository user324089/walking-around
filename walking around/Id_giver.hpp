#pragma once
#include "Windows_includes.hpp"
#include <map>
#include <string>

class Id_giver {
    private:
        unsigned int given_id = 0;
        std::map<std::string, unsigned int> str_to_id;

    public:
        unsigned int get_id(const std::string &str);

        constexpr static unsigned int no_id = (std::numeric_limits<unsigned int>::max)();
};