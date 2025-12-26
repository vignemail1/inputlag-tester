# inputlag-tester

Small Windows utility to measure **input → DXGI capture** latency of a game or application.

> ⚠️ Important: this program measures the time between a mouse movement and DXGI detecting a frame change on Windows.  
> It does **not** directly measure the actual display output time (scan-out + panel response).

## Features

- Automatically detects monitor **refresh rate (Hz)** via DXGI
- Reports latency in **milliseconds** and in **number of frames**
- Statistics: min, median, average, p95, p99, max, standard deviation
- Configurable parameters (sample count, interval, capture region, etc.)

## Local build (MSVC)

Requirements: Visual Studio with C++ toolset (MSVC) or Visual Studio Build Tools.

```powershell
cl /std:c++17 inputlag-tester.cpp /link dxgi.lib d3d11.lib kernel32.lib user32.lib
```

L'exécutable `inputlag-tester.exe` sera généré dans le répertoire courant.

## Usage

1. Launch your game and go to the firing range for example
2. Launch an Command prompt or Powershell window
3. drag-n-drop the .exe binary into the Command prompt window with additional options if needed

    ```powershell
    inputlag-tester.exe -n 100 -interval 200 -warmup 10
    ```

4. Go back to the game in less than 3 seconds

### Options

- `-n`            : total number of samples (default: 210)  
- `-warmup`       : number of initial samples to ignore (default: 10)  
- `-interval`     : delay between mouse moves in milliseconds (default: 50)  
- `-x <X> -y <Y>` : top left corner of the capture region (0,0 = top left corner of the screen)
  - default: `X=((screenWidth / 2) - (width / 2))` et `Y=((screenHeight / 2) - (height / 2))`
- `-w <width> -h <height>` : capture region box size (default: 200x200 centered square)
- `-dx`           : horizontal mouse movement amplitude (default: 30)

## How to interpret results

- What is measured:  
  `mouse movement -> frame change observed by DXGI`
- Includes: game engine, GPU, Windows compositor, DXGI desktop duplication.
- Does **not** include: display scan-out time, panel response time.

For true **input-to-photon** measurements (up to the light emitted by the display), you need a high‑speed camera or a photodiode attached to the screen.

## Releases

Pre-built Windows binaries are automatically published in the **Releases** tab whenever a `vX.Y.Z` tag is pushed.

## Output example

Here an example for expected output (output from my computer, below my computer specs)

- AMD Ryzen 7 7700x 8-core
- Nvidia GeForce RTX 4080
- écran ROG PG27AQN (IPS 360Hz screen) en DisplayPort
- Windows 11 25H2

```powershell
PS C:\Users\Vigne> C:\Users\Vigne\Downloads\inputlag-tester-windows-amd64\inputlag-tester.exe

========================================
   inputlag-tester (Auto-Detect Hz)
========================================

Config: dx=30 interval=50ms n=210 warmup=10

[DXGI] OK Screen resolution: 2560 x 1440
[DXGI] OK Detected refresh rate: 360 Hz
[DXGI] OK Auto-region: x=1180 y=620 w=200 h=200 (center)
[DXGI] OK Desktop Duplication initialized
Monitor: 360Hz (2.78 ms per frame)

[OK] Starting test in 3 seconds...
[OK] Measurements starting (pure DXGI-based)...

[1/210] Latency: 0.31 ms (0.11 frames)
[2/210] Latency: 0.62 ms (0.22 frames)
[3/210] Latency: 0.39 ms (0.14 frames)
...
[209/210] Latency: 0.40 ms (0.14 frames)
[210/210] Latency: 0.44 ms (0.16 frames)

==========================================
              FINAL RESULTS
==========================================

[SYS ] CPU           : AMD Ryzen 7 7700X 8-Core Processor
[SYS ] GPU           : NVIDIA GeForce RTX 4080
[SYS ] Driver        : 32.0.15.9159
[SYS ] Monitor       : \\.\DISPLAY1
[SYS ] Refresh Rate  : 360 Hz

[*] Input -> DXGI Capture Latency (milliseconds)
    Samples       : 200
    Min           : 0.34 ms (0.12 frames)
    P50 (Median)  : 0.58 ms (0.21 frames)
    Avg           : 0.60 ms (0.21 frames)
    P95           : 0.83 ms (0.30 frames)
    P99           : 1.06 ms (0.38 frames)
    Max           : 1.17 ms (0.42 frames)
    Std Dev       : 0.13 ms

[*] Monitor Analysis (360Hz)
    Frame time    : 2.78 ms
    Verdict       : EXCELLENT - Under 1 frame of lag

[*] Test Characteristics
    Test Duration : 6582 ms
    Measurement Rate : 30.39 Hz
    Interval      : 1 ms

[+] Test completed successfully

Note: these measurements represent the time between a mouse movement
      and DXGI detecting a frame change on Windows.
      They do not include the exact display output time
      (scan-out + panel response), which requires a hardware sensor.
```
