// inputlag-tester.cpp - Version avec overlay temps réel optionnel
// OVERLAY: Affichage en temps réel avec --overlay (désactivé par défaut)
// OVERLAY-SIZE: Facteur de dimensionnement pour l'overlay
// 
// Compile: cl /std:c++17 /W4 /O2 /EHsc inputlag-tester.cpp /link dxgi.lib d3d11.lib kernel32.lib user32.lib advapi32.lib gdi32.lib

#include <windows.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <winuser.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "gdi32.lib")

using Microsoft::WRL::ComPtr;
using namespace std::chrono;

// Overlay state
static HWND g_overlayWindow = nullptr;
static int g_overlayCurrentRun = 0;
static int g_overlayTotalRuns = 0;
static int g_overlaySampleCount = 0;
static int g_overlayTotalSamples = 0;
static std::string g_overlayLastError = "";
static double g_overlayLastLatency = 0.0;
static double g_overlayFrameTimeMs = 0.0;
static bool g_showOverlay = false;  // Désactivé par défaut
static double g_overlaySizeFactor = 1.0;  // Facteur de dimensionnement (défaut: 1.0)

// Infos système / run
static std::string g_cpuName;
static std::string g_gpuName;
static std::string g_monitorName;
static std::string g_gpuVram;
static std::string g_gpuDriverVersion;
static int g_monitorHz = 0;
static double g_totalRamMB = 0.0;
static std::string g_osVersion;
static std::string g_cpuCores;

// Infos carte mère / BIOS
static std::string g_mbVendor;
static std::string g_mbProduct;
static std::string g_biosVersion;

// Résultats + sortie fichier
static std::vector<int64_t> g_results;
static std::vector<std::vector<int64_t>> g_allResults;
static std::string g_outputFilePath;

// Configuration multi-run
static int g_nbRun = 3;
static int g_pauseSeconds = 3;
static bool g_verbose = false;
static bool g_diagnostic = false;
static int g_maxWaitMs = 500;

// Statistiques de diagnostic
struct DiagnosticStats {
    int totalAttempts = 0;
    int successfulCaptures = 0;
    int timeouts = 0;
    int sameChecksum = 0;
    int mouseUpdatesOnly = 0;
    int acquireErrors = 0;
    int checksumChanges = 0;
    int exclusiveScreenDetected = 0;
};

static DiagnosticStats g_diagStats;

// -------- Overlay Window Procedure --------
LRESULT CALLBACK OverlayWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Fond noir semi-transparent
            RECT rect;
            GetClientRect(hwnd, &rect);
            HBRUSH hbrush = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdc, &rect, hbrush);
            DeleteObject(hbrush);

            // Texte blanc
            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkMode(hdc, TRANSPARENT);

            // Police monospace - dimensionnée selon le facteur
            int fontSize = static_cast<int>(14.0 * g_overlaySizeFactor);
            HFONT hfont = CreateFontA(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
            HFONT oldFont = (HFONT)SelectObject(hdc, hfont);

            int x = static_cast<int>(10.0 * g_overlaySizeFactor);
            int y = static_cast<int>(5.0 * g_overlaySizeFactor);
            int lineHeight = static_cast<int>(18.0 * g_overlaySizeFactor);

            // Construire le texte
            char buffer[512];
            sprintf_s(buffer, sizeof(buffer), "RUN %d/%d | Sample %d/%d",
                     g_overlayCurrentRun, g_overlayTotalRuns,
                     g_overlaySampleCount, g_overlayTotalSamples);
            TextOutA(hdc, x, y, buffer, (int)strlen(buffer));
            y += lineHeight;

            if (g_overlayLastLatency > 0.0) {
                double frames = g_overlayFrameTimeMs > 0 ? (g_overlayLastLatency / g_overlayFrameTimeMs) : 0.0;
                sprintf_s(buffer, sizeof(buffer), "Last: %.2f ms (%.2f fr)",
                         g_overlayLastLatency, frames);
                TextOutA(hdc, x, y, buffer, (int)strlen(buffer));
                y += lineHeight;
            }

            if (!g_overlayLastError.empty()) {
                // Afficher erreur en rouge
                SetTextColor(hdc, RGB(255, 100, 100));
                sprintf_s(buffer, sizeof(buffer), "Error: %s", g_overlayLastError.c_str());
                TextOutA(hdc, x, y, buffer, (int)strlen(buffer));
                y += lineHeight;
                SetTextColor(hdc, RGB(255, 255, 255));
            } else {
                sprintf_s(buffer, sizeof(buffer), "Status: OK");
                TextOutA(hdc, x, y, buffer, (int)strlen(buffer));
                y += lineHeight;
            }

            SelectObject(hdc, oldFont);
            DeleteObject(hfont);
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Créer la fenêtre overlay
void CreateOverlayWindow() {
    const char* className = "InputLagOverlay";

    // Enregistrer la classe de fenêtre
    WNDCLASSA wc = {};
    wc.lpfnWndProc = OverlayWindowProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = className;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassA(&wc);

    // Créer la fenêtre en haut-droite avec dimensionnement selon le facteur
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);

    int windowWidth = static_cast<int>(350.0 * g_overlaySizeFactor);
    int windowHeight = static_cast<int>(100.0 * g_overlaySizeFactor);
    int x = screenWidth - windowWidth - 20;
    int y = 20;

    g_overlayWindow = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        className,
        "InputLag Monitor",
        WS_POPUP | WS_VISIBLE,
        x, y, windowWidth, windowHeight,
        nullptr, nullptr, GetModuleHandleA(nullptr), nullptr
    );

    if (g_overlayWindow) {
        printf("[OVERLAY] Window created at (%d, %d) size %dx%d (scale: %.2f)\n", 
               x, y, windowWidth, windowHeight, g_overlaySizeFactor);
        // Opacité 80% (204/255)
        SetLayeredWindowAttributes(g_overlayWindow, 0, 204, LWA_ALPHA);
    }
}

