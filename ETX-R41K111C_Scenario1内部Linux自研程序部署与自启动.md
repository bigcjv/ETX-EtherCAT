# ETX-R41K111C 内部 Linux 运行自研程序部署与自启动说明

适用产品：

- 厂商：TPM / Taiwan Pulse Motion / 台湾脉动股份有限公司
- 型号：ETX-R41K111C
- 机身标识：ETX-R41K111C-4P6
- 用途：在 ETX 内部 Linux 上直接运行自研 EtherCAT 运动控制程序
- 从站示例：6 个 Panasonic MINAS A6 EtherCAT 伺服驱动器

参考手册：

- `ETX-R41K111_Series_UserManual_1.0.pdf`
- `ETX-R41K111_Series_ECATNaviManual_1.0.pdf`
- `ETX-R41K111_Series_ProgramingManual_1.0.pdf`

## 1. 你要使用的是哪种方式

ETX-R41K111C 有两种典型使用场景：

| 场景 | 程序运行位置 | 说明 |
|---|---|---|
| Scenario 1 | ETX 内部 Linux | ETX 上电后自己运行控制程序，不依赖 Windows PC |
| Scenario 2 | Windows PC | Windows 程序通过 Ethernet 连接 ETX，适合调试和上位机控制 |

你现在要做的是 Scenario 1：

```text
自研程序运行在 ETX 内部 Linux
ETX 作为 EtherCAT 主站
ETX EtherCAT 口直接控制 6 个 A6 伺服驱动器
上电后 Linux 自动启动你的程序
```

Scenario 1 下开发用到的库：

| 功能 | Linux 库 |
|---|---|
| EtherCAT 运动控制 | `libECPW.so` |
| ETX 本机 DI/DO、编码器、比较输出等 IO | `libetxio.so` |

手册要求 Linux C++ 应用使用 `gcc 12.2.0` 或更高版本。

## 2. 整体流程

推荐按这个顺序做：

1. 先用 ECATNavi/ECATScan 调通 6 个 A6 驱动器。
2. 生成最终可用的 `ENI.xml`。
3. 把 ETX 切换到 Scenario 1。
4. 登录 ETX 内部 Linux。
5. 把 `ENI.xml` 放到 `/usr/lib/ECPL/ENI/`。
6. 在 ETX Linux 上编译自研 C/C++ 程序。
7. 手动运行程序，确认能 `ECPWInit -> ECPWConnect -> ServoON -> Move/Jog/Stop`。
8. 建立 `systemd` 服务。
9. 设置服务开机自启动。
10. 断电重启 ETX，验证程序自动运行。

## 3. 先准备 ENI 文件

自研程序不建议一启动就扫描 EtherCAT 网络。稳定做法是先用 ECATNavi/ECATScan 生成固定 ENI。

### 3.1 在 Windows 上生成 ENI

1. 把松下 A6 ESI 文件放到：

```text
C:\TPM\ECPW\ESI
```

2. 打开 ECATNavi。
3. 点击 `Scan` 打开 ECATScan。
4. 点击 `Reload ESI`。
5. 点击 `Scan`，确认扫描到 6 个 A6 从站。
6. 使用默认 PDO 或按需要配置 PDO。
7. 点击 `Quick Connect` 或 `File -> Export ENI`。
8. 保存最终 ENI 文件，例如：

```text
ENI_6Axis_A6_final.xml
```

Windows 默认 ENI 目录：

```text
C:\TPM\ECPW\ENI
```

### 3.2 在 ETX Linux 上放置 ENI

Scenario 1 下，ECPW 默认从下面目录读取 ENI：

```text
/usr/lib/ECPL/ENI
```

默认文件名：

```text
ENI.xml
```

所以最终建议在 ETX 上放成：

```text
/usr/lib/ECPL/ENI/ENI.xml
```

也可以放成其他名字，例如：

```text
/usr/lib/ECPL/ENI/ENI_6Axis_A6_final.xml
```

但程序里需要在 `ECPWInit()` 之后、`ECPWConnect()` 之前调用：

```cpp
ECPWSetENIFileName((char*)"ENI_6Axis_A6_final.xml");
```

文件必须仍然位于 `/usr/lib/ECPL/ENI/`，这个目录是固定的。

## 4. 切换 ETX 到 Scenario 1

如果当前是 Windows 调试模式 Scenario 2，需要切换到 Scenario 1。

操作步骤：

1. PC 网口连接 ETX 的 Ethernet 口。
2. 打开 `EtxTool`。
3. 输入 ETX 的 IP。
4. 点击 `Current Scenario` 查看当前场景。
5. 点击 `Use S1 Linux-based`。
6. 等待切换完成。
7. 重启 ETX。

