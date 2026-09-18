#include "src/media/video_preview.h"
#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;
struct VideoPreview::Impl
{
    std::mutex mutex;
    std::condition_variable wake;
    bool stop = false, ready = false, failed = false;
    UINT32 width = 0, height = 0;
    std::vector<BYTE> pixels;
    std::thread worker;

    bool WaitUntil(Clock::time_point due)
    {
        std::unique_lock lock(mutex);
        return !wake.wait_until(lock, due, [this] { return stop; });
    }

    HRESULT Decode(const std::filesystem::path& path)
    {
        ComPtr<IMFAttributes> attributes;
        HRESULT hr = MFCreateAttributes(&attributes, 1);
        if (FAILED(hr)) return hr;
        attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        ComPtr<IMFSourceReader> reader;
        hr = MFCreateSourceReaderFromURL(path.c_str(), attributes.Get(), &reader);
        if (FAILED(hr)) return hr;
        // No audio samples are requested or rendered.
        hr = reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
        if (FAILED(hr)) return hr;
        hr = reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
        if (FAILED(hr)) return hr;
        ComPtr<IMFMediaType> requested;
        hr = MFCreateMediaType(&requested);
        if (FAILED(hr)) return hr;
        requested->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        requested->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, requested.Get());
        if (FAILED(hr)) return hr;
        auto start = Clock::now();
        LONGLONG first = -1, last = 0, duration = 333333;
        bool seen = false;
        while (WaitUntil(Clock::now()))
        {
            DWORD flags = 0;
            LONGLONG timestamp = 0;
            ComPtr<IMFSample> sample;
            hr = reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                nullptr, &flags, &timestamp, &sample);
            if (FAILED(hr)) return hr;
            if (flags & MF_SOURCE_READERF_ERROR) return E_FAIL;
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            {
                if (!seen) return E_FAIL;
                if (!WaitUntil(start + std::chrono::nanoseconds((last - first + duration) * 100))) return S_OK;
                PROPVARIANT position{};
                position.vt = VT_I8;
                position.hVal.QuadPart = 0;
                hr = reader->SetCurrentPosition(GUID_NULL, position);
                if (FAILED(hr)) return hr;
                start = Clock::now();
                first = -1; seen = false;
                continue;
            }
            if (!sample) continue;
            ComPtr<IMFMediaType> type;
            hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &type);
            if (FAILED(hr)) return hr;
            UINT32 w = 0, h = 0;
            hr = MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &w, &h);
            if (FAILED(hr) || !w || !h || w > 8192 || h > 8192) return E_INVALIDARG;
            ComPtr<IMFMediaBuffer> buffer;
            hr = sample->ConvertToContiguousBuffer(&buffer);
            if (FAILED(hr)) return hr;
            std::vector<BYTE> frame(static_cast<size_t>(w) * h * 4);
            ComPtr<IMF2DBuffer> twoD;
            BYTE* scan = nullptr;
            LONG pitch = 0;
            if (SUCCEEDED(buffer.As(&twoD)))
            {
                hr = twoD->Lock2D(&scan, &pitch);
                if (FAILED(hr)) return hr;
                for (UINT32 y = 0; y < h; ++y)
                    std::memcpy(frame.data() + static_cast<size_t>(y) * w * 4,
                        scan + static_cast<ptrdiff_t>(y) * pitch, static_cast<size_t>(w) * 4);
                twoD->Unlock2D();
            }
            else
            {
                UINT32 stride = 0;
                if (SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride))) pitch = static_cast<LONG>(stride);
                else if (FAILED(MFGetStrideForBitmapInfoHeader(MFVideoFormat_RGB32.Data1, w, &pitch))) return E_FAIL;
                DWORD length = 0;
                hr = buffer->Lock(&scan, nullptr, &length);
                if (FAILED(hr)) return hr;
                const size_t rowBytes = static_cast<size_t>(pitch < 0 ? -static_cast<LONGLONG>(pitch) : pitch);
                if (rowBytes < static_cast<size_t>(w) * 4 || rowBytes * h > length)
                { buffer->Unlock(); return E_FAIL; }
                if (pitch < 0) scan += rowBytes * (h - 1);
                for (UINT32 y = 0; y < h; ++y)
                    std::memcpy(frame.data() + static_cast<size_t>(y) * w * 4,
                        scan + static_cast<ptrdiff_t>(y) * pitch, static_cast<size_t>(w) * 4);
                buffer->Unlock();
            }
            if (!seen) { first = timestamp; start = Clock::now(); }
            if (!WaitUntil(start + std::chrono::nanoseconds((timestamp - first) * 100))) return S_OK;
            if (FAILED(sample->GetSampleDuration(&duration)) || duration <= 0)
                duration = seen && timestamp > last ? timestamp - last : 333333;
            last = timestamp; seen = true;
            {
                std::lock_guard lock(mutex);
                pixels.swap(frame); width = w; height = h; ready = true;
            }
        }
        return S_OK;
    }
};

VideoPreview::VideoPreview(const std::filesystem::path& path) : impl_(std::make_unique<Impl>())
{
    impl_->worker = std::thread([state = impl_.get(), path] {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool com = SUCCEEDED(hr);
        bool mf = false;
        if (com) { hr = MFStartup(MF_VERSION); mf = SUCCEEDED(hr); }
        if (mf)
        {
            try { hr = state->Decode(path); }
            catch (...) { hr = E_FAIL; }
            MFShutdown();
        }
        if (com) CoUninitialize();
        if (FAILED(hr))
        {
            SDL_Log("O2EM-NG: video preview could not load (HRESULT %08lX)", static_cast<unsigned long>(hr));
            std::lock_guard lock(state->mutex);
            state->failed = true;
        }
    });
}

VideoPreview::~VideoPreview()
{
    { std::lock_guard lock(impl_->mutex); impl_->stop = true; }
    impl_->wake.notify_all();
    impl_->worker.join();
}

bool VideoPreview::Failed() const
{
    std::lock_guard lock(impl_->mutex);
    return impl_->failed;
}

bool VideoPreview::Update(SDL_Renderer* renderer, SDL_Texture*& texture)
{
    std::lock_guard lock(impl_->mutex);
    if (!impl_->ready) return false;
    float w = 0, h = 0;
    if (texture) SDL_GetTextureSize(texture, &w, &h);
    if (!texture || w != impl_->width || h != impl_->height)
    {
        SDL_DestroyTexture(texture);
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888,
            SDL_TEXTUREACCESS_STREAMING, impl_->width, impl_->height);
    }
    impl_->ready = false;
    return texture && SDL_UpdateTexture(texture, nullptr, impl_->pixels.data(), impl_->width * 4);
}
