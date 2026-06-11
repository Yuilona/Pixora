#pragma once

#include <QImage>
#include <QString>

namespace pixora {

class SettingsService;

// 统一输出管线:剪贴板 / 文件;上传等出口随后续里程碑加入
// (见 ARCHITECTURE §6)。
class OutputService {
public:
    // settings 可空(用系统图片目录)
    explicit OutputService(const SettingsService* settings = nullptr);

    void copyToClipboard(const QImage& image);

    // 弹出另存对话框;返回保存路径,用户取消返回空串。
    QString saveWithDialog(const QImage& image);

    // 默认目录(设置的保存目录,否则图片库),文件名含时间戳。
    QString suggestedFileName() const;

private:
    const SettingsService* settings_;
};

} // namespace pixora
