# LocalSenseNova v0.3

LocalSenseNova 是我们围绕课程设计 B9 完成的 Windows 本地大模型聊天程序。项目不训练模型，也不通过 Python、网页或远程 API 转发请求；界面使用 C++ MFC，推理由 llama.cpp 的原生 C/C++ API 完成，模型和聊天记录都保存在本机。

当前版本面向 SenseNova 8B GGUF，同时也可以尝试加载 llama.cpp 支持的其他 GGUF 模型。不同模型的指令格式和回答效果可能有差异。

## 当前完成情况

- [x] 选择并加载本地 SenseNova 8B `.gguf` 模型
- [x] 直接集成 llama.cpp，完成 Chat Template、Tokenize、Decode 与 Sampling
- [x] `std::thread + PostMessage` 后台推理和 Token 级流式显示
- [x] Stop 协作式停止生成
- [x] 多轮对话和多会话的新建、切换、删除
- [x] 会话历史本地持久化，程序重启后自动恢复
- [x] System Prompt 和 Temperature、Top-P、Top-K、Repeat Penalty、Max Tokens 调节
- [x] 普通助手、C++ 代码助手、中英翻译、公文写作四套预设
- [x] Prompt token、生成 token 和生成阶段 `tok/s` 统计
- [x] MFC 自定义浅色主题、会话列表自绘、可收起参数面板和窗口自适应布局
- [x] Per-Monitor V2 高 DPI 启动缩放适配
- [x] x64 Release 的 CPU 构建脚本和可选 NVIDIA CUDA 构建脚本

> 模型文件不在仓库和课程提交包中。8B GGUF 通常有数 GB，请按模型发布方的许可单独取得并放在本机。

## 技术路线

```text
SenseNova 8B GGUF
        │
        ▼
llama.cpp 原生 C/C++ API
        │
        ▼
LLMEngine：模型、上下文、采样、停止与统计
        │ Token callback
        ▼
std::thread 后台任务 ── PostMessage ──► MFC UI 线程
        │                                  │
        │                                  ├─ CRichEditCtrl 流式显示
        │                                  ├─ 参数与状态栏
        │                                  └─ 自绘会话列表
        ▼
ConversationManager ── LocalSenseNovaHistory.dat
```

这套结构里，`MainDlg` 只负责界面和任务调度，`LLMEngine` 只负责模型推理，`ConversationManager` 只负责会话数据。推理线程不直接操作 MFC 控件，而是把生成片段投递给 UI 线程，因此模型计算时窗口仍能刷新和响应 Stop。

## 工程结构

```text
2026_MFC_design/
├─ CMakeLists.txt                    CMake 工程入口，链接 MFC 与 llama.cpp
├─ setup_llama.bat                   llama.cpp 缺失时恢复固定版本
├─ build_cpu.bat                     VS2022 x64 CPU Release 构建
├─ build_cuda.bat                    VS2022 x64 CUDA Release 构建
├─ REPORT_AI_ASSISTANCE_TEMPLATE.md  AI 辅助过程记录模板
├─ src/
│  ├─ LocalSenseNova.cpp/.h          MFC 应用入口、高 DPI 与 RichEdit 初始化
│  ├─ MainDlg.cpp/.h                 主界面、线程调度、流式消息和参数预设
│  ├─ LLMEngine.cpp/.h               GGUF 加载、Prompt、Decode 与 Sampling
│  ├─ ConversationManager.cpp/.h     多会话和本地历史文件
│  ├─ AppSettings.h                  默认推理参数
│  ├─ Utf8.cpp/.h                    UTF-8 与 UTF-16 转换
│  ├─ LocalSenseNova.rc              MFC 对话框资源
│  └─ resource.h
└─ third_party/
   └─ llama.cpp/                     当前仓库随附的 llama.cpp 源码
```

## 构建与运行

### 1. 准备环境

建议使用 Windows 10/11 x64 和 Visual Studio 2022。在 Visual Studio Installer 中安装：

1. 使用 C++ 的桌面开发；
2. MSVC v143；
3. Windows 10/11 SDK；
4. C++ MFC for latest v143 build tools（x86 & x64）；
5. C++ CMake tools for Windows。

只有在需要重新下载 `third_party/llama.cpp` 时才需要 Git for Windows。

### 2. 取得源码

```bat
git clone https://github.com/7Lexington8/2026_MFC_design.git
cd 2026_MFC_design
```

