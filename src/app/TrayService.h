#pragma once

#include <QImage>
#include <QKeySequence>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

#include <memory>

namespace pixora {

class ToastWindow;

// 托盘图标与托盘菜单。M0 仅提供版本信息与退出入口,
// 后续里程碑在此挂截图/贴图/长截图/设置等动作。
// 通知走自绘 ToastWindow:系统气泡强制带 "pixora.exe" 署名行。
class TrayService : public QObject {
    Q_OBJECT
public:
    explicit TrayService(QObject* parent = nullptr);
    ~TrayService() override;

    void show();
    // link 非空时通知卡可点击打开(更新提示用)
    // link:点击打开的网址或本地文件夹;thumbnail:替代 logo 的缩略图
    void notify(const QString& title, const QString& message,
                const QString& link = {}, const QImage& thumbnail = {});
    void retranslate(); // 语言切换后重建菜单文案
    QMenu* menuForPreview() { return &menu_; } // 仅 --ui-gallery 离屏渲染用
    // 菜单项右侧显示的热键(随设置变化刷新;空序列 = 未绑定,不显示)
    void setHotkeys(const QKeySequence& capture, const QKeySequence& pin,
                    const QKeySequence& repeatLastRegion);

signals:
    void captureRequested();
    void pinFromClipboardRequested();
    void repeatLastRegionRequested();
    void colorPickRequested();
    void closeAllPinsRequested();
    void historyRequested();
    void settingsRequested();

private:
    void buildMenu();

    QSystemTrayIcon tray_;
    QMenu menu_;
    QKeySequence captureKey_{Qt::Key_F1};
    QKeySequence pinKey_{Qt::Key_F3};
    QKeySequence repeatKey_;
    std::unique_ptr<ToastWindow> toast_; // 懒创建,复用同一窗口
};

} // namespace pixora
