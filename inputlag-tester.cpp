// inputlag-tester.cpp - Auto-detect Monitor Refresh Rate (No Warnings)
// Compile: cl /std:c++17 inputlag-tester.cpp /link dxgi.lib d3d11.lib kernel32.lib user32.lib

#include <windows.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")

using Microsoft::WRL::ComPtr;
using namespace std::chrono;

class DXGICapture {
public:
    int refreshRateHz;

    HRESULT init(int regionX, int regionY, int regionW, int regionH) {
        regionX_ = regionX;
        regionY_ = regionY;
        regionW_ = regionW;
        regionH_ = regionH;
        refreshRateHz = 60; // default fallback

        D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        HRESULT hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            featureLevels, ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            device_.ReleaseAndGetAddressOf(),
            nullptr,
            context_.ReleaseAndGetAddressOf()
        );
        if (FAILED(hr)) {
            printf("[DXGI] ERROR D3D11CreateDevice failed: 0x%X\n", hr);
            return hr;
        }

        ComPtr<IDXGIDevice> dxgiDevice;
        hr = device_.As(&dxgiDevice);
        if (FAILED(hr)) {
            printf("[DXGI] ERROR device.As failed: 0x%X\n", hr);
            return hr;
        }

        ComPtr<IDXGIAdapter> adapter;
        hr = dxgiDevice->GetAdapter(adapter.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            printf("[DXGI] ERROR GetAdapter failed: 0x%X\n", hr);
            return hr;
        }

        ComPtr<IDXGIOutput> output;
        hr = adapter->EnumOutputs(0, output.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            printf("[DXGI] ERROR EnumOutputs failed: 0x%X\n", hr);
            return hr;
        }

        // Detect refresh rate from output modes
        detectRefreshRate(output.Get());

        DXGI_OUTPUT_DESC outputDesc;
        hr = output->GetDesc(&outputDesc);
        if (SUCCEEDED(hr)) {
            int screenWidth = outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left;
            int screenHeight = outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top;
            printf("[DXGI] OK Screen resolution: %d x %d\n", screenWidth, screenHeight);
            printf("[DXGI] OK Detected refresh rate: %d Hz\n", refreshRateHz);

            if (regionX_ == 0 && regionY_ == 0 && regionW_ == 0 && regionH_ == 0) {
                regionW_ = 200;
                regionH_ = 200;
                regionX_ = (screenWidth / 2) - (regionW_ / 2);
                regionY_ = (screenHeight / 2) - (regionH_ / 2);
                printf("[DXGI] OK Auto-region: x=%d y=%d w=%d h=%d (center)\n",
                    regionX_, regionY_, regionW_, regionH_);
            } else {
                if (regionX_ + regionW_ > screenWidth) regionW_ = screenWidth - regionX_;
                if (regionY_ + regionH_ > screenHeight) regionH_ = screenHeight - regionY_;
                printf("[DXGI] OK Capture region: x=%d y=%d w=%d h=%d\n",
                    regionX_, regionY_, regionW_, regionH_);
            }
        }

        ComPtr<IDXGIOutput1> output1;
        hr = output.As(&output1);
        if (FAILED(hr)) {
            printf("[DXGI] ERROR output.As<IDXGIOutput1> failed: 0x%X\n", hr);
            return hr;
        }

        hr = output1->DuplicateOutput(device_.Get(), duplication_.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            printf("[DXGI] ERROR DuplicateOutput failed: 0x%X\n", hr);
            return hr;
        }

