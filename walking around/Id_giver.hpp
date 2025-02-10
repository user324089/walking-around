#pragma once
#include "Windows_includes.hpp"
#include <map>
#include <string>

#include <sstream>

class Id_giver {
    private:
        unsigned int given_id = 0;
        std::map<std::string, unsigned int> str_to_id;

    public:
        unsigned int get_id(const std::string &str);

        constexpr static unsigned int no_id = (std::numeric_limits<unsigned int>::max)();

        void write() {
            std::stringstream s;
            s << "--------IDs:\n";
            for (auto [a, b] : str_to_id) {
                s << a << " " << b << "\n";
            }
            OutputDebugStringA(s.str().c_str());
        }
};