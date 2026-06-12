<p align="center">
  <img src="resources/icons/pixora-256.png" width="128" alt="Pixora logo"/>
</p>

<h1 align="center">Pixora</h1>

<p align="center">
  截图 · 标注 · 贴图 · <b>长截图</b> —— 轻快顺手的截图工具,把滚动长截图做成一等公民
</p>

<p align="center">
  <a href="https://github.com/Yuilona/Pixora/releases/latest"><img src="https://img.shields.io/github/v/release/Yuilona/Pixora" alt="Release"/></a>
  <a href="https://github.com/Yuilona/Pixora/actions/workflows/ci.yml"><img src="https://github.com/Yuilona/Pixora/actions/workflows/ci.yml/badge.svg" alt="CI"/></a>
  <img src="https://img.shields.io/badge/platform-Windows-0078D6" alt="Platform"/>
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue" alt="C++20"/>
  <img src="https://img.shields.io/badge/Qt-6-41CD52" alt="Qt 6"/>
</p>

---

## 功能

**截图**
- `F1` 全局热键,冻结画面所见即所得;悬停自动吸附窗口,按住 `Shift` 细化到**按钮/输入框级 UI 元素**(UIAutomation)
- 自由框选 + 八向手柄微调;方向键 1px 移动、`Ctrl+方向键` 1px 调边、`Shift` 步长 ×10
- 放大镜实时显示坐标与色值,按 `C` 复制 HEX(`Shift+C` 复制 RGB)——内置取色器

**标注**
- 矩形 / 椭圆 / 箭头 / 画笔 / 马克笔 / 文字 / 序号 / 马赛克 / 模糊,九件套图标工具栏
- 条目可二次编辑:选中后拖拽缩放、改色改粗细、文字双击重写,全程可撤销重做

**贴图**
- 截图一键贴出、`F3` 剪贴板贴图;滚轮缩放、边角拖拽缩放、`Ctrl+滚轮`调透明度
- `Space` 折叠成小条、`R` 旋转、`H` 翻转、点击穿透
- **跨重启恢复**:位置/缩放/透明度/折叠态全部记住

**长截图(差异化能力)**
- 截图工具栏一键转入滚动拼接,手动滚动 / 自动滚动双模式,30fps 抓帧快滚不断链
- NCC 三带中位数匹配 + sticky 头尾检测,实时预览条贴在捕获区旁,错位当场可见
- 滚轮失效自动切 PageDown 驱动;复制 / 贴图 / 另存多出口

**输出与历史**
- 文件名模板 `{yyyy}{MM}{dd}{HH}{mm}{ss}`、PNG/JPEG + 质量、复制时自动存盘
- 截图历史自动留底,托盘随时翻看、重新复制 / 贴图 / 另存

## 安装

到 [Releases](https://github.com/Yuilona/Pixora/releases/latest) 下载 `Pixora-x.y.z-win64-portable.zip`,解压即用(免安装)。

> 暂无代码签名,Windows SmartScreen 拦截时选「更多信息 → 仍要运行」。

## 默认快捷键

| 按键 | 动作 |
|---|---|
| `F1` | 截图(长截图拼接中 = 完成并复制) |
| `F3` | 剪贴板图像贴出为贴图 |
| `Ctrl+A` | 全选整个桌面(悬停桌面空白处也会吸附整屏) |
| `Enter` / 双击 | 复制选区并结束 |
| `Ctrl+S` | 另存为… |
| `C` / `Shift+C` | 取色(HEX / RGB) |
| `Esc` | 逐层退出(工具 → 选中 → 会话) |

热键、保存目录、历史张数、开机自启均可在 托盘 → 设置 中修改。

## 从源码构建

```bash
git clone https://github.com/Yuilona/Pixora.git
cd Pixora
cmake --preset win-debug   # 需要 Qt 6.5+、vcpkg(VCPKG_ROOT)、MSVC 2022
cmake --build --preset win-debug
ctest --preset win-debug
```

详见 [docs/DEV-SETUP.md](docs/DEV-SETUP.md)。

## 文档

| 文档 | 内容 |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 总体架构:分层设计、平台抽象、长截图引擎、里程碑路线图 |
| [docs/DEV-SETUP.md](docs/DEV-SETUP.md) | Windows 开发环境搭建指南 |
| [docs/DISTRIBUTION.md](docs/DISTRIBUTION.md) | 发布与分发策略(打包、签名、各平台信任机制) |
| [docs/POLISH-BACKLOG.md](docs/POLISH-BACKLOG.md) | 打磨待办与完成记录 |

## 路线图

- **M4 跨平台**:macOS(权限引导)→ Linux X11 → Wayland 降级路径
- **M5 打磨**:贴图分组、标注序列化二次编辑、性能优化
- 分发:winget 上架、代码签名

技术栈:C++20 · Qt 6 (Widgets) · CMake + vcpkg · OpenCV(拼接模板匹配)

## License

TBD
