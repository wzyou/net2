#include <string>

#include "netp2/core/version.h"

int main() {
    return std::string(netp2::core::kVersion).empty() ? 1 : 0;
}
