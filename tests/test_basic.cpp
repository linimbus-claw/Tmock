// tests/test_basic.cpp

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
    MOCK_METHOD2(bool, login, const std::string&, int);
    MOCK_METHOD1(int, getScore, const std::string&);
};

int main() {
    std::cout << "=== test_basic ===" << std::endl;

    // Test 1: 基础返回值注入
    {
        MockUserService mock;
        mock.core_login.expect("alice", 1234).willReturn(true).times(1);

        bool result = mock.login("alice", 1234);
        assert(result == true);
        std::cout << "  [PASS] basic return injection" << std::endl;
    }

    // Test 2: 无匹配返回默认值
    {
        MockUserService mock;
        // 不设置任何预期
        bool r1 = mock.login("alice", 1234);  // 应返回 false
        int  r2 = mock.getScore("bob");       // 应返回 0
        assert(r1 == false);
        assert(r2 == 0);
        std::cout << "  [PASS] default return value" << std::endl;
    }

    // Test 3: 次数饱和
    {
        MockUserService mock;
        mock.core_login.expect("alice", 1234).willReturn(true).times(3);

        mock.login("alice", 1234);
        mock.login("alice", 1234);
        mock.login("alice", 1234);
        // 第4次：饱和，不执行 action
        bool r = mock.login("alice", 1234);
        assert(r == false);
        std::cout << "  [PASS] times saturation" << std::endl;
    }

    // Test 4: atLeast
    {
        MockUserService mock;
        auto& e = mock.core_login.addExpectation();
        e.withArgs("alice", 1234).willReturn(true).atLeast(2);

        mock.login("alice", 1234);
        mock.login("alice", 1234);
        mock.login("alice", 1234);  // 第3次仍匹配（atLeast）
        std::cout << "  [PASS] atLeast" << std::endl;
    }

    // Test 5: 自定义 matcher
    {
        MockUserService mock;
        auto& e = mock.core_getScore.addExpectation();
        e.with([](const std::string& u) { return u.find("vip_") == 0; })
         .willReturn(999)
         .times(1);

        int r1 = mock.getScore("vip_alice");  // 匹配
        int r2 = mock.getScore("normal_bob");  // 不匹配
        assert(r1 == 999);
        assert(r2 == 0);
        std::cout << "  [PASS] custom matcher" << std::endl;
    }

    // Test 6: 多预期按序匹配
    {
        MockUserService mock;
        mock.core_login.expect("alice", 1111).willReturn(false).times(1);
        mock.core_login.expect("alice", 2222).willReturn(true).times(1);

        assert(mock.login("alice", 1111) == false);
        assert(mock.login("alice", 2222) == true);
        std::cout << "  [PASS] multiple expectations order" << std::endl;
    }

    std::cout << "  ALL PASSED" << std::endl;
    return 0;
}
