#include <drogon/HttpAppFramework.h>

#include <api/v1.0/controllers/realtimeGateway_RealtimeController.h>
#include <middleware/WSAuthFilter.hpp>

#include <memory>

int main() {
    drogon::app().loadConfigFile("config/app/config.json");

    drogon::app().registerFilter(std::make_shared<realtimeGateway::WSAuthFilter>());
    drogon::app().registerController(std::make_shared<realtimeGateway::RealtimeController>());

    drogon::app().run();
    return 0;
}
