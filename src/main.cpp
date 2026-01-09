// Zed Text Editor
// High-performance C++ text editor with OpenGL rendering

#include <cstdio>
#include <cstdlib>
#include <sys/time.h>

#include "platform.h"
#include "renderer.h"
#include "editor.h"
#include "config.h"

// Get time in seconds
inline double get_time() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int main(int argc, char** argv) {
    printf("Zed Text Editor - Starting...\n");

    // TODO: Parse command line arguments
    const char* file_to_open = nullptr;
    if (argc > 1) {
        file_to_open = argv[1];
    }

    // Load configuration
    Config config;
    if (!config_load(&config, "assets/default_config.json")) {
        fprintf(stderr, "Warning: Could not load config, using defaults\n");
        config_set_defaults(&config);
    }

    // Initialize platform (X11 window + OpenGL context)
    Platform platform;
    if (!platform_init(&platform, &config)) {
        fprintf(stderr, "Error: Failed to initialize platform\n");
        return 1;
    }

    // Initialize renderer
    Renderer renderer;
    if (!renderer_init(&renderer, &config)) {
        fprintf(stderr, "Error: Failed to initialize renderer\n");
        platform_shutdown(&platform);
        return 1;
    }

    // Initialize editor
    Editor editor;
    editor_init(&editor, &config);

    // Sync editor font metrics from renderer
    editor_sync_font_metrics(&editor, &renderer);

    // Load file if specified
    if (file_to_open) {
        if (!editor_open_file(&editor, file_to_open)) {
            fprintf(stderr, "Warning: Could not open file: %s\n", file_to_open);
        }
    }

    // Main event loop
    printf("Entering main loop...\n");
    bool running = true;
    bool show_fps = true;  // Toggle with F3
    int frame_count = 0;
    double last_time = get_time();
    double fps_update_time = last_time;
    int fps_frame_count = 0;
    float current_fps = 0.0f;

    // Adaptive VSync state
    struct AdaptiveVSyncState {
        double monitor_refresh_rate;    // Hz (detected or assumed 60)
        double target_frame_time;       // Seconds (1/refresh_rate)
        double vsync_threshold_high;    // Enable VSync above this (95% of frame time)
        double vsync_threshold_low;     // Disable VSync below this (110% of frame time)
        int consecutive_fast_frames;    // Counter for hysteresis
        int consecutive_slow_frames;    // Counter for hysteresis
        int hysteresis_count;           // Frames needed to switch (prevent flicker)
        bool adaptive_enabled;          // From config (or default true)
    } vsync_state;

    // Initialize adaptive VSync state from config
    vsync_state.monitor_refresh_rate = 60.0;  // TODO: Detect via XRandR
    vsync_state.target_frame_time = 1.0 / vsync_state.monitor_refresh_rate;  // 16.67ms for 60Hz
    vsync_state.vsync_threshold_high = vsync_state.target_frame_time * 0.95;  // Enable at 95%
    vsync_state.vsync_threshold_low = vsync_state.target_frame_time * 1.1;    // Disable at 110%
    vsync_state.consecutive_fast_frames = 0;
    vsync_state.consecutive_slow_frames = 0;
    vsync_state.hysteresis_count = config.vsync_hysteresis_frames;
    vsync_state.adaptive_enabled = config.adaptive_vsync;

    printf("Adaptive VSync: Target frame time: %.2f ms (%.1f Hz)\n",
           vsync_state.target_frame_time * 1000.0,
           vsync_state.monitor_refresh_rate);

    // Apply config overrides (force modes disable adaptive)
    if (config.force_vsync_off) {
        platform_set_swap_interval(&platform, 0);
        vsync_state.adaptive_enabled = false;
        printf("Adaptive VSync: FORCE OFF - VSync permanently disabled (uncapped FPS)\n");
    } else if (config.force_vsync_on) {
        platform_set_swap_interval(&platform, 1);
        vsync_state.adaptive_enabled = false;
        printf("Adaptive VSync: FORCE ON - VSync permanently enabled (locked 60 FPS)\n");
    } else if (vsync_state.adaptive_enabled) {
        printf("Adaptive VSync: ENABLED - Smart switching with %d-frame hysteresis\n",
               vsync_state.hysteresis_count);
    } else {
        printf("Adaptive VSync: DISABLED - Using driver default\n");
    }

    while (running) {
        // Calculate delta time
        double current_time = get_time();
        float delta_time = (float)(current_time - last_time);
        last_time = current_time;

        // Adaptive VSync decision logic
        if (vsync_state.adaptive_enabled && platform.adaptive_vsync_supported) {
            // Check if rendering faster than monitor refresh
            if (delta_time < vsync_state.vsync_threshold_high) {
                // Fast frame - rendering faster than refresh rate
                vsync_state.consecutive_fast_frames++;
                vsync_state.consecutive_slow_frames = 0;

                // Enable VSync if consistently fast (hysteresis prevents rapid toggling)
                if (vsync_state.consecutive_fast_frames >= vsync_state.hysteresis_count &&
                    !platform.vsync_enabled) {
                    platform_set_swap_interval(&platform, 1);
                    printf("Adaptive VSync: ENABLED (smooth %.1f fps)\n", current_fps);
                }
            }
            else if (delta_time > vsync_state.vsync_threshold_low) {
                // Slow frame - rendering slower than refresh rate
                vsync_state.consecutive_slow_frames++;
                vsync_state.consecutive_fast_frames = 0;

                // Disable VSync if consistently slow (hysteresis prevents rapid toggling)
                if (vsync_state.consecutive_slow_frames >= vsync_state.hysteresis_count &&
                    platform.vsync_enabled) {
                    platform_set_swap_interval(&platform, 0);
                    printf("Adaptive VSync: DISABLED (unlocked for %.1f fps)\n", current_fps);
                }
            }
            else {
                // Frame time in middle zone - don't switch (dead zone prevents flickering)
                vsync_state.consecutive_fast_frames = 0;
                vsync_state.consecutive_slow_frames = 0;
            }
        }

        // Calculate FPS (update every 0.5 seconds)
        fps_frame_count++;
        if (current_time - fps_update_time >= 0.5) {
            current_fps = fps_frame_count / (current_time - fps_update_time);
            fps_frame_count = 0;
            fps_update_time = current_time;
        }

        // Process events
        PlatformEvent event;
        while (platform_poll_event(&platform, &event)) {
            if (event.type == PLATFORM_EVENT_QUIT) {
                printf("Quit event received\n");
                running = false;
            } else if (event.type == PLATFORM_EVENT_KEY_PRESS && event.key.key == 0xffc0) {
                // F3 key - toggle FPS display
                show_fps = !show_fps;
                printf("FPS display: %s\n", show_fps ? "ON" : "OFF");
            } else {
                editor_handle_event(&editor, &event, &renderer, &platform);
            }
        }

        if (frame_count == 0) {
            printf("Rendering first frame...\n");
        }

        // Update editor state
        editor_update(&editor, delta_time);

        // Render
        renderer_begin_frame(&renderer);
        editor_render(&editor, &renderer);

        // Render FPS overlay
        if (show_fps) {
            char fps_text[32];
            snprintf(fps_text, sizeof(fps_text), "FPS: %.1f", current_fps);
            Color fps_color = {0.5f, 0.8f, 0.5f, 1.0f};  // Light green
            renderer_add_text(&renderer, fps_text,
                            (float)renderer.viewport_width - 100.0f, 20.0f,
                            fps_color);
        }

        renderer_end_frame(&renderer);
        platform_swap_buffers(&platform);

        frame_count++;
        // Temporarily disabled for debugging
        // if (frame_count % 60 == 0) {
        //     printf("Frame %d (%.1f FPS)\n", frame_count, current_fps);
        // }
    }

    // Cleanup
    printf("Shutting down...\n");
    editor_shutdown(&editor);
    renderer_shutdown(&renderer);
    platform_shutdown(&platform);
    config_free(&config);

    return 0;
}
