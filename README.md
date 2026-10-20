# dcc-proxy

UOS / Deepin V25 控制中心的私人代理插件。插件按 `plugins_v1.1` QML 插件 ABI 构建，
模块名为 `dccproxy`，界面由 11 个 QML 页面组成；C++ 侧只提供数据对象 `dccData`，
不使用 QtWidgets。后台进程为 `dde-mihomo-daemon`，以 systemd 用户服务
`dde-mihomo-daemon.service` 运行。关闭控制中心后，后台内核继续提供代理。

## 构建和安装

需要使用已安装的 CMake（≥ 3.23）、C++17 编译器、Qt 6.8（Core、Gui、Network、DBus、
Qml、Quick、Test、LinguistTools）、DTK 6 与控制中心 6.1 的开发文件
（`dde-control-center-dev`）。YAML 解析器已随源码提供，不需要安装新依赖。

标准 Debian 打包（产物位于项目目录的上一级）：

```sh
cd /path/to/deepin-proxy-tools
dpkg-buildpackage -us -uc -b
sudo dpkg -i ../dcc-proxy_0.2.0_$(dpkg --print-architecture).deb
```

也可使用 CPack 脚本（产物位于 `dist/`）：

```sh
./tools/build-deb.sh
sudo dpkg -i dist/dcc-proxy_0.2.0_$(dpkg --print-architecture).deb
```

开发文件装在 `/usr` 时无需额外参数；如果放到了别处（例如解包到 `/tmp/prefix/usr`），
先在同一个 shell 导出 `export CMAKE_PREFIX_PATH=/tmp/prefix/usr`（两条命令都读取该环境变量）。

两种方式共用 `tools/prepare-core.sh` 和 `tools/prepare-geodata.py`，优先复用 `cores/` 中通过 SHA-256 校验的官方内核和源码归档；
缓存缺失时从官方 HTTPS 地址下载。断网构建前需准备相同缓存。
需要使用现有内核时，标准打包可设置 `DCC_PROXY_BUNDLE_CORE=0`；CPack 使用 `--without-core`。
内核和数据下载仅发生于构建缓存缺失时；deb 安装和首次启动不执行依赖下载。
默认最多使用 2 个编译任务（Debian 打包默认 12），`DCC_PROXY_JOBS` 可调整。

脚本根据**构建机器**的架构自动选择官方 Mihomo v1.19.16 内核，只打包对应架构。
内核是官方预编译二进制；插件与后台管理程序在本机编译。下载缓存保存在 `cores/`，
构建和打包不自动执行 apt 安装。跨架构构建需要对应工具链和 Qt sysroot，
也可直接在目标架构机器执行脚本。

本机直接构建与测试：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

`-DCMAKE_PREFIX_PATH` 只在 `dde-control-center-dev` 与 DTK6 开发文件没有装入 `/usr`
时需要，指向其安装前缀。
构建始终使用目标系统实际的插件接口头文件（`/usr/include/dde-control-center`）；
仓库内的 `third_party/dcc-interface` 只是历史接口快照，不作为构建来源。
插件在构建树中位于 `build/lib/plugins_v1.1/dccproxy/`（`dccproxy.so`、
`libdccproxy_qml.so`、`qmldir`），安装后位于控制中心的插件目录
`/usr/lib/<multiarch>/dde-control-center/plugins_v1.1/dccproxy/`。
随包内核是静态 Go 程序，不需要 Qt 运行库；Qt / DTK 等系统运行库由目标 UOS / Deepin 提供，
安装和升级脚本不会执行 apt 或下载。

开发时也可以不安装，直接把控制中心指向构建树中的插件目录：

```sh
unshare --user --map-root-user --fork --pid dbus-run-session -- \
  env QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  /usr/libexec/deepin/dde-control-center --spec ./build/lib/plugins_v1.1/
```

模块状态行 `"dccproxy" : status … "PluginEnd"` 表示加载与析构正常。

## 页面

控制中心“私人代理”按权重顺序包含 11 个页面：

| 页面 | 名称 | 内容 |
| --- | --- | --- |
| overview | 概览 | 内核状态、代理模式、流量与连接统计 |
| proxies | 代理组 | 策略组、节点网格、切换与测速 |
| profiles | 订阅 | 导入、更新、启用、编辑本地配置 |
| connections | 连接 | 当前连接、搜索、单独/全部关闭 |
| rules | 规则 | 分流规则、搜索、临时禁用、网址快捷规则 |
| providers | 资源 | 节点/规则提供者、健康检查、GEO 更新 |
| logs | 日志 | 内核实时日志与后台历史记录 |
| override | 覆写 | YAML 与 JavaScript 覆写 |
| tools | 工具 | DNS 查询、缓存清理 |
| settings | 设置 | 控制器、端口、系统代理、备份恢复 |
| tun | 虚拟网卡 | TUN 网卡、协议栈、路由与 DNS 劫持 |

