// tests/test_async.cpp

#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "tmock/tmock.h"

class ProfileService {
public:
    virtual ~ProfileService() = default;
    virtual void getProfile(const std::string& user,
                            std::function<void(const std::string&, int)> cb) = 0;
    virtual bool isOnline(const std::string& user) = 0;
};

class MockProfileService : public ProfileService {
public:
    MOCK_METHOD_VOID2(getProfile,
                         const std::string&,
                         std::function<void(const std::string&, int)>);
    MOCK_METHOD1(bool, isOnline, const std::string&);
};

int main() {
    std::cout << "=== test_async ===" << std::endl;

    // Test 1: 异步立即回调
    {
        MockProfileService mock;
        auto& e = mock.core_getProfile.addExpectation();
        e.withArgs("alice",
                    std::function<void(const std::string&, int)>{})
         .willInvoke([=](const std::string& user,
                         std::function<void(const std::string&, int)> cb) {
             cb("Alice", 100);
         })
         .times(1);

        bool called = false;
        std::string gotName;
        int gotScore = 0;

        mock.getProfile("alice", [&](const std::string& n, int s) {
            called = true;
            gotName = n;
            gotScore = s;
        });

        assert(called);
        assert(gotName == "Alice");
        assert(gotScore == 100);
        std::cout << "  [PASS] async immediate callback" << std::endl;
    }

    // Test 2: 异步延迟回调
    {
        MockProfileService mock;
        auto& e = mock.core_getProfile.addExpectation();
        e.withArgs("bob",
                    std::function<void(const std::string&, int)>{})
         .willInvoke([=](const std::string& user,
                         std::function<void(const std::string&, int)> cb) {
             // 延迟 50ms 后回调
             std::thread([cb]() {
                 std::this_thread::sleep_for(std::chrono::milliseconds(50));
                 cb("Bob", 200);
             }).detach();
         })
         .times(1);

        bool called = false;
        std::string gotName;
        int gotScore = 0;

        mock.getProfile("bob", [&](const std::string& n, int s) {
            called = true;
            gotName = n;
            gotScore = s;
        });

        // 等待延迟回调
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        assert(called);
        assert(gotName == "Bob");
        assert(gotScore == 200);
        std::cout << "  [PASS] async delayed callback (50ms)" << std::endl;
    }

    // Test 3: 同步 + 异步混合
    {
        MockProfileService mock;
        mock.core_isOnline.expect("alice").willReturn(true).times(1);

        auto& e = mock.core_getProfile.addExpectation();
        e.withArgs("alice",
                    std::function<void(const std::string&, int)>{})
         .willInvoke([](const std::string&, auto cb) { cb("Alice", 100); })
         .times(1);

        assert(mock.isOnline("alice") == true);

        bool called = false;
        mock.getProfile("alice", [&](const std::string& n, int) {
            called = true;
            assert(n == "Alice");
        });
        assert(called);
        std::cout << "  [PASS] sync + async mixed" << std::endl;
    }

    std::cout << "  ALL PASSED" << std::endl;
    return 0;
}
