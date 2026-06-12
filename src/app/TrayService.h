#pragma once

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
    void notify(const QString& title, const QString& message);
    void retranslate(); // 语言切换后重建菜单文案

signals:
    void captureRequested();
    void closeAllPinsRequested();
    void historyRequested();
    void settingsRequested();

private:
    void buildMenu();

    QSystemTrayIcon tray_;
    QMenu menu_;
    std::unique_ptr<ToastWindow> toast_; // 懒创建,复用同一窗口
};

} // namespace pixora