页面沿用控制中心的版式：`DccTitleObject` 分节标题、`DccGroupView` 分组卡片、
右对齐的 Editor 行与操作按钮（主操作为 `RecommandButton`），表格使用统一的表头与
细分隔线。模块与页面图标是随包的 12 个 `.dci`（浅色/深色两套调色板，按 24×24 符号
图形绘制），界面颜色全部取自系统调色板，因此跟随控制中心的浅色/深色主题。

## 使用

1. 重新打开控制中心，进入“私人代理”。
2. 在“订阅”顶部粘贴 URL，点击“URL 导入”，也可导入 Clash / Mihomo YAML 文件。
3. 在“设置”确认内核路径和控制器地址，保存并连接。
4. 导入后即可在“代理组”预览订阅中的节点，无需启动内核或开启系统代理；顶部可切换预览订阅。首次使用时可打开概览中的“代理服务”开关；已有多个订阅时，选择订阅并点击“使用此配置”切换。
5. 在“代理组”选择策略组，点击节点卡片主体直接使用该节点；点击延迟或失败状态只测速，不切换节点。
6. 启动成功后自动启用系统代理，浏览器可直接使用；停止时恢复原设置。随包内核默认以 root 身份启动，TUN 按需开启。

“代理组”页的订阅预览直接读取已导入 YAML / URI 订阅，显示节点名称、协议、国旗和策略组。
支持 inline provider、include-all 与分组过滤；远程 provider 的节点需加载其内容后显示。
预览不会启动后台、改动系统代理或标记节点已启用。启动并使用配置后切回“自动”显示实时
状态，测速和切换可在关闭系统代理时使用；离线预览不会把未测速节点显示为失败。

节点使用自适应卡片网格显示，每张卡片包含国旗、名称、协议和延迟。
国旗优先识别节点名中的地区旗帜，再识别中文地区名、常见英文地名及 ISO 地区代码。
图标使用内置 SVG，不依赖国旗字体；不能识别的节点使用地球图标，避免猜测位置。
节点名较长时省略显示，悬停可查看完整原始名称和内核信息；切换请求始终使用原始名称。
测速时对应卡片的勾选位置显示旋转进度环；整组测速逐节点回收结果，失败节点不会覆盖其他节点的成功结果。
测速口径按 Clash Party v2.0.3：默认启用 `unified-delay`，由内核计算连接建立后的第二次 HTTP 请求延迟；界面显示内核原始返回值。
默认超时 5000 ms、并发 50，均可调整并保存；测速地址优先使用手动设置，其次策略组 `url`，最后使用 `https://www.gstatic.com/generate_204`。
Provider 节点使用对应健康检查接口。定时刷新只更新变化的数据，保留卡片、选中项和滚动位置。
可调超时范围为 1–30000 ms，适配随包 Mihomo 的超时参数范围。

URL 下载参考 Clash Party 的 User-Agent，支持重定向、订阅流量信息和更新周期。
“订阅设置”可设置 User-Agent、Authorization、超时、更新周期和本地代理下载。
支持完整 YAML，以及原始或 Base64 编码的 Hysteria2、AnyTLS、Trojan、VLESS、Shadowsocks、TUIC URI 列表。
其他分享格式建议由订阅服务输出 Clash / Mihomo YAML。

“设置 → 代理绕过”支持逐行填写域名、`*.example.com`、IPv4/IPv6 地址和 CIDR 网段。
默认绕过本机及常见局域网地址，点击“保存并应用绕过”立即更新已开启的系统代理或内置 PAC；
尚未启用代理时只保存列表。清空列表并保存可取消全部绕过。
关闭系统代理或停止后台时，会恢复启用前的代理模式、地址和绕过列表。
该列表作用于遵循系统代理设置的应用；TUN 和应用自行指定代理时，请使用 Mihomo 的 DIRECT 规则。

Linux 的通配符与 Clash Party 一致，只支持开头的 `*.`，不支持 `*.uniontech.*`。
例如匹配 uniontech.com 的子域名应填写 `*.uniontech.com`；其它顶级域名需要分别填写。
输入无效时显示具体项与原因，保留原文并阻止保存；也支持逗号、分号分隔粘贴。

