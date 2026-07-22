# ETX-R41K111C 六轴 EtherCAT 控制项目

本仓库用于通过 TPM ETX-R41K111C 控制器控制 6 台松下 MINAS A6B
EtherCAT 伺服驱动器，包含六轴 ENI、Windows 部署脚本和运行在 ETX 内部
Linux 上的 Scenario 1 C++17 控制程序。

## 1. 硬件与网络

| 项目 | 当前配置 |
|---|---|
| 厂商 | TPM（Taiwan Pulse Motion，台湾脉动） |
| 控制器 | ETX-R41K111C-4P6 |
| 料号 | 90-BR40112-130 |
| 架构 | ARM64 独立式 EtherCAT 主站，基于 Raspberry Pi |
| ETX 管理 IP | `192.168.0.10` |
| Windows 开发电脑 IP | `192.168.0.100` |
| Linux 用户 | `tpm` |
| SSH Host Key | `SHA256:xBzCH8w5lTi3ExQakSZm9lXGXlcDDXBhhNumThbSNGE` |
| EtherCAT 从站 | 6 台松下 A6B，当前 ENI 型号名为 `MADLT05BF` |

密码不得提交到 Git。当前电脑上的连接凭据记录在未跟踪文件
`docs/ssh_info.txt` 中，部署时通过参数临时传入。EtxTool 1.0.3.0 内部使用
厂商固定的 SSH 凭据；修改 ETX Linux 密码可能导致 EtxTool 无法查询 Version
和 Scenario，即使 ping 和设备扫描仍然正常。

## 2. 开发环境

Windows 软件和路径：

- TPM 安装目录：`C:\TPM\ETX-R4K111`
- 官方示例：`C:\TPM\ETX-R4K111\Sample`
- EtxTool：`C:\TPM\ETX-R4K111\Tool\EtxTool_1.0.3.0\EtxTool.exe`
- PuTTY：`C:\Program Files\PuTTY\plink.exe`、`pscp.exe`
- ECATNavi_ETX/ECATScan：扫描、配置、测试从站并导出 ENI

仓库外参考资料：

- `E:\work\refer\taiwan_ethercat\ETX-R41K111_Series_UserManual_1.0.pdf`
- `E:\work\refer\taiwan_ethercat\ETX-R41K111_Series_ProgramingManual_1.0.pdf`
- `E:\work\refer\taiwan_ethercat\ETX-R41K111_Series_ECATNaviManual_1.0.pdf`
- `E:\work\refer\taiwan_ethercat\panasonic_minas-a6bf_v1_10.xml`
- `E:\work\refer\电机说明书\A6B 系列 (EtherCAT) -EtherCAT 通信规格篇.pdf`

ETX Linux 关键路径：

| 内容 | 路径 |
|---|---|
| ECPW 头文件 | `/home/tpm/ETX-R4K111/include` |
| ECPW 动态库 | `/usr/lib/libECPW.so` |
| ECPW 主配置 | `/usr/lib/ECPL/Config/Setting.json` |
| 生效 ENI 目录 | `/usr/lib/ECPL/ENI` |
| 部署后的项目 | `/home/tpm/etx_6axis_csp` |
| Windows 场景服务端 | `/usr/lib/etxEthSvr/etxEthSvr` |

## 3. 模式选择

本项目有两类容易混淆的“模式”：

| 模式 | 用途 |
|---|---|
| Scenario 2 Windows-based | 运行 `etx.service`，ECATNavi 和 Windows 程序通过 TCP 5886 控制 ETX |
| Scenario 1 Linux-based | 停止/禁用 `etx.service`，自研 C++ 程序直接在 ETX Linux 上控制主站 |
| FreeRun | 从站非同步运行，不用于本项目六轴 CSP |
| DC SYNC0 | EtherCAT 分布式时钟同步，六台 CSP 驱动器均应使用 |

配置、扫描和导出 ENI 时使用 Scenario 2；最终独立运行 Linux 自研程序时使用
Scenario 1。`etx.service` 和 `etx_6axis_csp_demo` 会争用同一个 EtherCAT 主站，
严禁同时运行。

截至 2026-07-22，最后一次实测状态是 Scenario 2：

```text
etx.service: enabled, active
TCP 管理服务: 0.0.0.0:5886
ECPW: 3.11.1.0
ETX server: 01.03.07_20250526
ETX tools package: 1.2.2.0
```

