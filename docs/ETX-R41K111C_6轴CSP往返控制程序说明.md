# ETX-R41K111C 6轴 CSP 同步往返控制程序说明

## 目标

本程序用于 ETX-R41K111C 的 Scenario 1：C++ 程序直接运行在 ETX 内部 Linux 上，通过 TPM `libECPW.so` 控制 6 个 Panasonic MINAS A6B EtherCAT 伺服驱动器。

当前程序使用：

- `6axis_eni/ENI.xml` 连接 6 轴 EtherCAT 网络。
- `ECPWInit()` / `ECPWConnect()` 初始化主站。
- `ECPWGetSlvConf()` 校验 6 个从站均为 CiA402 驱动器。
- `ECPWGetIsSupportMOP(..., ECP_MOP_CSP)` 检查 CSP 支持。
- `ECPWSetMOP(..., ECP_MOP_CSP)` 将每个轴切到 CSP。
- `ECPWGroupAddAxis()` 将 6 个轴加入 Group 0。
- `ECPWGroupMoveLin()` 让 6 个轴同步做相对往返运动。

程序默认不会运动，必须显式加 `--enable-motion`。

## 为什么之前看起来不同步

第一版程序是逐轴调用：

```cpp
ECPWPositioning(axis1)
ECPWPositioning(axis2)
...
ECPWPositioning(axis6)
```

并且 `cbConfig=NULL`，等价于 `WAIT_COMPLETE`。这意味着 Axis1 完成后才启动 Axis2，Axis2 完成后才启动 Axis3，所以它不是严格同步运动。

当前版本改为：

```cpp
ECPWGroupMoveLin(group0, ...)
```

一次命令同时下发 6 个轴的位置数组，才是同步往返。

## 为什么 `distance=100000` 动得不明显

你的命令：

```bash
sudo ./etx_6axis_csp_demo --eni ENI.xml --axes 6 --enable-motion --cycles 3 --distance 100000 --feed 4000000 --accel 4000000 --decel 4000000
```

从输出看，每次位置确实变化了约 `100000` user unit，说明电机在执行命令。

但肉眼不明显通常有两个原因：

- A6B 的用户单位可能接近编码器 count，若一圈是几百万 count，`100000` 只是一小段角度。
- `feed=4000000` 太快，运动时间很短，肉眼可能只看到轻微抖动或一下就结束。

首次观察建议降低速度并增加位移，例如：

```bash
sudo ./etx_6axis_csp_demo --eni ENI.xml --axes 6 --enable-motion --cycles 3 --distance 1000000 --feed 200000 --accel 200000 --decel 200000
```

如果机械没有负载且确认安全，也可以逐步增加 `distance`。

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

## 编译

ETX Linux 上需要存在：

```text
/home/tpm/ETX-R4K111/include
/usr/lib/libECPW.so
/usr/lib/ECPL/ENI/ENI.xml
```

编译：

```bash
cd /home/tpm/etx_6axis_csp
make clean
make
```

如果直接运行出现：

```text
ECPWInit err=10
```

表示 `ECP_ERR_PERMISSION_DENIED`，需要用 `sudo` 运行。

## Win11 部署到 ETX

部署并编译：

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 -Password "123" -Build
```

部署、编译并做不运动检查：

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 -Password "123" -Build -RunDryCheck
```

部署、编译并运行默认低速同步往返：

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 -Password "123" -Build -RunMotion
```

## 手动运行

只检查，不运动：

```bash
cd /home/tpm/etx_6axis_csp
sudo ./etx_6axis_csp_demo --eni ENI.xml --axes 6
```

同步低速往返：

```bash
sudo ./etx_6axis_csp_demo --eni ENI.xml --axes 6 --enable-motion --cycles 3 --distance 1000000 --feed 200000 --accel 200000 --decel 200000
sudo ./etx_6axis_csp_demo --eni ENI.xml --axes 6 --enable-motion --cycles 3 --distance 1000000 --feed 4000000 --accel 4000000 --decel 4000000
```

## 常用参数

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

## 安全建议

1. 先运行不带 `--enable-motion` 的检查。
2. 第一次运动使用低速度和小位移。
3. 确认急停、限位、安全回路有效。
4. 逐轴确认方向后，再运行 6 轴同步。
5. 如果方向不对，优先修改驱动器/机械方向参数，不要在多层软件里反号。