// Mettre à jour l'overlay
void UpdateOverlay() {
    if (g_overlayWindow && IsWindow(g_overlayWindow)) {
        InvalidateRect(g_overlayWindow, nullptr, FALSE);
        UpdateWindow(g_overlayWindow);
    }
}

// Traiter les messages Windows
void ProcessWindowMessages() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

// Forward declarations
std::string GetCpuLogicalCoresString();
std::string GetOsVersionString();
std::string GetGpuDriverVersion();

// -------- Fonction d'analyse des arguments --------
void PrintUsage(const char* programName) {
    printf("\n=== Input Lag Tester - Usage ===\n");
    printf(" %s [OPTIONS]\n\n", programName);
    printf("Options:\n");
    printf(" -x NUM         Region X coordinate (default: auto-center)\n");
    printf(" -y NUM         Region Y coordinate (default: auto-center)\n");
    printf(" -w NUM         Region width (default: 200)\n");
    printf(" -h NUM         Region height (default: 200)\n");
    printf(" -n NUM         Number of samples (default: 210)\n");
    printf(" -warmup NUM    Warmup samples (default: 10)\n");
    printf(" -interval NUM  Interval between tests in ms (default: 50)\n");
    printf(" -dx NUM        Mouse movement distance (default: 30)\n");
    printf(" -o FILE        Output file path (default: none)\n");
    printf(" --nb-run NUM   Number of test runs (default: 3)\n");
    printf(" --pause SEC    Pause between runs in seconds (default: 3)\n");
    printf(" --timeout MS   Max wait time for screen change in ms (default: 500)\n");
    printf(" --overlay      Enable overlay window (disabled by default)\n");
    printf(" --overlay-size FACTOR  Overlay size scaling factor (default: 1.0)\n");
    printf(" -v             Verbose mode - display each sample\n");
    printf(" --diagnostic   Enable diagnostic mode (detailed logs)\n");
    printf(" --help         Show this help message\n\n");
    printf("Examples:\n");
    printf(" %s --diagnostic -n 50\n", programName);
    printf(" %s --diagnostic --overlay -n 50\n", programName);
    printf(" %s --overlay --overlay-size 1.5 -n 50\n", programName);
    printf(" %s --nb-run 5 --pause 2 -v --overlay --overlay-size 0.8\n", programName);
}

