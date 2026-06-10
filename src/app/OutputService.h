#pragma once

#include <QImage>
#include <QString>

namespace pixora {

// 统一输出管线:剪贴板 / 文件;贴图、上传等出口随后续里程碑加入
// (见 ARCHITECTURE §6)。
class OutputService {
public:
    void copyToClipboard(const QImage& image);

    // 弹出另存对话框;返回保存路径,用户取消返回空串。
    QString saveWithDialog(const QImage& image);

    // 默认保存目录(图片库),文件名含时间戳。
    QString suggestedFileName() const;
};

} // namespace pixora
