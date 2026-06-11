# 长截图真实帧序列回归集

每个子目录是一个用例:

```
scroll_cases/
└── chrome_baidu_125dpi/      # 命名:应用_页面_环境特征
    ├── frame_0000.png        # 按顺序喂给 Stitcher 的帧(物理像素)
    ├── frame_0001.png
    ├── ...
    └── expected.png          # 期望成图(人工确认无误后入库)
```

## 采集方法

设好环境变量后正常做一次长截图,帧与成图自动落盘:

```powershell
$env:PIXORA_RECORD_FRAMES = "D:\scroll-record"
.\pixora.exe
# F1 框选 → 工具栏[长截图] → 滚动 → 完成;D:\scroll-record\case_<时间戳>\ 即为用例
```

人工确认 `expected.png` 拼接无误后,把整个目录改名拷入本目录提交。

## 运行

`pixora_tests` 自动扫描本目录全部用例(`stitcher_fixture_test.cpp`),
逐帧重放并与 expected.png 逐像素比对;无用例时测试跳过。
积累原则(见 ARCHITECTURE §10):每修一个 bad case,先把帧序列入库。
