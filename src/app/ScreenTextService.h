#pragma once

#include "app/OcrClient.h"
#include "app/TranslateClient.h"
#include "core/text/OcrTypes.h"

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QRect>

namespace pixora {

class PinService;
class PinWindow;
class SettingsService;

// 截图文字智能编排:
// 提取文字 = OCR → 剪贴板;
// 截图翻译 = 先把原图贴回原位(带"翻译中"角标)→ OCR → 逐行翻译 →
//            背景抹除 + 译文回绘 → 贴图原地换图(无感替换)。
// 一次只跑一个任务;服务未配置/任务进行中再触发 → failed 提示。
class ScreenTextService : public QObject {
    Q_OBJECT
public:
    ScreenTextService(const SettingsService* settings, PinService* pins,
                      QObject* parent = nullptr);

    void extractText(const QImage& image);
    void translateInPlace(const QImage& image, const QRect& regionGlobal);

signals:
    void started(const QString& message); // 任务开始(供托盘提示进行中)
    void textCopied(int lineCount);
    void translateFinished(int lineCount);
    void failed(const QString& reason);

private:
    void onOcrFinished(const QList<OcrLine>& lines);
    void onTranslated(const QStringList& translations);
    void onFailed(const QString& reason);
    QString configError(bool needTranslate) const;
    OcrClient::Config ocrConfig() const;
    TranslateClient::Config translateConfig() const;

    const SettingsService* settings_;
    PinService* pins_;
    OcrClient ocr_;
    TranslateClient translator_;

    enum class Job { None, Extract, Translate };
    Job job_ = Job::None;
    QImage image_;
    QList<OcrLine> lines_;
    QPointer<PinWindow> pin_; // 翻译期间的占位贴图;用户随时可关
};

} // namespace pixora
