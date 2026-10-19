#pragma once

#include <handlers/matchmakingService/getmatch/requests.hpp>
#include <handlers/matchmakingService/getmatch/responses.hpp>
#include <string>
#include <userver/chaotic/openapi/server/dependencies.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/request/request_context.hpp>

namespace handlers::matchmakingService::getmatch {

struct HandlerTag;

class View final {
 public:
  using Deps =
      USERVER_NAMESPACE::chaotic::openapi::server::dependencies::ForHandler<
          HandlerTag>;
  using RequestContext = USERVER_NAMESPACE::server::request::RequestContext;

  static Response Handle(Request&& request, Deps&& deps,
                         RequestContext& context);

  /* Uncomment, if you want to define a custom logging for request/response
  body.
   * E.g. you want to log several fields, but omit the others (secrets, etc.).
   *

  static std::string GetResponseForLogging(
      const Response& response,
      const std::string& serialized_response,
      RequestContext& context);
  */
};

}  // namespace handlers::matchmakingService::getmatch