bool ParseCommandLineArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "/?") {
            PrintUsage(argv[0]);
            return false;
        }
        else if (arg == "-v") {
            g_verbose = true;
            printf("[CONFIG] Verbose mode enabled\n");
        }
        else if (arg == "--diagnostic") {
            g_diagnostic = true;
            g_verbose = true;
            printf("[CONFIG] Diagnostic mode enabled\n");
        }
        else if (arg == "--overlay") {
            g_showOverlay = true;
            printf("[CONFIG] Overlay enabled\n");
        }
        else if (arg == "--overlay-size" && i + 1 < argc) {
            g_overlaySizeFactor = std::atof(argv[++i]);
            if (g_overlaySizeFactor <= 0.0) {
                printf("[ERROR] --overlay-size must be > 0\n");
                return false;
            }
            printf("[CONFIG] Overlay size factor set to %.2f\n", g_overlaySizeFactor);
        }
        else if (arg == "--timeout" && i + 1 < argc) {
            g_maxWaitMs = std::atoi(argv[++i]);
            if (g_maxWaitMs < 50) {
                printf("[ERROR] --timeout must be >= 50 ms\n");
                return false;
            }
            printf("[CONFIG] Max wait timeout set to %d ms\n", g_maxWaitMs);
        }
        else if (arg == "--nb-run" && i + 1 < argc) {
            g_nbRun = std::atoi(argv[++i]);
            if (g_nbRun < 1) {
                printf("[ERROR] --nb-run must be >= 1\n");
                return false;
            }
            printf("[CONFIG] NB_RUN set to %d\n", g_nbRun);
        }
        else if (arg == "--pause" && i + 1 < argc) {
            g_pauseSeconds = std::atoi(argv[++i]);
            if (g_pauseSeconds < 0) {
                printf("[ERROR] --pause must be >= 0\n");
                return false;
            }
            printf("[CONFIG] PAUSE set to %d seconds\n", g_pauseSeconds);
        }
    }
    return true;
}

// -------- Fonction de calcul des moyennes --------
void PrintAverageResults() {
    if (g_allResults.empty()) {
        printf("\n[STATS] No results to average\n");
        return;
    }

    printf("\n");
    printf("==========================================\n");
    printf(" AVERAGE RESULTS OVER %d RUNS\n", (int)g_allResults.size());
    printf("==========================================\n\n");

    std::vector<int64_t> allLatencies;
    for (const auto& runResults : g_allResults) {
        for (const auto& latency : runResults) {
            allLatencies.push_back(latency);
        }
    }

    if (allLatencies.empty()) {
        printf("[STATS] No latency data collected\n");
        return;
    }

    std::sort(allLatencies.begin(), allLatencies.end());

    int64_t minNs = allLatencies.front();
    int64_t maxNs = allLatencies.back();
    int64_t sumNs = 0;
    for (auto v : allLatencies) sumNs += v;
    int64_t avgNs = sumNs / static_cast<int64_t>(allLatencies.size());

    size_t p95_idx = static_cast<size_t>(allLatencies.size() * 0.95);
    size_t p99_idx = static_cast<size_t>(allLatencies.size() * 0.99);
    int64_t p95Ns = (p95_idx < allLatencies.size()) ? allLatencies[p95_idx] : allLatencies.back();
    int64_t p99Ns = (p99_idx < allLatencies.size()) ? allLatencies[p99_idx] : allLatencies.back();

    int64_t medianNs;
    if (allLatencies.size() % 2 == 0) {
        medianNs = (allLatencies[allLatencies.size()/2 - 1] + allLatencies[allLatencies.size()/2]) / 2;
    } else {
        medianNs = allLatencies[allLatencies.size()/2];
    }

    double variance = 0.0;
    for (auto v : allLatencies) {
        double diff = static_cast<double>(v) - static_cast<double>(avgNs);
        variance += diff * diff;
    }
    variance /= allLatencies.size();
    double stdDev = sqrt(variance);

    double frameTimeMs = 1000.0 / g_monitorHz;

    printf("[*] System Information\n");
    printf(" CPU       : %s\n", g_cpuName.c_str());
    printf(" CPU Cores : %s\n", g_cpuCores.c_str());
    printf(" RAM       : %.0f MB\n", g_totalRamMB);
    printf(" OS        : %s\n", g_osVersion.c_str());
    printf(" MB        : %s %s\n", g_mbVendor.c_str(), g_mbProduct.c_str());
    printf(" BIOS      : %s\n", g_biosVersion.c_str());
    printf(" GPU       : %s (%s)\n", g_gpuName.empty() ? "Unknown" : g_gpuName.c_str(),
           g_gpuVram.empty() ? "Unknown" : g_gpuVram.c_str());
    printf(" GPU Driver: %s\n", g_gpuDriverVersion.empty() ? "Unknown" : g_gpuDriverVersion.c_str());
    printf(" Monitor   : %s @ %d Hz\n\n", g_monitorName.empty() ? "Unknown" : g_monitorName.c_str(), g_monitorHz);

    printf("[*] Global Statistics Over %zu Measurements\n", allLatencies.size());
    printf(" Samples   : %zu\n", allLatencies.size());
    printf(" Min       : %.2f ms (%.2f frames)\n", minNs / 1000000.0, (minNs / 1000000.0) / frameTimeMs);
    printf(" P50 (Med) : %.2f ms (%.2f frames)\n", medianNs / 1000000.0, (medianNs / 1000000.0) / frameTimeMs);
    printf(" Avg       : %.2f ms (%.2f frames)\n", avgNs / 1000000.0, (avgNs / 1000000.0) / frameTimeMs);
    printf(" P95       : %.2f ms (%.2f frames)\n", p95Ns / 1000000.0, (p95Ns / 1000000.0) / frameTimeMs);
    printf(" P99       : %.2f ms (%.2f frames)\n", p99Ns / 1000000.0, (p99Ns / 1000000.0) / frameTimeMs);
    printf(" Max       : %.2f ms (%.2f frames)\n", maxNs / 1000000.0, (maxNs / 1000000.0) / frameTimeMs);
    printf(" Std Dev   : %.2f ms\n\n", stdDev / 1000000.0);

    double avgFrames = (avgNs / 1000000.0) / frameTimeMs;
    printf("[*] Monitor Analysis (%dHz)\n", g_monitorHz);
    printf("    Frame time: %.2f ms\n", frameTimeMs);
    if (avgFrames < 1.0) {
        printf("    Verdict   : EXCELLENT - Under 1 frame of lag\n");
    } else if (avgFrames < 2.0) {
        printf("    Verdict   : VERY GOOD - Under 2 frames of lag\n");
    } else if (avgFrames < 3.0) {
        printf("    Verdict   : GOOD - Under 3 frames of lag\n");
    } else {
        printf("    Verdict   : CHECK SETTINGS - Above 3 frames of lag\n");
    }
    printf("\n");

    printf("[*] Per-Run Statistics\n");
    for (size_t runIdx = 0; runIdx < g_allResults.size(); runIdx++) {
        const auto& runResults = g_allResults[runIdx];
        if (runResults.empty()) continue;

        std::vector<int64_t> sorted = runResults;
        std::sort(sorted.begin(), sorted.end());

        int64_t runMin = sorted.front();
        int64_t runMax = sorted.back();
        int64_t runSum = 0;
        for (auto v : sorted) runSum += v;
        int64_t runAvg = runSum / static_cast<int64_t>(sorted.size());

        int64_t runP50 = sorted[sorted.size() / 2];
        size_t p99_idx_run = static_cast<size_t>(sorted.size() * 0.99);
        int64_t runP99 = (p99_idx_run < sorted.size()) ? sorted[p99_idx_run] : sorted.back();

        printf(" Run %zu: Min=%.2f, P50=%.2f, Avg=%.2f, P99=%.2f, Max=%.2f ms, Samples=%zu\n",
               runIdx + 1,
               runMin / 1000000.0,
               runP50 / 1000000.0,
               runAvg / 1000000.0,
               runP99 / 1000000.0,
               runMax / 1000000.0,
               sorted.size());
    }

    printf("\n");
}