        printf("[DXGI] OK Desktop Duplication initialized\n");
        return S_OK;
    }

    HRESULT captureFrameWithTimestamp(uint32_t& checksumOut, int64_t& timestampNsOut) {
        ComPtr<IDXGIResource> desktopResource;
        DXGI_OUTDUPL_FRAME_INFO frameInfo;

        // Capture timestamp BEFORE AcquireNextFrame
        int64_t captureTimeNs = duration_cast<nanoseconds>(
            high_resolution_clock::now().time_since_epoch()
        ).count();

        HRESULT hr = duplication_->AcquireNextFrame(10, &frameInfo, desktopResource.ReleaseAndGetAddressOf());
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            return hr;
        }
        if (FAILED(hr)) {
            return hr;
        }

        ComPtr<ID3D11Texture2D> texture;
        hr = desktopResource.As(&texture);
        if (FAILED(hr)) {
            duplication_->ReleaseFrame();
            return hr;
        }

        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);

        if (!stagingTexture_) {
            D3D11_TEXTURE2D_DESC stagingDesc = desc;
            stagingDesc.Usage = D3D11_USAGE_STAGING;
            stagingDesc.BindFlags = 0;
            stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            hr = device_->CreateTexture2D(&stagingDesc, nullptr, stagingTexture_.ReleaseAndGetAddressOf());
            if (FAILED(hr)) {
                duplication_->ReleaseFrame();
                return hr;
            }
        }

        context_->CopyResource(stagingTexture_.Get(), texture.Get());

        D3D11_MAPPED_SUBRESOURCE mapped;
        hr = context_->Map(stagingTexture_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr)) {
            duplication_->ReleaseFrame();
            return hr;
        }

        uint32_t checksum = checksumRegion(
            (uint8_t*)mapped.pData,
            mapped.RowPitch,
            regionX_, regionY_, regionW_, regionH_
        );

        context_->Unmap(stagingTexture_.Get(), 0);
        duplication_->ReleaseFrame();

        checksumOut = checksum;
        timestampNsOut = captureTimeNs;
        return S_OK;
    }

private:
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGIOutputDuplication> duplication_;
    ComPtr<ID3D11Texture2D> stagingTexture_;

    int regionX_, regionY_, regionW_, regionH_;

    uint32_t checksumRegion(uint8_t* data, int pitch, int x, int y, int w, int h) {
        uint32_t sum = 0;
        for (int py = y; py < y + h; py += 4) {
            for (int px = x; px < x + w; px += 4) {
                if (py >= 0 && px >= 0) {
                    uint32_t offset = py * pitch + px * 4;
                    sum ^= *(uint32_t*)(data + offset);
                }
            }
        }
        return sum;
    }

    void detectRefreshRate(IDXGIOutput* output) {
        ComPtr<IDXGIOutput1> output1;
        HRESULT hr = output->QueryInterface(IID_PPV_ARGS(&output1));
        if (FAILED(hr)) {
            printf("[DXGI] INFO Could not get IDXGIOutput1 for refresh rate detection\n");
            return;
        }

        // Query for matching modes
        UINT numModes = 0;
        hr = output1->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, 0, &numModes, nullptr);
        if (FAILED(hr) || numModes == 0) {
            printf("[DXGI] INFO Could not enumerate display modes\n");
            return;
        }

        std::vector<DXGI_MODE_DESC> modes(numModes);
        hr = output1->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, 0, &numModes, modes.data());
        if (FAILED(hr)) {
            printf("[DXGI] INFO Could not get display mode list\n");
            return;
        }

        // Find the highest refresh rate
        int maxRefreshRate = 60;
        for (UINT i = 0; i < numModes; i++) {
            if (modes[i].RefreshRate.Numerator > 0 && modes[i].RefreshRate.Denominator > 0) {
                int refreshRate = modes[i].RefreshRate.Numerator / modes[i].RefreshRate.Denominator;
                if (refreshRate > maxRefreshRate) {
                    maxRefreshRate = refreshRate;
                }
            }
        }

        refreshRateHz = maxRefreshRate;
    }
};

// ==================== Main ====================

static std::vector<int64_t> g_results;

