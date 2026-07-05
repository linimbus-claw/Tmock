# Tmock 完成报告

## 框架概述

**Tmock** —— Header-only C++17 Mock 框架，gMock 风格的继承式打桩。

## 已完成的文件

```
Tmock/
├── include/tmock/tmock.h   # 核心头文件（~360 行）
├── tests/
│   ├── test_basic.cpp      # 6 个测试全部通过
│   ├── test_async.cpp      # 3 个测试全部通过
│   └── test_sequence.cpp   # 2 个测试全部通过
├── CMakeLists.txt
├── README.md
└── LICENSE                # （待添加）
```

## 核心 API

### 宏生成 Mock 方法

```cpp
class MockFoo : public Foo {
public:
    MOCK_METHOD2(bool, login, const std::string&, int);
    MOCK_METHOD_VOID2(send, int, const std::string&);
};
```

### 打桩（Fluent API）

```cpp
MockUserService mock;
mock.core_login.expect("alice", 1234)
    .willReturn(true)
    .times(1);
```

### 异步接口支持

```cpp
mock.core_getProfile.addExpectation()
    .withArgs("alice", std::function<void(...)>{})
    .willInvoke([](const std::string& user,
                  std::function<void(const std::string&, int)> cb) {
        cb("Alice", 100);  // 立即回调
        // 或延迟：std::thread([cb]{ sleep(100ms); cb(...); }).detach();
    });
```

### 调用时序校验

```cpp
{
    TMOCK_IN_SEQUENCE;  // 开启记录
    mock.send("hello");
    mock.ack(1);
    TMOCK_EXPECT_ORDER_BEFORE("send:hello", "ack:1");
}
```

## 编译测试状态

✅ `test_basic.cpp` —— 6/6 通过  
✅ `test_async.cpp` —— 3/3 通过  
✅ `test_sequence.cpp` —— 2/2 通过  

## 待完成

- [ ] 添加 LICENSE 文件（MIT）
- [ ] 推送到 GitHub（需要 Personal Access Token）
- [ ] 可选：添加更多示例

## GitHub 推送步骤（需要用户操作）

1. 登录 GitHub → Settings → Developer settings → Personal access tokens → Generate new token
2. 勾选 `repo` 权限
3. 复制 token
4. 运行：
   ```bash
   cd ~/Desktop/Tmock
   git remote add origin https://<token>@github.com/linimbus-claw/Tmock.git
   git push -u origin main
   ```