// Affichage des statistiques de diagnostic
void PrintDiagnosticStats() {
    if (!g_diagnostic) return;

    int maxAttempts = (g_diagStats.totalAttempts > 0) ? g_diagStats.totalAttempts : 1;

    printf("\n");
    printf("==========================================\n");
    printf(" DIAGNOSTIC STATISTICS\n");
    printf("==========================================\n");
    printf(" Total capture attempts    : %d\n", g_diagStats.totalAttempts);
    printf(" Successful captures       : %d (%.1f%%%%)\n", 
           g_diagStats.successfulCaptures,
           100.0 * g_diagStats.successfulCaptures / maxAttempts);
    printf(" DXGI timeouts             : %d (%.1f%%%%)\n", 
           g_diagStats.timeouts,
           100.0 * g_diagStats.timeouts / maxAttempts);
    printf(" Same checksum (no change) : %d (%.1f%%%%)\n", 
           g_diagStats.sameChecksum,
           100.0 * g_diagStats.sameChecksum / maxAttempts);
    printf(" Mouse-only updates        : %d (%.1f%%%%)\n", 
           g_diagStats.mouseUpdatesOnly,
           100.0 * g_diagStats.mouseUpdatesOnly / maxAttempts);
    printf(" Acquire errors            : %d (%.1f%%%%)\n", 
           g_diagStats.acquireErrors,
           100.0 * g_diagStats.acquireErrors / maxAttempts);
    printf(" Checksum changes detected : %d\n", g_diagStats.checksumChanges);
    printf(" No screen change detected : %d (Exclusive screen mode?)\n", g_diagStats.exclusiveScreenDetected);
    printf("\n");

    if (g_diagStats.exclusiveScreenDetected > g_diagStats.totalAttempts * 0.1) {
        printf("[DIAG] WARNING: Frequent 'no screen change detected' (>10%%%%)\n");
        printf("       This is NORMAL in exclusive screen mode (fullscreen games).\n");
        printf("       The game's exclusive mode window isn't captured by DXGI.\n");
        printf("       Solutions:\n");
        printf("       1. Run game in Windowed or Windowed Fullscreen mode\n");
        printf("       2. Disable exclusive fullscreen in game settings\n");
        printf("       3. Use a different test region (desktop overlay area)\n\n");
    }

    if (g_diagStats.sameChecksum > g_diagStats.totalAttempts * 0.3) {
        printf("[DIAG] WARNING: High rate of 'same checksum' (>30%%%%)\n");
        printf("       Possible causes:\n");
        printf("       - Region being monitored doesn't update with mouse moves\n");
        printf("       - Mouse movement too small (try increasing -dx)\n");
        printf("       - Application running doesn't respond to mouse input\n");
        printf("       - Hardware cursor being used (not visible in capture)\n\n");
    }

    if (g_diagStats.timeouts > g_diagStats.totalAttempts * 0.2) {
        printf("[DIAG] WARNING: High timeout rate (>20%%%%)\n");
        printf("       Possible causes:\n");
        printf("       - System under heavy load\n");
        printf("       - Desktop composition disabled\n");
        printf("       - Driver issues\n\n");
    }

    if (g_diagStats.mouseUpdatesOnly > g_diagStats.totalAttempts * 0.2) {
        printf("[DIAG] INFO: High mouse-only update rate (>20%%%%)\n");
        printf("       This is normal - DXGI returns on mouse cursor changes too.\n\n");
    }
}