## 4. Windows 11 连接 ETX

基础连通检查：

```powershell
ping 192.168.0.10
Test-NetConnection 192.168.0.10 -Port 22
Test-NetConnection 192.168.0.10 -Port 5886
```

使用固定 Host Key 进行 SSH 连接：

```powershell
& "C:\Program Files\PuTTY\plink.exe" `
  -ssh tpm@192.168.0.10 `
  -hostkey "SHA256:xBzCH8w5lTi3ExQakSZm9lXGXlcDDXBhhNumThbSNGE"
```

Scenario 2 服务检查：

```bash
systemctl is-enabled etx.service
systemctl is-active etx.service
ss -lnt | grep ':5886'
```

设置 Scenario 2 服务立即运行并开机自启：

```bash
sudo systemctl enable --now etx.service
```

EtxTool 的广播发现、SSH 版本查询和 ECATNavi 的 TCP 5886 连接是三条不同路径。
因此可能出现“能扫描到 ETX，但 Version/Scenario 为 unknown”或“SSH 正常，但
ECATNavi 连不上”的情况，应分别检查 SSH 凭据、`etx.service` 和 5886 端口。

## 5. 六轴上位机配置与 ENI 导出

1. 在 EtxTool 中选择 Scenario 2，确认 `etx.service` 正常。
2. 在 ECATNavi/ECATScan 中加载松下 A6B ESI。
3. 扫描并确认站号 1 到 6 共 6 台 `MADLT05BF`。
4. 检查 CiA402 PDO，至少包含 Controlword、Modes of operation、Target
   position、Statusword、Mode display 和 Position actual value。
5. 打开第一台驱动器的 `Device Settings -> DC`。
6. 选择 `DC SYNC0`，Sync0 倍率选择 `x1`，点击 `Apply to All`。
7. 保存配置并重新导出 ENI。
8. 替换 `6axis_eni/ENI.xml`，检查 Git 差异后再部署。

当前 `6axis_eni/ENI.xml` 已重新导出为 DC SYNC0 配置，经解析确认：

- 包含 6 个 `<DC>` 节点；
- 每台驱动器 `CycleTime0` 和 `CycleTime1` 均为 `1000000 ns`；
- Drive 1 是唯一分布式时钟参考，从站 2 到 6 不是参考时钟；
- 包含 DC 激活值 `0x0300` 和 1,000,000 ns 周期寄存器写入。

因此当前 ENI 的六轴 Sync0 周期为 1 ms。

2026-07-22 检查 ECATNavi 默认目录 `C:\TPM\ECPW\ENI\ENI.xml` 中新导出的
文件时，确认它虽有 6 台驱动器，但没有 `<DC>` 节点，不能用于 500 us CSP。
必须按上述步骤重新导出，不能手改现有 1 ms ENI 的周期字节代替导出。

## 6. EtherCAT 通信周期

ETX 主站实际周期由以下文件控制：

```text
/usr/lib/ECPL/Config/Setting.json
```

当前实测相关配置：

```json
{
  "CycleTime": 500,
  "ENABLE_DC": true
}
```

该值于 2026-07-22 为重新导出 500 us ENI 准备完成；当前 Scenario 2 服务已启动，
但新的 500 us DC ENI 尚未替换项目文件，因此尚未执行 OP/dry check 或运动测试。

`CycleTime` 单位是微秒，ENI 的 `CycleTime0/1` 单位是纳秒。主站和所有
Sync0 从站必须保持一致：

| 目标周期 | `Setting.json` | ENI `CycleTime0/1` | ECATNavi |
|---|---:|---:|---|
| 1 ms | `1000` | `1000000` | `DC SYNC0`、`x1` |
| 500 us | `500` | `500000` | `DC SYNC0`、`x1` |

安全修改周期的顺序：

1. 所有轴 Servo OFF，并停止运动程序。
2. 停止当前主站占用者：Scenario 2 停 `etx.service`，Scenario 1 停自研程序。
3. 备份并修改 `/usr/lib/ECPL/Config/Setting.json`。
4. 在 ECATNavi 中设置同样的 Master Cycle，六轴保持 `DC SYNC0 x1`，重新导出 ENI。
5. 把新 ENI 部署为 `/usr/lib/ECPL/ENI/ENI.xml`。
6. 只启动选定的一个主站程序，先执行不运动连接检查。
7. 确认六轴均进入 OP，且没有 DC、WKC、看门狗或 `Err81.0` 错误。