注意：

- Scenario 1 是自研程序直接运行在 ETX Linux 上。
- Scenario 2 是 Windows 通过 Ethernet 控制 ETX。
- 切换场景后，调试方式会变化。Scenario 1 下重点是登录 ETX Linux 后本机运行程序。

## 5. 怎么连接 ETX 内部 Linux

手册没有写默认 Linux 用户名和密码。现场需要使用厂商提供账号，或你们已经配置过的 Linux 账号。

常见连接方式有三种。

### 5.1 HDMI + 键盘鼠标直连

ETX-R41K111C 基于 Raspberry Pi OS，机身有 HDMI 和 USB。

步骤：

1. 给 ETX 接 HDMI 显示器。
2. 接 USB 键盘鼠标。
3. 上电进入 Linux 桌面或终端。
4. 用厂商提供账号登录。
5. 打开终端。

这种方式最适合第一次确认系统状态、IP、账号、目录和库文件。

### 5.2 SSH 远程登录

如果 ETX Linux 已开启 SSH，可从 Windows PowerShell 登录：

```powershell
ssh <用户名>@192.168.0.10
```

示例：

```powershell
ssh tpm@192.168.0.10
```

如果 IP 已修改，把 `192.168.0.10` 换成实际 IP。

登录后建议先确认系统和架构：

```bash
uname -a
uname -m
gcc --version
g++ --version
ip addr
```

### 5.3 用 SCP 传文件

从 Windows PowerShell 传程序或 ENI 到 ETX：

```powershell
scp .\ENI_6Axis_A6_final.xml <用户名>@192.168.0.10:/home/<用户名>/
scp .\my_motion_app <用户名>@192.168.0.10:/home/<用户名>/
```

登录 ETX 后再复制到正式目录：

```bash
sudo mkdir -p /usr/lib/ECPL/ENI
sudo cp ~/ENI_6Axis_A6_final.xml /usr/lib/ECPL/ENI/ENI.xml
sudo chmod 644 /usr/lib/ECPL/ENI/ENI.xml
```

## 6. ETX Linux 上的关键目录

根据手册，常用目录如下：

| 内容 | 路径 |
|---|---|
| C/C++ 头文件 | `/home/tpm/ETX-R4K111/include` |
| Linux 示例程序 | `/home/tpm/ETX-R4K111/sample` |
| `libECPW.so` | `/usr/lib` |
| ENI 文件目录 | `/usr/lib/ECPL/ENI` |
| ESI 文件目录 | `/usr/lib/ECPL/ESI` |
| ECPW 日志 | `/var/log/ECPL.log` |
| ECPW 错误日志 | `/var/log/ECPL.err` |

先登录 ETX 后确认这些目录是否存在：

```bash
ls -la /home/tpm/ETX-R4K111
ls -la /home/tpm/ETX-R4K111/include
ls -la /home/tpm/ETX-R4K111/sample
ls -la /usr/lib/libECPW.so
ls -la /usr/lib/ECPL/ENI
```

如果路径不存在，以实际安装目录为准，可用下面命令查找：

```bash
sudo find / -name "libECPW.so" 2>/dev/null
sudo find / -name "*.h" | grep ETX-R4K111
sudo find / -name "ECPSrv_Linux.out" 2>/dev/null
```

## 7. 在哪里编译程序

推荐方案：直接在 ETX 内部 Linux 上编译。

原因：

- 库文件、头文件、运行环境都在 ETX 上。
- 不需要处理 ARM 交叉编译工具链。
- 和最终运行环境一致，最少踩坑。

### 7.1 复制官方 sample 做起点

登录 ETX 后：

```bash
mkdir -p ~/work
cp -r /home/tpm/ETX-R4K111/sample ~/work/etx_sample
cd ~/work/etx_sample
ls -la
```

如果 sample 里已有 Makefile，优先使用原 Makefile：

```bash
make
```

如果 sample 里没有 Makefile，就按 sample 的源文件和 include 路径自己建一个。

### 7.2 建议项目目录

建议你的项目放在：

```text
/home/<用户名>/motion_app
```

例如：

```bash
mkdir -p ~/motion_app/src
mkdir -p ~/motion_app/config
mkdir -p ~/motion_app/log
```

程序部署后也可以统一放到：

```text
/opt/etx-motion
```

正式运行建议：

```bash
sudo mkdir -p /opt/etx-motion/bin
sudo mkdir -p /opt/etx-motion/config
sudo mkdir -p /opt/etx-motion/log
```