// -------- Helpers système --------
std::string GetCpuName() {
    HKEY hKey;
    const char* subKey = "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
    const char* valueName = "ProcessorNameString";

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return "Unknown CPU";
    }

    char buffer[256] = {};
    DWORD bufferSize = sizeof(buffer);
    DWORD type = 0;
    LONG ret = RegGetValueA(hKey, nullptr, valueName, RRF_RT_REG_SZ, &type, buffer, &bufferSize);
    RegCloseKey(hKey);

    if (ret != ERROR_SUCCESS) {
        return "Unknown CPU";
    }

    return std::string(buffer);
}

std::string GetOsVersionString() {
    OSVERSIONINFOEXA osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(osvi);

#pragma warning(push)
#pragma warning(disable:4996)
    if (!GetVersionExA((OSVERSIONINFOA*)&osvi)) {
#pragma warning(pop)
        return "Unknown OS";
    }

    char buf[128] = {};
    sprintf_s(buf, sizeof(buf), "Windows %lu.%lu (build %lu)",
              osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
    return std::string(buf);
}

std::string GetCpuLogicalCoresString() {
    SYSTEM_INFO si = {};
    GetSystemInfo(&si);
    char buf[64] = {};
    sprintf_s(buf, sizeof(buf), "%u logical cores", si.dwNumberOfProcessors);
    return std::string(buf);
}

std::string FormatBytesToMB(size_t bytes) {
    double mb = bytes / (1024.0 * 1024.0);
    char buf[64] = {};
    sprintf_s(buf, sizeof(buf), "%.0f MB", mb);
    return std::string(buf);
}

std::string ReadBiosStringValue(const char* valueName) {
    HKEY hKey;
    const char* subKey = "HARDWARE\\DESCRIPTION\\System\\BIOS";

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return "";
    }

    char buffer[256] = {};
    DWORD bufferSize = sizeof(buffer);
    DWORD type = 0;
    LONG ret = RegGetValueA(hKey, nullptr, valueName, RRF_RT_REG_SZ, &type, buffer, &bufferSize);
    RegCloseKey(hKey);

    if (ret != ERROR_SUCCESS) {
        return "";
    }

    return std::string(buffer);
}

std::string GetGpuDriverVersion() {
    HKEY hKey;
    const char* subKey = "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E968-E325-11CE-BFC1-08002BE10318}\\0000";

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return "Unknown";
    }

    char buffer[256] = {};
    DWORD bufferSize = sizeof(buffer);
    DWORD type = 0;

    LONG ret = RegGetValueA(hKey, nullptr, "DriverVersion", RRF_RT_REG_SZ, &type, buffer, &bufferSize);
    RegCloseKey(hKey);

    if (ret != ERROR_SUCCESS) {
        return "Unknown";
    }

    return std::string(buffer);
}

void InitMotherboardAndBiosInfo() {
    g_mbVendor = ReadBiosStringValue("BaseBoardManufacturer");
    g_mbProduct = ReadBiosStringValue("BaseBoardProduct");
    g_biosVersion = ReadBiosStringValue("BIOSVersion");

    if (g_mbVendor.empty()) g_mbVendor = "Unknown";
    if (g_mbProduct.empty()) g_mbProduct = "Unknown";
    if (g_biosVersion.empty()) g_biosVersion = "Unknown";
}

