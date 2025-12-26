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

- `-n`            : total number of samples (default: 100)  
- `-warmup`       : number of initial samples to ignore (default: 10)  
- `-interval`     : delay between mouse moves in milliseconds (default: 200)  
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

- AMD 7700x
- Nvidia RTX 4080
- écran ROG PG27AQN (dalle IPS 360Hz) en DisplayPort
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
           RESULTATS FINAUX
==========================================

[*] Input -> DXGI Capture Latency (milliseconds)
    Samples       : 200
    Min           : 0.24 ms (0.09 frames)
    P50 (Median)  : 0.43 ms (0.16 frames)
    Avg           : 0.45 ms (0.16 frames)
    P95           : 0.69 ms (0.25 frames)
    P99           : 0.81 ms (0.29 frames)
    Max           : 0.86 ms (0.31 frames)
    Std Dev       : 0.12 ms

[*] Monitor Analysis (360Hz)
    Frame time    : 2.78 ms
    Verdict       : EXCELLENT - Under 1 frame lag

[*] Test Characteristics
    Test Duration : 16436 ms
    Measurement Rate : 12.17 Hz
    Interval      : 50 ms

[+] Test completed successfully

Note: these measurements represent the time between a mouse movement
      and DXGI detecting a frame change on Windows.
      They do not include the exact display output time
      (scan-out + panel response), which requires a hardware sensor.
```