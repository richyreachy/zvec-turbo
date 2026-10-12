# Turbo 独立发行规划

日期：2026-10-12。基线：`UPSTREAM.json` 中的 zvec 提交。

## 目标与定位

把 zvec 中可复用的向量计算能力发行成独立 C++ 库，服务向量检索引擎、
重排序和批量相似度计算等调用场景。借鉴 Highway 的独立构建、按 ISA
编译、运行时选择、安装导出和跨平台测试方式。Turbo 的公开产品边界是
距离、编码和量化相关算法；通用 SIMD 向量类型和指令抽象不纳入首版。

成功标准是：外部工程只依赖安装后的 Turbo 即可使用；zvec 最终成为
同一接口的消费者；新增优化有明确数值契约、平台覆盖及性能证据。

## 本次交付：0.1.0 内核预览版

独立目录包含自己的头文件、源码、CMake、测试、示例、CI 和源码打包工具。
不使用 zvec 的 CMake 宏，不链接 Ailego/core，不携带 Eigen/RaBitQ。

| 范围 | 本版内容 |
|---|---|
| 数值内核 | FP32/FP16、Record INT8/INT4、部分 raw/uniform 距离及批量路径 |
| PQ | INT8/INT4 ADC、SDC、批量 ADC、32 向量 FastScan 块及打包助手 |
| 旋转 | FHT 内核、拥有上下文的 FhtRotation、已有 OPQ 矩阵的应用 |
| 分发 | 已编译后端与 CPU/OS 能力共同决定；显式请求不可用时返回空 |
| 使用方式 | `turbo::` 命名空间、C++17、函数指针内核、CMake 导出 target |
| 构建发行 | static/shared、Scalar 分发开关、安装、pkg-config、源码 tar.gz |
| 可追踪性 | 原始提交、源文件哈希、许可证及本地验证记录 |

本版不包含 Quantizer 训练类、IndexHolder 适配、索引工厂、数据库功能、
模型序列化兼容承诺、SVE/RVV 后端和跨工具链稳定 C ABI。低层 Record /
uniform 布局仍属于高级实验接口，需要调用者遵守尺寸和预处理约定。

## 参考 Highway 的决策

| 参考点 | Turbo 的实现/决策 |
|---|---|
| 可嵌入与可安装 | `add_subdirectory` / `find_package(Turbo)` 使用同一 `turbo::turbo` target |
| 组件可选 | 测试、示例、安装、SIMD、动态库均为项目局部选项 |
| CPU/OS 分发 | x86 CPUID 加 OSXSAVE/XCR0；AArch64 FP16 使用 OS 能力探测 |
| 编译可用性 | CMake 探测生成私有配置，不向未编译的占位函数分发 |
| ABI 管理 | 初期源码发行；0.x 同 minor 版本匹配，不宣称跨 STL ABI 稳定 |
| 后端抽象 | 首版保留专用 intrinsics；以后根据实测选择 Highway 后端 |

