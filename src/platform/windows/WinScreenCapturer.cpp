#include "platform/windows/WinScreenCapturer.h"

#include "platform/windows/WinCoordinates.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <QScreen>
#include <QtGui/qscreen_platform.h>

#include <spdlog/spdlog.h>

using Microsoft::WRL::ComPtr;

namespace pixora {

namespace {

constexpr UINT kFirstFrameTimeoutMs = 200; // 新会话首帧最多等这么久,超时先用 GDI 顶上

unsigned long hex(HRESULT hr) {
    return static_cast<unsigned long>(hr);
}

HMONITOR monitorOf(QScreen* screen) {
    if (auto* native = screen->nativeInterface<QNativeInterface::QWindowsScreen>()) {
        return native->handle();
    }
    const QPoint phys = wincoord::logicalToPhysical(screen->geometry().center());
    return ::MonitorFromPoint(POINT{phys.x(), phys.y()}, MONITOR_DEFAULTTONULL);
}

class DxgiFrameStream : public IFrameStream {
public:
    DxgiFrameStream(IScreenCapturer& fallback, QScreen* screen, const QRect& regionLocal)
        : fallback_(fallback, screen, regionLocal), monitor_(monitorOf(screen)) {
        const qreal dpr = screen->devicePixelRatio();
        physical_ = QRect(qRound(regionLocal.x() * dpr), qRound(regionLocal.y() * dpr),
                          qRound(regionLocal.width() * dpr), qRound(regionLocal.height() * dpr));
    }

    // 建立(或重建)复制会话:找到该屏所属的适配器与输出,在其上建设备
    bool open() {
        dup_.Reset();
        staging_.Reset();
        context_.Reset();
        device_.Reset();
        if (!monitor_) {
            return false;
        }
        ComPtr<IDXGIFactory1> factory;
        if (FAILED(::CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
            return false;
        }
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT a = 0; factory->EnumAdapters1(a, &adapter) != DXGI_ERROR_NOT_FOUND; ++a) {
            ComPtr<IDXGIOutput> output;
            for (UINT o = 0; adapter->EnumOutputs(o, &output) != DXGI_ERROR_NOT_FOUND; ++o) {
                DXGI_OUTPUT_DESC desc{};
                if (SUCCEEDED(output->GetDesc(&desc)) && desc.Monitor == monitor_) {
                    first_ = true;
                    return openOn(adapter.Get(), output.Get());
                }
            }
        }
        return false;
    }

    QImage grab(bool* changed) override {
        if (useFallback_) {
            return grabFallback(changed);
        }
        if (!dup_ && !open()) {
            spdlog::info("dxgi: reopen failed, falling back to GDI frame grabs");
            useFallback_ = true;
            return grabFallback(changed);
        }
        DXGI_OUTDUPL_FRAME_INFO info{};
        ComPtr<IDXGIResource> resource;
        const HRESULT hr =
            dup_->AcquireNextFrame(first_ ? kFirstFrameTimeoutMs : 0, &info, &resource);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            if (first_) {
                return grabFallback(changed);
            }
            *changed = false; // 桌面自上次以来没有更新
            return {};
        }
        if (FAILED(hr)) {
            // ACCESS_LOST:分辨率/方向变化、全屏独占、安全桌面。本帧用 GDI,下帧重建
            spdlog::info("dxgi: AcquireNextFrame failed {:#x}, reopening", hex(hr));
            dup_.Reset();
            return grabFallback(changed);
        }
        // LastPresentTime 为 0:没有新画面(只有鼠标指针变化)。新会话的
        // 首帧常属此类且纹理全黑——实测如此,不能当画面用
        QImage frame;
        if (info.LastPresentTime.QuadPart != 0) {
            frame = copyFrame(resource.Get());
        }
        dup_->ReleaseFrame();
        if (frame.isNull()) {
            if (first_) {
                // 会话首帧没有画面:用 GDI 取基线,之后只在桌面真有更新时复制
                first_ = false;
                return grabFallback(changed);
            }
            *changed = false;
            return {};
        }
        first_ = false;
        *changed = true;
        return frame;
    }

