#include "Utility.hpp"
#include <sstream>

void check_output(HRESULT res, std::source_location loc) {
    if (res != S_OK) {
        std::stringstream stream;
        stream << "EXCEPTION in function " << loc.function_name() << " on line " << loc.line();
        throw std::runtime_error(stream.str());
    }
}
