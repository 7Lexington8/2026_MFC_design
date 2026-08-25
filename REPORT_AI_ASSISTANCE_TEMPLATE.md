# 实验报告：AI 辅助编程过程记录模板

> 这是素材模板，不是最终报告。请按你们实际过程修改，不要虚构。

## AI 辅助范围

本项目在 C++ MFC 与 llama.cpp 集成过程中使用生成式 AI 作为辅助工具，主要用于：

1. 将 B9 需求拆解为模型推理、MFC 界面、流式输出、会话管理、参数配置等独立模块；
2. 查询和解释 llama.cpp 的模型加载、Chat Template、Tokenize、Decode 与 Sampling API；
3. 辅助设计 `std::thread + PostMessage` 的后台推理与 UI 消息回传机制；
4. 对编译错误、运行时错误和参数边界进行辅助排查；
5. 对代码结构和实验报告表述提出修改建议。

## 人工完成与验证内容

小组成员负责 Visual Studio/MFC 环境搭建、SenseNova GGUF 实际加载、各模块代码整合、编译修正、运行测试、采样参数实验、UI 调整以及最终演示。AI 给出的代码或建议均需在本机环境中编译、运行并由组员确认后使用。

## 建议记录的配置

- Windows 版本：
- Visual Studio 版本：
- MSVC 工具集：
- llama.cpp commit：
- SenseNova GGUF 文件名：
- GGUF 量化类型：
- CPU：
- GPU：
- 内存：
- n_ctx：4096（按实际填写）
- Temperature：
- Top-P：
- Top-K：
- Repeat Penalty：