## 8. 最小 C/C++ 程序流程

Scenario 1 的初始化流程来自手册：

```text
ECPWInit
ECPWConnect
ECPWGetSlvConf
控制轴 / 读状态 / IO
ECPWDisconnect
ECPWClose
```

其中：

- `ECPWInit()` 会启动实时进程 `ECPSrv_Linux.out`，并建立库和实时进程之间的通信。
- `ECPWConnect()` 会读取 ENI，启动 EtherCAT Master，扫描从站并切到 Operation。
- 如果实际从站数量、顺序或型号与 ENI 不一致，`ECPWConnect()` 会报错。

### 8.1 程序骨架

下面是骨架示例。实际头文件名、结构体初始化方式请以 `/home/tpm/ETX-R4K111/sample` 为准。

```cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

// 头文件名称以 ETX sample 为准
// 例如可能是 ECPW.h、ECPWApi.h 或类似名称
#include "ECPW.h"

static void check_ecp(const char* step, ECP_ERR err)
{
    if (err != 0) {
        std::printf("%s failed, err=%d\n", step, err);
        std::exit(1);
    }
    std::printf("%s ok\n", step);
}

int main()
{
    ECP_ERR err;
    U16 slave_count = 0;

    err = ECPWInit();
    check_ecp("ECPWInit", err);

    // 如果使用默认 /usr/lib/ECPL/ENI/ENI.xml，可以不调用这一句
    // err = ECPWSetENIFileName((char*)"ENI_6Axis_A6_final.xml");
    // check_ecp("ECPWSetENIFileName", err);

    err = ECPWConnect(&slave_count);
    check_ecp("ECPWConnect", err);
    std::printf("connected slave_count=%u\n", slave_count);

    // A6 通常一个驱动器一个轴，axisNo = 0
    // SERIAL 表示按 EtherCAT 物理连接顺序，从 1 开始
    ECP_SLV_TAG axis1_slv;
    std::memset(&axis1_slv, 0, sizeof(axis1_slv));
    axis1_slv.TagType = SERIAL;
    axis1_slv.StationTag = 1;

    ECP_SLV_CONFIG conf;
    std::memset(&conf, 0, sizeof(conf));
    err = ECPWGetSlvConf(&axis1_slv, &conf);
    check_ecp("ECPWGetSlvConf axis1", err);
    std::printf("axis1 AxisNumber=%u VendorID=0x%x ProductCode=0x%x\n",
                conf.AxisNumber, conf.VendorID, conf.ProductCode);

    // 初次测试建议先只做连接和读取配置，不要一启动就自动运动
    // 后续再加入 ClearAlarm / ServoON / ResetPos / MoveVel / Stop / Positioning

    ECPWDisconnect();
    ECPWClose();
    return 0;
}
```

### 8.2 轴号和从站号

手册说明：

- `ECP_SLV_TAG.TagType = SERIAL` 时，`StationTag` 表示 EtherCAT 物理连接顺序。
- 顺序从 `1` 开始。
- `axisNo` 从 `0` 开始。

6 个 A6 驱动器通常对应：

| 物理顺序 | ECP_SLV_TAG | axisNo |
|---:|---|---:|
| 第 1 台 A6 | `SERIAL, StationTag=1` | `0` |
| 第 2 台 A6 | `SERIAL, StationTag=2` | `0` |
| 第 3 台 A6 | `SERIAL, StationTag=3` | `0` |
| 第 4 台 A6 | `SERIAL, StationTag=4` | `0` |
| 第 5 台 A6 | `SERIAL, StationTag=5` | `0` |
| 第 6 台 A6 | `SERIAL, StationTag=6` | `0` |

如果某个从站是多轴模块，才会用 `axisNo=1,2...`。

## 9. 编译方式

### 9.1 优先参考官方 sample

先看官方 sample 怎么编译：

```bash
cd /home/tpm/ETX-R4K111/sample
find . -maxdepth 2 -type f
```

如果里面有 Makefile：

```bash
make clean
make
```

如果 Makefile 里有完整链接参数，直接复制到你的项目。

### 9.2 Makefile 模板

下面是参考模板，最终以 ETX 上 sample 的 Makefile 为准。

```makefile
CXX := g++
TARGET := my_motion_app

INCLUDES := -I/home/tpm/ETX-R4K111/include
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra $(INCLUDES)

LIBS := -L/usr/lib -lECPW -letxio -letx -lpthread -ldl

SRCS := src/main.cpp
OBJS := $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(OBJS) $(TARGET)
```