概览不再显示重复的 TUN 开关，统一在“虚拟网卡”页面启用和设置。该页面提供网卡名称、Mixed/gVisor/System/Mips 协议栈、自动路由、Linux 自动重定向、
出口网卡检测、严格路由、DNS 劫持、路由排除 CIDR 和 MTU。默认值对照 Clash Party v2.0.3：
网卡 Mihomo、Mixed、自动路由与出口检测开启、自动重定向关闭、DNS 劫持 any:53、MTU 1500。
各协议栈是否可用取决于所选内核；校验或创建失败时显示内核错误并恢复原配置。
启用时联动开启 DNS，关闭 TUN 时保留 DNS；保存其它设置不会意外关闭 TUN。
已获得网络管理权限的内核通过热加载应用设置，不重启进程；首次文件授权后，若当前进程
仍没有 CAP_NET_ADMIN，会自动重启本插件自己的内核一次使授权生效。后续切换不重复鉴权。
恢复唤醒 5 秒后重新加载配置。
安装和升级时将 `/usr/lib/dcc-proxy/mihomo` 设置为 `root:root`、权限 `4755`，
使随包内核在 TUN 关闭时也以 root 有效身份启动；后台守护进程和控制中心仍以当前用户运行。
启动后检查内核实际有效 UID，授权未生效时明确报错；后台状态的 `root` 字段反映实际进程身份。
“内核授权”可修复被移除的授权，自定义内核仍需自行授权；已存在的同名网卡不会被接管。

概览的后台状态、内核连接摘要与流量统计分别显示，只更新变化内容，避免异步请求反复改写同一
标签导致闪烁。内核连接摘要仅在内核已连接时出现，包含混合、HTTP、SOCKS 端口与局域网允许状态。
代理服务开关绑定后台真实状态，操作期间禁用，失败时显示后台返回的错误文本并读回真实状态。
后台推送的状态消息显示在页面标题区，错误消息使用错误样式。

默认开启“节点域名使用系统 DNS”，在订阅启用了 DNS 且未指定 `proxy-server-nameserver` 时，
为节点服务器域名添加 `proxy-server-nameserver: [system]`，避免节点解析依赖尚未连通的代理。
订阅已有的节点专用 DNS 会保留；可在设置中关闭兼容选项，重启内核后生效。

设置页提供混合、HTTP、SOCKS、重定向和 TProxy 端口编辑，默认混合端口 16666，
其余未配置时关闭；不默认使用 7890/7891。已明确指定的订阅/用户端口可以通过专用设置修改。
点击“保存并应用端口”，运行中会验证并重启本插件内核，同步已由本插件启用的手动/PAC
系统代理；关闭时仅保存，下次启动生效。端口冲突、重复、范围错误或控制器端口冲突会拒绝，
失败恢复原配置；专用端口设置最后应用，避免被订阅或脚本覆写回旧值。
本机端口检查和控制器连接明确使用直连，不经当前桌面代理。
订阅下载默认使用 Mihomo 兼容的 `mihomo/1.19.16`（多数订阅面板按 User-Agent 区分客户端，
浏览器 UA 会被拒绝或返回 HTML）；订阅设置里显式填写的 User-Agent 优先，默认 UA 收到
403/404 时会再用 `Mozilla/5.0` 重试一次。核心资源下载仍使用 `Mozilla/5.0`。
应用转发流量保留原有请求头，不添加软件名称。

长提示在有限高度内换行和滚动，保留完整内容工具提示。API 密钥使用 DTK 的密码输入框
（`D.PasswordEdit`）小眼睛按钮，默认隐藏，离开页面后重新隐藏。
DNS、嗅探、Hosts、监听器、隧道的编辑入口包含用途、字段说明和可填入的示例，
未配置时直接显示示例，取消不生效；YAML 覆写页也提供说明和示例，保存前不会修改运行配置。
导出、导入、备份与恢复使用 Qt Quick Dialogs 的原生文件选择框（`QtQuick.Dialogs`），
路径经 `dccData.localPath()` 转换，非 ASCII 路径不会被破坏。
日志页先展示后台最近记录，再追加实时日志；没有新连接时也可查看启动日志。
日志级别同时更新核心与保存配置，暂停/清空/导出保持可用，断线显示原因并重连。

