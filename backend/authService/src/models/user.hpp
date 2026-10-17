#pragma once

#include <string>

namespace shop {
    struct User {
        int id;
        std::string email;
        std::string username;
        std::string avatar_url;
    };

    struct PurchaseMethod {
        int id;
        int userId;
        std::string type;
        std::string details;
    };
}