如果链接报错：

1. 先看 sample 的链接方式。
2. 确认库是否存在：

```bash
ls -la /usr/lib/libECPW.so
ls -la /usr/lib/libetxio.so
ls -la /usr/lib/libetx.so
```

3. 查看库依赖：

```bash
ldd /usr/lib/libECPW.so
```

4. 如果提示找不到库，临时测试可设置：

```bash
export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH
```

## 10. 手动运行测试

### 10.1 确认 ENI

```bash
ls -la /usr/lib/ECPL/ENI/ENI.xml
```

### 10.2 确认 EtherCAT 硬件连接

1. 6 台 A6 驱动器上电。
2. EtherCAT 网线顺序与生成 ENI 时一致。
3. 急停、安全回路、限位处于安全状态。

### 10.3 第一次运行

```bash
cd ~/motion_app
./my_motion_app
```

如果程序需要 root 权限：

```bash
sudo ./my_motion_app
```

### 10.4 查看日志

ECPW 日志路径：

```bash
sudo tail -n 200 /var/log/ECPL.log
sudo tail -n 200 /var/log/ECPL.err
```

程序自己的日志建议写到：

```text
/opt/etx-motion/log
```

## 11. 运动控制程序建议结构

不要把所有逻辑都写在 `main()` 里。建议拆成：

```text
main.cpp
  初始化日志
  读取配置
  初始化 ECPW
  连接 EtherCAT
  建立 6 轴对象
  进入控制循环
  退出时 Stop / ServoOff / Disconnect / Close

axis_controller.cpp
  ClearAlarm
  ServoOn
  ResetPos
  Jog
  MoveAbs / MoveRel
  Stop
  Home

machine_safety.cpp
  急停输入
  限位输入
  驱动器报警
  EtherCAT 断线
  安全停机
```

建议启动后只做：

1. 初始化。
2. 连接 EtherCAT。
3. 读取 6 轴状态。
4. 确认安全输入。
5. 等待外部启动命令。

不要上电后立刻 Servo ON 或运动，除非设备安全链路已经完整验证。

## 12. 开机自动运行

Linux 上推荐用 `systemd` 做开机自启。

### 12.1 部署程序到固定目录

假设你的程序编译结果是：

```text
~/motion_app/my_motion_app
```

部署到：

```bash
sudo mkdir -p /opt/etx-motion/bin
sudo mkdir -p /opt/etx-motion/config
sudo mkdir -p /opt/etx-motion/log

sudo cp ~/motion_app/my_motion_app /opt/etx-motion/bin/
sudo chmod +x /opt/etx-motion/bin/my_motion_app
```

放置 ENI：

```bash
sudo mkdir -p /usr/lib/ECPL/ENI
sudo cp ~/ENI_6Axis_A6_final.xml /usr/lib/ECPL/ENI/ENI.xml
sudo chmod 644 /usr/lib/ECPL/ENI/ENI.xml
```

### 12.2 创建 systemd service

创建服务文件：

```bash
sudo nano /etc/systemd/system/etx-motion.service
```

内容示例：

```ini
[Unit]
Description=ETX-R41K111C Motion Control Application
After=multi-user.target

[Service]
Type=simple
WorkingDirectory=/opt/etx-motion
Environment=LD_LIBRARY_PATH=/usr/lib
ExecStartPre=/bin/sleep 5
ExecStart=/opt/etx-motion/bin/my_motion_app
Restart=on-failure
RestartSec=3

# 如果你的程序必须用 root 访问实时进程或硬件，保持 root 运行。
# 如果已经验证普通用户权限足够，可以取消下面两行注释并改成实际用户。
# User=tpm
# Group=tpm

StandardOutput=append:/opt/etx-motion/log/stdout.log
StandardError=append:/opt/etx-motion/log/stderr.log

[Install]
WantedBy=multi-user.target
```

说明：

- `ExecStartPre=/bin/sleep 5` 用于等待系统和驱动器上电稳定。
- 如果现场 A6 上电比 ETX 慢，可以改成 `10` 或在程序里做连接重试。
- 正式设备建议程序内部实现“等待 EtherCAT 从站就绪”和“连接失败重试”，不要只靠 sleep。

### 12.3 启用自启动

```bash
sudo systemctl daemon-reload
sudo systemctl enable etx-motion.service
sudo systemctl start etx-motion.service
```

查看状态：

```bash
systemctl status etx-motion.service
```

查看日志：

```bash
journalctl -u etx-motion.service -n 200 --no-pager
tail -n 200 /opt/etx-motion/log/stdout.log
tail -n 200 /opt/etx-motion/log/stderr.log
```