// -------- Classe DXGICapture avec diagnostic --------
class DXGICapture {
public:
    int refreshRateHz;

    HRESULT init(int regionX, int regionY, int regionW, int regionH) {
        regionX_ = regionX;
        regionY_ = regionY;
        regionW_ = regionW;
        regionH_ = regionH;
        refreshRateHz = 60;

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

        DXGI_ADAPTER_DESC adapterDesc;
        hr = adapter->GetDesc(&adapterDesc);
        if (SUCCEEDED(hr)) {
            char gpuName[128] = {};
            size_t converted = 0;
            wcstombs_s(&converted, gpuName, sizeof(gpuName), adapterDesc.Description, _TRUNCATE);
            g_gpuName = gpuName;
            g_gpuVram = FormatBytesToMB(static_cast<size_t>(adapterDesc.DedicatedVideoMemory));
        }

        ComPtr<IDXGIOutput> output;
        hr = adapter->EnumOutputs(0, output.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            printf("[DXGI] ERROR EnumOutputs failed: 0x%X\n", hr);
            return hr;
        }

        detectRefreshRate(output.Get());

        DXGI_OUTPUT_DESC outputDesc;
        hr = output->GetDesc(&outputDesc);
        if (SUCCEEDED(hr)) {
            int screenWidth = outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left;
            printf("[DXGI] OK Screen resolution: %d x %d\n", screenWidth, 
                   outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top);
            printf("[DXGI] OK Detected refresh rate: %d Hz\n", refreshRateHz);

            char monitorName[64] = {};
            size_t converted2 = 0;
            wcstombs_s(&converted2, monitorName, sizeof(monitorName), outputDesc.DeviceName, _TRUNCATE);
            g_monitorName = monitorName;
            g_monitorHz = refreshRateHz;

            if (regionX_ == 0 && regionY_ == 0 && regionW_ == 0 && regionH_ == 0) {
                regionW_ = 200;
                regionH_ = 200;
                regionX_ = (screenWidth / 2) - (regionW_ / 2);
                regionY_ = (outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top) / 2 - (regionH_ / 2);
                printf("[DXGI] OK Auto-region: x=%d y=%d w=%d h=%d (center)\n",
                       regionX_, regionY_, regionW_, regionH_);
            } else {
                if (regionX_ + regionW_ > screenWidth) regionW_ = screenWidth - regionX_;
                if (regionY_ + regionH_ > outputDesc.DesktopCoordinates.bottom) 
                    regionH_ = outputDesc.DesktopCoordinates.bottom - regionY_;
                printf("[DXGI] OK Capture region: x=%d y=%d w=%d h=%d\n",
                       regionX_, regionY_, regionW_, regionH_);
            }
        }

        ComPtr<IDXGIOutput1> output1;
        hr = output.As(&output1);
        if (FAILED(hr)) {
            printf("[DXGI] ERROR output.As failed: 0x%X\n", hr);
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

    HRESULT captureFrameWithTimestampDiag(uint32_t& checksumOut, int64_t& timestampNsOut, 
                                          bool& isMouseOnlyUpdate, int64_t& acquireTimeUs) {
        ComPtr<IDXGIResource> desktopResource;
        DXGI_OUTDUPL_FRAME_INFO frameInfo;

        int64_t captureTimeNs = duration_cast<nanoseconds>(
            high_resolution_clock::now().time_since_epoch()
        ).count();

        auto acquireStart = high_resolution_clock::now();
        HRESULT hr = duplication_->AcquireNextFrame(10, &frameInfo, desktopResource.ReleaseAndGetAddressOf());
        auto acquireDuration = duration_cast<microseconds>(high_resolution_clock::now() - acquireStart);
        acquireTimeUs = acquireDuration.count();

        isMouseOnlyUpdate = false;
        if (SUCCEEDED(hr)) {
            if (frameInfo.LastMouseUpdateTime.QuadPart != 0 && 
                frameInfo.TotalMetadataBufferSize == 0 &&
                frameInfo.AccumulatedFrames == 0) {
                isMouseOnlyUpdate = true;
                if (g_diagnostic) {
                    g_diagStats.mouseUpdatesOnly++;
                }
            }
        }

        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            if (g_diagnostic) {
                g_diagStats.timeouts++;
            }
            return hr;
        }

        if (FAILED(hr)) {
            if (g_diagnostic) {
                g_diagStats.acquireErrors++;
            }
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

        if (g_diagnostic) {
            g_diagStats.successfulCaptures++;
        }

        return S_OK;
    }

    HRESULT captureFrameWithTimestamp(uint32_t& checksumOut, int64_t& timestampNsOut) {
        bool dummy1;
        int64_t dummy2;
        return captureFrameWithTimestampDiag(checksumOut, timestampNsOut, dummy1, dummy2);
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
int main(int argc, char** argv) {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    printf("\n========================================\n");
    printf(" inputlag-tester with Overlay\n");
    printf(" (Timeout: %d ms)\n", g_maxWaitMs);
    printf("========================================\n\n");

    if (!ParseCommandLineArgs(argc, argv)) {
        return 1;
    }

    g_cpuName = GetCpuName();
    g_osVersion = GetOsVersionString();
    g_cpuCores = GetCpuLogicalCoresString();

    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    g_totalRamMB = mem.ullTotalPhys / (1024.0 * 1024.0);

    InitMotherboardAndBiosInfo();
    g_gpuDriverVersion = GetGpuDriverVersion();

    int regionX = 0, regionY = 0, regionW = 0, regionH = 0;
    int numSamples = 210;
    int warmupSamples = 10;
    int intervalMs = 50;
    int dx = 30;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-x" && i + 1 < argc) regionX = atoi(argv[++i]);
        else if (arg == "-y" && i + 1 < argc) regionY = atoi(argv[++i]);
        else if (arg == "-w" && i + 1 < argc) regionW = atoi(argv[++i]);
        else if (arg == "-h" && i + 1 < argc) regionH = atoi(argv[++i]);
        else if (arg == "-n" && i + 1 < argc) numSamples = atoi(argv[++i]);
        else if (arg == "-warmup" && i + 1 < argc) warmupSamples = atoi(argv[++i]);
        else if (arg == "-interval" && i + 1 < argc) intervalMs = atoi(argv[++i]);
        else if (arg == "-dx" && i + 1 < argc) dx = atoi(argv[++i]);
        else if (arg == "-o" && i + 1 < argc) g_outputFilePath = argv[++i];
    }

    printf("Config: dx=%d interval=%dms n=%d warmup=%d timeout=%dms\n\n", 
           dx, intervalMs, numSamples, warmupSamples, g_maxWaitMs);

    DXGICapture capture;
    HRESULT hr = capture.init(regionX, regionY, regionW, regionH);
    if (FAILED(hr)) {
        printf("[ERROR] Capture init failed: 0x%X\n", hr);
        return 1;
    }

    double frameTimeMs = 1000.0 / capture.refreshRateHz;
    g_overlayFrameTimeMs = frameTimeMs;
    printf("Monitor: %dHz (%.2f ms per frame)\n\n", capture.refreshRateHz, frameTimeMs);

    // Créer l'overlay si activé
    if (g_showOverlay) {
        CreateOverlayWindow();
    }

    printf("\n========================================\n");
    printf(" STARTING MULTI-RUN TEST\n");
    printf(" Runs  : %d\n", g_nbRun);
    printf(" Pause : %d seconds\n", g_pauseSeconds);
    if (g_diagnostic) {
        printf(" Mode  : DIAGNOSTIC ENABLED\n");
    }
    if (g_showOverlay) {
        printf(" Overlay: ENABLED (top-right corner, scale: %.2f)\n", g_overlaySizeFactor);
    }
    printf("========================================\n\n");

    g_overlayTotalRuns = g_nbRun;
    g_overlayTotalSamples = numSamples;

    for (int runNumber = 1; runNumber <= g_nbRun; runNumber++) {
        g_overlayCurrentRun = runNumber;
        g_overlaySampleCount = 0;
        g_overlayLastError = "";
        UpdateOverlay();

        printf("\n===============================================\n");
        printf(" RUN %d / %d\n", runNumber, g_nbRun);
        printf("===============================================\n\n");

        g_results.clear();

        printf("[OK] Starting test in 3 seconds...\n");
        Sleep(3000);
        printf("[OK] Measurements starting...\n\n");

        int sampleCount = 0;
        uint32_t baselineChecksum = 0;
        ULONGLONG nextInputTime = GetTickCount64() + intervalMs;

        int64_t dummyTs = 0;
        capture.captureFrameWithTimestamp(baselineChecksum, dummyTs);

        while (sampleCount < numSamples) {
            if (GetTickCount64() >= nextInputTime) {
                INPUT inp = {};
                inp.type = INPUT_MOUSE;
                inp.mi.dx = (sampleCount % 2 == 0) ? dx : -dx;
                inp.mi.dy = 0;
                inp.mi.dwFlags = MOUSEEVENTF_MOVE;

                int64_t inputTimeNs = duration_cast<nanoseconds>(
                    high_resolution_clock::now().time_since_epoch()
                ).count();

                SendInput(1, &inp, sizeof(INPUT));

                bool found = false;
                int waitCount = 0;
                int maxWaitCount = g_maxWaitMs;

                while (waitCount < maxWaitCount && !found) {
                    uint32_t checksum = 0;
                    int64_t captureTimeNs = 0;
                    bool isMouseOnly = false;
                    int64_t acquireTimeUs = 0;

                    HRESULT captureHr;
                    if (g_diagnostic) {
                        g_diagStats.totalAttempts++;
                        captureHr = capture.captureFrameWithTimestampDiag(checksum, captureTimeNs, 
                                                                          isMouseOnly, acquireTimeUs);
                    } else {
                        captureHr = capture.captureFrameWithTimestamp(checksum, captureTimeNs);
                    }

                    if (SUCCEEDED(captureHr)) {
                        if (checksum != baselineChecksum) {
                            int64_t latencyNs = captureTimeNs - inputTimeNs;

                            if (latencyNs > 0 && latencyNs < 500000000) {
                                if (sampleCount >= warmupSamples) {
                                    g_results.push_back(latencyNs);
                                }

                                sampleCount++;
                                g_overlaySampleCount = sampleCount;
                                double latencyMs = latencyNs / 1000000.0;
                                g_overlayLastLatency = latencyMs;
                                g_overlayLastError = "";
                                double frames = latencyMs / frameTimeMs;

                                if (g_verbose) {
                                    if (g_diagnostic) {
                                        printf("[%d/%d] Latency: %.2f ms (%.2f frames) [AcquireTime: %lld µs%s]\n",
                                               sampleCount, numSamples, latencyMs, frames, 
                                               (long long)acquireTimeUs,
                                               isMouseOnly ? ", MouseOnly" : "");
                                    } else {
                                        printf("[%d/%d] Latency: %.2f ms (%.2f frames)\n",
                                               sampleCount, numSamples, latencyMs, frames);
                                    }
                                }

                                baselineChecksum = checksum;
                                if (g_diagnostic) {
                                    g_diagStats.checksumChanges++;
                                }
                                found = true;
                            }
                        } else {
                            if (g_diagnostic) {
                                g_diagStats.sameChecksum++;
                            }
                        }
                    }

                    Sleep(1);
                    waitCount++;
                    if (g_showOverlay) {
                        ProcessWindowMessages();
                        UpdateOverlay();
                    }
                }

                if (!found) {
                    sampleCount++;
                    g_overlaySampleCount = sampleCount;
                    g_diagStats.exclusiveScreenDetected++;
                    g_overlayLastError = "No screen change";
                    g_overlayLastLatency = 0.0;
                    
                    if (g_diagnostic) {
                        printf("[%d/%d] No screen change detected (T/O:%d, Same:%d, Wait:%d/%d ms)\n", 
                               sampleCount, numSamples, 
                               g_diagStats.timeouts, g_diagStats.sameChecksum, waitCount, g_maxWaitMs);
                    } else {
                        printf("[%d/%d] No screen change detected\n", sampleCount, numSamples);
                    }
                }

                nextInputTime = GetTickCount64() + intervalMs;
            }

            Sleep(1);
            if (g_showOverlay) {
                ProcessWindowMessages();
                UpdateOverlay();
            }
        }

        printf("\n[RUN %d] Test completed: %zu samples collected\n\n", runNumber, g_results.size());

        g_allResults.push_back(g_results);

        if (runNumber < g_nbRun) {
            printf("[PAUSE] Waiting %d seconds before next run...\n", g_pauseSeconds);
            for (int i = g_pauseSeconds; i > 0; i--) {
                printf(" %d...\n", i);
                Sleep(1000);
                if (g_showOverlay) {
                    ProcessWindowMessages();
                }
            }
        }
    }

    printf("\n\n========================================\n");
    printf(" ALL RUNS COMPLETED\n");
    printf("========================================\n");

    PrintAverageResults();
    PrintDiagnosticStats();

    // Laisser la fenêtre affichée 5 secondes avant de fermer
    if (g_overlayWindow) {
        printf("\n[OVERLAY] Closing in 5 seconds...\n");
        for (int i = 5; i > 0; i--) {
            Sleep(1000);
            ProcessWindowMessages();
        }
        DestroyWindow(g_overlayWindow);
    }

    printf("\n[+] Test completed successfully\n\n");

    return 0;
}
