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

    // 弹出另存对话框(默认格式跟随设置);返回保存路径,取消返回空串。
    QString saveWithDialog(const QImage& image);

    // 自动存盘开关开启时静默保存到保存目录(重名自动加序号),
    // 返回保存路径;开关关闭返回空串。复制出口调用。
    QString autoSave(const QImage& image);

    // 默认目录(设置的保存目录,否则图片库)+ 模板展开的文件名。
    QString suggestedFileName() const;

private:
    const SettingsService* settings_;
};

} // namespace pixora