当前 `main` 已包含项目所用的 llama.cpp 源码。如果 `third_party\llama.cpp\CMakeLists.txt` 缺失，再运行：

```bat
setup_llama.bat
```

脚本固定拉取 llama.cpp `b10516`，但目录已经存在时只会跳过下载，不会自动检查或修正现有源码版本。因此应以最终实际构建所用的 llama.cpp commit 为准，并把它记录到实验报告。不要在答辩前临时升级到最新 `master`，否则上游 API 变化可能导致编译失败。

### 3. 构建 CPU 版本

双击或在命令行运行：

```bat
build_cpu.bat
```

生成文件：

```text
build\Release\LocalSenseNova.exe
```

脚本会优先使用当前命令行里的 CMake；找不到时，会通过 Visual Studio Installer 的 `vswhere.exe` 定位 VS2022 并初始化 x64 编译环境。CPU 配置关闭 CUDA、OpenMP 和本机专用指令优化，便于在不同 x64 电脑间演示。

### 4. 构建 CUDA 版本（可选）

需要兼容的 NVIDIA GPU、驱动和 CUDA Toolkit：

```bat
build_cuda.bat
```

生成文件：

```text
build-cuda\Release\LocalSenseNova.exe
```

CUDA 构建通过 `GGML_CUDA=ON` 启用后端。运行时如果 llama.cpp 检测到 GPU offload 支持，程序会尽量卸载模型层到 GPU；实际卸载层数和速度取决于显卡、显存、驱动、CUDA 版本和模型大小。CPU 构建会自动走 CPU。

### 5. 开始对话

1. 启动 `LocalSenseNova.exe`；
2. 点击“选择并加载 GGUF”；
3. 选择本机的 SenseNova 8B GGUF，等待状态栏显示加载成功；
4. 输入问题并发送；
5. 需要时展开“参数”面板，选择预设或修改参数，修改从下一次生成开始生效。

为减少模型库对 Windows 路径处理方式不同带来的问题，演示时建议使用较短的纯英文模型路径，例如：

```text
D:\models\SenseNova8B.gguf
```

## 一次生成是怎样完成的

```text
点击发送
   ↓
校验模型、输入和参数
   ↓
把用户消息加入当前会话，复制完整历史与当前参数
   ↓
std::thread 调用 LLMEngine::Generate
   ↓
System Prompt + 完整历史 → 模型 Chat Template
   ↓
Tokenize → 分批 Decode Prompt
   ↓
Repeat Penalty → Top-K → Top-P → Temperature + Dist
（Temperature = 0 时改用 Greedy）
   ↓
逐 Token Sample、Decode，并通过 callback 返回 UTF-8 片段
   ↓
PostMessage(WM_STREAM_TOKEN) 交给 MFC UI 线程
   ↓
CRichEditCtrl 追加显示
   ↓
保存回答与会话历史，状态栏显示 token 数和 tok/s
```

模型加载也在后台线程完成，不过 v0.3 暂不支持中途取消加载。Stop 针对生成过程使用原子标志协作停止，会在当前一次 `llama_decode` 返回后响应，而不是强制终止线程。

## 参数与预设

| 参数 | 默认值 | 界面范围 | 作用 |
|---|---:|---:|---|
| System Prompt | 中文通用助手 | 文本 | 设定角色、语气和任务约束 |
| Temperature | 0.80 | 0–2 | 越低越稳定；设为 0 时使用贪心采样 |
| Top-P | 0.95 | 0–1 | 只保留累计概率达到阈值的候选 token |
| Top-K | 40 | 0–500 | 只保留概率最高的 K 个候选；0 表示不限制 |
| Repeat Penalty | 1.10 | 0.5–2 | 对最近 64 个 token 做重复惩罚，1.0 相当于关闭 |
| Max Tokens | 512 | 1–4096 | 本次最多生成的 token 数，仍受剩余上下文限制 |
| Context Size | 4096 | v0.3 固定 | Prompt 与回答共用的上下文容量，当前没有界面输入框 |

四套预设会同时调整 System Prompt 和采样参数：

| 预设 | Temperature | Top-P | Top-K | Repeat Penalty | 使用场景 |
|---|---:|---:|---:|---:|---|
| 普通助手 | 0.80 | 0.95 | 40 | 1.10 | 一般问答 |
| C++ 代码助手 | 0.20 | 0.90 | 40 | 1.05 | 更稳定的代码与解释 |
| 中英翻译 | 0.20 | 0.90 | 40 | 1.05 | 减少无关扩写 |
| 公文写作 | 0.40 | 0.90 | 40 | 1.10 | 正式、结构化中文 |

