// tests/test_basic.cpp — Tmock v1.0

#include <cassert>
#include <iostream>
#include <string>
#include "tmock/tmock.h"

class UserService {
public:
    virtual ~UserService() = default;
    virtual bool login(const std::string& user, int pwd) = 0;
    virtual int getScore(const std::string& user) = 0;
};

class MockUserService : public UserService {
public:
    MOCK_METHOD(bool, login, (const std::string&, int));
    MOCK_METHOD(int, getScore, (const std::string&));
};

int main() {
    std::cout << "=== test_basic ===" << std::endl;

    // Test 1: 基础返回值注入
    {
        MockUserService mock;
        EXPECT_CALL(mock, login, bool, (const std::string&, int), "alice", _).Return(true);

        bool result = mock.login("alice", 1234);
        assert(result == true);
        Verify(mock.core_login);
        std::cout << "  [PASS] basic return injection" << std::endl;
    }

    // Test 2: 无匹配返回默认值
    {
        MockUserService mock;
        bool r1 = mock.login("alice", 1234);  // false
        int  r2 = mock.getScore("bob");       // 0
        assert(r1 == false);
        assert(r2 == 0);
        std::cout << "  [PASS] default return value" << std::endl;
    }

    // Test 3: 次数饱和
    {
        MockUserService mock;
        EXPECT_CALL(mock, login, bool, (const std::string&, int), "alice", _)
            .Return(true).Times(3);

        mock.login("alice", 1234);
        mock.login("alice", 1234);
        mock.login("alice", 1234);
        bool r = mock.login("alice", 1234);  // 饱和，返回 false
        assert(r == false);
        std::cout << "  [PASS] times saturation" << std::endl;
    }

    // Test 4: AtLeast
    {
        MockUserService mock;
        EXPECT_CALL(mock, login, bool, (const std::string&, int), "alice", _)
            .Return(true).AtLeast(2);

        mock.login("alice", 1234);
        mock.login("alice", 1234);
        mock.login("alice", 1234);
        std::cout << "  [PASS] AtLeast" << std::endl;
    }

    // Test 5: .with() 自定义 matcher
    {
        MockUserService mock;
        EXPECT_CALL(mock, getScore, int, (const std::string&), An<std::string>())
            .with([](const std::string& u){ return u.find("vip_") == 0; })
            .Return(999).Times(1);

        int r1 = mock.getScore("vip_alice");
        int r2 = mock.getScore("normal_bob");
        assert(r1 == 999);
        assert(r2 == 0);
        std::cout << "  [PASS] custom matcher with .with()" << std::endl;
    }

    // Test 6: 多预期按序匹配
    {
        MockUserService mock;
        EXPECT_CALL(mock, login, bool, (const std::string&, int), "alice", Eq(1111))
            .Return(false);
        EXPECT_CALL(mock, login, bool, (const std::string&, int), "alice", Eq(2222))
            .Return(true);

        assert(mock.login("alice", 1111) == false);
        assert(mock.login("alice", 2222) == true);
        std::cout << "  [PASS] multiple expectations order" << std::endl;
    }

    std::cout << "  ALL PASSED" << std::endl;
    return 0;
}