松下 A6B 手册列出的 DC/SM2 周期为 125、250、500、1000、2000、4000、
8000 和 10000 us，其中 125/250 us 存在控制模式限制。这只是驱动器能力，
不代表 ETX 在六轴负载下必然稳定。当前正在从 1000 us 基线迁移到 500 us；
500 us 必须先通过六轴不运动 OP、DC、WKC、看门狗和错误日志检查，再允许运动。

## 7. Scenario 1 六轴 C++ 程序

项目结构：

```text
6axis_eni/ENI.xml                  六轴网络配置
scripts/deploy_etx_6axis_csp.ps1  上传、安装 ENI、编译和可选运行
src/etx_6axis_csp/                模块化 C++17 控制程序
docs/                             详细中文操作说明
```

程序流程：

1. 在 `ECPWInit()` 前校验 `Setting.json`、ENI 六轴 DC 周期、唯一参考时钟和
   DC 周期寄存器写入，默认要求 500 us。
2. `ECPWInit()`、选择 ENI 并执行 `ECPWConnect()`。
3. 校验 6 个 CiA402 从站、驱动器报警和 CSP 支持。
4. 使用 `ECPWSetMOP()` 设置 CSP。
5. 经显式授权后清除报警并 Servo ON。
6. 把六轴加入 Group 0。
7. 使用 `ECPWGroupMoveLin()` 同步执行相对正向/反向往返。
8. 正常或异常退出均平滑停止、禁用组、Servo OFF、断开并关闭 ECPW。

程序只有出现 `--enable-motion` 才会下发运动。

确认六轴停止并 Servo OFF 后，先为 ECATNavi 的 500 us ENI 导出准备 Scenario 2：

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 `
  -Password "<ETX_PASSWORD>" `
  -CycleTimeUs 500 `
  -PrepareEniExport `
  -ConfirmAxesStoppedAndServoOff
```

在 ECATNavi 重新导出并替换 `6axis_eni/ENI.xml` 后，切换到 Scenario 1、备份并
安装周期配置、编译并执行不运动检查：

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 `
  -Password "<ETX_PASSWORD>" `
  -CycleTimeUs 500 `
  -ConfigureCycleTime `
  -ActivateScenario1 `
  -Build `
  -RunDryCheck
```

ETX 上手动执行不运动检查：

```bash
cd /home/tpm/etx_6axis_csp
sudo ./etx_6axis_csp_demo --eni ENI.xml --axes 6 --cycle-us 500
```

只有在急停、限位、方向、机械空间和人员安全均已确认后，才执行保守运动：

```bash
sudo ./etx_6axis_csp_demo \
  --eni ENI.xml \
  --axes 6 \
  --cycle-us 500 \
  --enable-motion \
  --cycles 1 \
  --distance 1000 \
  --feed 500 \
  --accel 1000 \
  --decel 1000
```

`distance`、`feed`、`accel`、`decel` 是 ECPW user unit，不能直接假设为
编码器脉冲、角度或毫米。增大位移前必须确认单位换算。速度过高时，即使电机
已正常执行，运动时间也可能短到肉眼不明显。

## 8. 上电自启动

当前没有确认任何自研运动程序的 systemd 自启动服务。创建 Scenario 1 服务前，
必须先禁用 `etx.service`，避免双主站争用。正式服务应设置明确工作目录，默认不
自动运动或必须等待外部安全就绪信号，并在 SIGTERM 时执行停止和 Servo OFF。

不得让 Scenario 1 自研服务与 Scenario 2 的 `etx.service` 同时开机启动。

## 9. 安全与故障排查

- 人员或工装处于运动范围内时禁止测试。
- 每次更换 ENI、周期、驱动器或程序版本后，先执行 dry check。
- 驱动器带电时，不把重启 ETX 当作普通验证步骤。
- `systemctl status` 中乱码通常是终端编码问题，应以 `is-active`、日志和端口为准。
- `ECPWInit err=10` 表示权限不足，当前程序通常需要 `sudo`。
- 六轴不同步时，依次检查 GroupMoveLin、6 个 DC 节点、唯一参考时钟、相同
  Sync0 周期和主站 `CycleTime`，不要先靠修改速度参数掩盖问题。

更多专题说明见 `docs/`。
