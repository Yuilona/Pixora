#include "core/text/TextProtocols.h"

#include <catch2/catch_test_macros.hpp>

using namespace pixora;
using namespace pixora::textproto;

TEST_CASE("openai content extracted from chat completions reply", "[textproto]") {
    const QByteArray reply = R"({
        "choices": [{"message": {"role": "assistant", "content": "hello"}}]
    })";
    QString error;
    CHECK(openAiContent(reply, &error) == QStringLiteral("hello"));
    CHECK(error.isEmpty());
}

TEST_CASE("openai error object surfaces message", "[textproto]") {
    const QByteArray reply =
        R"({"error": {"message": "Invalid API key", "type": "auth"}})";
    QString error;
    CHECK(openAiContent(reply, &error).isEmpty());
    CHECK(error.contains(QStringLiteral("Invalid API key")));
}

TEST_CASE("ocr lines parsed with absolute pixel boxes and code fences",
          "[textproto]") {
    const QString content = QStringLiteral(
        "```json\n"
        "[{\"text\":\"Hello\",\"box\":[10,20,110,44]},"
        " {\"text\":\"World\",\"box\":[10,50,90,74]}]\n"
        "```");
    QString error;
    const auto lines = parseOcrLines(content, QSize(200, 100), &error);
    REQUIRE(lines.size() == 2);
    CHECK(lines[0].text == QStringLiteral("Hello"));
    CHECK(lines[0].box == QRect(QPoint(10, 20), QPoint(110, 44)));
    CHECK(lines[1].box == QRect(QPoint(10, 50), QPoint(90, 74)));
    CHECK(error.isEmpty());
}

TEST_CASE("ocr normalized coordinates scaled to image size", "[textproto]") {
    const QString content = QStringLiteral(
        R"([{"text":"A","box":[0.1,0.2,0.5,0.4]}])");
    QString error;
    const auto lines = parseOcrLines(content, QSize(1000, 500), &error);
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].box == QRect(QPoint(100, 100), QPoint(500, 200)));
}

TEST_CASE("ocr line without box kept with null rect", "[textproto]") {
    const QString content = QStringLiteral(R"([{"text":"no box"}])");
    QString error;
    const auto lines = parseOcrLines(content, QSize(100, 100), &error);
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].box.isNull());
}

TEST_CASE("string array parsed through code fences", "[textproto]") {
    QString error;
    const QStringList list = parseStringArray(
        QStringLiteral("结果如下:\n```\n[\"你好\",\"世界\"]\n```"), &error);
    REQUIRE(list.size() == 2);
    CHECK(list[0] == QStringLiteral("你好"));
    CHECK(error.isEmpty());
}

TEST_CASE("umi-ocr reply parsed: corners become bounding rect", "[textproto]") {
    const QByteArray reply = R"({
        "code": 100,
        "data": [{"text": "测试", "score": 0.99,
                  "box": [[10,10],[110,12],[110,40],[10,38]]}]
    })";
    QString error;
    const auto lines = parseUmiOcrReply(reply, &error);
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].text == QStringLiteral("测试"));
    CHECK(lines[0].box == QRect(QPoint(10, 10), QPoint(110, 40)));
    CHECK(error.isEmpty());
}

TEST_CASE("umi-ocr code 101 means no text, not an error", "[textproto]") {
    QString error;
    const auto lines =
        parseUmiOcrReply(R"({"code": 101, "data": ""})", &error);
    CHECK(lines.isEmpty());
    CHECK(error.isEmpty());
}

TEST_CASE("umi-ocr error code surfaces message", "[textproto]") {
    QString error;
    const auto lines = parseUmiOcrReply(
        R"({"code": 902, "data": "engine not ready"})", &error);
    CHECK(lines.isEmpty());
    CHECK(error.contains(QStringLiteral("902")));
    CHECK(error.contains(QStringLiteral("engine not ready")));
}

TEST_CASE("deepl reply parsed in order", "[textproto]") {
    const QByteArray reply = R"({
        "translations": [
            {"detected_source_language": "EN", "text": "你好"},
            {"detected_source_language": "EN", "text": "世界"}
        ]
    })";
    QString error;
    const QStringList list = parseDeepLReply(reply, &error);
    REQUIRE(list.size() == 2);
    CHECK(list[1] == QStringLiteral("世界"));
    CHECK(error.isEmpty());
}

