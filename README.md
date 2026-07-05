# Tmock

Header-only C++17 mock framework. 支持继承式打桩、std::function 异步接口、多接口调用顺序校验。

## 设计原则

- **header-only**: 一行 `#include "tmock/tmock.h"` 即可使用
- **无需代码生成**: 纯模板 + 宏，编译时完成所有工作
- **线程安全**: 所有内部状态受 `std::mutex` 保护

## 快速开始

```cpp
#include "tmock/tmock.h"

// 1. 定义接口（纯虚类）
class Database {
public:
    virtual ~Database() = default;
    virtual bool login(const std::string& user, int pwd) = 0;
    virtual int query(const std::string& sql) = 0;
    virtual void asyncFetch(const std::string& key,
                            std::function<void(const std::string&)> cb) = 0;
};

// 2. 用 MOCK_METHOD 声明 mock 类
class MockDatabase : public Database {
public:
    MOCK_METHOD(bool, login, (const std::string&, int));
    MOCK_METHOD(int, query, (const std::string&));
    MOCK_METHOD(void, asyncFetch, (const std::string&, std::function<void(const std::string&)>));
};
```

## EXPECT_CALL 用法

```cpp
MockDatabase m;

// 基本用法：参数匹配 + 返回值
EXPECT_CALL(m, login, bool, (const std::string&, int), "alice", Eq(1234))
    .Return(true);

// 通配符 + 自定义 matcher
EXPECT_CALL(m, query, int, (const std::string&), An<std::string>())
    .with([](const std::string& s){ return s.find("SELECT") == 0; })
    .Return(42)
    .Times(3);

// 异步回调
EXPECT_CALL(m, asyncFetch, void, (const std::string&, std::function<void(const std::string&)>), _, _)
    .Invoke([](const std::string&, std::function<void(const std::string&)> cb) {
        cb("mocked result");
    });

// 多次匹配
EXPECT_CALL(m, login, bool, (const std::string&, int), _, _)
    .Return(false)
    .AtLeast(2);
```

## 内置 Matcher

| Matcher | 说明 |
|---------|------|
| `_` | 匹配任意值 |
| `Eq(v)` | 精确等于 `v` |
| `An<T>()` | 匹配任意 `T` 类型值 |
| `HasPrefix(s)` | 字符串前缀匹配 |
| `Matcher<T>(fn)` | 自定义 predicate |
| `Not(m)` | 取反 |

## 链式方法

```cpp
EXPECT_CALL(mock, method, Ret, (Args...), matchers...)
    .Return(val)                    // 指定返回值
    .Invoke(fn)                     // 自定义回调函数
    .Times(n)                       // 精确次数
    .Times(lo, hi)                  // 范围次数
    .AtLeast(n)                     // 最少次数
    .with(fn)                       // 自定义参数匹配
    .WillRepeatedly(fn)             // 重复使用的默认行为
```

## 顺序校验

```cpp
auto& seq = TmockSequence::global();
seq.enabled = true;

mock.login("alice", 1234);
mock.commit();

assert(tmock_verify_order("login_ok", "commit_ok"));
seq.enabled = false;
```

## 测试覆盖

```
test_all.cpp     17 个测试用例
test_basic.cpp    6 个测试用例
test_async.cpp    3 个测试用例
test_sequence.cpp 2 个测试用例
```

## 编译

```bash
g++ -std=c++17 -I include tests/test_basic.cpp -o test_basic
./test_basic
```

## 版本

- **v1.0.1**: EXPECT_CALL API + 内置 matchers + 4 套测试全通过
- v0.x: 早期原型（已废弃）

## 许可证

MIT
