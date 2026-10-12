# FoloToy AI Passport 定制固件 · 构建与烧录指南

基于 `xiaozhi-esp32` v2.5.0（commit ac6deed）的 AI Passport 改造版源码，覆盖：
显示优化（iOS 电池胶囊 / 默认暗色 / 默认亮度 30% / 表情包替换 / 息屏跳过刷新）+ 省电链（息屏 60s→背光0+LVGL停+CPU降频，**息屏保留麦克风与唤醒词、语音仍可唤醒**，唤醒还原）+ 多 WiFi（纳入）。

本机环境**无 ESP-IDF、无 Docker**，下列命令须在能编译的环境执行（本机装 Docker，或 GitHub Actions 云端）。

---

## 1. 编译通道怎么选（回答你的疑问）

你问的是：**Docker 本地编译占多大空间、有没有别的用；GitHub 云端编译可用吗、会不会不稳？**

| 维度 | Docker 本地编译 | GitHub Actions 云端 |
|---|---|---|
| 安装占用 | `espressif/idf:v6.1` 镜像解压后约 **8–9GB**；加本项目 `build/` 产物约 1.5–2.5GB；WSL2 虚拟盘会动态增长且不自动回收，**建议预留 15GB**。 | 零安装、零磁盘占用 |
| 稳定性 | 稳定（镜像固定 tag v6.1） | 稳定（与项目官方 CI 同款 `espressif/idf:v6.1` 镜像，社区广泛验证） |
| 速度 | 首次拉镜像慢；之后**增量编译秒级~分钟级** | 每次全量构建 + 排队，约 3–6 分钟 |
| 额外能力 | 可 `idf.py monitor` 看串口日志（电量%/省电切换/唤醒）、可本地 GDB | 仅产出 .bin，需下载回本机烧录 |
| 风险 | Windows 路径挂载偶有坑；需给 Docker 分配 ≥4GB 内存 | 需把代码 push 到仓库（可私有）；免费额度足够 |
| 适用 | **打算持续迭代**（你已在反复调表情/参数/功能） | **只想尽快拿到能用的 .bin** |

**结论建议**：你这一版已经多次迭代、且重视看运行日志，**长期用 Docker 更顺手**；但本轮“先把改造固件烧进去验证”，**GitHub Actions 最快最省事且零占用**。两边风险都低——你已有 `G:\张弢\小智AI\` 的整片备份 + 一键还原兜底，编译翻车也能 10 秒回滚。

---

## 2. 方案 A：Docker 本地编译（单板）

```bash
# 1) 拉取官方镜像（固定 v6.1，与 CI 一致）
docker pull espressif/idf:v6.1

# 2) 进入仓库目录，启动容器并挂载源码
#    Windows PowerShell / Git Bash 下把 <REPO> 换成你的 xiaozhi-esp32 绝对路径
docker run --rm -it -v "<REPO>":/project -w /project espressif/idf:v6.1

# 3) 容器内编译 AI Passport 单板（板型 ID 用 folotoy/ai-passport，与 --list-boards 一致）
source $IDF_PATH/export.sh
python scripts/build.py folotoy/ai-passport --name ai-passport