TEST_CASE("deepl error message surfaced", "[textproto]") {
    QString error;
    const QStringList list =
        parseDeepLReply(R"({"message": "Wrong endpoint"})", &error);
    CHECK(list.isEmpty());
    CHECK(error.contains(QStringLiteral("Wrong endpoint")));
}

TEST_CASE("line batches split by item count keeping order", "[textproto]") {
    QStringList lines;
    for (int i = 0; i < 7; ++i) {
        lines << QStringLiteral("line%1").arg(i);
    }
    const auto batches = splitLineBatches(lines, 3, -1, -1);
    REQUIRE(batches.size() == 3);
    CHECK(batches[0].size() == 3);
    CHECK(batches[2].size() == 1);
    CHECK(batches[2][0] == QStringLiteral("line6"));
}

TEST_CASE("line batches split by char budget, oversized line own batch",
          "[textproto]") {
    const QStringList lines = {QStringLiteral("aaaa"), QStringLiteral("bbbb"),
                               QString(40, QLatin1Char('c')),
                               QStringLiteral("dddd")};
    // 上限 12 字符(含 \n 连接符):aaaa+bbbb 恰好一批,超长 c 行独占
    const auto batches = splitLineBatches(lines, -1, -1, 12);
    REQUIRE(batches.size() == 3);
    CHECK(batches[0] == QStringList({QStringLiteral("aaaa"), QStringLiteral("bbbb")}));
    CHECK(batches[1].size() == 1);
    CHECK(batches[1][0].size() == 40);
    CHECK(batches[2] == QStringList{QStringLiteral("dddd")});
}

TEST_CASE("line batches single batch when under all limits", "[textproto]") {
    const QStringList lines = {QStringLiteral("a"), QStringLiteral("b")};
    const auto batches = splitLineBatches(lines, 50, 110 * 1024, -1);
    REQUIRE(batches.size() == 1);
    CHECK(batches[0] == lines);
}

TEST_CASE("deeplx reply split back into lines", "[textproto]") {
    const QByteArray reply =
        R"({"code": 200, "id": 123, "data": "你好\n世界", "method": "Free"})";
    QString error;
    const QStringList list = parseDeepLXReply(reply, &error);
    REQUIRE(list.size() == 2);
    CHECK(list[0] == QStringLiteral("你好"));
    CHECK(list[1] == QStringLiteral("世界"));
    CHECK(error.isEmpty());
}

TEST_CASE("deeplx error code and message surfaced", "[textproto]") {
    QString error;
    const QStringList list = parseDeepLXReply(
        R"({"code": 429, "message": "Too many requests"})", &error);
    CHECK(list.isEmpty());
    CHECK(error.contains(QStringLiteral("429")));
    CHECK(error.contains(QStringLiteral("Too many requests")));
}

TEST_CASE("deeplx invalid json rejected", "[textproto]") {
    QString error;
    const QStringList list = parseDeepLXReply("<html>busy</html>", &error);
    CHECK(list.isEmpty());
    CHECK_FALSE(error.isEmpty());
}

TEST_CASE("baidu reply parsed per line", "[textproto]") {
    const QByteArray reply = R"({
        "from": "en", "to": "zh",
        "trans_result": [{"src": "Hello", "dst": "你好"},
                          {"src": "World", "dst": "世界"}]
    })";
    QString error;
    const QStringList list = parseBaiduReply(reply, &error);
    REQUIRE(list.size() == 2);
    CHECK(list[0] == QStringLiteral("你好"));
    CHECK(error.isEmpty());
}

TEST_CASE("baidu error code surfaced", "[textproto]") {
    QString error;
    const QStringList list = parseBaiduReply(
        R"({"error_code": "54001", "error_msg": "Invalid Sign"})", &error);
    CHECK(list.isEmpty());
    CHECK(error.contains(QStringLiteral("54001")));
    CHECK(error.contains(QStringLiteral("Invalid Sign")));
}
