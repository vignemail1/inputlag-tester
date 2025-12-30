# inputlag-tester

Small Windows utility to measure **input → DXGI capture** latency of a game or application.

> ⚠️ Important: this program measures the time between a mouse movement and DXGI detecting a frame change on Windows.  
> It does **not** directly measure the actual display output time (scan-out + panel response).

also very important, this tool **doesn't work**:

- if you are performing a **screen cloning**, it works when **expanding screen**
- or with **exclusive fullscreen mode**, need to be **borderless/windowed mode**.

## Features

- Automatically detects monitor **refresh rate (Hz)** via DXGI
- Reports latency in **milliseconds** and in **number of frames**
- Statistics: min, median, average, p95, p99, max, standard deviation
- Configurable parameters (sample count, interval, capture region, etc.)
- **Multi-run support** with averaging across multiple test runs
- **Verbose mode** to display detailed per-sample results
- Collects **system information** (CPU, GPU, RAM, motherboard, BIOS)
- Per-run and global statistics aggregation

## Local build (MSVC)

Requirements: Visual Studio with C++ toolset (MSVC) or Visual Studio Build Tools.

```powershell
cl /std:c++17 inputlag-tester.cpp /link dxgi.lib d3d11.lib kernel32.lib user32.lib advapi32.lib
```

The executable `inputlag-tester.exe` will be generated in the current directory.

## Usage

1. Launch your game and go to the firing range for example
2. Launch a Command prompt or Powershell window
3. Drag-n-drop the .exe binary into the Command prompt window with additional options if needed

    ```powershell
    inputlag-tester.exe -n 100 -interval 200 -warmup 10
    ```

4. Go back to the game in less than 3 seconds

### Options

#### Test Configuration

- `-n NUM`              : Total number of samples per run (default: 210)  
- `-warmup NUM`        : Number of initial samples to ignore (default: 10)  
- `-interval NUM`      : Delay between mouse moves in milliseconds (default: 50)  
- `-dx NUM`            : Horizontal mouse movement amplitude (default: 30)

#### Capture Region

- `-x <X> -y <Y>` : Top left corner of the capture region (0,0 = top left corner of the screen)
  - Default: `X=((screenWidth / 2) - (width / 2))` and `Y=((screenHeight / 2) - (height / 2))`
- `-w <width> -h <height>` : Capture region box size (default: 200x200 centered square)

#### Multi-Run & Output

- `--nb-run NUM`       : Number of test runs (default: 3)  
- `--pause SECONDS`    : Pause between runs in seconds (default: 3)  
- `-v, --verbose`      : Verbose mode - display each sample measurement (default: off)
- `-o FILE`            : Output file path for detailed results (default: none)

#### Help

- `--help`, `-h`, `/?` : Display help message and exit

### Example Commands

```powershell
# Single run with custom parameters
inputlag-tester.exe -n 100 -interval 50

# Multiple runs with averaging (5 runs, 2-second pause between runs)
inputlag-tester.exe --nb-run 5 --pause 2

# Verbose mode with 10 runs
inputlag-tester.exe --nb-run 10 -v

# Save results to file in verbose mode
inputlag-tester.exe --nb-run 3 -o results.txt -v

# Custom capture region (top-left corner, 400x300 box)
inputlag-tester.exe -x 100 -y 100 -w 400 -h 300

# Minimal interval for fastest testing
inputlag-tester.exe -interval 25 --nb-run 5
```

## Output & Results

### Single Run Output

Each measurement displays:

```text
[N/TOTAL] Latency: X.XX ms (Y.YY frames)
```

### Multi-Run Statistics

When multiple runs are executed (`--nb-run > 1`), the tool outputs:

- **Per-Run Statistics**: Min, Average, Max for each individual run
- **Global Statistics**: Aggregated statistics across all runs:
  - Min, Median (P50), Average, P95, P99, Max
  - Standard deviation
  - Total sample count

### System Information

The tool collects and reports:

