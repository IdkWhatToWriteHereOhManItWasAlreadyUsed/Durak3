#include <greeting.hpp>
#include <userver/utest/utest.hpp>

using matchmakingService::UserType;

UTEST(SayHelloTo, Basic) {
    EXPECT_EQ(matchmakingService::SayHelloTo("Developer", UserType::kFirstTime), "Hello, Developer!\n");
    EXPECT_EQ(matchmakingService::SayHelloTo({}, UserType::kFirstTime), "Hello, unknown user!\n");

    EXPECT_EQ(matchmakingService::SayHelloTo("Developer", UserType::kKnown), "Hi again, Developer!\n");
    EXPECT_EQ(matchmakingService::SayHelloTo({}, UserType::kKnown), "Hi again, unknown user!\n");
}