JavaScript 覆写由后台在独立子进程中执行，限时 4 秒，不会阻塞控制中心。
入口为 `function main(config, profileName)`，第二个参数是订阅名称；必须同步返回配置对象。
脚本使用 Qt 6 的 JavaScript 引擎，已原生提供 `Array.prototype.includes()`（NaN、负索引、
Infinity 与类数组语义均为引擎实现），可直接在原有 DNS 数组前插入并去重。
脚本执行异常会显示实际错误、脚本文件名和行号，不再统一误报为返回值错误。

默认控制器地址为 `http://127.0.0.1:19999`，首次运行生成随机 API 密钥。
连接已运行内核时填写其控制器地址和密钥。远端控制器要求 HTTPS。

deb 同时包含官方固定版本的 `Country.mmdb`、`GeoIP.dat`、`GeoSite.dat`、`ASN.mmdb`，
安装到 `/usr/share/dcc-proxy/`，适用于全部包架构。来源版本和 SHA-256 记录在 `geodata.json`。
后台启动时从随包资源准备用户的 `work/` 数据目录，缺失或下载中断的缓存会补齐；
已有可用数据库保留，损坏文件先备份为 `.invalid-时间戳`。默认关闭基础数据库自动更新，
可按需使用“资源”页的 GEO 更新操作。
仅含端口/节点/规则的完整 YAML 如果缺少 mixed-port，
会默认提供 16666；显式配置的端口以及覆写保持有效。
用户的 URL 订阅、外部 rule-providers 和节点连接属于配置内容，按其指定地址联网；
随包数据库不替代这些用户指定资源。

## 后台服务

后台通过当前用户专用 Unix socket 接受控制命令，控制中心关闭不会结束后台。
登录自启由用户级 systemd 单元控制，安装默认不启用（属于用户选择）：

```sh
systemctl --user status dde-mihomo-daemon.service
systemctl --user start dde-mihomo-daemon.service
systemctl --user stop dde-mihomo-daemon.service
systemctl --user enable dde-mihomo-daemon.service
/usr/lib/dcc-proxy/dde-mihomo-daemon --command status
/usr/lib/dcc-proxy/dde-mihomo-daemon --command stop
```

“启动内核时自动启用系统代理”默认开启，后台确认控制器和混合端口可用后才启用代理。
打开“代理服务”开关、“使用此配置”和重启内核都适用；首次导入尚无启用记录时，启动会使用首个已导入订阅。
启动前检查混合端口占用；与其他代理软件冲突时给出明确错误并保留现有内核。
基础地理数据库已随包提供，首次启动无需下载；外部订阅规则资源仍按配置获取，配置验证失败时保留已有内核和代理设置。
只需要应用手动指定代理的用户，可在设置中取消该选项。
默认手动启动：打开“代理服务”开关或点击“使用此配置”才启动内核，打开控制中心本身不会启动内核。
勾选登录自启由单元持久化为 systemd 用户服务；取消勾选不停止当前内核。

本地配置在 `~/.config/dcc-proxy/`，目录 0700、文件 0600。备份包含订阅和 API 密钥。
后台日志轮转为两个 2 MiB 文件，界面实时日志最多保留 2000 行。
测试可通过 `DCC_PROXY_DATA_DIR` 指定隔离数据目录，不影响已有订阅。

## 功能范围

详见 [Clash Party 功能对照](docs/feature-matrix.md)。本项目使用 QML 与原生数据对象重写，
并非将 Electron 窗口嵌入控制中心，也不是把控件版界面平移成 QML；当前版本不能宣称与
Clash Party v2.0.3 全功能等价。构建、插件加载与真实内核验证见
[测试记录](docs/TESTING.md) 和 [V25 验证状态](docs/QA-V25.md)。

## 来源与许可证

本项目沿用控制中心插件接口的 LGPL-3.0-or-later 许可，见 `LICENSE`。
`third_party/dcc-interface` 来自 dde-control-center 的接口快照。
`third_party/yaml-cpp` 来自 yaml-cpp 0.8.0（MIT），保留其许可证。
`third_party/flag-icons` 来自 [flag-icons](https://github.com/lipis/flag-icons)
的 4:3 SVG 资源（提交 `086f7e97d657358203916dbe84f61c2bccaa81eb`，MIT），保留其许可证。
功能参考 [Clash Party v2.0.3](https://github.com/mihomo-party-org/clash-party/tree/v2.0.3)，
API 依据 [Mihomo 官方文档](https://wiki.metacubex.one/api/)，未复制上游应用代码。
随包内核来自 [MetaCubeX/mihomo](https://github.com/MetaCubeX/mihomo/releases/tag/v1.19.16)，
其 GPL 许可证与对应源码归档安装在 `/usr/share/doc/dcc-proxy/`。
