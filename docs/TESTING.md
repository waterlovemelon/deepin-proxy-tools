# 验证记录

本文件记录各版本的验证方式与实际结果。0.1.x 各章对应移植前的 Qt 5 控件版
（`interface/moduleinterface.h` widget 插件）：其中的 `modules/` 安装路径、
`dcc-proxy-preview` 程序和 `QT_SELECT=qt5` 命令已不再适用，仅作为历史记录保留。
最新一轮是 0.2.0 的 V25 版本（Qt 6 / DTK 6 QML 插件），见下节。

## 0.2.0 V25 QML 插件（Qt 6 / DTK 6）

目标环境（本机已安装）：dde-control-center 6.1.104、Qt 6.8.0、DTK 6.7.47。
插件按 `plugins_v1.1` ABI 安装：`dccproxy.so`、`libdccproxy_qml.so`、`qmldir`
在控制中心插件目录 `…/dde-control-center/plugins_v1.1/dccproxy/`，构建树中位于
`build/lib/plugins_v1.1/dccproxy/`。后台仍为
`/usr/lib/dcc-proxy/dde-mihomo-daemon`，随包内核 `/usr/lib/dcc-proxy/mihomo`，
登录自启由用户级单元 `dde-mihomo-daemon.service` 控制。

### 构建

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

本机已安装 `dde-control-center-dev`、`debhelper` 与 Qt 6 开发包，因此不需要
`-DCMAKE_PREFIX_PATH`；开发文件放在非 `/usr` 前缀时再指向该前缀。C++ 侧不链接
QtWidgets，页面全部由 QML 构成。

### 插件独立加载（无需安装）

```sh
unshare --user --map-root-user --fork --pid dbus-run-session -- \
  env QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  /usr/libexec/deepin/dde-control-center --spec ./build/lib/plugins_v1.1/
```

模块状态行 `"dccproxy" : status … "PluginEnd"` 表示插件加载与析构正常。
页面树为 overview / proxies / profiles / connections / rules / providers /
logs / override / tools / settings / tun 共 11 个页面。控制中心关闭后后台
与内核继续运行，此命令同时用于验证“关闭界面不影响内核”。

### 后台与内核冒烟测试

```sh
python3 tests/smoke_real_core.py \
  --service /usr/lib/dcc-proxy/dde-mihomo-daemon \
  --core /usr/lib/dcc-proxy/mihomo --expect-root
```

0.2.0 已删除 `--preview`：控件版预览程序随 Qt 5 代码一并移除，插件界面改由上面的
控制中心加载命令验证。`smoke_real_core.py` 其余断言（首个订阅自动启动、DIRECT /
REJECT 真实流量、无效配置与占用端口保留现有内核、混合/HTTP/SOCKS 端口变更与
TCP/UDP 冲突、订阅更新、崩溃恢复、停止与后台退出释放内核）保持原样。
`smoke_modes.py`、`smoke_tun.py`、`smoke_geodata.py`、`smoke_private_proxy.py`
与 `smoke_system_proxy.py` 通过 `--service` / `--core` 指向构建产物，与 Qt 版本无关。

### 本轮结果（实际执行并观察）

整包构建、CTest、插件加载、页面导航、dccData 数据通路、systemd 生命周期与后台冒烟测试
均已在本机执行，结果如下；未执行的项目列在 `QA-V25.md` 的“未运行”表中。

