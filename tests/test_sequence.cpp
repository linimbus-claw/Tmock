// tests/test_sequence.cpp — Tmock v1.0

#include <cassert>
#include <iostream>
#include <string>
#include <functional>
#include "tmock/tmock.h"

class DataService {
public:
    virtual ~DataService() = default;
    virtual void send(const std::string& data) = 0;
    virtual bool ack(int id) = 0;
};

class MockDataService : public DataService {
public:
    MOCK_METHOD(void, send, (const std::string&));
    MOCK_METHOD(bool, ack, (int));
};

int main() {
    std::cout << "=== test_sequence ===" << std::endl;

    // Test 1: 基础顺序记录
    {
        auto& seq = TmockSequence::global();
        {
            std::lock_guard<std::mutex> lock(seq.mu);
            seq.calls.clear();
        }
        seq.enabled = true;

        MockDataService mock;
        EXPECT_CALL(mock, send, _)
            .Invoke([](const std::string& d) {
                tmock_record_call("send:" + d);
            });
        EXPECT_CALL(mock, ack, _)
            .Invoke([](int id) {
                tmock_record_call("ack:" + std::to_string(id));
                return true;
            });

        mock.send("hello");
        mock.ack(1);

        bool ok = tmock_verify_order("send:hello", "ack:1");
        assert(ok);
        seq.enabled = false;
        std::cout << "  [PASS] basic sequence order" << std::endl;
    }

    // Test 2: 顺序错误应检测
    {
        auto& seq = TmockSequence::global();
        {
            std::lock_guard<std::mutex> lock(seq.mu);
            seq.calls.clear();
        }
        seq.enabled = true;

        MockDataService mock;
        EXPECT_CALL(mock, send, _)
            .Invoke([](const std::string& d) {
                tmock_record_call("send:" + d);
            });
        EXPECT_CALL(mock, ack, _)
            .Invoke([](int id) {
                tmock_record_call("ack:" + std::to_string(id));
                return true;
            });

        // 故意逆序调用
        mock.ack(1);
        mock.send("hello");

        bool ok = tmock_verify_order("send:hello", "ack:1");
        assert(!ok);  // 应失败
        seq.enabled = false;
        std::cout << "  [PASS] wrong order detected" << std::endl;
    }

    std::cout << "  ALL PASSED" << std::endl;
    return 0;
}
