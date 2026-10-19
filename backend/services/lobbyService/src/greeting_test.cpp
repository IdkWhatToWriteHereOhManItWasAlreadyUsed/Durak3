#include <greeting.hpp>
#include <userver/utest/utest.hpp>

using lobbyService::UserType;

UTEST(SayHelloTo, Basic) {
    EXPECT_EQ(lobbyService::SayHelloTo("Developer", UserType::kFirstTime), "Hello, Developer!\n");
    EXPECT_EQ(lobbyService::SayHelloTo({}, UserType::kFirstTime), "Hello, unknown user!\n");

    EXPECT_EQ(lobbyService::SayHelloTo("Developer", UserType::kKnown), "Hi again, Developer!\n");
    EXPECT_EQ(lobbyService::SayHelloTo({}, UserType::kKnown), "Hi again, unknown user!\n");
}