int main(int argc, char** argv) {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    printf("\n========================================\n");
    printf("   inputlag-tester (Auto-Detect Hz)\n");
    printf("========================================\n\n");

    int regionX = 0, regionY = 0, regionW = 0, regionH = 0;
    int numSamples = 210;
    int warmupSamples = 10;
    int intervalMs = 50;
    int dx = 30;

    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "-x") == 0) regionX = atoi(argv[++i]);
        if (strcmp(argv[i], "-y") == 0) regionY = atoi(argv[++i]);
        if (strcmp(argv[i], "-w") == 0) regionW = atoi(argv[++i]);
        if (strcmp(argv[i], "-h") == 0) regionH = atoi(argv[++i]);
        if (strcmp(argv[i], "-n") == 0) numSamples = atoi(argv[++i]);
        if (strcmp(argv[i], "-warmup") == 0) warmupSamples = atoi(argv[++i]);
        if (strcmp(argv[i], "-interval") == 0) intervalMs = atoi(argv[++i]);
        if (strcmp(argv[i], "-dx") == 0) dx = atoi(argv[++i]);
    }

    printf("Config: dx=%d interval=%dms n=%d warmup=%d\n\n", dx, intervalMs, numSamples, warmupSamples);

    // Initialize DXGI
    DXGICapture capture;
    HRESULT hr = capture.init(regionX, regionY, regionW, regionH);
    if (FAILED(hr)) {
        printf("[ERROR] Capture init failed: 0x%X\n", hr);
        return 1;
    }

    double frameTimeMs = 1000.0 / capture.refreshRateHz;
    printf("Monitor: %dHz (%.2f ms per frame)\n\n", capture.refreshRateHz, frameTimeMs);

    printf("[OK] Starting test in 3 seconds...\n");
    Sleep(3000);

    printf("[OK] Measurements starting (pure DXGI-based)...\n\n");

    int sampleCount = 0;
    uint32_t baselineChecksum = 0;
    ULONGLONG nextInputTime = GetTickCount64() + intervalMs;
    auto testStartTime = high_resolution_clock::now();

    // Get baseline checksum
    int64_t dummyTs = 0;
    capture.captureFrameWithTimestamp(baselineChecksum, dummyTs);

    while (sampleCount < numSamples) {
        if (GetTickCount64() >= nextInputTime) {
            // Send input
            INPUT inp = {};
            inp.type = INPUT_MOUSE;
            inp.mi.dx = (sampleCount % 2 == 0) ? dx : -dx;
            inp.mi.dy = 0;
            inp.mi.dwFlags = MOUSEEVENTF_MOVE;

            int64_t inputTimeNs = duration_cast<nanoseconds>(
                high_resolution_clock::now().time_since_epoch()
            ).count();

            SendInput(1, &inp, sizeof(INPUT));

            // Poll for screen change
            bool found = false;
            int waitCount = 0;
            while (waitCount < 1000 && !found) {
                uint32_t checksum = 0;
                int64_t captureTimeNs = 0;
                HRESULT captureHr = capture.captureFrameWithTimestamp(checksum, captureTimeNs);

                if (SUCCEEDED(captureHr) && checksum != baselineChecksum) {
                    // Screen changed, calculate latency
                    int64_t latencyNs = captureTimeNs - inputTimeNs;

                    if (latencyNs > 0 && latencyNs < 500000000) { // 0-500ms reasonable range
                        if (sampleCount >= warmupSamples) {
                            g_results.push_back(latencyNs);
                        }
                        sampleCount++;

                        double latencyMs = latencyNs / 1000000.0;
                        double frames = latencyMs / frameTimeMs;
                        printf("[%d/%d] Latency: %.2f ms (%.2f frames)\n", sampleCount, numSamples, latencyMs, frames);

                        baselineChecksum = checksum;
                        found = true;
                    }
                }

                Sleep(1);
                waitCount++;
            }

            if (!found) {
                sampleCount++;
                printf("[%d/%d] No screen change detected\n", sampleCount, numSamples);
            }

            nextInputTime = GetTickCount64() + intervalMs;
        }

        Sleep(1);

        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    // Calculate stats
    if (g_results.empty()) {
        printf("\n[ERROR] No valid measurements collected\n");
        return 1;
    }

    auto testEndTime = high_resolution_clock::now();
    auto testDuration = duration_cast<milliseconds>(testEndTime - testStartTime).count();

    std::sort(g_results.begin(), g_results.end());

    int64_t minNs = g_results.front();
    int64_t maxNs = g_results.back();
    int64_t sumNs = 0;
    for (auto v : g_results) sumNs += v;
    int64_t avgNs = sumNs / static_cast<int64_t>(g_results.size());

    // Calculate percentiles
    size_t p95_idx = static_cast<size_t>(g_results.size() * 0.95);
    size_t p99_idx = static_cast<size_t>(g_results.size() * 0.99);

    int64_t p95Ns = (p95_idx < g_results.size()) ? g_results[p95_idx] : g_results.back();
    int64_t p99Ns = (p99_idx < g_results.size()) ? g_results[p99_idx] : g_results.back();

    // Calculate median
    int64_t medianNs;
    if (g_results.size() % 2 == 0) {
        medianNs = (g_results[g_results.size()/2 - 1] + g_results[g_results.size()/2]) / 2;
    } else {
        medianNs = g_results[g_results.size()/2];
    }

    // Calculate std deviation
    double variance = 0.0;
    for (auto v : g_results) {
        double diff = static_cast<double>(v) - static_cast<double>(avgNs);
        variance += diff * diff;
    }
    variance /= g_results.size();
    double stdDev = sqrt(variance);

    // Calculate measurement rate
    double measurementRateHz = (g_results.size() * 1000.0) / testDuration;

    // Frame calculations
    double minFrames = (minNs / 1000000.0) / frameTimeMs;
    double avgFrames = (avgNs / 1000000.0) / frameTimeMs;
    double p95Frames = (p95Ns / 1000000.0) / frameTimeMs;
    double maxFrames = (maxNs / 1000000.0) / frameTimeMs;

    printf("\n");
    printf("==========================================\n");
    printf("           RESULTATS FINAUX              \n");
    printf("==========================================\n\n");
    printf("[*] Input -> DXGI Capture Latency (milliseconds)\n");
    printf("    Samples       : %zu\n", g_results.size());
    printf("    Min           : %.2f ms (%.2f frames)\n", minNs / 1000000.0, minFrames);
    printf("    P50 (Median)  : %.2f ms (%.2f frames)\n", medianNs / 1000000.0, (medianNs / 1000000.0) / frameTimeMs);
    printf("    Avg           : %.2f ms (%.2f frames)\n", avgNs / 1000000.0, avgFrames);
    printf("    P95           : %.2f ms (%.2f frames)\n", p95Ns / 1000000.0, p95Frames);
    printf("    P99           : %.2f ms (%.2f frames)\n", p99Ns / 1000000.0, (p99Ns / 1000000.0) / frameTimeMs);
    printf("    Max           : %.2f ms (%.2f frames)\n", maxNs / 1000000.0, maxFrames);
    printf("    Std Dev       : %.2f ms\n\n", stdDev / 1000000.0);

    printf("[*] Monitor Analysis (%dHz)\n", capture.refreshRateHz);
    printf("    Frame time    : %.2f ms\n", frameTimeMs);
    if (avgFrames < 1.0) {
        printf("    Verdict       : EXCELLENT - Under 1 frame lag\n");
    } else if (avgFrames < 2.0) {
        printf("    Verdict       : VERY GOOD - Under 2 frames lag\n");
    } else if (avgFrames < 3.0) {
        printf("    Verdict       : GOOD - Under 3 frames lag\n");
    } else {
        printf("    Verdict       : CHECK SETTINGS - Above 3 frames lag\n");
    }
    printf("\n");

    printf("[*] Test Characteristics\n");
    printf("    Test Duration : %lld ms\n", testDuration);
    printf("    Measurement Rate : %.2f Hz\n", measurementRateHz);
    printf("    Interval      : %d ms\n\n", intervalMs);

    printf("[+] Test completed successfully\n\n");

    // Simple context explanation
    printf("Note: ces mesures représentent le temps entre un mouvement souris\n");
    printf("      et la détection du changement d'image par DXGI sur Windows.\n");
    printf("      Elles n'incluent pas exactement le temps d'affichage réel\n");
    printf("      (scanout + réponse du panneau), qui nécessite un capteur sur l'écran.\n\n");

    return 0;
}
