#pragma once

#include "Windows_includes.hpp"
#include <source_location>

void check_output(HRESULT res, std::source_location loc = std::source_location::current());