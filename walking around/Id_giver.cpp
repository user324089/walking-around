#include "Id_giver.hpp"

unsigned int Id_giver::get_id(const std::string &str) {
    auto found = str_to_id.find(str);
    if (found == str_to_id.end()) {
        return (str_to_id[str] = given_id++);
    }
    return found->second;
}