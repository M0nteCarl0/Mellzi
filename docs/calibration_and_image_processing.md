# Flat-Panel Detector Calibration & Image Processing

This document details the mathematical algorithms, physics principles, and file formats utilized in the **Mellzi** calibration subsystem (`mellzi::calib::CalibrationEngine`) and image processing pipeline (`mellzi::image::Image`).

---

## 1. Physics & Detector Imperfections

Digital radiography flat-panel detectors (FPDs) using amorphous Silicon (a-Si:H) thin-film transistor (TFT) arrays paired with Cesium Iodide (CsI:Tl) scintillators suffer from physical non-idealities:

1. **Dark Current & Fixed Pattern Noise (FPN)**:
   Thermal generation of electron-hole pairs inside the photodiode layer produces dark current even in the complete absence of X-rays. Due to silicon doping variations and readout integrated circuit (ROIC) bias offsets, each pixel exhibits an individual dark baseline offset.
2. **Photo-Response Non-Uniformity (PRNU)**:
   Variations in scintillator crystal thickness, fiber-optic taper alignment, and individual photodiode quantum efficiencies cause different pixels to produce varying charge under uniform X-ray illumination.
3. **Defective Elements (Dead & Hot Pixels)**:
   Manufacturing imperfections lead to non-responsive pixels (open circuit / dead), shorted saturated pixels (hot), and complete readout line dropouts.
4. **Impulse Spikes (Speckle Noise)**:
   High-energy direct photon interaction with the silicon substrate or cosmic background radiation creates localized charge spikes.

To produce diagnostic-quality images, the raw 16-bit acquisition stream ($I_{raw} \in [0, 65535]$) must undergo multi-stage flat-field calibration.

---

## 2. Mathematical Calibration Pipeline

The complete calibration pipeline executes the following stages:

```
[ Raw Frame (16-bit) ]
          |
          v
[ 1. Dark Offset Subtraction + Baseline Shift ]
          |
          v
[ 2. Flat-Field Gain Normalization ]
          |
          v
[ 3. Bad Pixel Map (BPM) 8-Neighbor Interpolation ]
          |
          v
[ 4. Impulse Despeckle Filtering ]
          |
          v
[ 5. Active Area Boundary Cropping (ImgCut) ]
          |
          v
[ Calibrated Medical Radiograph (16-bit) ]
          |
          v
[ 6. VOI Window / Level Contrast Mapping (8-bit) ]
```

---

### 2.1 Dark Offset Subtraction

Dark frames ($I_{dark}$) are acquired with identical integration time and analog gain settings, but without generator triggering. To suppress random thermal noise, multiple dark frames ($N \ge 4$) are averaged:

$$\bar{I}_{dark}(x, y) = \frac{1}{N} \sum_{k=1}^{N} I_{dark, k}(x, y)$$

Subtracting $\bar{I}_{dark}$ directly would result in negative values for stochastic noise fluctuations around the baseline, which would be truncated to zero by unsigned integer types, causing severe non-linear histogram distortion. To prevent this, Mellzi applies a fixed **Baseline Shift** ($B = 1000$ LSB):

$$I_{offset}(x, y) = \max\left(0, \min\left(65535, I_{raw}(x, y) - \bar{I}_{dark}(x, y) + B\right)\right)$$

---

### 2.2 Flat-Field Gain Normalization

A flat-field image ($I_{bright}$) is acquired by exposing the detector to a uniform, unattenuated X-ray field (e.g. 70 kVp, 20 mAs at 1.5 m distance with 20 mm Al filtration).

First, the dark baseline is removed from the bright frame:

$$I_{bright, net}(x, y) = \bar{I}_{bright}(x, y) - \bar{I}_{dark}(x, y)$$

The global mean signal across the active panel area $\Omega$ is determined:

$$\mu_{bright} = \frac{1}{|\Omega|} \sum_{(x,y) \in \Omega} I_{bright, net}(x, y)$$

The normalized 32-bit floating-point gain correction coefficient $G(x, y)$ is computed as the reciprocal sensitivity:

$$G(x, y) = \frac{\mu_{bright}}{I_{bright, net}(x, y)}$$

During frame calibration, the offset-corrected signal (less baseline $B$) is multiplied by $G(x, y)$ and baseline-restored:

$$I_{gain}(x, y) = \left( (I_{offset}(x, y) - B) \cdot G(x, y) \right) + B$$

Values exceeding the 16-bit dynamic range ($[0, 65535]$) are clamped.

---

### 2.3 Bad Pixel Detection & Defect Map Generation