```text
[SYS] CPU           : Processor name and model
[SYS] CPU Cores     : Logical core count
[SYS] RAM           : Total system memory (MB)
[SYS] OS            : Windows version and build number
[SYS] MB            : Motherboard vendor and product
[SYS] BIOS          : BIOS version
[SYS] GPU           : Graphics card name and VRAM
[SYS] Monitor       : Display device and refresh rate
```

## How to interpret results

- **What is measured:**  
  `mouse movement → frame change observed by DXGI`

- **Includes:**
  - Game engine processing time
  - GPU rendering time
  - Windows compositor
  - DXGI desktop duplication

- **Does NOT include:**
  - Display scan-out time
  - Panel response time
  - True input-to-photon time

For true **input-to-photon** measurements (up to the light emitted by the display), you need a high-speed camera or a photodiode attached to the screen.

## Verdict Interpretation

The tool provides a verdict based on latency relative to frame time:

- **EXCELLENT**: Under 1 frame of lag
- **GOOD**: Between 1-2 frames
- **ACCEPTABLE**: Between 2-3 frames
- **POOR**: Over 3 frames

## Releases

Pre-built Windows binaries are automatically published in the **Releases** tab whenever a `vX.Y.Z` tag is pushed.

## Output Example

Example output from a 3-run test on high-end gaming setup:

```text
===============================================
 RUN 1 / 3
===============================================

[OK] Starting test in 3 seconds...
[OK] Measurements starting...

[1/210] Latency: 0.31 ms (0.11 frames)
[2/210] Latency: 0.62 ms (0.22 frames)
[3/210] Latency: 0.39 ms (0.14 frames)
...
[210/210] Latency: 0.83 ms (0.30 frames)

[RUN 1] Test completed: 200 samples collected

[PAUSE] Waiting 3 seconds before next run...

===============================================
 RUN 2 / 3
===============================================
...

========================================
 ALL RUNS COMPLETED
========================================

==========================================
 AVERAGE RESULTS OVER 3 RUNS
==========================================

[*] System Information
 CPU      : AMD Ryzen 7 7700X 8-Core Processor
 CPU Cores: 16 logical cores
 RAM      : 32422 MB
 OS       : Windows 6.2 (build 9200)
 MB       : ASUSTeK COMPUTER INC. TUF GAMING B650-PLUS
 BIOS     : 3602
 GPU      : NVIDIA GeForce RTX 4080 (16048 MB)
 Monitor  : \\.\DISPLAY1 @ 360 Hz

[*] Global Statistics Over 1500 Measurements
 Samples    : 1500
 Min        : 0.25 ms (0.09 frames)
 P50 (Med)  : 0.50 ms (0.18 frames)
 Avg        : 0.52 ms (0.19 frames)
 P95        : 0.74 ms (0.27 frames)
 P99        : 0.90 ms (0.32 frames)
 Max        : 1.67 ms (0.60 frames)
 Std Dev    : 0.12 ms

[*] Per-Run Statistics
  Run 1: Min=0.26, P50=0.49, Avg=0.51, P99=0.96, Max=1.67 ms, Samples=500
  Run 2: Min=0.27, P50=0.49, Avg=0.52, P99=1.00, Max=1.23 ms, Samples=500
  Run 3: Min=0.25, P50=0.50, Avg=0.51, P99=0.86, Max=1.02 ms, Samples=500


[+] Test completed successfully
```

## Troubleshooting

### "DXGI initialization failed"

- Ensure your GPU supports DXGI desktop duplication
- Try running with administrator privileges
- Update your GPU drivers

### "No screen change detected"

- The capture region may not be in the game window
- Use `-x`, `-y`, `-w`, `-h` parameters to adjust the capture area
- Ensure the region is actively updating in your application

### Inconsistent results between runs

- Close background applications during testing
- Ensure consistent system load
- Use multiple runs (`--nb-run`) to average out variance
- Check CPU/GPU usage with task manager while running tests

## License

See LICENSE file for details.