### 12.4 停止和禁用自启动

停止当前运行：

```bash
sudo systemctl stop etx-motion.service
```

禁止开机自启：

```bash
sudo systemctl disable etx-motion.service
```

修改服务文件后必须执行：

```bash
sudo systemctl daemon-reload
sudo systemctl restart etx-motion.service
```

## 13. 程序内必须做的安全处理

正式上电自启前，程序至少要处理：

1. `ECPWInit()` 失败：
   - 记录错误码。
   - 不继续运动。
2. `ECPWConnect()` 失败：
   - 检查 ENI 是否存在。
   - 检查从站数量和顺序是否匹配。
   - 检查 EtherCAT 网线和 A6 上电。
3. 连接后校验从站数量：
   - 期望 6 个 A6。
   - 少于 6 个时禁止 Servo ON。
4. Servo ON 前检查：
   - 急停未触发。
   - 限位未异常。
   - 驱动器无报警。
5. 任意轴报警：
   - 立即停止相关轴或全部轴。
   - 记录轴号和错误码。
6. 程序退出：
   - `ECPWStop(..., SMOOTH)` 或 group stop。
   - 必要时 Servo OFF。
   - `ECPWDisconnect()`。
   - `ECPWClose()`。

## 14. 开发阶段推荐验收顺序

不要一开始就上电自动 Servo ON。建议按下面顺序验收：

1. 程序只调用 `ECPWInit()` 和 `ECPWClose()`。
2. 程序调用 `ECPWConnect()`，打印从站数量。
3. 逐个 `ECPWGetSlvConf()`，确认 6 个 A6 顺序正确。
4. 加入读取状态和报警码。
5. 手动命令触发第 1 轴 `ServoON`。
6. 第 1 轴低速 `MoveVel`，1 秒后 `ECPWStop(SMOOTH)`。
7. 第 1 轴小距离 `Positioning`。
8. 扩展到第 2 到第 6 轴。
9. 加入 group，但先只 group stop / group status。
10. 再做 group 运动。
11. 最后才启用 `systemd` 开机自启。

## 15. 常见问题

### 15.1 SSH 连不上

检查：

- ETX 和 PC 是否同一网段。
- ETX IP 是否正确。
- SSH 服务是否启用。
- 用户名和密码是否正确。
- 第一次可用 HDMI + 键盘鼠标进入系统确认。

### 15.2 程序提示找不到 `libECPW.so`

检查：

```bash
ls -la /usr/lib/libECPW.so
ldd ./my_motion_app
```

临时测试：

```bash
export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH
./my_motion_app
```

systemd 中可加入：

```ini
Environment=LD_LIBRARY_PATH=/usr/lib
```

### 15.3 `ECPWConnect()` 失败

常见原因：

- `/usr/lib/ECPL/ENI/ENI.xml` 不存在。
- ENI 与实际从站数量不一致。
- A6 从站顺序变了。
- 某个 A6 未上电。
- EtherCAT IN/OUT 接错。
- 用 Windows 生成 ENI 后没有复制到 ETX Linux。

查看日志：

```bash
sudo tail -n 200 /var/log/ECPL.log
sudo tail -n 200 /var/log/ECPL.err
```

### 15.4 systemd 服务启动失败，但手动运行正常

常见原因：

- 工作目录不同。
- 环境变量不同。
- 权限不同。
- ENI 文件路径不同。
- 驱动器上电比服务启动慢。

排查：

```bash
systemctl status etx-motion.service
journalctl -u etx-motion.service -n 200 --no-pager
```

可以先把 service 的 `ExecStartPre=/bin/sleep 5` 改成：

```ini
ExecStartPre=/bin/sleep 15
```

更好的方式是在程序里对 `ECPWConnect()` 做有限次数重试。

## 16. 最终上线检查表

上线前确认：

- ETX 已切换到 Scenario 1。
- 可以登录 ETX Linux。
- `gcc/g++` 版本满足要求。
- `/home/tpm/ETX-R4K111/include` 存在。
- `/usr/lib/libECPW.so` 存在。
- `/usr/lib/ECPL/ENI/ENI.xml` 是最终 6 轴 ENI。
- 手动运行程序可连接 6 个 A6。
- 程序不会上电立即危险运动。
- 急停和限位能让程序停止运动。
- `systemd` 服务可手动 start/stop。
- `systemd` 服务已 enable。
- 断电重启后程序自动运行。
- `/var/log/ECPL.log` 和 `/var/log/ECPL.err` 可用于排错。

