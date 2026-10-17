#pragma once

#include <string>
#include <string_view>

namespace authService {

enum class UserType { kFirstTime, kKnown };

std::string SayHelloTo(std::string_view name, UserType type);

}  // namespace authService