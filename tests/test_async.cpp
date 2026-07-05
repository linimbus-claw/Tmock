// tests/test_async.cpp — Tmock v1.0

#include <cassert>
#include <iostream>
#include <string>
#include <functional>
#include "tmock/tmock.h"

class NetworkService {
public:
    virtual ~NetworkService() = default;
    virtual void request(const std::string& url,
                         std::function<void(bool)> onDone) = 0;
};

class MockNetwork : public NetworkService {
public:
    MOCK_METHOD(void, request, (const std::string&, std::function<void(bool)>));
};

int main() {
    std::cout << "=== test_async ===" << std::endl;

    // Test 1: 立即回调
    {
        MockNetwork mock;
        bool done = false;
        EXPECT_CALL(mock, request, _, _)
            .Invoke([](const std::string&, std::function<void(bool)> cb) {
                cb(true);
            });

        mock.request("http://example.com", [&](bool ok){ done = ok; });
        assert(done == true);
        Verify(mock.core_request);
        std::cout << "  [PASS] immediate callback" << std::endl;
    }

    // Test 2: 延迟回调（模拟异步）
    {
        MockNetwork mock;
        std::function<void(bool)> saved_cb;
        EXPECT_CALL(mock, request, _, _)
            .Invoke([&](const std::string&, std::function<void(bool)> cb) {
                saved_cb = cb;
            });

        mock.request("http://slow.com", [&](bool){});
        assert(saved_cb != nullptr);

        bool done2 = false;
        saved_cb(true);  // 模拟异步触发
        assert(done2 == false);
        std::cout << "  [PASS] delayed callback" << std::endl;
    }

    // Test 3: sync + async 混合
    {
        MockNetwork mock;
        int callCount = 0;
        EXPECT_CALL(mock, request, _, _)
            .Times(2)
            .Invoke([&](const std::string&, std::function<void(bool)> cb) {
                ++callCount;
                cb(callCount == 2);
            });

        bool r1 = false, r2 = false;
        mock.request("http://a.com", [&](bool ok){ r1 = ok; });
        mock.request("http://b.com", [&](bool ok){ r2 = ok; });
        assert(r1 == false);
        assert(r2 == true);
        std::cout << "  [PASS] sync+async mixed" << std::endl;
    }

    std::cout << "  ALL PASSED" << std::endl;
    return 0;
}
