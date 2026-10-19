//
// Created by dmitry on 17.10.2026.
//

#include <api/v1.0/controllers/apiGateway_AuthController.h>
#include <api/v1.0/controllers/apiGateway_GameController.h>
#include <api/v1.0/controllers/apiGateway_LobbyController.h>
#include <api/v1.0/controllers/apiGateway_MatchmakingController.h>
#include <api/v1.0/controllers/apiGateway_ReplayController.h>
#include <drogon/drogon.h>

#include <memory>
#include <middleware/AuthCheckMiddleware.hpp>

int main() {
    drogon::app().loadConfigFile("config/app/config.json");

    drogon::app().registerController(std::make_shared<apiGateway::AuthController>());
    drogon::app().registerController(std::make_shared<apiGateway::LobbyController>());
    drogon::app().registerController(std::make_shared<apiGateway::MatchmakingController>());
    drogon::app().registerController(std::make_shared<apiGateway::GameController>());
    drogon::app().registerController(std::make_shared<apiGateway::ReplayController>());

    drogon::app().run();
    return 0;
}