| 项目 | 命令 | 结果 |
| --- | --- | --- |
| 全量构建 | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j4` | 通过，`-Wall -Wextra -Werror` 无警告；产出 `build/dde-mihomo-daemon`、`build/lib/plugins_v1.1/dccproxy/{dccproxy.so,libdccproxy_qml.so,qmldir}`、`dccproxy_zh_CN.qm`/`dccproxy_en_US.qm` |
| CTest | `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure` | 1/1 通过（`party-tests`：8 项 `dccData` 契约检查，覆盖模型角色名、延迟文案、容量格式化、分区示例 YAML、订阅与文档控制器） |
| 插件加载 | 上面的 `--spec` 命令 | 状态走到 `PluginEnd`（`MetaDataEnd → ModuleEnd → DataEnd → MainObjCreate → MainObjAdd`），无 `undefined symbol`、无跨线程对象警告 |
| 页面树构建 | `--spec` 加载（`-m/-p` 在 `--spec` 模式下不生效，仅构建页面树） | `DccproxyMain.qml` 与 11 个页面对象全部创建，无 `TypeError`、`ReferenceError`、`Unable to assign` |
| 图标资源 | 上面两次运行的标准错误 | 无 `Cannot open`；`proxy.svg` 与 `icons/*.svg` 均由插件资源解析（DCI provider 回退到 QIcon 路径），页面与模块图标请求全部命中 |
| 11 页导航与页面内容 | 通过私有会话总线调用 `ShowPage("dccproxy/<page>")` 逐页打开：11 次导航全部命中，页面内容（编辑器、开关、列表、对话框组件）全部实例化，插件 QML 文件零诊断 | 私有 `dbus-daemon` + `qdbus … ShowPage`，`--spec` 运行（见下方脚本） |
| 绑定循环修复 | 首次实例化页面内容时发现 6 处 `checked: <属性>` + `onCheckedChanged: <属性> = checked`（以及同类 `text`）双向写回触发绑定循环；改为“值不同才写回”后 11 页零诊断 | 同上运行的警告对比 |
| dccData 数据通路（全页） | 假控制器记录到 16×`GET /configs`、8×`GET /proxies`、2×`GET /providers/proxies`、1×`GET /rules`、1×`GET /providers/rules`、1×`GET /connections`、1×`GET /logs`，即设置读取、`Api` 配置、3 秒轮询、各页模型与日志流都在主线程正常工作 | 假控制器 + 逐页 `ShowPage` 运行 |
| systemd 生命周期 | 临时用户单元（`dde-mihomo-daemon.service`，指向构建产物）＋ 直接调用 `Party::Service` 的临时程序 | `unitAvailable` 由 false→true、`setEnabled(true)` 后 `unitEnabled=true`（`systemctl --user enable`）、`start()` 经 systemd 起后台并连上 socket（`daemonReachable=true`，返回后台错误文本并只提示一次）、`stop()` 后 socket 断开、`setEnabled(false)` 复位；`systemctl --user is-active` 为 `inactive`，无残留进程，临时单元已删除 |
| JS 覆写 | `dde-mihomo-daemon --eval /tmp/override.js 中文订阅` | DNS prepend 保留原顺序且不重复插入、中文订阅名传入第二参数、`includes` 的 NaN/负索引/Infinity/类数组语义为 Qt 6 引擎原生实现；`return null` 与返回 Promise 分别报“必须返回配置对象”与“必须同步返回” |
| 真实内核集成（随包官方内核） | `python3 tests/smoke_real_core.py --service /usr/lib/dcc-proxy/dde-mihomo-daemon --core /usr/lib/dcc-proxy/mihomo --expect-root`：`PASS official core: v1.19.16`、`desktop user daemon starts a root core with TUN disabled`、DIRECT/REJECT 真实流量、端口/订阅/崩溃恢复、停止释放 | 全部通过（SHA-256 `95a8bd10…a062c`，`root:root 4755`）；同一订阅在 v1.19.16 上启动、控制面连接、混合端口真实转发均通过 |
| 真实内核集成（历史，v1.19.32） | `python3 tests/smoke_real_core.py --service build/dde-mihomo-daemon --core <本机 Mihomo Meta v1.19.32>` | 全部通过：首个订阅自动启动、DIRECT/REJECT 真实流量与请求头保持、无效配置保留运行内核、端口占用报错、运行时模式切换、端口变更生效、TCP/UDP 与控制器端口冲突处理、离线端口保存、订阅刷新使用 Mihomo 兼容 UA（`mihomo/1.19.16`，403/404 时回退 `Mozilla/5.0` 重试一次）、内核异常退出自动恢复、停止与后台退出释放内核 |
| 三种模式真实路由 | `python3 tests/smoke_modes.py …` | 通过：规则模式按规则独立选择 proxy/DIRECT/REJECT、全局模式覆盖规则、直连模式绕过 REJECT 与分组选择、切回规则模式恢复 |
| 私有总线代理联动 | `GSETTINGS_BACKEND=memory python3 tests/smoke_private_proxy.py --private …` | 通过：端口变更同步已启用的手动/PAC 代理、冲突拒绝、启动后自动启用、校验失败保留原代理、重启更新端口、停止恢复、绕过列表编辑与空列表、非法输入 |

补充说明：真实主机桌面代理联动测试（`smoke_system_proxy.py --allow-system-proxy`）
会临时改写当前会话的桌面代理设置，本轮未执行；只读确认了本机
`org.deepin.dde.Network1` 与 `com.deepin.daemon.Network` 两套接口与对象路径均存在
（`GetProxyMethod=auto`），即测试内使用的接口探测逻辑在本机成立。

### 安装包验证（Debian 出包与安装后行为）

本机已安装 `DCC_PROXY_BUNDLE_CORE` / `DCC_PROXY_BUNDLE_GEODATA` 依赖的构建依赖
（`debhelper`、`dde-control-center-dev`），出包与安装命令：

```sh
DCC_PROXY_BUNDLE_CORE=0 DCC_PROXY_BUNDLE_GEODATA=0 dpkg-buildpackage -us -uc -b
sudo dpkg -i ../dcc-proxy_0.2.0_$(dpkg --print-architecture).deb
```

随包内核与离线数据库需先联网准备缓存；本机下载受阻，因此上述命令显式关闭了两者，
安装的是使用现有内核的包（可在“设置 → 内核路径”指定内核）。

| 项目 | 结果 |
| --- | --- |
| 包内容 | `plugins_v1.1/dccproxy/{dccproxy.so 2.8 MB, libdccproxy_qml.so 472 KB, qmldir}`、`/usr/lib/dcc-proxy/dde-mihomo-daemon`、`/usr/lib/systemd/user/dde-mihomo-daemon.service`、`/usr/share/dde-control-center/translations/v1.1/dccproxy_{zh_CN,en_US}.qm`，全部 `root:root`；包体 1.2 MB |
| 依赖 | `dh_shlibdeps` 生成 Qt6/glibc/libstdc++ 依赖，另含 `dde-control-center (>= 6.1.104)`、`libdtk6declarative`、`libqt6svg6` 与 4 个 `qml6-module-qtquick*` |
| 剥离 | 仅排除官方内核（`--exclude=usr/lib/dcc-proxy/mihomo`）；插件与后台正常剥离，调试符号进 `-dbgsym`。此前误把插件也排除，导致 `dccproxy.so` 达 19 MB，已修正 |
| 安装后加载 | 不加 `--spec` 从正式路径加载：`dccproxy` 走到 `MainObjEnd`/`PluginEnd`，插件 QML 零诊断、无缺失图标，翻译器告警消失（安装的 `.qm` 被找到） |
| 安装后 systemd | `systemctl --user daemon-reload` 后单元 `Loaded`、`disabled`；`start` → `active (running)` 且有 MainPID，`dde-mihomo-daemon --command status` 返回 JSON；`enable` 在 `~/.config/systemd/user/default.target.wants/` 建链，`disable` 删除；`stop` → `inactive`，无残留进程 |
| 安装后真实内核 | 以 `/usr/lib/dcc-proxy/dde-mihomo-daemon` 重跑 `smoke_real_core.py`、`smoke_modes.py`、`smoke_private_proxy.py --private` 全部通过 |
| TUN | `unshare --user --map-root-user --net python3 tests/smoke_tun.py --host-netns "$(readlink /proc/self/ns/net)" --service /usr/lib/dcc-proxy/dde-mihomo-daemon --core <本机内核>` 全部通过（授权后重启获得 CAP_NET_ADMIN、Mixed TUN 自动路由、DNS 劫持与 fake-IP HTTP、MTU 与设备冲突保留内核、唤醒修复、关闭恢复路由、权限失败回滚） |
| 真实桌面代理 | `python3 tests/smoke_system_proxy.py --service /usr/lib/dcc-proxy/dde-mihomo-daemon --core <本机内核> --allow-system-proxy` 通过；运行前后用 `qdbus … GetProxyMethod/GetProxyIgnoreHosts/GetAutoProxy/GetProxy` 抓取的状态 `diff` 完全一致 |

安装后需要重新打开控制中心，侧栏才会出现“私人代理”（已运行的实例不会热加载新插件）。

### 界面与图标验证

页面样式与图标在无显示环境下通过 Xvfb + 私有会话总线逐页截图复核（不触碰桌面会话）：

```sh
Xvfb :99 -screen 0 1440x1000x24 -ac -nolisten tcp &
xvfb=$!
dbus-run-session -- bash -c '
  TMPDIR=$(mktemp -d) DISPLAY=:99 QT_QPA_PLATFORM=xcb QT_QUICK_BACKEND=software \
    /usr/libexec/deepin/dde-control-center --show --spec "$1" &
  dcc=$!
  sleep 8
  # 逐页导航并截取窗口
  /usr/lib/qt6/bin/qdbus org.deepin.dde.ControlCenter1 /org/deepin/dde/ControlCenter1 \
    org.deepin.dde.ControlCenter1.ShowPage dccproxy/settings
  sleep 1.2
  xwd -silent -id $(xdotool search --class dde-control-center | head -1) | xwdtopnm | pnmtopng > shot.png
  kill $dcc
' -- <插件目录>
kill $xvfb
```

私有总线是必需的：`TMPDIR` 隔离了 DTK 的单实例锁，但控制中心仍会在会话总线上注册
`org.deepin.dde.ControlCenter1`，与会话中已运行的控制中心冲突。

用 `dbus-run-session` 而不是 `dbus-daemon --session --fork`：后者 `--fork` 后不会随命令退出，
每次验证都会留下一套总线及其激活的 `xdg-desktop-portal`/`xdg-permission-store`/`gvfsd`；
累积约 250 个 X 客户端后 Xorg 报 `Maximum number of clients reached`，控制中心将无法建连。

图标：`qml/*.dci` 由 `dci-icon-theme` 从 24×24 网格的符号图形生成（light/dark 两套调色板），
打包进 QML 模块资源；页面以图标名（`overview`、`proxies` … `proxy`）引用，
加载日志中既无 `Failed to get image from provider` 也无 `Cannot open`。

本轮结果：11 个页面逐页截图复核，加载日志零 QML 诊断；`DccTitleObject` 分节、
分组卡片、控件对齐、右对齐操作行、表格分隔线与空状态均符合控制中心版式。
深色主题未在会话中切换验证，但 QML 不含硬编码颜色，全部取自 `palette.*`。

### 空闲占用回归（控制中心 CPU）

用户反馈控制中心占用高。复现与定位：用**真实用户数据目录**（62 节点 / 22 策略组 / 内核运行中）
在 Xvfb 中启动控制中心，逐项测量主进程 CPU（`/proc/<pid>/stat` 增量）：

| 配置 | 修复前 | 修复后 |
| --- | --- | --- |
| 空数据目录（基线） | 0.2% | 0.2% |
| 用户数据目录 + 模块页（控制器可达） | 70–85% | 0.4% |
| 用户数据目录 + 概览页 | 74% | 0.0% |
| 用户数据目录 + 代理组页（稳定后） | 102% | 0.2%（首次渲染一次性约 33%） |
| 用户数据目录 + 日志页 | — | 0.1% |
| 订阅预览模式（控制器不可达/密钥不符） | 48% | 0.2% |
| 仅订阅、无 settings | 47.9% | 0.2% |

逐键二分定位到触发组合：`active` + `endpoint` + `secret` 同时存在时必现（单独任意键均 <1%）——
即"已配置订阅 + 控制器可连"。此时每 3 秒把所有策略组/节点模型整体重建一次：
运行时 `/proxies` 的健康检查历史令数据指纹每轮都变化；预览模式（控制器不可达或密钥不符）
则在"指纹未变"分支里仍调用 `rebuild()`。两种路径都会重置 22 个分组模型与 62 个节点模型，
使屏幕上 22 张分组卡片、62 张节点卡片（含国旗 SVG 解码）被反复重建。

修复：

1. 模块把控制中心当前页（`DccApp.activeObject`）上报给 `dccData`；离开本插件页面时
   停止 systemd 查询、控制器轮询与日志流，回到页面立即刷新一次；
2. 运行时数据按"界面真正显示的字段"计算指纹（名称/类型/当前节点/可用性/延迟/成员列表），
   指纹不变就不重建模型；
3. 连接列表的 3 秒轮询只在连接页显示时运行；日志流只在日志页打开；
4. `NodeModel::reset` / `GroupModel::updateRow` 在行内容一致时不发信号，`rebuild()` 先比较
   "数据 + 视图状态（搜索/排序/隐藏/展开/测速结果/当前组）"签名，一致则整轮跳过；
5. 日志行改用 `D.Label`（超长行仍截断显示，但悬停可看完整内容）。

### 逐页打开页面内容（无显示环境）

`--spec` 模式下 `-m/-p` 不会触发子页面导航，页面内容也不会实例化。要在无显示环境
实际打开每个页面，用私有会话总线调用控制中心的 `ShowPage`：

```sh
unshare --user --map-root-user --fork --pid dbus-run-session -- bash -c '
  export XDG_RUNTIME_DIR=$(mktemp -d)
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    /usr/libexec/deepin/dde-control-center --spec "$PWD/build/lib/plugins_v1.1/" &
  dcc=$!
  sleep 8
  for p in overview proxies profiles connections rules providers logs override tools settings tun; do
    /usr/lib/qt6/bin/qdbus org.deepin.dde.ControlCenter1 /org/deepin/dde/ControlCenter1 \
      org.deepin.dde.ControlCenter1.ShowPage "dccproxy/$p"
    sleep 1.2
  done
  kill $dcc'
```

私有总线必须在同一个用户命名空间内启动：`unshare --map-root-user` 的进程在总线
认证中声明 uid 0，宿主会话总线会接受（因此 `--spec` 单独运行照常工作），但手动启动的
`dbus-daemon` 会拒绝，表现为日志中的 `dbus service already registered!`。
这里用 `dbus-run-session` 托管总线，命令结束即回收；不要改回
`dbus-daemon --session --fork`，否则总线和它激活的 portal 进程都会残留。

页面内容实例化会暴露仅靠页面树加载看不到的问题：本轮由此发现并修复了 6 处
`checked`/`text` 双向写回导致的绑定循环（`onCheckedChanged: <属性> = checked`
改为 `if (<属性> !== checked) <属性> = checked`）。修复后 11 页零诊断。
把实测的假控制器（记录请求的本地 HTTP 服务）设为 `endpoint` 并逐页打开，可以同时
验证各页数据通路：实测收到 `/configs`、`/proxies`、`/providers/proxies`、`/rules`、
`/providers/rules`、`/connections` 与 `/logs` 流请求。

## 0.1.9 AnyTLS 订阅导入

Qt 回归共 37 项，全部通过。新增混合订阅用例：8 个 Hysteria2 和 1 个 AnyTLS，
覆盖原始 URI、标准 Base64、无填充 URL-safe Base64、密码百分号解码、节点名称、
SNI、证书校验、ALPN、客户端指纹、IPv6 和默认端口。
验证保存后的策略组保留全部节点，预览显示 AnyTLS 且不暴露密码；
生成的配置通过随包 Mihomo v1.19.16 的 `-t` 校验。

实际订阅也已验证：9 个节点全部导入并通过内核配置校验。
真实订阅地址和凭据未写入仓库或测试输出；验证未切换当前代理或节点。

## 0.1.8 默认 root 内核

安装和升级统一为随包内核设置 `root:root`、`4755`，后台守护进程保持桌面用户身份。
启动时校验随包内核的实际有效 UID，状态中的 `root` 字段也读取实际进程身份。

真实内核验证（独立临时目录、随机端口、不启用系统代理）：

```sh
python3 tests/smoke_real_core.py \
  --service build-debian/dde-mihomo-daemon \
  --core /usr/lib/dcc-proxy/mihomo --expect-root
```

本机通过：TUN 关闭时有效 UID 为 0、真实 UID 为桌面用户，父进程为用户守护进程；
端口变更与崩溃恢复后仍为 root。普通用户可停止内核，重启和后台正常退出均不遗留旧进程。
本地 HTTP/SOCKS 实际流量、无效配置和占用端口保留现有内核的检查同时通过。

## 自动测试（0.1.x 控件版记录）

以下结果属于移植前的 Qt 5 控件版；0.2.0 的测试二进制为 `party-tests`（`tests/test_plugindata.cpp`），
运行方式见 0.2.0 一节，本轮未运行。

`dpkg-buildpackage -us -uc -b` 和 `tools/build-deb.sh` 支持标准 Debian 与 CPack 两种打包方式。
标准打包已在两个架构上完整编译、执行测试并生成对应 deb；内置 QtTest 的 18 项结果全部通过。

覆盖配置保存/合并/备份、安全校验、原始和 Base64 URI 订阅、真实本地 HTTP 请求、
可见 URL 导入按钮、来源 URL 与订阅元数据、节点切换成功读回和失败保留当前节点。
国旗测试覆盖中文/英文/地区代码/表情优先级、未知地区回退，并渲染全部内置 SVG。
网格测试检查宽窗口、窄窗口同一行多列卡片。
插件生命周期测试模拟控制中心延迟销毁旧页面，确认再次打开页面时不会变成空白。
测速回归测试检查单节点和整组进度动画、成功/失败独立完成、旧历史不覆盖新结果，
并确认轮询和手动刷新不销毁现有卡片模型，防止闪烁与丢失选择。
卡片点击测试验证主体区域直接切换，点击未选中节点的延迟或失败状态仅测速，
既不改变当前选中项，也不发送切换节点请求。

绕过测试覆盖规范化和去重、无效输入拒绝、设置恢复与空列表语义，
并通过现有 Qt JavaScript 引擎执行实际 PAC 函数，验证域名、IPv4/IPv6 CIDR 和 DNS 解析后的路由。
测速回归还验证 Clash Party 默认 5000 ms / 50 并发、策略组地址继承、用户地址覆盖与持久化、
Provider 健康检查请求，以及统一延迟默认开启、用户明确关闭时保留。
节点 DNS 回归测试验证兼容选项只补充缺失的节点解析器，保留订阅其余 DNS 配置及明确指定的节点 DNS。

## 真实内核集成测试

两个架构均运行 `tests/smoke_real_core.py`，使用独立临时数据目录和官方 Mihomo v1.19.16：

- 没有已启用配置记录时，“启动”会使用首个导入订阅。
- DIRECT 选择后，实际 HTTP 流量到达本地目标。
- REJECT 选择后，实际流量被阻断。
- 关闭插件预览窗口后，内核 PID 不变且仍能代理流量。
- 无效配置验证失败时，原内核 PID 和流量保持正常。
- 候选配置端口被其他程序占用时明确报错，现有内核 PID 和代理流量保持正常。
- 修改运行模式后 API 读回一致。
- 停止后台后释放内核进程。

## 本机与现有 Clash 共存验证

0.1.1 已安装到本机。真实订阅测试使用独立临时数据目录、随机控制器/代理端口、
随机控制器密钥，并明确关闭测试实例的系统代理联动；所有请求显式指向测试实例。
测试前后检查现有 Mihomo PID 和桌面代理模式/地址/绕过列表，确认未被替换或改变。

同一个日本节点、同一个 `https://www.gstatic.com/generate_204` 地址和 5000 ms 超时，
各预热一次、采样三次：统一延迟关闭为 326/274/327 ms（中位数 326 ms），
开启为 120/125/120 ms（中位数 120 ms）。数值直接来自官方内核，无显示修正。
隔离代理实际访问 Google、YouTube 均返回 200。此记录只代表该次测试，实际延迟随线路变化。

实现对照 Clash Party v2.0.3 的 `src/main/utils/template.ts`、
`src/main/core/mihomoApi.ts` 和 `src/renderer/src/pages/proxies.tsx`：
默认统一延迟、5000 ms 超时、50 并发、测速 URL 优先级、逐节点回收及 Provider 健康检查。

## 远端实际环境（0.1.x 控件版记录）

ARM64 deb（Qt 5 控件版）已安装到 `/usr/lib/dde-control-center/modules/libdcc-proxy.so`；
0.2.0 改为 V25 的 `plugins_v1.1` 布局，安装到 `…/dde-control-center/plugins_v1.1/dccproxy/`，
插件名改为 `dccproxy`。以下远端结论仍然成立，因为它们取自后台与内核行为，与插件 ABI 无关。
控制中心可加载“私人代理”，节点页使用多列国旗卡片。
真实 URL 订阅读取到 10 个 Hysteria2 节点。针对“全部测速失败”，同一 ARM64 内核和同一组节点进行隔离对比：
完整订阅配置下测试失败；仅移除订阅 DNS，或仅增加 `proxy-server-nameserver: [system]` 后测试成功。
最终修复使用后一种方式，保留订阅原有 DNS 规则，只解决节点服务器域名的启动解析问题。
安装修复后的后台，10 个节点中 4 个测速成功；日本约 92–139 ms，美国约 190–215 ms。
选择可用节点后实际 HTTPS 请求返回 204。其余节点仍可能因线路、IPv6 或服务端状态失败，不能保证订阅所有节点可用。

`tests/smoke_system_proxy.py` 在远端实际 DDE Network DBus 上验证：关闭状态保存不启用代理、
手动代理绕过写入/立即修改、内置 PAC 动态更新、空列表、无效输入不改变设置，
以及退出时完整恢复原代理模式、地址、PAC URL 和绕过列表。
新增启动联动验证：内核就绪后自动启用系统代理、无效配置保持原代理、重启跟随新端口、
停止恢复原设置，以及明确关闭自动启用选项时不修改桌面代理。

真实订阅、节点密钥、测试机登录凭据和实际屏幕截图均未加入仓库。
`docs/screenshots/node-grid.png` 使用虚拟节点生成，仅作布局示例。

## 重现（0.2.0）

标准 Debian 打包，产物在项目目录上一级：

```sh
dpkg-buildpackage -us -uc -b
```

默认包包含当前架构官方内核及对应源码归档。`cores/` 缓存存在且校验通过时无需下载；
不存在时从官方地址下载。可使用 `DCC_PROXY_BUNDLE_CORE=0` 打包不含内核的版本。
安装不会自动启用登录自启，继续由插件设置控制。

CPack 和内核集成测试（`--preview` 已随控件版预览程序一起删除）：

```sh
./tools/build-deb.sh
python3 tests/smoke_real_core.py \
  --service build-$(dpkg --print-architecture)/dde-mihomo-daemon \
  --core cores/$(dpkg --print-architecture)/mihomo
```

系统代理集成测试会短暂修改当前桌面代理，并在结束时恢复，须明确传入测试开关。
测试使用 Qt 6 的 `qdbus`（脚本优先使用 `/usr/lib/qt6/bin/qdbus`，`/usr/bin/qdbus`
只是无法解析 qt6 profile 的 qtchooser 包装）：

```sh
python3 tests/smoke_private_proxy.py \
  --service build-$(dpkg --print-architecture)/dde-mihomo-daemon \
  --core cores/$(dpkg --print-architecture)/mihomo
python3 tests/smoke_system_proxy.py \
  --service build-$(dpkg --print-architecture)/dde-mihomo-daemon \
  --allow-system-proxy
```

插件本体的验证使用 0.2.0 一节的独立加载命令；本轮未运行上述任何命令。


## 0.1.2 概览与 TUN 回归

Qt 回归共 20 项。新增模拟异步返回顺序的概览测试：内核请求先返回、统计请求延迟返回，
重复刷新时两个标签的文字、位置保持稳定，隐藏连接表不重建。原生 DSwitchButton 验证
停止、启动、操作禁用、失败读回；绕过列表的错误项保留并精确提示。

TUN 测试在全新 user/network namespace 中运行，要求宿主网络标识不同且初始只有 lo。
它创建临时出口网卡、本地 DNS 与 HTTP 服务，临时数据目录、独立端口、关闭系统代理联动，
不使用真实订阅或宿主 Clash。测试验证 Mixed TUN、自动路由、any:53 DNS 劫持、Fake IP
实际 HTTP、MTU 生效、保存时保留 enable、网卡冲突拒绝、热加载与唤醒重载、关闭后恢复所有
路由/规则并保留 DNS。使用现有 setpriv 在测试子进程中撤销 CAP_NET_ADMIN，验证真实
EPERM 创建错误及恢复可用内核；不改变宿主内核文件权限。

```sh
unshare --user --map-root-user --net python3 tests/smoke_tun.py \
  --host-netns "$(readlink /proc/self/ns/net)" \
  --service build/dde-mihomo-daemon --core cores/amd64/mihomo
```

本轮只在本机安装和测试，不同步远端机器；未安装新的依赖。


## 0.1.3 首次授权生效

本机复现：内核文件已为 root:root / 4755，但授权前启动的内核仍以普通用户运行，
CAP_NET_ADMIN 不存在。热加载不能改变已有进程权限。后台现在读取该进程的
`/proc/<pid>/status` 能力位；首次启用时检测到缺失会重新 exec 本插件的内核，
已有权限时继续热加载，不触碰其它 Clash 实例。

隔离 TUN 测试新增：先让内核以撤销 CAP_NET_ADMIN 的状态启动，再模拟完成授权。
启用 TUN 必须自动更换该内核 PID、获得真实能力位，并成功执行 DNS 劫持与 HTTP。
后续保存设置保持 PID。持续缺权的失败场景仍回退到可用配置。


## 0.1.4 离线首次启动与完整数据

远端 10.20.8.241 的启动失败来自缺少 MMDB，官方 GitHub 下载直连 5 秒连接超时，
内核初始化下载等待 90 秒后失败。补齐 MMDB 和订阅缺失的 mixed-port 后，控制器 19090、
混合代理 17890 就绪，显式通过本插件访问百度返回 200、Google generate_204 返回 204。
现有 Clash 进程保持不变。

包内提供 Country.mmdb、GeoIP.dat、GeoSite.dat、ASN.mmdb；固定官方仓库提交、
Git blob 与 SHA-256 校验，复用现有 Python 标准库作为构建工具。安装脚本不执行 apt，
运行时不下载上述基础资源。目标机器插件、后台全部动态库均可解析，Mihomo 是静态程序。

`smoke_geodata.py` 在只有 lo 的独立网络空间中，用全新配置、空数据目录启动真正内核，
配置同时使用 GEOIP、GEOSITE、IP-ASN，并将下载 URL 指向拒绝请求的本地服务。
检查零下载请求、真实代理 HTTP 成功；覆盖 MMDB 与 GeoIP.dat 两种模式，四类下载中断
缓存的备份恢复，以及已有可用旧数据库/geoip.metadb 别名不被覆盖。测试不使用真实订阅。

```sh
unshare --user --map-root-user --net python3 tests/smoke_geodata.py \
  --host-netns "$(readlink /proc/self/ns/net)" \
  --service build/dde-mihomo-daemon --resources cores/geodata
```

GeoIP 等构建缓存完整时，可以用 `tools/prepare-geodata.py --offline` 只校验不下载。
Qt 回归增加缺失 mixed-port 时的默认监听及显式覆写优先级检查。


导入预览回归：URL 导入完成后直接在节点页显示订阅内容；内核地址不可达时也可切换
多个订阅，展示名称/协议/国旗与分组，重复刷新保留卡片。预览不创建后台 socket、不写
active、不发切换/测速请求，不将未启动状态标为节点失败。inline provider、include-all、
正则分组与私密节点字段不进入预览模型也有覆盖。

## JavaScript 覆写回归（0.2.0，Qt 6 引擎）

覆写通过后台真实的 `--eval` 子进程执行（`dde-mihomo-daemon --eval <override.js> <订阅名>`，
输入为运行配置 JSON，输出为覆写后的配置）。0.2.0 起使用 Qt 6 的 V4 引擎，
`Array.prototype.includes()` 由引擎原生提供，兼容补丁已移除；本轮以真实子进程验证：

- DNS prepend 脚本保留原顺序、插入 `10.20.0.10`，重复执行不重复插入，缺失 DNS 或
  nameserver 非数组时按原逻辑处理，中文订阅名正确传入第二个参数。
- 引擎语义：`[NaN].includes(NaN)`、负索引、`Infinity`、`Array.prototype.includes.call`
  的类数组行为均与规范一致。
- 错误路径：`return null`、返回数组、返回 Promise（异步）以及非法 JSON 输入分别报
  `Override must return a configuration object`、
  `Override main must return a configuration object synchronously` 等原有错误文本。
  脚本运行时异常、语法错误仍会带文件名与行号。

## 0.1.5 全功能回归清单（0.1.x 控件版记录）

该轮的逐项自测清单已由 `QA-V25.md` 取代：`QA-V25.md` 记录 0.2.0 的环境事实、
已验证项与未运行项，本节保留仍然有效的验证点。完整 Qt 回归包含真实内核日志、HTTPS
WebDAV、原生密码小眼睛、概览五种有效示例、YAML 覆写说明和 460px 长提示布局检查。

`smoke_real_core.py` 增加混合/HTTP/SOCKS 真实流量、TCP/UDP 冲突、控制器冲突/重复/范围
校验、保存失败原配置保留、离线保存与端口持久化、订阅强制更新及异常退出自动恢复。
应用 HTTP 头保持原样；订阅下载使用 Mihomo 兼容 UA（多数订阅面板按 UA 决定导出格式，浏览器 UA 会被拒绝），核心资源下载仍使用 `Mozilla/5.0`。

系统代理测试可以使用环境已有 DBus/GLib，在独立会话总线上验证，避免修改宿主设置：

```sh
python3 tests/smoke_private_proxy.py --service build/dde-mihomo-daemon --core cores/amd64/mihomo
```

测试中的模拟 DDE 服务只存在于私有总线；GSettings 使用内存后端。覆盖手动/PAC 端口同步、
显式关闭保持关闭、冲突不影响代理、绕过和完整恢复。无须为自测切换宿主 Clash。


## 0.1.6 默认端口

默认控制器改为 19999，默认混合代理改为 16666；更新默认配置、导入、新建、界面提示及
端口回归预期。0.1.5 及更早章节中的端口是对应版本的历史验证记录。已有显式配置不会
仅因升级被改写；本机按用户要求迁移配置并重启后台/内核使两项生效。


## 三种代理模式实际流量补测

`tests/smoke_modes.py` 使用已安装的后台与官方内核、独立临时目录、随机端口、本地 HTTP
目标及真实 CONNECT 中转。系统代理联动关闭，测试实例不连接宿主会话/系统总线。

- 规则：三个不同目的端口分别命中策略组、DIRECT、REJECT；只允许代理分支访问中转，
  改变策略组选择只影响该分支。
- 全局：将 GLOBAL 设为测试代理后，连 DIRECT/REJECT 规则的目标也实际经过中转；将
  GLOBAL 改为 DIRECT 或 REJECT 后分别直连或阻断，证明全局以 GLOBAL 的当前选择为准。
- 直连：同时将 GLOBAL 和策略组选为 REJECT，所有目标仍直达本地服务，中转连接数不增加。
- 切回规则后恢复原规则行为。每次请求新建连接，避免复用旧连接掩盖模式切换结果。

Qt 回归补充三项中文选项到 rule/global/direct 的对应关系、实际 PATCH 请求与读回、
保存到 controlled.yaml 后合成运行配置，以及重开页面后的读回。

```sh
python3 tests/smoke_modes.py
```

这轮仅增加测试与记录，未更改已安装 0.1.6 的程序逻辑或本机当前模式。

## 0.1.7 本机直接替换

本轮不执行 dpkg-buildpackage、不生成或安装 deb。使用 `cmake --build build-debian --target dcc-proxy -j12` 编译插件，直接替换安装目录的 `.so`；官方内核对齐本机 Clash Party 使用的 v1.19.16，保留替换前备份。Debian 默认并行数改为 12，并更新 AMD64、ARM64、源码的固定 SHA-256。

新增测试覆盖策略组出口路径、循环引用、隐藏组、两个组独立选择、卡片稳定刷新、测速不选中、网址/域名/IP/IDNA 规则生成及覆写冲突回滚。网址规则写入所选订阅的 rules 首部；远程订阅更新可能覆盖手动修改，与高级规则编辑行为一致。

此前的 Codex `connection failed` 原因尚未确认。连接页面顺序差异只是显示问题；同节点的带登录 HTTP/1.1、HTTP/2 SSE 和 WebSocket 最小请求已经完成，但这不能证明完整既有会话的问题已修复。按用户要求停止继续定位，先直接替换内核与插件。
