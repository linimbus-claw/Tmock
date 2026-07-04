// tests/test_sequence.cpp

#include <cassert>
#include <iostream>
#include <string>
#include "tmock/tmock.h"

class DataService {
public:
    virtual ~DataService() = default;
    virtual void send(const std::string& data) = 0;
    virtual void recv(std::function<void(const std::string&)>) = 0;
    virtual bool ack(int id) = 0;
};

class MockDataService : public DataService {
public:
    MOCK_METHOD_VOID1(send, const std::string&);
    MOCK_METHOD_VOID1(recv, std::function<void(const std::string&)>);
    MOCK_METHOD1(bool, ack, int);
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
        mock.core_send.addExpectation()
            .willInvoke([=](const std::string& d) {
                tmock_record_call("send:" + d);
            });
        mock.core_ack.addExpectation()
            .willInvoke([=](int id) {
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
        mock.core_send.addExpectation()
            .willInvoke([=](const std::string& d) {
                tmock_record_call("send:" + d);
            });
        mock.core_ack.addExpectation()
            .willInvoke([=](int id) {
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
