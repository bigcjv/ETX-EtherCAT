# ETX-R41K111C 6轴 CSP 同步往返控制程序说明

## 目标

本程序用于 ETX-R41K111C 的 Scenario 1：C++ 程序直接运行在 ETX 内部 Linux 上，通过 TPM `libECPW.so` 控制 6 个 Panasonic MINAS A6B EtherCAT 伺服驱动器。

当前程序使用：

- 使用部署到 ETX `/usr/lib/ECPL/ENI/ENI.xml` 的六轴 EtherCAT 配置。
- `ECPWInit()` / `ECPWConnect()` 初始化主站。
- `ECPWGetSlvConf()` 校验 6 个从站均为 CiA402 驱动器。
- `ECPWGetIsSupportMOP(..., ECP_MOP_CSP)` 检查 CSP 支持。
- `ECPWSetMOP(..., ECP_MOP_CSP)` 将每个轴切到 CSP。
- `ECPWGroupAddAxis()` 将 6 个轴加入 Group 0。
- `ECPWGroupMoveLin()` 让 6 个轴同步做相对往返运动。
- 启动前要求 ETX `Setting.json`、六轴 DC ENI、寄存器写入和 `--cycle-us` 完全一致。
- 读取六台驱动器错误码；异常时通过 RAII 平滑停止、禁用 Group 并 Servo OFF。

程序默认不会运动，必须显式加 `--enable-motion`。

## 2026-07-22 周期实测结论

当前可运行基线仍为 **500 us**，250 us 尚不可用：

- 松下 A6B EtherCAT 手册允许普通 CSP 使用 250 us；全闭环控制不支持 250 us。
- 六轴 250 us 候选 ENI 已通过本地结构校验：六轴 `CycleTime0/1=250000 ns`，
  ESC `0x09A0` 写值均为 `90D0030000000000`，只有 Drive 1 是参考时钟。
- ETX `Setting.json` 改为 250 us 后，ARM64 编译成功，但两次无运动检查均在
  `ECPWInit()` 返回 `6002`（`ECP_ERR_ECAT_SYS`），未连接从站、未 Servo ON、
  未执行位移。
- ECPW 日志均显示：读取到 `Cycle time: 250 us` 后等待 INT0 约 1 秒，SPI 返回
  CRC `0x0`（期望 `0x12345678`），最后 `ECM_InitLibrary fail`。
- 恢复 500 us 后 `ECPWInit()` 重新成功，证明 250 us 的主要阻塞点位于当前 ETX
  主站实时硬件/固件链路，不是运动参数或六台驱动器的 CSP 能力。
- 恢复连接时 Axis 1 报 `0xFF58`，对应松下 `Err88.0`：主电源不足电压保护
  （AC 关闭检测 2）。再次测试前应检查主回路电源、接触器和各相供电，再按现场
  流程清除报警。

因此，在 TPM 确认该 ETX 硬件、ECPW/ECM 固件组合支持 250 us，并解决
SPI/INT0 初始化错误前，禁止执行 250 us 运动命令。未通过的 250 us 候选 ENI
没有保留在仓库，避免误部署。

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

准备 ECATNavi 重新导出 500 us ENI（必须先确认六轴停止并 Servo OFF）：

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 -Password "<ETX_PASSWORD>" -CycleTimeUs 500 -PrepareEniExport -ConfirmAxesStoppedAndServoOff
```

使用当前已验证的 500 us ENI，切换 Scenario 1、部署、编译并做不运动检查：

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 -Password "<ETX_PASSWORD>" -EniPath ".\6axis_eni\ENI_3_fixed_6axis_500us.xml" -CycleTimeUs 500 -ConfigureCycleTime -ActivateScenario1 -Build -RunDryCheck
```

只有 dry check 通过且机械安全全部确认后，才允许默认低速同步往返：

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 -Password "<ETX_PASSWORD>" -EniPath ".\6axis_eni\ENI_3_fixed_6axis_500us.xml" -CycleTimeUs 500 -ActivateScenario1 -Build -RunDryCheck -RunMotion -ConfirmMotionSafety
```

### 将来重新验证 250 us

只有 TPM 确认 ETX 支持并解决 `ECPWInit err=6002` 后，才按以下顺序重新测试：

1. 所有轴停止、Servo OFF，确认主回路供电正常且没有 `Err88.0`。
2. 在 Windows 和 ETX 的 `Setting.json` 中同时设置 `CycleTime=250`、`ENABLE_DC=true`。
3. 在 ECATNavi 中让六轴全部使用 `DC SYNC0 x1`，逐轴确认 250 us，重新导出 ENI。
4. 先部署并执行不运动检查：

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 `
  -Password "<ETX_PASSWORD>" `
  -EniPath ".\6axis_eni\ENI_250us_from_ECATNavi.xml" `
  -CycleTimeUs 250 `
  -ConfigureCycleTime `
  -ActivateScenario1 `
  -Build `
  -RunDryCheck
```

必须看到 6 个从站全部连接、六轴错误码为 0、CSP 准备完成并正常断开；如出现
`6002`、INT0/SPI/CRC、DC、WKC、看门狗、`Err81.*` 或 `Err88.0`，立即停止，
恢复 500 us 配置，不得增加 `--enable-motion`。

## 手动运行

只检查，不运动：

```bash
cd /home/tpm/etx_6axis_csp
sudo ./etx_6axis_csp_demo --eni ENI.xml --axes 6 --cycle-us 500
```

同步低速往返：

```bash
sudo ./etx_6axis_csp_demo --eni ENI.xml --axes 6 --cycle-us 500 --enable-motion --cycles 3 --distance 1000000 --feed 200000 --accel 200000 --decel 200000
sudo ./etx_6axis_csp_demo --eni ENI.xml --axes 6 --cycle-us 500 --enable-motion --cycles 3 --distance 1000000 --feed 4000000 --accel 4000000 --decel 4000000
```

如果将来 250 us 的 dry check 真正通过，运动命令必须显式写周期：

```bash
sudo ./etx_6axis_csp_demo \
  --eni ENI.xml \
  --axes 6 \
  --cycle-us 250 \
  --enable-motion \
  --cycles 3 \
  --distance 1000000 \
  --feed 4000000 \
  --accel 4000000 \
  --decel 4000000
```

当前 ETX 不得执行上述 250 us 运动命令；它仅用于厂商问题解决后的验收。

## 常用参数

```text
--axes N             轴数，默认 6
--first-station N    第一台驱动器 EtherCAT serial station，默认 1
--axis-no N          每个 A6B 内部轴号，默认 0
--cycle-us US        必须与主站和六轴 DC ENI 一致，默认 500
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
6. 发现 `0xFF58`/`Err88.0` 时先检查主回路 AC 电源和接触器，不要直接继续运动。
