// tests/test_all.cpp - Tmock v1.0 comprehensive test
// EXPECT_CALL(mock, Name, matchers...).Return(val).Times(n)

#include <cassert>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <functional>
#include <any>
#include "tmock/tmock.h"

using namespace std;

class IDatabase {
public:
    virtual ~IDatabase() = default;
    virtual bool login(const string& user, int pwd) = 0;
    virtual int query(const string& sql) = 0;
    virtual void execute(const string& sql, function<void(bool)> cb) = 0;
    virtual string fetch(const string& table, int id) = 0;
};

class MockDatabase : public IDatabase {
public:
    MOCK_METHOD(bool,   login,  (const string&, int));
    MOCK_METHOD(int,    query,  (const string&));
    MOCK_METHOD(void,   execute,(const string&, function<void(bool)>));
    MOCK_METHOD(string, fetch,  (const string&, int));
};

int main() {
    cout << "=== Tmock v1.0 test ===" << endl;

    // [1] wildcard + Eq
    cout << "  [1] wildcard _ + Eq matcher..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, login, _, Eq(1234)).Return(true);
        assert(m.login("alice", 1234));
        Verify(m.core_login);
        cout << "  [PASS]" << endl;
    }

    // [2] An<T>()
    cout << "  [2] An<int>()..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, login, "bob", An<int>()).Return(false);
        assert(!m.login("bob", 9999));
        Verify(m.core_login);
        cout << "  [PASS]" << endl;
    }

    // [3] Matcher<string>(lambda)
    cout << "  [3] Matcher<string>(lambda)..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, query,
            Matcher<string>([](const string& s){ return s.find("SELECT") == 0; }))
            .Return(42);
        assert(m.query("SELECT * FROM users") == 42);
        Verify(m.core_query);
        cout << "  [PASS]" << endl;
    }

    // [4] HasPrefix
    cout << "  [4] HasPrefix..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, query, HasPrefix("INSERT")).Return(1);
        assert(m.query("INSERT INTO t VALUES(1)") == 1);
        Verify(m.core_query);
        cout << "  [PASS]" << endl;
    }

    // [5] string literal vs const string& (自动转)
    cout << "  [5] string literal vs const string&..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, query, "SELECT 1").Return(99);
        assert(m.query("SELECT 1") == 99);
        Verify(m.core_query);
        cout << "  [PASS]" << endl;
    }

    // [6] multiple expectations
    cout << "  [6] multiple expectations..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, login, "alice", 1).Return(false);
        EXPECT_CALL(m, login, "alice", 2).Return(true);
        assert(!m.login("alice", 1));
        assert(m.login("alice", 2));
        Verify(m.core_login);
        cout << "  [PASS]" << endl;
    }

    // [7] Times(n)
    cout << "  [7] Times(n)..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, login, "carol", 8888).Times(3).Return(true);
        m.login("carol", 8888);
        m.login("carol", 8888);
        m.login("carol", 8888);
        Verify(m.core_login);
        cout << "  [PASS]" << endl;
    }

    // [8] AtLeast + Return
    cout << "  [8] AtLeast + Return..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, query, "ping").AtLeast(2).Return(1);
        m.query("ping");
        m.query("ping");
        m.query("ping");
        Verify(m.core_query);
        cout << "  [PASS]" << endl;
    }

    // [9] Times(lo,hi) + chain
    cout << "  [9] Times(lo,hi)..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, query, "check").Times(1,3).Return(7);
        m.query("check");
        Verify(m.core_query);
        cout << "  [PASS]" << endl;
    }

    // [10] .with() custom matcher
    cout << "  [10] .with() custom matcher..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, query, "xyz")
            .with([](const string& s){ return s == "xyz"; })
            .Return(88);
        assert(m.query("xyz") == 88);
        Verify(m.core_query);
        cout << "  [PASS]" << endl;
    }

    // [11] Return().Times() chain
    cout << "  [11] Return().Times() chain..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, login, "dave", 111)
            .Return(true).Times(2);
        assert(m.login("dave", 111));
        assert(m.login("dave", 111));
        Verify(m.core_login);
        cout << "  [PASS]" << endl;
    }

    // [12] default return
    cout << "  [12] default return..." << endl;
    {
        MockDatabase m;
        assert(m.login("stranger", 0) == false);
        assert(m.query("?") == 0);
        assert(m.fetch("t", 0) == "");
        cout << "  [PASS]" << endl;
    }

    // [13] async callback
    cout << "  [13] async callback..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, execute, _, _)
            .Invoke([](const string&, function<void(bool)> cb){ cb(true); });
        bool ok = false;
        m.execute("DROP t", [&](bool r){ ok = r; });
        assert(ok);
        Verify(m.core_execute);
        cout << "  [PASS]" << endl;
    }

    // [14] delayed async callback
    cout << "  [14] async delayed callback..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, execute, "slow", _)
            .Invoke([](const string&, function<void(bool)> cb){
                thread([cb](){
                    this_thread::sleep_for(chrono::milliseconds(50));
                    cb(true);
                }).detach();
            });
        bool ok = false;
        m.execute("slow", [&](bool r){ ok = r; });
        this_thread::sleep_for(chrono::milliseconds(200));
        assert(ok);
        Verify(m.core_execute);
        cout << "  [PASS]" << endl;
    }

    // [15] Not matcher
    cout << "  [15] Not matcher..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, login, Not(Eq(std::string("hacker"))), _).Return(true);
        assert(m.login("admin", 0));
        Verify(m.core_login);
        cout << "  [PASS]" << endl;
    }

    // [16] fetch string return
    cout << "  [16] fetch string return..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, fetch, "users", 42).Return(string("alice"));
        assert(m.fetch("users", 42) == "alice");
        Verify(m.core_fetch);
        cout << "  [PASS]" << endl;
    }

    // [17] sequence
    cout << "  [17] call order..." << endl;
    {
        auto& seq = TmockSequence::global();
        { lock_guard<mutex> lock(seq.mu); seq.calls.clear(); }
        seq.enabled = true;
        MockDatabase m;
        EXPECT_CALL(m, query, "BEGIN").Invoke(
            [](const string&) -> int { tmock_record_call("BEGIN"); return 0; });
        EXPECT_CALL(m, query, "COMMIT").Invoke(
            [](const string&) -> int { tmock_record_call("COMMIT"); return 0; });
        m.query("BEGIN");
        m.query("COMMIT");
        assert(tmock_verify_order("BEGIN", "COMMIT"));
        seq.enabled = false;
        Verify(m.core_query);
        cout << "  [PASS]" << endl;
    }

    // [18] std::any matcher
    cout << "  [18] std::any matcher..." << endl;
    {
        MockDatabase m;
        any val = string("alice");
        EXPECT_CALL(m, login, val, _).Return(true);
        assert(m.login("alice", 9999));
        Verify(m.core_login);

        any val2 = 42;
        EXPECT_CALL(m, fetch, string("users"), val2).Return(string("bob"));
        assert(m.fetch("users", 42) == "bob");
        Verify(m.core_fetch);
        cout << "  [PASS]" << endl;
    }

    // [19] zero matchers = all wildcards
    cout << "  [19] zero matchers (all wildcards)..." << endl;
    {
        MockDatabase m;
        EXPECT_CALL(m, query).Return(100).Times(2);
        assert(m.query("anything") == 100);
        assert(m.query("whatever") == 100);
        Verify(m.core_query);

        EXPECT_CALL(m, login).Return(true).Times(2);
        assert(m.login("any", 0));
        assert(m.login("body", 999));
        Verify(m.core_login);
        cout << "  [PASS]" << endl;
    }

    cout << "\n  ALL 19 TESTS PASSED!" << endl;
    return 0;
}
