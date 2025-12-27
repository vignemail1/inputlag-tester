# inputlag-tester

Small Windows utility to measure **input → DXGI capture** latency of a game or application.

> ⚠️ Important: this program measures the time between a mouse movement and DXGI detecting a frame change on Windows.  
> It does **not** directly measure the actual display output time (scan-out + panel response).

also very important: **this tool doesn't work if you are performing a screen cloning**, it works when expanding screen.

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

## Command-line options

All options are optional. Defaults are chosen to give a reasonable balance between test duration and statistical quality.

### Region selection

The capture region is where DXGI checks for the first frame change.

- `-x <int>`  
  Capture region top‑left X in pixels (desktop coordinates).  
  Default: `0` (auto‑centered region if all four of `-x -y -w -h` are 0).

- `-y <int>`  
  Capture region top‑left Y in pixels.  
  Default: `0`.

- `-w <int>`  
  Capture region width in pixels.  
  Default: `0` → if all four (`-x -y -w -h`) are zero, the program will choose a `200x200` region centered on the primary monitor.

- `-h <int>`  
  Capture region height in pixels.  
  Default: `0`.

If the region would extend beyond the desktop, it is automatically clamped to the screen bounds.

Default: `X=((screenWidth / 2) - (width / 2))` et `Y=((screenHeight / 2) - (height / 2))`

### Sampling / timing

- `-n <int>`  
  Total number of measurement attempts.  
  Each attempt sends one mouse move and waits for a screen change.  
  Default: `210`.

- `-warmup <int>`  
  Number of initial attempts to discard as warm‑up.  
  Warm‑up samples are executed but not included in the statistics.  
  Default: `10`.

- `-interval <int>`  
  Interval between input events in milliseconds.  
  The tool schedules each mouse move at `GetTickCount64() + interval`.  
  Default: `50` ms.

- `-dx <int>`  
  Horizontal mouse movement (in mouse units) for each input event.  
  The sign alternates every sample: `+dx, -dx, +dx, ...`  
  Default: `30`.

### Output

- `-o <path>`  
  Write a human‑readable summary to the given file path.  
  The file contains the `[SYS ]` system block and all latency statistics (min, median, avg, percentiles, etc.).  
  Example:

  ```powershell
  inputlag-tester.exe -n 300 -warmup 30 -interval 40 -dx 40 -o results.txt
  ```

If `-o` is not provided, the summary is printed only to the console.

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
PS C:\Users\Vigne> PS Z:\sv\CODE\inputlag-tester> .\inputlag-tester.exe -interval 1ms -o results.txt

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
[209/210] Latency: 0.36 ms (0.13 frames)
[210/210] Latency: 0.37 ms (0.13 frames)

==========================================
              FINAL RESULTS
==========================================

[SYS ] CPU           : AMD Ryzen 7 7700X 8-Core Processor
[SYS ] CPU Cores     : 16 logical cores
[SYS ] RAM           : 31911 MB
[SYS ] OS            : Windows 6.2 (build 9200)
[SYS ] Motherboard   : ASUSTeK COMPUTER INC. TUF GAMING B650-PLUS
[SYS ] BIOS          : 3287
[SYS ] XMP Profile   : Unknown
[SYS ] Resizable BAR : Unknown
[SYS ] GPU           : NVIDIA GeForce RTX 4080
[SYS ] GPU VRAM      : 16048 MB
[SYS ] Monitor       : \\.\DISPLAY1
[SYS ] Refresh Rate  : 360 Hz

[*] Input -> DXGI Capture Latency (milliseconds)
    Samples       : 200
    Min           : 0.29 ms (0.10 frames)
    P50 (Median)  : 0.52 ms (0.19 frames)
    Avg           : 0.56 ms (0.20 frames)
    P95           : 0.81 ms (0.29 frames)
    P99           : 0.92 ms (0.33 frames)
    Max           : 0.95 ms (0.34 frames)
    Std Dev       : 0.14 ms

[*] Monitor Analysis (360Hz)
    Frame time    : 2.78 ms
    Verdict       : EXCELLENT - Under 1 frame of lag

[*] Test Characteristics
    Test Duration : 6597 ms
    Measurement Rate : 30.32 Hz
    Interval      : 1 ms

[+] Test completed successfully

Note: these measurements represent the time between a mouse movement
      and DXGI detecting a frame change on Windows.
      They do not include the exact display output time
      (scan-out + panel response), which requires a hardware sensor.

[OK ] Results written to: results.txt
```