预设不会改动 Max Tokens。参数是整个应用共享的运行时设置，目前不会按会话分别保存，也不会在重启后恢复。

## 多会话与本地持久化

- 左侧列表支持新建、切换和删除会话；第一条用户消息会自动生成最多 14 个字符的会话标题。
- 启动时自动读取历史；每次生成结束和程序正常退出时自动保存。
- 历史文件位于 EXE 同目录：`LocalSenseNovaHistory.dat`。
- 文件使用项目自定义的 `LSNV1` 二进制格式，保存会话标题及用户/助手消息。
- 模型路径、System Prompt、参数、预设和性能统计不会写入历史文件。

历史文件没有加密。不要在公共提交包里附带包含个人信息的聊天记录，同时要保证 EXE 所在目录具有写权限。

## 已验证环境

以下是项目组当前的实机验证记录。仓库没有附带 GGUF、运行日志和性能截图，最终提交时应补上演示机的完整配置与复测结果。

| 项目 | 验证情况 |
|---|---|
| 操作系统 | Windows 11 x64 |
| 开发工具 | Visual Studio 2022、MSVC v143、MFC、CMake |
| 构建配置 | x64 Release CPU |
| 推理库 | 按 llama.cpp `b10516` 接口完成集成；最终提交记录实际源码 commit |
| 模型流程 | SenseNova 8B GGUF 加载、对话、流式输出、Stop、多会话与历史恢复 |
| 界面 | 程序启动、窗口缩放和高 DPI 启动布局 |

CUDA 路径是项目提供的可选构建与自动 offload 支持，不在这里写未经本机记录证明的显卡型号、速度或“所有设备均可运行”。最终性能应以答辩电脑上的实测结果为准。

## 已知限制

1. 每轮生成都会清空当前推理 memory，并把完整会话历史重新套入 Chat Template 后重新编码。实现容易维护、切换会话不串上下文，但长对话的首 Token 等待会变长。
2. 默认 Context Size 为 4096，当前不在界面中开放；历史超限时会提示新建会话、减少内容或修改代码配置，不会自动截断或摘要。
3. 同一时间只运行一个模型加载或生成任务；模型加载不可取消，Stop 也要等当前一次 Decode 返回。
4. 聊天区支持富文本字体和颜色，但暂不渲染 Markdown，也没有代码块语法高亮。
5. 历史文件未加密；模型路径和参数不持久化；删除会话没有撤销。保存失败目前没有单独的界面提示，异常退出也可能丢失最后一次未落盘的会话操作。
6. 高 DPI 缩放按启动时所在屏幕初始化。把窗口拖到缩放比例不同的显示器后，如布局不理想，重启程序可重新按当前屏幕适配。
7. 兼容性取决于 GGUF 的模型架构和 Chat Template。模型没有可用模板时会回退到通用的 `System/User/Assistant` 文本格式，回答效果不保证与 SenseNova 一致。
8. `tok/s` 统计的是逐 Token 生成阶段，不包含模型加载和 Prompt 预填时间；不同电脑、上下文长度和量化版本之间不能直接横向比较。

## AI 辅助说明

本项目在开发中使用生成式 AI 辅助需求拆解、llama.cpp API 查询、MFC 消息机制设计、编译错误定位和文档检查。AI 给出的建议没有直接作为最终结果提交；小组成员负责环境搭建、代码整合、编译修正、模型联调、参数测试、界面调整和最终演示。

课程报告应按实际过程填写 [`REPORT_AI_ASSISTANCE_TEMPLATE.md`](REPORT_AI_ASSISTANCE_TEMPLATE.md)，至少记录：

- AI 参与了哪些问题，给出了什么建议；
- 小组怎样核对、修改或放弃这些建议；
- Windows、VS、MSVC、llama.cpp、模型量化版本和硬件配置；
- 遇到的代表性错误、定位过程和最终修复；
- CPU/CUDA、采样参数和稳定性测试结果。


## 版本说明

- 项目版本：`v0.3`
- llama.cpp 接口与恢复脚本基线：`b10516`（提交时另记实际源码 commit）
- 语言与界面：C++17、MFC、Unicode
- 目标平台：Windows x64
