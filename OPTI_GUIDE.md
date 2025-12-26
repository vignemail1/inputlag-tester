# PC Gaming Latency Optimization Guide (Using DXGI-Based Measurements)

This guide explains how to use a DXGI-based input‑to‑capture latency tool (like the current program) to optimize in‑game settings for **G‑SYNC / FreeSync**, **V‑Sync**, **NVIDIA Reflex**, and **AMD Anti‑Lag**, without any external hardware.

---

## 1. What The Tool Actually Measures

The tool measures the time between:

- A mouse movement event injected via `SendInput`.
- The first frame where a region of the screen changes, detected via DXGI desktop duplication.

This approximates **input → GPU render → present → first visible change in the captured region**, but **does not** precisely separate:

- Controller → CPU processing.
- CPU → GPU queueing.
- GPU render time.
- Scan‑out + panel response.

However, it is very useful for **relative comparisons** between settings, as long as you keep test conditions identical.

---

## 2. Key Metrics To Interpret

At the end of each run, the tool prints:

- **P50 (Median)** - "typical" latency; what you feel most of the time.
- **Average (Avg)** - mean latency; less robust if there are spikes.
- **P95 / P99** - upper tail; how bad the worst frames get.
- **Max** - absolute worst (often an outlier).
- **Std Dev** - how stable your latencies are.
- **Latency in frames** - latency normalized by frame time (based on detected refresh rate).
- **Measurement Rate (Hz)** - how often a change was detected per second; sanity check.

For tuning, focus on:

- **P50 (ms / frames)** for baseline responsiveness.
- **P95/P99 (ms / frames)** for consistency and "stutter spikes".
- **Std Dev** and the gap **P95 − P50** for smoothness.

---

## 3. Test Protocol

### 3.1. General Conditions

For any comparison:

1. Use a **repeatable in‑game scene**  
   - Example: a bot match, a fixed camera path, or a training range.  
   - Keep the camera movement and action pattern as consistent as possible.

2. Fix system conditions:
   - No recording software, no heavy overlays.
   - Same resolution, same graphics preset, same driver version for all runs.

3. Keep each test run **long enough**:
   - Aim for **at least 200-300 samples** per configuration.
   - Ideally, run for **60-120 seconds** per setting to stabilize statistics.

4. Make sure you are either:
   - Clearly **GPU‑bound** (GPU close to 95-99%), or  
   - Clearly **CPU‑bound** (GPU well below 90%, very high FPS).  
   This will change how Reflex / Anti‑Lag behave.

---

## 4. G‑SYNC / FreeSync / V‑Sync Testing

### 4.1. Baseline runs (no VRR)

Run the following first:

1. **VRR OFF, V‑Sync OFF, no FPS cap**
   - Expect: lowest raw P50, but tearing and possibly higher P95/P99 due to unstable frametimes.
   - Use this as your "minimum latency" reference.

2. **VRR OFF, V‑Sync ON, no FPS cap**
   - Expect:  
     - P50 often higher (1-2 extra frames) because of back pressure when FPS exceeds refresh rate.  
     - P95/P99 may increase further due to queuing.

Interpretation:

- If P50 and especially P95 jump significantly when enabling V‑Sync, you know your game often runs **over** your monitor's refresh rate and is hitting the V‑Sync queue.

### 4.2. VRR (G‑SYNC / FreeSync) runs

On a G‑SYNC/FreeSync monitor:

1. **VRR ON, V‑Sync OFF, FPS cap just below max refresh**
   - Example: 240 Hz monitor → cap at 230-237 FPS via in‑game limiter or RTSS.
   - Expect:
     - P50 close to baseline VRR OFF / V‑Sync OFF.
     - P95 and Std Dev better than uncapped, with much less visible tearing.

2. **VRR ON + V‑Sync ON (driver or control panel), V‑Sync OFF in game**
   - This is a commonly recommended G‑SYNC setup to remove tearing without heavy V‑Sync queuing.
   - Expect:
     - When FPS stays **within VRR range**, P50 very close to case 1.
     - P95 improved vs pure V‑Sync, since VRR reduces mismatch between FPS and refresh.

Interpretation rules:

- If **VRR ON + V‑Sync ON + FPS cap below max** gives:
  - P50 ≤ baseline P50 + ~1 frame.
  - P95/P99 significantly lower and Std Dev lower.  
  Then this is usually the **best "no tearing, low lag" configuration**.

- If P50 and P95 both get worse compared to **VRR ON, V‑Sync OFF, capped**, you might prefer **V‑Sync OFF** with VRR to minimize latency, accepting a bit of tearing.

---

## 5. NVIDIA Reflex Testing (NVIDIA GPUs)

### 5.1. Test matrix

