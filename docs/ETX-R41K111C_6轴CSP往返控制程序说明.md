# ETX-R41K111C 6 轴松下 A6B CSP 往返控制程序说明

## 目标

本程序用于 ETX-R41K111C 的 Scenario 1：自研 C++ 程序直接运行在 ETX 内部 Linux 上，通过 `libECPW.so` 作为 EtherCAT Master 控制 6 个 Panasonic MINAS A6B EtherCAT 伺服驱动器。

当前功能：

- 使用 `6axis_eni/ENI.xml` 连接 6 轴 EtherCAT 网络。
- 校验 6 个从站均为 CiA402 驱动器。
- 检查并切换每轴到 CSP，`ECP_MOP_CSP`。
- 可选清报警、Servo ON、当前位置置零。
- 使用 `ECPWPositioning` 在 CSP 模式下做简单相对往返运动。
- 默认不运动，必须显式加 `--enable-motion`。

## 目录结构

```text
src/etx_6axis_csp/
  Makefile
  include/
    AppConfig.h
    AxisController.h
    EcpwSession.h
    EcpwUtil.h
  src/
    AppConfig.cpp
    AxisController.cpp
    EcpwSession.cpp
    main.cpp

6axis_eni/
  ENI.xml

scripts/
  deploy_etx_6axis_csp.ps1
```

## 程序设计

`EcpwSession`：

- 调用 `ECPWInit()` 初始化 ECPW。
- 可选调用 `ECPWSetENIFileName()`。
- 调用 `ECPWConnect()` 连接 EtherCAT 从站。
- 析构时自动 `ECPWDisconnect()` 和 `ECPWClose()`。

`AxisController`：

- 用 `ECP_SLV_TAG_TYPE::ECP_TAG_SERIAL` 按 EtherCAT 物理顺序识别从站。
- 默认 6 台 A6B 对应 StationTag 1 到 6，每台 `axisNo=0`。
- 调用 `ECPWGetSlvConf()` 校验 CiA402。
- 调用 `ECPWGetIsSupportMOP(..., ECP_MOP_CSP)` 和 `ECPWSetMOP(..., ECP_MOP_CSP)`。
- 调用 `ECPWServoON()`、`ECPWPositioning()`、`ECPWStop()`、`ECPWServoOFF()`。

`AppConfig`：

- 解析命令行参数。
- 控制 ENI 名称、轴数、站号、位移、速度、加速度、循环次数。

## 安全策略

程序默认只连接和检查，不会 Servo ON，也不会运动。

真正运动必须加：

```bash
--enable-motion
```

首次运行建议使用极小位移和低速度：

```bash
./etx_6axis_csp_demo --eni ENI.xml --axes 6 --enable-motion --cycles 1 --distance 100 --feed 100 --accel 200 --decel 200
```

确认方向、限位、急停、安全回路正常后，再逐步增大参数。

## 在 ETX 上编译

ETX Linux 上需要存在：

```text
/home/tpm/ETX-R4K111/include
/usr/lib/libECPW.so
/usr/lib/ECPL/ENI/ENI.xml
```

编译命令：

```bash
cd /home/tpm/etx_6axis_csp
make clean
make
```

运行程序需要硬件/实时进程权限。若直接运行出现：

```text
ECPWInit err=10
```

这表示 `ECP_ERR_PERMISSION_DENIED`，请使用 `sudo` 运行。

如果 ETX SDK 目录不同：

```bash
make ETX_HOME=/实际/ETX-R4K111/目录
```

## 从 Win11 部署到 ETX

当前 Win11 没有系统 `ssh/scp`，脚本使用 PuTTY：

```text
C:\Program Files\PuTTY\plink.exe
C:\Program Files\PuTTY\pscp.exe
```

部署源码和 ENI：

```powershell
.\scripts\deploy_etx_6axis_csp.ps1
```

部署并在 ETX 编译：

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 -Build
```

部署、编译并做干运行检查：

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 -Build -RunDryCheck
```

部署、编译并运行低速往返：

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 -Build -RunMotion
```

脚本会把本地：

```text
6axis_eni/ENI.xml
```

上传到 ETX，并复制为：

```text
/usr/lib/ECPL/ENI/ENI.xml
```

## 手动运行命令

只连接和检查，不运动：

```bash
cd /home/tpm/etx_6axis_csp
sudo ./etx_6axis_csp_demo --eni ENI.xml --axes 6
```

低速 1 次往返：

```bash
sudo ./etx_6axis_csp_demo --eni ENI.xml --axes 6 --enable-motion --cycles 1 --distance 100 --feed 100 --accel 200 --decel 200
```

默认参数 1 次往返：

```bash
sudo ./etx_6axis_csp_demo --eni ENI.xml --axes 6 --enable-motion
```

常用参数：

```text
--axes N             轴数，默认 6
--first-station N    第一台驱动器 EtherCAT serial station，默认 1
--axis-no N          每个 A6B 内部轴号，默认 0
--cycles N           往返循环次数，默认 1
--distance U         相对移动距离，单位为 user unit
--feed U_PER_SEC     速度
--accel U_PER_SEC2   加速度
--decel U_PER_SEC2   减速度
--reset-position     运动前把当前位置置零
--keep-servo-on      程序正常结束后保持 Servo ON
```

## 调试建议

1. 先运行不带 `--enable-motion` 的干检查。
2. 如果 `ECPWConnect()` 报错，检查 `/usr/lib/ECPL/ENI/ENI.xml` 是否与实际 6 轴顺序一致。
3. 如果 `GetIsSupportMOP(CSP)` 失败，检查 ENI/PDO 和 A6B ESI 是否正确。
4. 如果 Servo ON 失败，检查松下驱动器报警、安全输入、主电源、限位。
5. 如果方向不对，先统一驱动器参数和机械方向，不要在多个软件层反号。
6. 程序异常退出时，`EcpwSession` 析构会调用 `ECPWDisconnect()`，手册说明该动作会 Servo OFF all axes。
