
// Include libfreenect C++ wrapper - similar to "import" in Java/Python
#include <libfreenect.hpp>
#include <iostream>     // For console output (cout, cerr)
#include <vector>       // Like ArrayList in Java or list in Python
#include <cmath>        // Math functions (min, max, etc.)
#include <fstream>      // File operations
#include <fcntl.h>      // File control (open, close)
#include <linux/fb.h>   // Linux framebuffer structures
#include <sys/mman.h>   // Memory mapping
#include <sys/ioctl.h>  // Device control
#include <unistd.h>     // POSIX API (usleep, close)
#include <cstring>      // Memory operations (memcpy)

// Class that extends Freenect::FreenectDevice (like "extends" in Java)
// This is our main class that handles Kinect and display
class KinectDisplay : public Freenect::FreenectDevice {
private:
    // Private member variables (like private fields in Java)
    std::vector<uint8_t> depth_buffer;   // Vector = dynamic array (like ArrayList<Byte>)
    std::vector<uint16_t> raw_depth;     // Stores raw 16-bit depth values
    std::vector<uint8_t> rgb_buffer;     // Stores RGB camera data (640x480x3)
    int fb_fd;                           // File descriptor for framebuffer (like file handle)
    uint8_t* fb_ptr;                     // Pointer to framebuffer memory (like byte[] in Java)
    size_t fb_size;                      // Size of framebuffer in bytes
    int screen_width;                    // Display width in pixels
    int screen_height;                   // Display height in pixels
    int bits_per_pixel;                  // Color depth (16 or 32 typically)

public:
    // Constructor - called when object is created (like constructor in Java)
    // Takes context and device index as parameters
    KinectDisplay(freenect_context *ctx, int index)
        : Freenect::FreenectDevice(ctx, index),  // Call parent constructor
          depth_buffer(640 * 480 * 4),           // Initialize with size (640x480 pixels, 4 bytes per pixel RGBA)
          raw_depth(640 * 480),                  // Initialize 640x480 depth values
          rgb_buffer(640 * 480 * 3),             // Initialize RGB buffer (640x480x3 for RGB)
          fb_fd(-1),                             // -1 means "not open yet"
          fb_ptr(nullptr) {                      // nullptr = null in Java
        initFramebuffer();  // Call setup method
    }

    // Destructor - called when object is deleted (like finally block or AutoCloseable)
    // C++ doesn't have garbage collection, so we must clean up manually!
    ~KinectDisplay() {
        if (fb_ptr) {
            munmap(fb_ptr, fb_size);  // Unmap memory
        }
        if (fb_fd >= 0) {
            close(fb_fd);  // Close file
        }
    }

    // Initialize the framebuffer device (display)
    void initFramebuffer() {
        // Open the framebuffer device file
        // O_RDWR = Open for Read/Write (like FileMode in C#)
        fb_fd = open("/dev/fb0", O_RDWR);
        if (fb_fd < 0) {
            std::cerr << "Error: Cannot open framebuffer device" << std::endl;
            return;
        }

        // Get screen information using ioctl (system call)
        // ioctl = I/O Control, way to communicate with device drivers
        struct fb_var_screeninfo vinfo;  // Struct = like a class with only public fields
        if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {  // & = "address of" operator
            std::cerr << "Error: Cannot read framebuffer info" << std::endl;
            close(fb_fd);
            fb_fd = -1;
            return;
        }

        // Extract screen properties from the struct
        screen_width = vinfo.xres;
        screen_height = vinfo.yres;
        bits_per_pixel = vinfo.bits_per_pixel;

        std::cout << "Framebuffer: " << screen_width << "x" << screen_height
                  << " @ " << bits_per_pixel << "bpp" << std::endl;

        // Map framebuffer into our process memory
        // This lets us write directly to screen memory (super fast!)
        fb_size = screen_width * screen_height * (bits_per_pixel / 8);
        fb_ptr = (uint8_t*)mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
        // mmap = memory map, creates a mapping between file and memory
        // Like memory-mapped files in Java NIO

        if (fb_ptr == MAP_FAILED) {
            std::cerr << "Error: Cannot map framebuffer" << std::endl;
            close(fb_fd);
            fb_fd = -1;
        }
    }