On NVIDIA, for each VRR/V‑Sync combo you care about, run:

- **Reflex OFF**
- **Reflex ON**
- **Reflex ON + Boost / Ultra (if available)**

Test each in **GPU‑bound** and **CPU‑bound** scenarios.

### 5.2. Interpreting Reflex

- In **GPU‑bound** situations:
  - Reflex should **reduce P50 and P95** by reducing render queue depth.
  - If P50 drops by even 1-2 ms and P95/P99 also drop or stay similar, Reflex is doing its job.

- In **CPU‑bound** situations:
  - Reflex often has little to no effect, and in some cases may slightly increase variance.

- **Reflex + Boost / Ultra**:
  - May further reduce latency when GPU is the bottleneck, at the cost of more power and sometimes smoother but hotter operation.
  - Keep it only if P95/P99 improve or remain similar, without a noticeable FPS drop.

Practical rule:

- If your logs show:
  - **Reflex ON**: P50/P95 lower than OFF in GPU‑bound tests, with similar or slightly higher FPS → keep Reflex ON (or ON+Boost).
  - **Reflex ON**: no gain or worse P95/Std Dev in CPU‑bound tests → turn Reflex OFF for that game/config.

---

## 6. AMD Anti‑Lag Testing (AMD GPUs)

### 6.1. Test matrix

On AMD, for each VRR/V‑Sync setup:

- **Anti‑Lag OFF**
- **Anti‑Lag ON** (and Anti‑Lag 2 / Anti‑Lag+ when available and stable).

Run tests in GPU‑bound and CPU‑bound modes, similar to Reflex testing.

### 6.2. Interpreting Anti‑Lag

- In **GPU‑bound** situations:
  - Anti‑Lag should reduce input buffering between CPU and GPU, slightly lowering P50 and P95 (AMD claims up to ~30-40% in ideal cases, but real‑world gains are often smaller).
- In **CPU‑bound** situations:
  - Anti‑Lag may have minimal effect or slightly add overhead; your tool will show this as unchanged or slightly worse P50/P95.

Decision:

- If Anti‑Lag ON → **lower P50 with equal or lower P95** and negligible FPS loss → keep it enabled for that title.
- If Anti‑Lag ON → no measurable gain and/or P95/Std Dev worse → keep it disabled for that game/config.

---

## 7. Concrete Decision Rules From Your Stats

Once you have runs for all variants, you can choose a profile according to your priority.

### 7.1. Competitive profile (lowest possible latency)

Target:

- **Minimize P50** (and keep P95 reasonable), even if you accept tearing.

Typical pattern:

- VRR ON (G‑SYNC/FreeSync), **V‑Sync OFF**, FPS cap slightly below refresh.
- NVIDIA: Reflex ON (or ON+Boost) **if GPU‑bound**.
- AMD: Anti‑Lag ON **if GPU‑bound**.

Choose the configuration that:

- Has the **lowest P50**, and
- P95/P99 not significantly higher than competitors (e.g. < 1 extra frame).

### 7.2. Smoothness / "no tearing" profile

Target:

- Accept +1 frame in P50 if you get **much better P95/P99** and almost no visible artifacts.

Typical pattern:

- VRR ON + V‑Sync ON in driver, V‑Sync OFF in‑game, FPS cap a bit below monitor refresh.
- Reflex / Anti‑Lag ON when GPU‑bound.

Choose the configuration that:

- Has P50 within ~1 frame of the fastest config.
- Has significantly lower P95/P99 and Std Dev, especially under heavy action.

### 7.3. Avoiding spikes

Target:

- Minimize **P95 − P50** and Std Dev.

Look for:

- Config where P50 is slightly higher, but P95/P99 and Std Dev are considerably lower.
- This usually means fewer "micro‑stutters" and a more consistent feel.

---

## 8. Practical Workflow Example

For a given game:

1. Pick a resolution and graphics preset.
2. Decide on a test scene (e.g. training map path).
3. Run across this matrix (NVIDIA example):

   - VRR OFF, V‑Sync OFF, Reflex OFF.
   - VRR ON, V‑Sync OFF, cap FPS, Reflex OFF.
   - VRR ON, V‑Sync OFF, cap FPS, Reflex ON.
   - VRR ON, V‑Sync ON (driver), cap FPS, Reflex ON.
   - (Optionally Reflex ON+Boost variants.)

4. Export/record for each run:
   - P50, P95, P99 (ms and frames).
   - Std Dev.
   - Measurement Rate and approximate FPS.

5. Choose:
   - One **competitive** preset (min P50 within acceptable P95/P99).
   - One **smooth** preset (low P95/P99 and Std Dev, acceptable P50).

Repeat the same methodology on AMD, replacing Reflex with Anti‑Lag, if you have an AMD graphic card.
