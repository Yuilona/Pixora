#pragma once

#include "core/text/OcrTypes.h"

#include <QByteArray>
#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>

// 各家 OCR / 翻译服务的响应解析(纯函数,可单测)。
// 网络请求在 app 层客户端;本层只管字节 → 结构化结果。
// 失败时返回空并写 *error(error 可为 nullptr)。
namespace pixora::textproto {

// —— OpenAI 兼容 chat/completions ——
// 取 choices[0].message.content;服务端错误对象 → *error
QString openAiContent(const QByteArray& reply, QString* error);

// OCR 提示词约定的行数组:[{"text":"…","box":[x1,y1,x2,y2]},…]
// 容忍 markdown 代码块包裹与前后杂文;坐标若整体落在 0~1(归一化)
// 则按 imageSize 还原为像素。box 缺失/无效时仍保留行(box 为空矩形)。
QList<OcrLine> parseOcrLines(const QString& content, const QSize& imageSize,
                             QString* error);

// 翻译提示词约定的 JSON 字符串数组(同样容忍代码块包裹)
QStringList parseStringArray(const QString& content, QString* error);

// —— Umi-OCR(PaddleOCR 系)/api/ocr 响应 ——
// code 100 → 行列表;code 101 → 空列表(图中无文字,不算错);其余 → *error
QList<OcrLine> parseUmiOcrReply(const QByteArray& reply, QString* error);

// —— DeepL /v2/translate 响应 ——
QStringList parseDeepLReply(const QByteArray& reply, QString* error);

// —— DeepLX(自托管)/translate 响应 ——
// 成功:{"code":200,"data":"逐行以\n分隔的译文"} → 按 \n 拆回行;
// 失败:code != 200,取 message(DeepLX 对多行文本整体翻译,
// 行对应靠 DeepL 保留换行的行为,偶有合并由调用方补齐对齐)
QStringList parseDeepLXReply(const QByteArray& reply, QString* error);

// —— 百度翻译通用 API 响应 ——
QStringList parseBaiduReply(const QByteArray& reply, QString* error);

} // namespace pixora::textproto
