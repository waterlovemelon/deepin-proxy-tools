# 0.2.0 V25 版本验证状态

本文件取代控件版时期的 0.1.5 自测清单（原 `QA-0.1.5.md`）。控件版页面、预览程序与
Qt 5 依赖已随 0.2.0 删除，其中与后台、内核和用户数据相关且仍然成立的验证点保留在
`TESTING.md` 相应章节。

目标环境（本机）：dde-control-center 6.1.104、Qt 6.8.0、DTK 6.7.47。
0.2.0 已在本机完成整包构建、CTest、插件加载与逐页导航、dccData 数据通路、
systemd 生命周期、JS 覆写子进程、Debian 出包与安装、安装后加载，以及后台真实内核
冒烟测试（含 TUN/路由/DNS 与真实桌面代理联动；命令与输出见 `TESTING.md` 的
“本轮结果”与“安装包验证”）。下表区分“本轮实际观察到的事实”与“本轮未运行、
不得视为通过”的项目。内核使用本机可用的 Mihomo Meta v1.19.32 代替随包 v1.19.16
（随包内核在本机下载受阻，见“未运行”表），后台与插件代码路径相同。

## 已验证（本轮实际观察）

| 项目 | 观察结果 | 观察方式 |
| --- | --- | --- |
| 控制中心版本 | dde-control-center 6.1.104 | `dpkg-query -W dde-control-center` |
| Qt 版本 | 6.8.0（Core / Gui / Network / DBus / Qml / Quick） | `pkg-config --modversion Qt6…` |
| DTK 版本 | libdtk6declarative 6.7.47.1、libdtk6gui / libdtk6core 6.7.47 | `dpkg-query -W` |
| 插件 ABI 目录 | `DDE_CONTROL_CENTER_PLUGIN_DIR=plugins_v1.1`，安装目录 `/usr/lib/x86_64-linux-gnu/dde-control-center/plugins_v1.1` | 读取开发包 `DdeControlCenterConfig.cmake` |
| 构建树插件目录 | `<build>/lib/plugins_v1.1/<name>/`，内含 `qmldir` 与两个模块 `.so` | 读取 `DdeControlCenterPluginMacros.cmake` |
| 插件命名约束 | 插件名必须匹配 `[A-Za-z0-9]+`；`qml/*.qml` 必须首字母大写，`main.qml` 被拒绝并提示改名 | 同上 |
| 翻译工具 | `lupdate`、`lrelease` 位于 `/usr/lib/qt6/bin/`，由 **qt6-l10n-tools** 提供；`Qt6LinguistTools` 的 CMake 文件在本发行版由 **qt6-tools-dev** 提供 | `dpkg -S` |
| 开发文件接口 | `Dde::Control-Center` 是 `IMPORTED SHARED` 目标，接口仅含 `/usr/include/dde-control-center`（提供 `dccfactory.h`），不需要 DTK 开发头文件 | 读取 `DdeControlCenterTargets.cmake` |
| QML 模块来源 | `org.deepin.dtk` ← libdtk6declarative；`QtQuick` ← qml6-module-qtquick；`QtQuick.Controls` ← qml6-module-qtquick-controls；`QtQuick.Layouts` ← qml6-module-qtquick-layouts；`QtQuick.Window` ← qml6-module-qtquick-window；`QtQuick.Dialogs` ← qml6-module-qtquick-dialogs | `dpkg -S`、`dpkg -s` |
| 依赖完整性 | dde-control-center 声明了 layouts / window / dialogs / effects，但未声明 qtquick 与 qtquick-controls，因此插件包显式依赖这两个模块 | `dpkg -s dde-control-center` |
| SVG 图标渲染 | Qt 的 SVG 图像插件由 **libqt6svg6** 提供；dde-control-center 与 libdtk6declarative 均未声明该依赖，故插件包显式依赖它 | `dpkg -S …/imageformats/libqsvg.so`、`dpkg -s`、`ldd` |
| qdbus 工具 | `/usr/bin/qdbus` 只是 qtchooser 包装（本机无 qt6 profile）；Qt 6 的 `qdbus` 是 `/usr/lib/qt6/bin/qdbus`（**qdbus-qt6** 6.8.0） | `type -a qdbus`、`dpkg -S` |
| 后台与单元路径 | daemon 安装到 `lib/dcc-proxy`（`/usr/lib/dcc-proxy/dde-mihomo-daemon`），单元安装到 `lib/systemd/user` | 读取 `CMakeLists.txt` |
| 插件不依赖 QtWidgets | `src/plugin.cpp` 只包含 `QtCore/QtNetwork/QtQml`、`dccfactory.h` 与 `plugindata.h` | 源码检查 |
| 冒烟脚本与旧预览程序 | `tests/*.py` 中已无 `--preview`、`dcc-proxy-preview` 或 `modules/` 安装路径引用；`smoke_system_proxy.py` 改为优先调用 `/usr/lib/qt6/bin/qdbus` | 源码检查 |
| 整包构建 | 全部目标构建成功，`-Wall -Wextra -Werror` 无警告；插件、后台与测试可执行文件均产出 | `cmake -S . -B build … && cmake --build build -j4` |
| CTest | 1/1 通过（`party-tests`，8 项 dccData 契约检查） | `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure` |
| 插件加载与页面树 | 状态走到 `PluginEnd`，无 `undefined symbol`、无跨线程对象警告 | 控制中心 `--spec` 命令（见 `TESTING.md`） |
| 逐页内容与导航 | 私有总线 `ShowPage("dccproxy/<page>")` 打开 11 个页面：全部命中、页面内容全部实例化、插件 QML 零诊断；由此发现并修复 6 处 `checked`/`text` 双向写回绑定循环 | 见 `TESTING.md`“逐页打开页面内容” |
| 页面图标资源 | 模块与 11 个页面图标请求全部命中插件资源，无 `Cannot open` | 同上运行的日志 |
| dccData 数据通路 | 假控制器收到 `/configs`、`/proxies`、`/providers/proxies`、`/rules`、`/providers/rules`、`/connections`、`/logs` 请求，各页模型与日志流均在主线程工作 | 假控制器 + 逐页 `ShowPage` 运行 |
| systemd 生命周期 | 用户级单元的 enable/start/stop/disable 与 socket 状态读取全部按预期工作，停止后无残留进程（临时单元已清理） | 临时单元 + 直接调用 `Party::Service` 的临时程序 |
| JS 覆写子进程 | DNS prepend、中文订阅名、`includes` 原生语义与错误文本均符合预期 | `dde-mihomo-daemon --eval …` |
| 真实内核集成与三种模式 | `smoke_real_core.py`、`smoke_modes.py`、`smoke_private_proxy.py --private` 全部通过（内核为 Mihomo Meta v1.19.32） | 见 `TESTING.md` 命令 |
| Debian 出包 | `dpkg-buildpackage -us -uc -b` 成功，产出 `dcc-proxy_0.2.0_amd64.deb`（1.2 MB）与 `-dbgsym`；`Depends` 由 shlibdeps 生成并含 `dde-control-center (>= 6.1.104)` 与所需 QML 模块 | `dpkg-deb -I`、`dpkg-deb -c` |
| 包内布局与权限 | `plugins_v1.1/dccproxy/{dccproxy.so,libdccproxy_qml.so,qmldir}`、`/usr/lib/dcc-proxy/dde-mihomo-daemon`、`/usr/lib/systemd/user/dde-mihomo-daemon.service`、`/usr/share/dde-control-center/translations/v1.1/dccproxy_*.qm`；均为 `root:root` | `dpkg -L dcc-proxy`、`dpkg-deb -c` |
| 安装后加载（正式路径） | `dpkg -i` 后不加 `--spec` 启动控制中心：`dccproxy` 走到 `MainObjEnd`/`PluginEnd`，插件 QML 零诊断、无缺失图标；翻译器告警消失（安装的 `.qm` 被找到） | 私有会话总线运行已安装控制中心 |
| 安装后 systemd | 已安装单元 `Loaded`/`disabled`；`systemctl --user start` 后 `active (running)` 且有 MainPID，`--command status` 可读；`enable` 在 `~/.config/systemd/user/default.target.wants/` 建链、`disable` 删除；`stop` 后 `inactive`、无残留进程 | `systemctl --user …`、`/usr/lib/dcc-proxy/dde-mihomo-daemon --command status` |
| 安装后真实内核 | 用已安装 daemon 重跑 `smoke_real_core.py`、`smoke_modes.py`、`smoke_private_proxy.py --private` 全部通过 | 同前，路径改为 `/usr/lib/dcc-proxy/dde-mihomo-daemon` |
| TUN / 路由 / DNS 劫持 | `smoke_tun.py` 全部通过：授权后重启获得 CAP_NET_ADMIN、Mixed TUN + 自动路由 + DNS 劫持与 fake-IP HTTP、MTU/设备冲突保留现有内核、唤醒修复、关闭后恢复全部路由/规则、权限失败回滚 | `unshare --user --map-root-user --net python3 tests/smoke_tun.py …` |
| 真实桌面代理联动 | `smoke_system_proxy.py --allow-system-proxy` 通过；运行前后用 qdbus 抓取的桌面代理状态完全一致（`diff` 无差异），确认临时改动已恢复 | 见 `TESTING.md` |
| 图标集 | 12 个 `.dci`（模块 + 11 页）按控制中心符号图标风格重绘，含 light/dark 两套调色板，由 `dci-icon-theme` 打包；加载日志无图标 provider 失败、无 `Cannot open` | `dci-icon-theme`、控制中心加载日志、截图 |
| 界面风格 | 11 个页面全部按控制中心风格重排：`DccTitleObject` 分节、`DccGroupView` 分组卡片、Editor 行对齐、右对齐操作行（主操作用 `RecommandButton`）、表格 `TableHeader` + 细分隔线 + 选中/hover、空状态与提示；截图逐页复核 | Xvfb + 私有总线截图（`TESTING.md`） |
| 空闲占用 | 修复前：真实数据下模块页 70–85%、代理组页 102%、预览模式 48%；修复后：模块页 0.4%、概览 0.0%、代理组（稳定）0.2%、日志页 0.1%、预览模式 0.2%（代理组首次渲染一次性约 33%） | 见 `TESTING.md`“空闲占用回归” |
| 深色主题 | QML 未使用任何硬编码颜色，界面颜色全部取自 `palette.*`（highlight 37 处、windowText 21 处、base 3 处） | `grep` 审计 |

## 未运行（本轮未执行，不得视为通过）

| 项目 | 未运行原因与运行方式 |
| --- | --- |
| 页面交互（点击开关、节点卡片选择/测速、表单校验提示、文件选择对话框的实际操作） | 需要带显示的桌面会话：重新打开控制中心后逐页操作；本轮已验证页面内容实例化、数据通路与全部后台行为，未做人工交互 |
| 界面视觉确认（页面排版、卡片网格、长提示布局、深色主题） | 同上；本轮确认图标资源命中与 QML 零诊断，未做视觉确认 |
| 离线 GEO 数据 | 官方内核已按锁定版本随包（`95a8bd10…a062c`，`root:root 4755`），但本机到 `raw.githubusercontent.com` 持续超时，`cores/geodata` 仍无法准备；运行时改用本地数据库目录（无清单时按结构校验播种）。需在可下载环境执行 `tools/prepare-geodata.py` 后重新出包，并复跑 `smoke_geodata.py` |

未运行的项目在整包集成验证完成前不计作已交付；`TESTING.md` 0.2.0 一节与本表同时更新。