    bool accelerated() const override { return !useFallback_; }

private:
    bool openOn(IDXGIAdapter1* adapter, IDXGIOutput* output) {
        HRESULT hr = ::D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr,
                                         0, D3D11_SDK_VERSION, &device_, nullptr, &context_);
        if (FAILED(hr)) {
            spdlog::info("dxgi: D3D11CreateDevice failed {:#x}", hex(hr));
            return false;
        }
        ComPtr<IDXGIOutput1> output1;
        if (FAILED(output->QueryInterface(IID_PPV_ARGS(&output1)))) {
            return false;
        }
        hr = output1->DuplicateOutput(device_.Get(), &dup_);
        if (FAILED(hr)) {
            spdlog::info("dxgi: DuplicateOutput failed {:#x}", hex(hr));
            return false;
        }
        DXGI_OUTDUPL_DESC dupDesc{};
        dup_->GetDesc(&dupDesc);
        // 旋转屏的复制图是未旋转方向;非 8 位格式(HDR)需另行转换——都交给 GDI
        if ((dupDesc.Rotation != DXGI_MODE_ROTATION_IDENTITY &&
             dupDesc.Rotation != DXGI_MODE_ROTATION_UNSPECIFIED) ||
            dupDesc.ModeDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
            spdlog::info("dxgi: unsupported output (rotation {}, format {})",
                         static_cast<int>(dupDesc.Rotation),
                         static_cast<int>(dupDesc.ModeDesc.Format));
            dup_.Reset();
            return false;
        }
        region_ = physical_ & QRect(0, 0, static_cast<int>(dupDesc.ModeDesc.Width),
                                    static_cast<int>(dupDesc.ModeDesc.Height));
        if (region_.isEmpty() || (!frameSize_.isEmpty() && region_.size() != frameSize_)) {
            dup_.Reset(); // 重建后区域对不上(分辨率变了):帧尺寸必须全程一致
            return false;
        }
        D3D11_TEXTURE2D_DESC td{};
        td.Width = static_cast<UINT>(region_.width());
        td.Height = static_cast<UINT>(region_.height());
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_STAGING;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(device_->CreateTexture2D(&td, nullptr, &staging_))) {
            dup_.Reset();
            return false;
        }
        return true;
    }

    QImage copyFrame(IDXGIResource* resource) {
        ComPtr<ID3D11Texture2D> texture;
        if (FAILED(resource->QueryInterface(IID_PPV_ARGS(&texture)))) {
            return {};
        }
        const D3D11_BOX box{static_cast<UINT>(region_.left()),
                            static_cast<UINT>(region_.top()),
                            0,
                            static_cast<UINT>(region_.right() + 1),
                            static_cast<UINT>(region_.bottom() + 1),
                            1};
        context_->CopySubresourceRegion(staging_.Get(), 0, 0, 0, 0, texture.Get(), 0, &box);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context_->Map(staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
            return {};
        }
        QImage image(region_.size(), QImage::Format_RGB32);
        const int width = region_.width();
        for (int y = 0; y < region_.height(); ++y) {
            const auto* src = reinterpret_cast<const quint32*>(
                static_cast<const uchar*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch);
            auto* dst = reinterpret_cast<quint32*>(image.scanLine(y));
            for (int x = 0; x < width; ++x) {
                dst[x] = src[x] | 0xFF000000u; // 复制图的 alpha 不保证不透明
            }
        }
        context_->Unmap(staging_.Get(), 0);
        if (frameSize_.isEmpty()) {
            frameSize_ = image.size();
        }
        return image;
    }

    // GDI 兜底;帧尺寸与会话首帧对齐(DPR 取整差一像素也会让拼接器拒收)
    QImage grabFallback(bool* changed) {
        QImage image = fallback_.grab(changed);
        if (image.isNull()) {
            return image;
        }
        if (frameSize_.isEmpty()) {
            frameSize_ = image.size();
        } else if (image.size() != frameSize_) {
            image = image.copy(QRect(QPoint(0, 0), frameSize_));
        }
        return image;
    }

    RegionGrabStream fallback_;
    HMONITOR monitor_ = nullptr;
    QRect physical_; // 该屏内的物理像素区域(按 DPR 换算)
    QRect region_;   // physical_ 裁到输出范围内
    QSize frameSize_;
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGIOutputDuplication> dup_;
    ComPtr<ID3D11Texture2D> staging_;
    bool useFallback_ = false;
    bool first_ = true;
};

} // namespace

std::unique_ptr<IFrameStream> WinScreenCapturer::openFrameStream(QScreen* screen,
                                                                 const QRect& regionLocal) {
    auto stream = std::make_unique<DxgiFrameStream>(*this, screen, regionLocal);
    if (stream->open()) {
        spdlog::info("scroll frames: DXGI desktop duplication");
        return stream;
    }
    spdlog::info("scroll frames: DXGI unavailable, using GDI grabs");
    return QtScreenCapturer::openFrameStream(screen, regionLocal);
}

} // namespace pixora