参考源码：[Highway CMake](https://github.com/google/highway/blob/master/CMakeLists.txt)、
[Highway targets](https://github.com/google/highway/blob/master/hwy/targets.cc)。
当前没有引入 Highway 源码或链接依赖。

## 后续阶段及验收

### P0：内核发布候选版加固（优先级最高）

1. 在真实 Linux x86-64、Linux AArch64、Windows x64 上运行 CI。
2. 专门覆盖 SSE2/AVX2/AVX512/VNNI/FP16 硬件，记录每个实际执行的后端。
3. 扩充 Record/uniform 格式、尾部长度、query preprocess、极值和非法参数测试。
4. 为公开格式增加安全构造与尺寸查询，减少调用者直接拼接元数据的需要。
5. 审核跨 ISA 浮点误差、INT 累积溢出上限、对齐和所有 no-op 占位路径。
6. 建立发布包的构建、安装、迁移目录和独立消费门禁。

验收：所声明的发布平台均有执行记录；每个声明支持的组合有参考结果；
未覆盖平台标明 experimental；源码归档可在没有 zvec 的环境中复建。
当前本地验证的具体范围见 `VALIDATION.md`，CI 文件存在不等于 CI 已通过。

### P1：量化与编码 SDK

1. 定义独立 `QuantizerConfig`、类型/度量枚举和错误类型。
2. 用 `{data, count, dimension, stride}` 数据视图或 reader 回调替代 IndexHolder。
3. 将 Record 编码和归一化能力移出 core，提供尺寸、编码、查询预处理接口。
4. 把 KMeans 训练与推理/扫描拆成可选组件；线程执行器支持用户提供。
5. OPQ 训练单独依赖 Eigen，使用标准依赖发现，移除 RaBitQ 内嵌路径。
6. 明确模型 schema、字节序、布局版本和资源上限；旧 zvec 模型由兼容层转换。

验收：完整量化训练和推理无需 zvec 头文件；禁用训练组件后无需 Eigen；
历史模型有固定样本兼容测试；序列化格式变化与 C++ API 版本分别管理。

### P2：zvec 回接与正式发行

1. 在 zvec 内新增适配层，负责 IndexMeta、Params、IndexHolder 和工厂注册转换。
2. Flat/HNSW/IVF 按阶段迁移；保持已有磁盘格式和查询分数语义。
3. zvec 固定依赖 Turbo tag/commit，禁止两边长期维护不一致的内核副本。
4. 每次升级同时执行 zvec 索引回归与 Turbo 独立测试。
5. 根据外部消费者需求增加 C API、Conan/vcpkg recipe 和平台二进制包。

验收：至少一个 zvec 端到端路径和一个外部工程使用相同发布包；召回率、
建库/查询延迟、内存变化均有基准记录；版本、变更日志和回滚流程明确。

### P3：后端扩展与维护成本优化

用等价算法实现比较现有专用内核与 Highway 后端，再决定 SVE/RVV 等后端
采用哪种方式。保留经验证有价值的 VNNI、FP16、FastScan 专用路径。
不把全面重写后端作为独立发行的前置条件。

## 性能验证设计

维度至少覆盖 32/64/128/384/768/1536 及非整齐维度；同时测试单条、连续批量、
gather 批量和 FastScan 块。区分缓存内与内存带宽限制场景，记录 CPU、编译器、
实际选中 ISA、频率策略、线程数及数据布局。分别报告 kernel-only 和包含
编码/查询预处理的端到端时间，避免把布局变化收益归因于 ISA。

参考实现必须采用相同的负内积/cosine 约定和精度要求。Highway 比较使用
等价算法，不把不同工作量的测试作为速度结论。发布前给出误差容限、
性能变化门槛和重复测量方法；本次交付不宣称相对 Highway 的加速比。

## 版本、维护和发布流程

- 0.1.x 维护当前内核预览；0.2 可用于量化 API 实验；1.0 需通过真实消费者验收。
- 开发主线选定一个源头。当前为有来源清单的独立快照，后续 upstream 同步
  必须审查变化，不能自动覆盖独立 API、分发修复及许可证文件。
- 先构建并测试源码包，再生成平台包；签名/tag/公开上传在目标仓库确定后进行。
- 发布包包含 LICENSE、NOTICE、CHANGELOG、API 契约和支持矩阵。
- 0.x 不承诺跨编译器/STL 二进制兼容；安装 target 传播必要编译定义。

## 工作量与待决事项

粗估由熟悉 zvec 的 C++ 工程师推进：P0 加固约 2–4 人周，P1 解耦约 4–8 人周，
P2 回接与回归约 2–4 人周。硬件到位、历史格式兼容范围和测试发现会影响估算；
这些不是承诺排期。本次已提供 P0 的实现基础和本地可复验流程。

需要项目方确定：正式仓库/包名、维护责任人、首批必须支持的平台、模型兼容
范围，以及源码发行还是二进制发行优先。上述选择不阻塞当前本地预览包使用。