    // Callback function - called by libfreenect when new depth frame arrives
    // Similar to event handlers in JavaScript or callbacks in Node.js
    void DepthCallback(void* data, uint32_t timestamp) {
        // Cast void* to uint16_t* (like casting Object to specific type in Java)
        uint16_t* depth = static_cast<uint16_t*>(data);

        // Copy raw depth data for JSON export later
        // memcpy = memory copy, super fast way to copy arrays
        // Similar to System.arraycopy() in Java
        std::memcpy(raw_depth.data(), depth, 640 * 480 * sizeof(uint16_t));

        // Convert depth values to grayscale image for visualization
        // Loop through all 640x480 pixels
        for (int i = 0; i < 640 * 480; i++) {
            uint16_t d = depth[i];  // Get depth value (0-2047, 11-bit)
            uint8_t gray = 0;       // Default to black

            if (d > 0) {  // If depth is valid (0 means no reading)
                // Map depth to 0-255 range
                // Divide by 16 to scale down (2047/16 ≈ 128)
                // Invert so closer objects are brighter
                gray = 255 - std::min(255, (int)(d / 16));
            }

            // Store as BGRA format (Blue, Green, Red, Alpha)
            // Each pixel takes 4 bytes
            depth_buffer[i * 4 + 0] = gray;  // B
            depth_buffer[i * 4 + 1] = gray;  // G
            depth_buffer[i * 4 + 2] = gray;  // R
            depth_buffer[i * 4 + 3] = 255;   // A (fully opaque)
        }

        // Display the converted image to screen
        displayToFramebuffer();
    }

    // Display the depth buffer to the physical screen
    void displayToFramebuffer() {
        if (!fb_ptr) return;  // Safety check: if framebuffer not initialized, exit

        // Scale depth image to fill screen (simple nearest-neighbor scaling)
        float scale_x = (float)screen_width / 640.0f;
        float scale_y = (float)screen_height / 480.0f;

        // Draw fullscreen depth image first
        for (int y = 0; y < screen_height; y++) {
            for (int x = 0; x < screen_width; x++) {
                // Map screen coordinates back to source image
                int src_x = (int)(x / scale_x);
                int src_y = (int)(y / scale_y);

                // Clamp to image bounds
                if (src_x >= 640) src_x = 639;
                if (src_y >= 480) src_y = 479;

                // Calculate source index in depth_buffer
                int src_idx = (src_y * 640 + src_x) * 4;

                // Calculate destination index in framebuffer
                int dst_idx = (y * screen_width + x) * (bits_per_pixel / 8);

                // Copy pixel based on color depth
                if (bits_per_pixel == 32) {
                    // 32-bit color: direct copy of BGRA
                    fb_ptr[dst_idx + 0] = depth_buffer[src_idx + 0]; // B
                    fb_ptr[dst_idx + 1] = depth_buffer[src_idx + 1]; // G
                    fb_ptr[dst_idx + 2] = depth_buffer[src_idx + 2]; // R
                    fb_ptr[dst_idx + 3] = depth_buffer[src_idx + 3]; // A
                } else if (bits_per_pixel == 16) {
                    // 16-bit color (RGB565): need to compress
                    uint8_t r = depth_buffer[src_idx + 2] >> 3;
                    uint8_t g = depth_buffer[src_idx + 1] >> 2;
                    uint8_t b = depth_buffer[src_idx + 0] >> 3;

                    // Pack into 16-bit value using bit shifts and OR
                    uint16_t color = (r << 11) | (g << 5) | b;

                    // Cast and write 16-bit value
                    *((uint16_t*)(fb_ptr + dst_idx)) = color;
                }
            }
        }

        // Draw RGB camera in bottom-right corner (larger: 480x360)
        int rgb_width = 480;   // Changed from 320
        int rgb_height = 360;  // Changed from 240
        int rgb_offset_x = screen_width - rgb_width;
        int rgb_offset_y = screen_height - rgb_height;

        for (int y = 0; y < rgb_height; y++) {
            for (int x = 0; x < rgb_width; x++) {
                // Map to source RGB image (downscale by 640/480 = 1.33x)
                int src_x = (x * 640) / rgb_width;
                int src_y = (y * 480) / rgb_height;

                // Calculate source index in rgb_buffer (RGB format, 3 bytes per pixel)
                int src_idx = (src_y * 640 + src_x) * 3;

                // Calculate destination screen position
                int screen_x = rgb_offset_x + x;
                int screen_y = rgb_offset_y + y;

                // Calculate destination index in framebuffer
                int dst_idx = (screen_y * screen_width + screen_x) * (bits_per_pixel / 8);

                // Get RGB values from source (note: Kinect provides RGB, not BGR)
                uint8_t r = rgb_buffer[src_idx + 0];
                uint8_t g = rgb_buffer[src_idx + 1];
                uint8_t b = rgb_buffer[src_idx + 2];

                // Write to framebuffer
                if (bits_per_pixel == 32) {
                    // BGRA format
                    fb_ptr[dst_idx + 0] = b;
                    fb_ptr[dst_idx + 1] = g;
                    fb_ptr[dst_idx + 2] = r;
                    fb_ptr[dst_idx + 3] = 255;
                } else if (bits_per_pixel == 16) {
                    // RGB565 format
                    uint16_t color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                    *((uint16_t*)(fb_ptr + dst_idx)) = color;
                }
            }
        }
    }