Pixels exhibiting non-linear or extreme responses are cataloged in a **Bad Pixel Map** ($BPM(x, y) \in \{0, 1\}$), where `1` marks a defective element:

1. **Dark Outliers (Hot Pixels)**:
   A pixel is marked defective if its dark level deviates from the local mean by more than $k$ standard deviations (typically $k = 3.0$):
   $$| \bar{I}_{dark}(x, y) - \mu_{dark} | > 3.0 \cdot \sigma_{dark}$$
2. **Gain Outliers (Dead / Low-Sensitivity Pixels)**:
   A pixel is marked defective if its flat-field response deviates by more than 20% from the panel mean:
   $$| G(x, y) - 1.0 | > 0.20$$
3. **Dead / Unresponsive Pixels**:
   Pixels saturated ($I_{dark} > 24000$) or zeroed in dark state.

---

### 2.4 Spatial Defect Interpolation

Defective pixels flagged in $BPM(x, y) == 1$ are reconstructed using 8-neighbor spatial interpolation. For each defect pixel at $(x, y)$, its 8-connected neighborhood $\mathcal{N}_8(x, y)$ is evaluated:

$$\mathcal{N}_8(x, y) = \{ (x+i, y+j) \mid i, j \in \{-1, 0, 1\}, (i,j) \neq (0,0) \}$$

Defective neighbors ($BPM(x+i, y+j) == 1$) are strictly excluded from the calculation. The replacement value is the arithmetic mean of all valid neighboring pixels:

$$I_{corrected}(x, y) = \frac{\sum_{p \in \mathcal{N}_8(x,y), BPM(p)=0} I(p)}{\sum_{p \in \mathcal{N}_8(x,y), BPM(p)=0} 1}$$

If all 8 neighbors are defective (cluster defect), the search radius expands to a $5 \times 5$ window.

---

### 2.5 Active Margin Cropping (`ImgCut`)

Raw panel readouts include peripheral guard rings, dummy channels, and gate-line boundary pixels outside the scintillator area (e.g. 3328 $\times$ 3328 full frame down to 3072 $\times$ 3072 active area).

Given cropping parameters $(left, top, right, bottom)$:
- $width_{out} = width_{in} - (left + right)$
- $height_{out} = height_{in} - (top + bottom)$

$$I_{cropped}(x, y) = I(x + left, y + top)$$

---

### 2.6 Medical Contrast Display: Window / Level (VOI LUT)

To render 16-bit high-dynamic-range radiographic data onto standard 8-bit displays ($[0, 255]$), the DICOM standard Value of Interest (VOI) linear windowing transformation is applied:

Given **Window Center** ($C$) and **Window Width** ($W$):

$$x_{min} = C - \frac{W - 1}{2}, \quad x_{max} = C + \frac{W - 1}{2}$$

$$I_{8bit}(x, y) = \begin{cases}
0, & \text{if } I(x, y) \le x_{min} \\
255, & \text{if } I(x, y) > x_{max} \\
\left\lfloor \frac{I(x, y) - x_{min}}{W - 1} \cdot 255 \right\rfloor, & \text{otherwise}
\end{cases}$$

---

## 3. Calibration Storage & Binary File Formats

Calibration files are stored in the detector calibration directory (e.g. `./calib/`):

| File Name | Format | Data Type | Dimensions | Description |
|:---|:---|:---|:---|:---|
| `dark.raw` | Uncompressed Raw | `uint16_t` (Little-Endian) | $3328 \times 3328$ | Averaged baseline dark offset map |
| `bright.raw` | Uncompressed Raw | `uint16_t` (Little-Endian) | $3328 \times 3328$ | Averaged flat-field illumination frame |
| `gain.bin` | IEEE-754 Float | `float` (32-bit LE) | $3328 \times 3328$ | Multiplicative gain correction coefficients |
| `bpm.bpm` | Binary Mask | `uint8_t` | $3328 \times 3328$ | Defect map (`0x00`: Good, `0x01`: Defective) |

---

## 4. Performance Optimizations

1. **In-Place Mutation**: `CalibrationEngine::process_in_place()` processes frames within the pre-allocated acquisition buffer, eliminating intermediate memory copies and cache thrashing.
2. **Cache-Line Vectorization**: Continuous memory layout allows modern compilers (GCC, Clang, MSVC) to emit AVX2 / NEON vectorized fused multiply-accumulate (FMA) instructions during gain normalization.
3. **Branch Elimination**: Boundary clamping in window/level calculations is implemented using hardware min/max instructions (`std::clamp`).