# 产物：build/merged-binary.bin  （固件，烧录到 0x0）
```

> 不确定板型 ID 时，先跑 `python scripts/build.py --list-boards` 确认。

---

## 3. 方案 B：GitHub Actions（已加好工作流，手动触发）

仓库已新增 `.github/workflows/build-ai-passport.yml`（仅 `workflow_dispatch` 手动触发，不会和官方 `build.yml` 的 push 触发重复构建）。

1. 把改动 push 到 GitHub（**建议私有仓库**）。
2. 仓库 → Actions → **Build AI Passport (custom)** → **Run workflow**。
3. 跑完在两个 artifact 里下载：
   - `ai-passport-firmware` → `merged-binary.bin`
   - `ai-passport-assets` → `assets_64_rounded.bin`（定制表情包）

> 该工作流只编译单板，比官方全量矩阵快很多，且复用官方 `v6.1` 镜像，结果可复现。

---

## 4. 烧录

### 4.1 固件（merged-binary.bin → 0x0）

```bash
# COMx 换成设备实际串口号（USB-Serial-JTAG，config.json 已开 CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG）
esptool.py --chip esp32c3 --port COMx --baud 921600 write_flash 0x0 build/merged-binary.bin
```

也可用 xiaozhi.me 的 **Web ESP Launchpad** 直接拖 `merged-binary.bin` 烧录。

### 4.2 定制表情包（assets 分区 → 0x600000）

分区表 `partitions/v2/8m.csv`：`assets, data, spiffs, 0x600000, 2M`。
`assets_64_rounded.bin`（22 情绪重绘，约 1.75MB，已放入板目录）单独烧到该偏移，**与固件解耦、可随时换表情不重刷固件**：

```bash
esptool.py --chip esp32c3 --port COMx --baud 921600 write_flash 0x600000 main/boards/folotoy/ai-passport/assets_64_rounded.bin
```

> 想把它一并打进 `merged-binary.bin`，可在 `config.json` 的 `sdkconfig_append` 加
> `CONFIG_FLASH_CUSTOM_ASSETS=y` 与 `CONFIG_CUSTOM_ASSETS_FILE="boards/folotoy/ai-passport/assets_64_rounded.bin"`，
> 由构建系统自动烧写 assets 分区。默认用“单独烧录”方案，更可逆。

---

## 5. 源码改动清单（均已落地）

| 文件 | 位置 | 改动 |
|---|---|---|
| `main/display/lcd_display.cc` | ~83 行 | 默认主题 `"dark"`（固化为开机默认） |
| `main/boards/common/backlight.cc` | ~36 行 | `RestoreBrightness()` 默认亮度 `75 → 30` |
| `main/boards/folotoy/ai-passport/ai_passport_board.cc` | 顶部 includes + 构造函数 + `SetPowerSaveLevel` | 接入 `PowerSaveTimer`（60s 息屏→背光0+停LVGL+降频，**保留麦克风与唤醒词**）；改用 `AiPassportDisplay`；`SetDisableAudioInSleep(false)` 关闭框架默认的"息屏关麦" |
| `main/boards/common/power_save_timer.h` / `.cc` | 新增 `disable_audio_in_sleep_` 开关（默认 `true`，保持其他板原行为） | 仅 AI Passport 设为 `false`：**息屏不再关闭麦克风与唤醒词引擎**，语音仍可唤醒；CPU 降频/轻睡眠照常生效 |
| `main/boards/folotoy/ai-passport/ai_passport_display.h` / `.cc` | 新增 | iOS 风格白色圆角电池胶囊（内嵌电量%）；息屏跳过状态栏刷新（防 LVGL port 停止时 `DisplayLock` 死锁） |
| `main/boards/folotoy/ai-passport/assets_64_rounded.bin` | 新增 | 22 情绪重绘表情包（烧到 0x600000） |
| `.github/workflows/build-ai-passport.yml` | 新增 | 单板云端构建工作流 |

### 关键设计点
- **息屏防死锁**：`PowerSaveTimer` 进入省电会把 CPU 配成 `min_freq=40` + 开启轻睡眠；此时 LVGL port 被 `lvgl_port_stop()` 停止。原 1 秒时钟节拍仍会每 1s 调 `UpdateStatusBar`→`DisplayLock`(=`lvgl_port_lock`)，若 port 已停会阻塞主线程。因此 `AiPassportDisplay::UpdateStatusBar` 在 `power_save_mode_` 为真时**直接返回**，既省电又避免卡死。
- **电池%数据源**：原代码 `battery_label_` 只被创建、从未写入数值。新显示层用 5s `esp_timer` 轮询 `Board::GetInstance().GetBatteryLevel()`，把 `battery_label_` 复用成白色圆角胶囊显示 `NN%`（CW2017 缺失时显示 `--%`）。
- **显示锁**：所有新增 LVGL 操作均包在 `DisplayLockGuard` 内；板级 `OnEnter/ExitSleepMode` 经 `Application::Schedule` 在主线程执行，与 `sleep_timer` 既有范式一致。
- **语音唤醒保留（路径 B）**：框架默认息屏会 `EnableWakeWordDetection(false)` + `EnableInput(false)`（关麦即关唤醒词，只能按键唤醒）。本板在 `power_save_timer_` 上调用 `SetDisableAudioInSleep(false)` 改为息屏仍听唤醒词——用户说"小智小智"即可唤醒，唤醒词引擎中断触发应用状态变化 → `SetPowerSaveLevel(NORMAL)` → `WakeUp()` 还原亮度与 LVGL。省电幅度略小于"关麦"，但保留语音 UX。

---

## 6. 待你确认 / 后续项

| 项 | 状态 | 处理 |
|---|---|---|
| WiFi `listen_interval` 10→20 + MAX modem sleep | 外部组件 `WifiManager`，本仓库不含源码 | 你倾向“构建期联网拉取后定位改”——按此执行：构建出 `managed_components/` 后改 `wifi_manager` 的 listen interval / modem sleep 参数，再重编 |
| 按键扫描 5→50ms | 外部组件 `button_adc`，本仓库不含源码 | 同上，构建后在其源码里调整扫描间隔 |
| 功放 15s 无声断电 | 硬件 PA 常开（`config.h` 中 `AUDIO_CODEC_PA_PIN = GPIO_NUM_NC`，无控制引脚） | 等你确认 PA 引脚；若无独立控制脚则**无法实现真正断电**，建议放弃此条 |
| 多 WiFi 自动切换 | 取决于 `WifiManager` 是否支持多 AP 列表 | 构建后确认；若支持，在设备网页配置里填多个 WiFi 即可自动切换；不支持则需额外代码 |
| 显示锁（偶发花屏） | 你选“只做防御性加锁” | 已按上表落实 `DisplayLockGuard` + 息屏跳过刷新 |
| 暗色默认 / 亮度 30% / 表情替换 / 息屏 60s 链 | 已实现 | 烧录后实机验证 |

---

## 7. 回滚与风险

- **风险对冲**：`G:\张弢\小智AI\flash_full_backup.bin` + 一键还原.bat 仍在。任何时候 `esptool.py read_flash` 或一键还原即可回到原厂状态。
- 构建翻车（编译错误/烧录异常）：先 `esptool.py` 单独重烧 `merged-binary.bin`；表情不满意只重烧 `0x600000` 的 assets，不动固件。
- 本机**无法验证编译**，以上为基于源码静态分析的改动；首次烧录建议在原备份基础上先烧官方 v2.5.0 确认硬件正常，再烧定制固件。