    // Getter method to access raw depth data (for JSON export)
    // Returns reference to avoid copying (like returning the actual list, not a copy)
    std::vector<uint16_t>& getDepthData() {
        return raw_depth;
    }

    // Video callback - called when RGB camera frame arrives
    void VideoCallback(void* rgb, uint32_t timestamp) {
        // Copy RGB data (it comes as RGB888 format)
        uint8_t* rgb_data = static_cast<uint8_t*>(rgb);
        std::memcpy(rgb_buffer.data(), rgb_data, 640 * 480 * 3);
    }
};

// Main function - entry point of the program (like public static void main in Java)
int main(int argc, char** argv) {
    // Create Freenect context (manages USB communication)
    Freenect::Freenect freenect;
    KinectDisplay* device;  // Pointer to our device (like a reference in Java)

    // Try to create device, catch errors
    // try-catch in C++ is similar to Java
    try {
        // createDevice returns reference, we take address with &
        device = &freenect.createDevice<KinectDisplay>(0);  // 0 = first Kinect device
    } catch (std::runtime_error& e) {  // & means "reference" (like catching by reference)
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Make sure Kinect is connected!" << std::endl;
        return 1;  // Return non-zero = error (like System.exit(1))
    }

    std::cout << "Kinect initialized. Starting depth stream..." << std::endl;

    // Set video format to RGB (default)
    device->setVideoFormat(FREENECT_VIDEO_RGB);

    // Start receiving depth data from Kinect
    device->startDepth();  // -> is like . in Java (used with pointers)

    // Start receiving RGB video from Kinect
    device->startVideo();

    // Set LED to green to show it's running
    device->setLed(LED_GREEN);

    std::cout << "Displaying depth + RGB on /dev/fb0. Press Ctrl+C to stop." << std::endl;

    // Infinite loop - keep processing events
    // Similar to: while(true) in Java
    while (true) {
        // updateState() is void, just call it
        // It processes USB events and triggers callbacks like DepthCallback
        device->updateState();

        // Sleep for 30 milliseconds (30000 microseconds)
        // Adjusted from 10ms to reduce USB congestion
        // This gives USB more time to transfer data properly
        usleep(30000);  // Similar to Thread.sleep(30) in Java
    }

    // Stop depth stream (actually never reached because infinite loop)
    device->stopDepth();
    device->stopVideo();

    return 0;  // Return 0 = success
}
