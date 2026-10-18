#pragma once

#include <drogon/HttpClient.h>
#include <drogon/HttpController.h>

using namespace drogon;

namespace apiGateway
{
class ReplayController : public drogon::HttpController<ReplayController, false>
{
    HttpClientPtr client_ = nullptr;
public:
    explicit ReplayController();
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ReplayController::ListReplays, "/api/replays", drogon::Get);
        ADD_METHOD_TO(ReplayController::GetReplay, "/api/replays/{replayId}", drogon::Get);
        ADD_METHOD_TO(ReplayController::GetReplayFrames, "/api/replays/{replayId}/frames", drogon::Get);
    METHOD_LIST_END
private:
    void ListReplays(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void GetReplay(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string replayId);
    void GetReplayFrames(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string replayId);

    void Forward(const HttpRequestPtr& downstream, const std::function<void (const HttpResponsePtr &)>& callback);
    static void Unreachable(const std::function<void (const HttpResponsePtr &)>& callback);
};
}
