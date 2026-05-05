# Kinect Depth + RGB Display for Orange Pi Zero 2W

Real-time Kinect v1 (Xbox 360) depth visualization and RGB camera display on HDMI/LCD screens using Orange Pi Zero 2W.

![Kinect Display Demo](https://via.placeholder.com/800x400?text=Kinect+Depth+Visualization)

## Features

- **Dual camera display**: Depth sensor (main view) + RGB camera (bottom-right corner)
- **Color-coded depth**: Red (close) → Green (mid) → Blue (far)
- **Direct framebuffer rendering**: No X server required
- **Dockerized**: Clean, isolated environment
- **Low latency**: Direct hardware access via libfreenect

## Hardware Requirements

- **Orange Pi Zero 2W** (4GB recommended) with Armbian
- **Kinect v1** (Xbox 360) with power adapter
- **24-pin expansion board** (for USB connectivity)
- **HDMI display** or compatible LCD (tested on 1024x600)
- **USB hub** (optional but recommended for power stability)

### Display Options

This project outputs to `/dev/fb0` (framebuffer). Supported displays:
- HDMI monitors/TVs (via mini HDMI port)
- SPI LCD displays (slower framerate, ~15-20fps)
- CRT TVs via HDMI-to-Composite/SCART adapters

## Software Requirements

- Armbian (Ubuntu 22.04/24.04 base)
- Docker & Docker Compose
- libfreenect (installed via Docker)

## Quick Start

### 1. Clone Repository

```bash
git clone https://github.com/yourusername/kinect-display.git
cd kinect-display
```

### 2. Setup Hardware

1. Connect 24-pin expansion board to Orange Pi
2. Connect Kinect USB to expansion board
3. **IMPORTANT**: Plug in Kinect power adapter (required!)
4. Connect HDMI display

### 3. Configure Network

**The 24-pin expansion board disables Ethernet!** Set up WiFi first:

```bash
# Connect to WiFi
sudo nmcli device wifi connect "YourSSID" password "YourPassword"

# Verify WiFi works
ip addr show wlan0

# Optional: Disable Ethernet to avoid conflicts
sudo nmcli connection modify "Wired connection 1" connection.autoconnect no
```

### 4. Setup USB Permissions

```bash
# Create udev rules for Kinect
sudo nano /etc/udev/rules.d/51-kinect.rules
```

Add these lines:

```
SUBSYSTEM=="usb", ATTR{idVendor}=="045e", ATTR{idProduct}=="02ae", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="045e", ATTR{idProduct}=="02ad", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="045e", ATTR{idProduct}=="02b0", MODE="0666"
```

Reload rules:

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### 5. Verify Kinect Detection

```bash
lsusb | grep Xbox

# Should show 3 devices:
# Bus XXX Device XXX: ID 045e:02ae Microsoft Corp. Xbox NUI Camera
# Bus XXX Device XXX: ID 045e:02b0 Microsoft Corp. Xbox NUI Motor
# Bus XXX Device XXX: ID 045e:02ad Microsoft Corp. Xbox NUI Audio
```

### 6. Build and Run

```bash
# Build Docker container
docker-compose up -d

# Enter container
docker exec -it kinect-service /bin/bash

# Build C++ application
cd /app
mkdir build && cd build
cmake ..
make

# Run the display
./kinect_display
```

You should now see:
- **Main display**: Color-coded depth map (640x480 centered)
- **Bottom-right**: RGB camera feed (480x360)

Press `Ctrl+C` to stop.

## Project Structure

```
kinect-display/
├── Dockerfile              # Docker image definition
├── docker-compose.yml      # Docker service configuration
├── CMakeLists.txt          # CMake build configuration
├── kinect_display.cpp      # Main C++ application
├── Makefile                # (Optional) Build shortcuts
├── LICENSE                 # Project license
└── README.md               # This file
```

### Minimum Required Files

- ✅ `Dockerfile`
- ✅ `docker-compose.yml`
- ✅ `CMakeLists.txt`
- ✅ `kinect_display.cpp`
- ✅ `README.md`

The `Makefile` is optional (CMake handles builds inside Docker).

## Configuration

### Adjust RGB Camera Size

Edit `kinect_display.cpp`, find this section:

```cpp
// Draw RGB camera in bottom-right corner
int rgb_width = 480;   // Change size here (320-640)
int rgb_height = 360;  // Change size here (240-480)
```

### Adjust Display Mode

**Centered (current default):** Depth at 640x480, centered with black bars

**Fullscreen stretched:** Change in `displayToFramebuffer()` to scale up

### Change Depth Color Scheme

Modify the `DepthCallback()` function to adjust the color gradient.

## Troubleshooting

### "LIBUSB_ERROR_NO_DEVICE"

**Problem:** Container can't access Kinect USB

**Solutions:**
1. Check udev rules are set correctly
2. Verify Kinect power adapter is connected
3. Run: `sudo chmod -R 777 /dev/bus/usb/`
4. Restart container: `docker restart kinect-service`

### "Invalid magic" / "Lost packets" Errors

**Problem:** USB bandwidth issues (normal on USB 2.0)

**Solutions:**
1. Increase USB memory: `echo 1000 | sudo tee /sys/module/usbcore/parameters/usbfs_memory_mb`
2. Use a powered USB hub
3. These warnings are usually harmless - image still works

### WiFi Not Working After 24-Pin Board Connected

**Problem:** 24-pin expansion board shares pins with Ethernet PHY

**Solution:** WiFi must be configured and working **before** connecting the board

### Slow Framerate

**Expected:** 15-25 fps on Orange Pi Zero 2W (USB 2.0 + ARM CPU limitation)

**Improvements:**
- Use powered USB hub
- Increase `usleep()` value in main loop (counterintuitively helps USB)
- Reduce RGB camera size

### Display Not Showing

```bash
# Check framebuffer exists
ls -la /dev/fb0

# Check framebuffer info
fbset -fb /dev/fb0

# Give permissions
sudo chmod 666 /dev/fb0

# Test with solid color
dd if=/dev/zero of=/dev/fb0 bs=4 count=$((1024*600))
```

## CRT TV Display Setup

To display on a CRT TV via SCART or composite:

1. **HDMI to Composite adapter** (~$15)
   ```
   Orange Pi HDMI → HDMI-to-AV converter → Yellow RCA → TV
   ```

2. **HDMI to SCART** (~$30-50, rare)
   ```
   Orange Pi HDMI → HDMI-to-SCART converter → SCART → TV
   ```

3. **Composite + RCA-to-SCART adapter** (~$20 total)
   ```
   Orange Pi HDMI → HDMI-to-AV → RCA-to-SCART adapter → TV
   ```

**Note:** Standard "SCART to HDMI" adapters only work **one direction** (SCART→HDMI for old consoles to modern TVs). You need the reverse!

## Technical Details

### Kinect Data Format

- **Depth**: 640x480 @ 30fps, 11-bit depth values (0-2047mm)
- **RGB**: 640x480 @ 30fps, 8-bit RGB888
- **USB bandwidth**: ~35 MB/s (both streams active)

### Color Mapping

Depth values are mapped to RGB gradient:
- **0.0-0.5** (close-mid): Red (255,0,0) → Green (0,255,0)
- **0.5-1.0** (mid-far): Green (0,255,0) → Blue (0,0,255)

### Framebuffer Format

Supports both:
- **32-bit BGRA** (most common)
- **16-bit RGB565** (some LCD displays)

Auto-detected from `/dev/fb0` properties.

## Future Enhancements

- [ ] HTTPS JSON API for depth data export
- [ ] Flutter mobile app for remote viewing
- [ ] Skeleton tracking visualization
- [ ] Point cloud export
- [ ] Recording and playback

## Credits

- **libfreenect**: OpenKinect project (https://github.com/OpenKinect/libfreenect)
- **Orange Pi**: Allwinner H618 ARM board
- Built with Docker, CMake, C++17

## License

[Your License Here - MIT/GPL/etc.]

## Contributing

Pull requests welcome! Please test on actual hardware before submitting.

---

**Author:** Salvio  
**Hardware:** Orange Pi Zero 2W (4GB) + Kinect v1  
**Display:** 1024x600 HDMI LCD
