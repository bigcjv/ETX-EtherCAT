# Codex 开发说明

## 项目目标

维护 ETX-R41K111C 控制 6 台松下 A6B EtherCAT 伺服的项目。最终程序采用
Scenario 1，模块化 C++17 程序运行在 ETX ARM64 Linux 上；Windows 11 用于
编辑源码、ECATNavi 配置、部署和诊断。

开始工作前先阅读根目录 `README.md`。以当前源码和
`6axis_eni/ENI.xml` 为项目事实来源，并参考 README 中列出的厂商手册。

## 已验证环境

- 工作区：`E:\work\etx-r41k111`
- ETX：`192.168.0.10`，SSH 用户 `tpm`，ARM64 Debian/Raspberry Pi OS
- Windows PC：`192.168.0.100`
- PuTTY：`C:\Program Files\PuTTY\plink.exe`、`pscp.exe`
- SSH Host Key：`SHA256:xBzCH8w5lTi3ExQakSZm9lXGXlcDDXBhhNumThbSNGE`
- TPM 安装目录：`C:\TPM\ETX-R4K111`
- 官方示例：`C:\TPM\ETX-R4K111\Sample`
- ETX 头文件：`/home/tpm/ETX-R4K111/include`
- ECPW 库：`/usr/lib/libECPW.so`
- ECPW 设置：`/usr/lib/ECPL/Config/Setting.json`
- 生效 ENI：`/usr/lib/ECPL/ENI/ENI.xml`
- 远端项目：`/home/tpm/etx_6axis_csp`
- 最近实测 ECPW：`3.11.1.0`
- 最近实测 ETX server：`01.03.07_20250526`
- 最近实测 tools package：`1.2.2.0`

禁止把密码写入被 Git 跟踪的文件、命令日志或提交信息。需要连接且已获得用户授权
时，可读取本机未跟踪的 `docs/ssh_info.txt`，或从用户/环境取得密码。EtxTool
1.0.3.0 内嵌厂商 SSH 凭据，修改 ETX 密码会导致其 Version/Scenario 查询失败。

## 当前 EtherCAT 配置

当前 `6axis_eni/ENI.xml` 描述站号 1 到 6 共 6 台 `MADLT05BF`。它包含 6 个
DC 节点，所有 `CycleTime0/1=1000000 ns`；Drive 1 是唯一 DC 参考时钟，
Drive 2 到 6 不是参考时钟；DC 激活和 1 ms 周期寄存器写入均已存在。

ETX 最近实测 `/usr/lib/ECPL/Config/Setting.json` 包含：

```json
"CycleTime": 1000,
"ENABLE_DC": true
```

当前主站周期和六轴 DC SYNC0 均为 1 ms。必须保持这个不变量：修改周期时，
`Setting.json` 和重新导出的 ENI 必须使用同一周期。

## Scenario 资源归属

- Scenario 2：`etx.service` 占用 ECPW/EtherCAT，通过 TCP 5886 服务 Windows 客户端。
- Scenario 1：ETX 本机 Linux 自研程序占用 ECPW/EtherCAT。
- 严禁同时运行 `etx.service` 和 `etx_6axis_csp_demo`。
- 最近实测状态为 Scenario 2，`etx.service` enabled 且 active。
- 当前没有确认自研 Scenario 1 运动服务已设置开机自启。

EtxTool 广播发现、SSH Version/Scenario 查询和 ECATNavi TCP 5886 是三条独立
路径，应分别诊断。Discovery 能看到 ETX 不代表 SSH 查询或 5886 服务正常。

## 代码结构

- `src/etx_6axis_csp/src/main.cpp`：生命周期、Group、同步往返和信号处理。
- `src/etx_6axis_csp/src/EcpwSession.cpp`：ECPW 初始化、ENI、连接和 RAII 清理。
- `src/etx_6axis_csp/src/AxisController.cpp`：CiA402/CSP 校验、Servo 和监控。
- `src/etx_6axis_csp/src/AppConfig.cpp`：命令行解析和保守默认值。
- `scripts/deploy_etx_6axis_csp.ps1`：上传源码/ENI、远端编译、dry check 和显式运动。

六轴同步命令是 Group 0 上的 `ECPWGroupMoveLin()`。不要退回逐轴阻塞调用
`ECPWPositioning()`。程序必须继续要求显式 `--enable-motion` 才允许运动。

## 开发流程

1. 修改前执行 `git status`。工作区可能有用户改动，禁止回退、覆盖或暂存无关文件。
2. 修改 ECPW API 或结构体前，先阅读邻近源码、TPM 示例和相关手册。
3. 默认在 Windows 编辑和部署，在 ARM64 ETX 上使用厂商头文件/库直接编译。
4. 把 ENI 当作结构化 XML 校验。DC 模式必须有 6 个 DC 节点、1 个参考时钟、
   相同周期和站号 1 到 6。
5. 先部署编译和 dry check。运动必须得到本轮用户明确授权并确认机械安全。
6. 修改完成后只提交本任务涉及的文件并推送 `origin`。推送失败时保留本地提交，
   在 `docs/etx_deploy_status.md` 记录失败原因和下一步。

保持现有简洁、模块化 C++ 风格，保留 RAII 清理、`throwOnEcpError` 错误检查和
默认不运动的安全设计。

## 常用验证

本地状态和 ENI：

```powershell
git status --short
Select-String -Path .\6axis_eni\ENI.xml -Pattern '<DC>|<CycleTime0>|<ReferenceClock>'
```

部署并编译，不运动：

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 -Password "<ETX_PASSWORD>" -Build
```

部署、编译和不运动连接检查：

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 -Password "<ETX_PASSWORD>" -Build -RunDryCheck
```

Scenario 2 服务检查：

```bash
systemctl is-enabled etx.service
systemctl is-active etx.service
ss -lnt | grep ':5886'
```

不要为了普通验证而调用 `-RunMotion`、添加 `--enable-motion`、重启 ETX、修改
周期、切换 Scenario 或改动 systemd 服务。

## 运动安全

本项目控制真实六轴设备。下发运动前必须取得明确授权，并确认急停、限位、安全
空间、驱动方向、单位换算和速度/加速度。首次只执行一次保守往返。不得假设
ECPW user unit 等于编码器脉冲、角度或毫米。

程序中断或报错时，优先平滑停止、禁用 Group、Servo OFF、ECPWDisconnect 和
ECPWClose。默认不得保持 Servo ON。驱动器带电时禁止自动重启控制器。

## 周期修改约束

松下手册支持的最短周期不等于 ETX 六轴负载下可稳定使用的周期。1000 us 是当前
调试基线。测试 500 us 或更短周期必须满足：

- 所有轴停止并 Servo OFF；
- 只有一个 EtherCAT 主站占用者；
- Setting.json 和新导出的 ENI 同时修改；
- 六轴不运动 OP 状态测试通过；
- 检查 DC、WKC、看门狗、错误日志和实时周期抖动；
- 测试前准备可恢复的配置备份。

禁止只手改 ENI 周期字节或只改 `Setting.json` 后宣称周期修改完成。

## 文档要求

根目录和 `docs/` 文档使用 UTF-8，命令应可直接运行。拓扑、Scenario、路径、
周期、部署参数、CLI 行为或自启动服务发生变化时，同步更新 README 和相关说明。
明确区分实测事实与建议；没有真正执行重启或运动测试时，不得声称已验证。
