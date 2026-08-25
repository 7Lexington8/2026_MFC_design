# LocalSenseNova — B9 本地小模型课程作业

这是一个面向 **Visual Studio 2022 + C++ MFC + llama.cpp + SenseNova 8B GGUF** 的第一版可运行工程骨架。

当前版本已经把 B9 要求拆成了代码模块：

- MFC 对话框桌面端
- 选择并加载本地 `.gguf` 模型
- `llama.cpp` 原生 C/C++ API 推理
- `std::thread` 后台推理，避免 UI 假死
- Token 级流式输出到 `CRichEditCtrl`
- Stop 中止生成
- System Prompt
- Temperature / Top-P / Top-K / Repeat Penalty / Max Tokens
- 普通助手 / C++ 代码助手 / 中英翻译 / 公文写作一键预设
- 多会话：新建、切换、删除
- 多轮历史上下文
- 自动保存会话历史到 EXE 同目录 `LocalSenseNovaHistory.dat`
- 状态栏显示 Prompt token 数、生成 token 数和 tok/s
- CPU 构建脚本；另附可选 NVIDIA CUDA 构建脚本

> **模型文件不包含在压缩包内。** 8B GGUF 往往是数 GB，课程提交本身也要求控制体积。程序运行时点击“选择并加载 GGUF”即可。

## 1. 开发环境

建议 Windows 10/11 x64。

Visual Studio Installer 中至少安装：

1. **使用 C++ 的桌面开发**
2. MSVC v143
3. Windows 10/11 SDK
4. **C++ MFC for latest v143 build tools (x86 & x64)**
5. CMake tools for Windows（VS 工作负载通常可一起装）

另外安装 Git for Windows。

## 2. 第一次运行

### 2.1 拉取 llama.cpp

双击：

```bat
setup_llama.bat
```

脚本会克隆官方 `llama.cpp` 的 **b10516（2026-08-20 发布版）** 到：

```text
third_party/llama.cpp/
```

### 2.2 CPU 版本

双击：

```bat
build_cpu.bat
```

生成成功后运行：

```text
build/Release/LocalSenseNova.exe
```

### 2.3 NVIDIA GPU 版本（可选）

电脑装有兼容 NVIDIA GPU + CUDA Toolkit 时，可运行：

```bat
build_cuda.bat
```

生成：

```text
build-cuda/Release/LocalSenseNova.exe
```

程序会在 llama.cpp 构建支持 GPU offload 时尝试将模型层卸载到 GPU；CPU-only 构建自动走 CPU。

## 3. 模型

程序没有把具体文件名写死，只要求扩展名为 `.gguf`。运行后：

1. 点击 **选择并加载 GGUF**
2. 选择模型
3. 等待状态栏出现“模型加载成功”
4. 输入问题并发送

为了减少 Windows 路径兼容问题，建议模型放在纯英文路径，例如：

```text
D:\models\SenseNova8B.gguf
```

## 4. 工程结构

```text
LocalSenseNova/
├─ CMakeLists.txt
├─ setup_llama.bat
├─ build_cpu.bat
├─ build_cuda.bat
├─ src/
│  ├─ LocalSenseNova.cpp/.h      MFC App 入口
│  ├─ MainDlg.cpp/.h             主界面、消息分发、多线程
│  ├─ LLMEngine.cpp/.h           llama.cpp 模型加载与推理核心
│  ├─ ConversationManager.cpp/.h 多会话与本地历史保存
│  ├─ AppSettings.h              推理参数
│  ├─ Utf8.cpp/.h                UTF-8 / Unicode 转换
│  ├─ LocalSenseNova.rc          MFC 对话框资源
│  └─ resource.h
└─ third_party/
   └─ llama.cpp/                 setup 脚本自动下载
```

## 5. 模块工作流

```text
用户点击发送
    ↓
MainDlg 读取参数 + 当前会话历史
    ↓
启动 std::thread
    ↓
LLMEngine::Generate
    ↓
llama_chat_apply_template
    ↓
llama_tokenize
    ↓
llama_decode(prompt)
    ↓
Sampler: Repeat Penalty → Top-K → Top-P → Temperature → Dist
    ↓
逐 Token 生成
    ↓
PostMessage(WM_APP + ...)
    ↓
MFC UI 线程追加到 CRichEditCtrl
```

这正是汇报时最值得讲的“**推理线程与 UI 线程解耦**”。

## 6. 参数说明

- `Temperature`：0~2。低值更稳定，高值更多样。
- `Top-P`：0~1。核采样阈值。
- `Top-K`：只保留概率最高的 K 个候选，0 表示不限制。
- `Repeat Penalty`：重复惩罚，1.0 相当于关闭；通常可用 1.05~1.15。
- `Max Tokens`：一次最多生成多少 token。
- `System Prompt`：定义角色、回答风格与任务约束。

## 7. 当前实现的一个设计选择

每次生成时，本项目会把**当前会话完整历史重新套入模型自带 chat template**，然后清空 KV memory 后重新编码历史。

优点：实现简单、稳定、容易解释，多会话切换不会把不同会话的 KV cache 搞混。

缺点：对话很长时首 token 延迟会增加。

如果后面想继续冲“功能拓展”，可以再做：

- 每个会话独立 KV Cache / prompt cache
- Markdown 与代码块高亮
- 模型加载进度条
- Context 使用率可视化
- 导出 Markdown/TXT 聊天记录
- GPU/CPU 占用与内存统计
- 首 Token 延迟 TTFT
- 性能对比实验：Temperature / Top-K / Top-P 不同组合

## 8. 重要：AI 辅助过程要写进实验报告

课程允许 AI 辅助编程，但要求在报告中说明 AI 辅助过程与配置步骤。因此建议保存：

- AI 用于哪些模块：架构设计、MFC 消息机制、llama.cpp API 接入、Debug 等
- 你们自己进行了哪些验证和修改
- Visual Studio / CMake / llama.cpp / 模型版本
- CPU/GPU 配置
- 参数测试结果

不要写成“AI 一键生成整个程序”。报告里更合适的表述是：AI 用于 API 查询、代码框架建议和错误定位，最终由小组完成集成、测试、参数调节和功能拓展。

## 9. 本版本的验证边界

本工程是在非 Windows 环境中生成，因此这里无法实际启动 MFC 或加载你们手里的 GGUF 做最终编译运行测试。

`LLMEngine.cpp` 按 2026-08 的 llama.cpp 公共 C API 编写；`setup_llama.bat` 已固定到发布版 **b10516**，避免 master 分支更新导致接口漂移。若你们后续主动升级 llama.cpp，再出现 API 变化，应先回退到 b10516 验证。

建议课程工程记录：

```text
llama.cpp commit = xxxxxxxxx
```

之后全组都使用同一 commit。

## Windows 普通 CMD 找不到 CMake？

V0.2 的 `build_cpu.bat` / `build_cuda.bat` 会自动通过 Visual Studio Installer 的 `vswhere.exe` 定位 VS 2022，并调用 `VsDevCmd.bat` 初始化编译环境。因此即使普通 CMD 中 `cmake --version` 无法识别，也可以直接双击构建脚本。

如果自动检测仍失败，可打开 **Developer Command Prompt for VS 2022**，进入本项目目录后运行 `build_cpu.bat`。
