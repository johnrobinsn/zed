// Platform layer - X11/XCB window and input handling

#ifndef ZED_PLATFORM_H
#define ZED_PLATFORM_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "config.h"

// Event types
enum PlatformEventType {
    PLATFORM_EVENT_NONE,
    PLATFORM_EVENT_QUIT,
    PLATFORM_EVENT_KEY_PRESS,
    PLATFORM_EVENT_KEY_RELEASE,
    PLATFORM_EVENT_MOUSE_BUTTON,
    PLATFORM_EVENT_MOUSE_MOVE,
    PLATFORM_EVENT_MOUSE_WHEEL,
    PLATFORM_EVENT_RESIZE,
};

// Key modifiers
enum PlatformKeyMod {
    PLATFORM_MOD_SHIFT = (1 << 0),
    PLATFORM_MOD_CTRL  = (1 << 1),
    PLATFORM_MOD_ALT   = (1 << 2),
};

// Platform event structure
struct PlatformEvent {
    PlatformEventType type;

    union {
        struct {
            int key;
            int mods;
            char text[8];  // UTF-8 character
        } key;

        struct {
            int button;
            int x, y;
            bool pressed;
        } mouse_button;

        struct {
            int x, y;
        } mouse_move;

        struct {
            int delta;  // Positive = scroll up, negative = scroll down
            int x, y;   // Mouse position
        } mouse_wheel;

        struct {
            int width, height;
        } resize;
    };
};

// Platform state
struct Platform {
    Display* display;
    Window window;
    GLXContext gl_context;
    Atom wm_delete_window;

    // Clipboard atoms
    Atom clipboard_atom;
    Atom utf8_string_atom;
    Atom targets_atom;
    Atom text_atom;

    // Cursors
    Cursor arrow_cursor;
    Cursor ibeam_cursor;

    // Swap control extensions for VSync
    PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT;
    PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA;
    PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI;

    // VSync state
    bool vsync_enabled;
    bool adaptive_vsync_supported;
    int current_swap_interval;

    int width;
    int height;
    float dpi_scale;
};

// Clipboard storage for X11 SelectionRequest handling
static char g_clipboard_buffer[65536] = "";

// Test clipboard for headless testing
static char g_test_clipboard[4096] = "";

// Forward declarations
inline void platform_init_swap_control(Platform* platform);
inline void platform_set_swap_interval(Platform* platform, int interval);

// Initialize platform (create window and OpenGL context)
inline bool platform_init(Platform* platform, Config* config) {
    // Open X11 display
    platform->display = XOpenDisplay(nullptr);
    if (!platform->display) {
        fprintf(stderr, "Failed to open X11 display\n");
        return false;
    }

    // Get default screen
    int screen = DefaultScreen(platform->display);

    // Query DPI for HiDPI support
    int screen_width_px = DisplayWidth(platform->display, screen);
    int screen_width_mm = DisplayWidthMM(platform->display, screen);
    float dpi = (screen_width_px * 25.4f) / screen_width_mm;
    platform->dpi_scale = dpi / 96.0f;  // 96 DPI is baseline
    printf("Display DPI: %.1f (scale: %.2f)\n", dpi, platform->dpi_scale);

    // Adjust font size for DPI
    config->font_size = (int)(config->font_size * platform->dpi_scale);

    // Choose OpenGL visual
    GLint visual_attribs[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        None
    };

    XVisualInfo* visual = glXChooseVisual(platform->display, screen, visual_attribs);
    if (!visual) {
        fprintf(stderr, "Failed to choose OpenGL visual\n");
        XCloseDisplay(platform->display);
        return false;
    }

    // Create window
    Window root = RootWindow(platform->display, screen);
    XSetWindowAttributes window_attrs;
    window_attrs.colormap = XCreateColormap(platform->display, root, visual->visual, AllocNone);
    window_attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                              ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                              StructureNotifyMask;

    platform->width = 1280;
    platform->height = 720;

    platform->window = XCreateWindow(
        platform->display, root,
        0, 0, platform->width, platform->height,
        0, visual->depth, InputOutput, visual->visual,
        CWColormap | CWEventMask, &window_attrs
    );

    if (!platform->window) {
        fprintf(stderr, "Failed to create window\n");
        XFree(visual);
        XCloseDisplay(platform->display);
        return false;
    }

    // Set window title
    XStoreName(platform->display, platform->window, "Zed");

    // Handle window close event
    platform->wm_delete_window = XInternAtom(platform->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(platform->display, platform->window, &platform->wm_delete_window, 1);

    // Create OpenGL context
    platform->gl_context = glXCreateContext(platform->display, visual, nullptr, GL_TRUE);
    if (!platform->gl_context) {
        fprintf(stderr, "Failed to create OpenGL context\n");
        XDestroyWindow(platform->display, platform->window);
        XFree(visual);
        XCloseDisplay(platform->display);
        return false;
    }

    // Make context current
    glXMakeCurrent(platform->display, platform->window, platform->gl_context);

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum glew_err = glewInit();
    if (glew_err != GLEW_OK) {
        fprintf(stderr, "GLEW initialization failed: %s\n", glewGetErrorString(glew_err));
        glXMakeCurrent(platform->display, None, nullptr);
        glXDestroyContext(platform->display, platform->gl_context);
        XDestroyWindow(platform->display, platform->window);
        XFree(visual);
        XCloseDisplay(platform->display);
        return false;
    }

    // Show window
    XMapWindow(platform->display, platform->window);
    XFlush(platform->display);

    // Print OpenGL info
    printf("OpenGL Vendor: %s\n", glGetString(GL_VENDOR));
    printf("OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    // Initialize clipboard atoms
    platform->clipboard_atom = XInternAtom(platform->display, "CLIPBOARD", False);
    platform->utf8_string_atom = XInternAtom(platform->display, "UTF8_STRING", False);
    platform->targets_atom = XInternAtom(platform->display, "TARGETS", False);
    platform->text_atom = XInternAtom(platform->display, "TEXT", False);

    // Create cursors
    platform->arrow_cursor = XCreateFontCursor(platform->display, 2);  // XC_left_ptr
    platform->ibeam_cursor = XCreateFontCursor(platform->display, 152); // XC_xterm
    XDefineCursor(platform->display, platform->window, platform->arrow_cursor);

    // Initialize swap control for adaptive VSync
    platform_init_swap_control(platform);

    XFree(visual);
    return true;
}

// Detect and initialize swap interval extensions for adaptive VSync
inline void platform_init_swap_control(Platform* platform) {
    // Initialize function pointers to null
    platform->glXSwapIntervalEXT = nullptr;
    platform->glXSwapIntervalMESA = nullptr;
    platform->glXSwapIntervalSGI = nullptr;
    platform->vsync_enabled = true;  // Default to enabled (current behavior)
    platform->adaptive_vsync_supported = false;
    platform->current_swap_interval = 1;

    // Query GLX extensions
    const char* extensions = glXQueryExtensionsString(platform->display,
                                                       DefaultScreen(platform->display));

    if (!extensions) {
        printf("VSync: Unable to query GLX extensions\n");
        return;
    }

    // Try GLX_EXT_swap_control (most common, widely supported)
    if (strstr(extensions, "GLX_EXT_swap_control")) {
        platform->glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)
            glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT");

        if (platform->glXSwapIntervalEXT) {
            printf("VSync: GLX_EXT_swap_control available\n");
            platform->adaptive_vsync_supported = true;

            // Check for adaptive tear support (allows interval = -1)
            if (strstr(extensions, "GLX_EXT_swap_control_tear")) {
                printf("VSync: Hardware adaptive tear supported (GLX_EXT_swap_control_tear)\n");
            }
            return;  // Found primary extension
        }
    }

    // Fallback: Try GLX_MESA_swap_control
    if (strstr(extensions, "GLX_MESA_swap_control")) {
        platform->glXSwapIntervalMESA = (PFNGLXSWAPINTERVALMESAPROC)
            glXGetProcAddress((const GLubyte*)"glXSwapIntervalMESA");

        if (platform->glXSwapIntervalMESA) {
            printf("VSync: GLX_MESA_swap_control available (fallback)\n");
            platform->adaptive_vsync_supported = true;
            return;
        }
    }

    // Fallback: Try GLX_SGI_swap_control (older)
    if (strstr(extensions, "GLX_SGI_swap_control")) {
        platform->glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)
            glXGetProcAddress((const GLubyte*)"glXSwapIntervalSGI");

        if (platform->glXSwapIntervalSGI) {
            printf("VSync: GLX_SGI_swap_control available (old fallback)\n");
            platform->adaptive_vsync_supported = true;
            return;
        }
    }

    // No extensions found
    printf("VSync: No swap control extensions found, using driver default (60 FPS cap)\n");
}

// Poll for events
inline bool platform_poll_event(Platform* platform, PlatformEvent* event) {
    if (!XPending(platform->display)) {
        return false;
    }

    XEvent xevent;
    XNextEvent(platform->display, &xevent);

    event->type = PLATFORM_EVENT_NONE;

    switch (xevent.type) {
        case ClientMessage:
            if ((Atom)xevent.xclient.data.l[0] == platform->wm_delete_window) {
                event->type = PLATFORM_EVENT_QUIT;
            }
            break;

        case KeyPress:
        case KeyRelease: {
            event->type = (xevent.type == KeyPress) ? PLATFORM_EVENT_KEY_PRESS : PLATFORM_EVENT_KEY_RELEASE;
            event->key.key = XLookupKeysym(&xevent.xkey, 0);
            event->key.mods = 0;
            if (xevent.xkey.state & ShiftMask) event->key.mods |= PLATFORM_MOD_SHIFT;
            if (xevent.xkey.state & ControlMask) event->key.mods |= PLATFORM_MOD_CTRL;
            if (xevent.xkey.state & Mod1Mask) event->key.mods |= PLATFORM_MOD_ALT;

            // Get character
            char buffer[8] = {0};
            KeySym keysym;
            XLookupString(&xevent.xkey, buffer, sizeof(buffer) - 1, &keysym, nullptr);
            strcpy(event->key.text, buffer);
            break;
        }

        case ButtonPress:
        case ButtonRelease:
            // Mouse wheel events (buttons 4 and 5)
            if (xevent.xbutton.button == 4 || xevent.xbutton.button == 5) {
                if (xevent.type == ButtonPress) {  // Only handle press, not release
                    event->type = PLATFORM_EVENT_MOUSE_WHEEL;
                    event->mouse_wheel.delta = (xevent.xbutton.button == 4) ? 1 : -1;
                    event->mouse_wheel.x = xevent.xbutton.x;
                    event->mouse_wheel.y = xevent.xbutton.y;
                }
            } else {
                // Regular mouse buttons
                event->type = PLATFORM_EVENT_MOUSE_BUTTON;
                event->mouse_button.button = xevent.xbutton.button;
                event->mouse_button.x = xevent.xbutton.x;
                event->mouse_button.y = xevent.xbutton.y;
                event->mouse_button.pressed = (xevent.type == ButtonPress);
            }
            break;

        case MotionNotify:
            event->type = PLATFORM_EVENT_MOUSE_MOVE;
            event->mouse_move.x = xevent.xmotion.x;
            event->mouse_move.y = xevent.xmotion.y;
            break;

        case ConfigureNotify:
            if (xevent.xconfigure.width != platform->width ||
                xevent.xconfigure.height != platform->height) {
                platform->width = xevent.xconfigure.width;
                platform->height = xevent.xconfigure.height;
                event->type = PLATFORM_EVENT_RESIZE;
                event->resize.width = platform->width;
                event->resize.height = platform->height;
            }
            break;

        case SelectionRequest: {
            // Handle clipboard request from another application
            XSelectionRequestEvent* req = &xevent.xselectionrequest;

            printf("[CLIPBOARD] SelectionRequest received (selection=%ld, target=%ld)\n",
                   req->selection, req->target);

            XSelectionEvent sel_event;
            sel_event.type = SelectionNotify;
            sel_event.requestor = req->requestor;
            sel_event.selection = req->selection;
            sel_event.target = req->target;
            sel_event.time = req->time;
            sel_event.property = None;  // Default to failure

            // Handle TARGETS request - tell them what formats we support
            if (req->target == platform->targets_atom) {
                printf("[CLIPBOARD] Responding to TARGETS request\n");
                Atom supported_targets[] = {
                    platform->targets_atom,
                    platform->utf8_string_atom,
                    XA_STRING,
                    platform->text_atom
                };
                XChangeProperty(platform->display, req->requestor, req->property,
                               XA_ATOM, 32, PropModeReplace,
                               (unsigned char*)supported_targets,
                               sizeof(supported_targets) / sizeof(Atom));
                sel_event.property = req->property;
            }
            // Handle actual data request
            else if ((req->selection == platform->clipboard_atom || req->selection == XA_PRIMARY) &&
                     (req->target == platform->utf8_string_atom || req->target == XA_STRING || req->target == platform->text_atom)) {
                // Set the property with our clipboard content
                printf("[CLIPBOARD] Responding with text: '%s' (len=%zu)\n",
                       g_clipboard_buffer, strlen(g_clipboard_buffer));
                XChangeProperty(platform->display, req->requestor, req->property,
                               req->target, 8, PropModeReplace,
                               (unsigned char*)g_clipboard_buffer, strlen(g_clipboard_buffer));
                sel_event.property = req->property;  // Success
            } else {
                printf("[CLIPBOARD] Request rejected (selection=%ld, target=%ld, targets_atom=%ld, utf8=%ld, string=%ld)\n",
                       req->selection, req->target, platform->targets_atom,
                       platform->utf8_string_atom, XA_STRING);
            }

            // Send SelectionNotify event back to requestor
            XSendEvent(platform->display, req->requestor, False, 0, (XEvent*)&sel_event);
            XFlush(platform->display);
            break;
        }
    }

    return event->type != PLATFORM_EVENT_NONE;
}

// Set cursor shape
inline void platform_set_cursor(Platform* platform, bool ibeam) {
    Cursor cursor = ibeam ? platform->ibeam_cursor : platform->arrow_cursor;
    XDefineCursor(platform->display, platform->window, cursor);
}

// Swap buffers
inline void platform_swap_buffers(Platform* platform) {
    glXSwapBuffers(platform->display, platform->window);
}

// Set swap interval (0 = no vsync, 1 = vsync enabled)
inline void platform_set_swap_interval(Platform* platform, int interval) {
    if (!platform->adaptive_vsync_supported) {
        return;  // No extension available, can't change swap interval
    }

    if (platform->current_swap_interval == interval) {
        return;  // Already set to this interval, avoid redundant calls
    }

    bool success = false;

    // Try extensions in priority order
    if (platform->glXSwapIntervalEXT) {
        platform->glXSwapIntervalEXT(platform->display, platform->window, interval);
        success = true;
    } else if (platform->glXSwapIntervalMESA) {
        platform->glXSwapIntervalMESA(interval);
        success = true;
    } else if (platform->glXSwapIntervalSGI) {
        platform->glXSwapIntervalSGI(interval);
        success = true;
    }

    if (success) {
        platform->current_swap_interval = interval;
        platform->vsync_enabled = (interval > 0);
    }
}

// Shutdown platform
inline void platform_shutdown(Platform* platform) {
    if (platform->gl_context) {
        glXMakeCurrent(platform->display, None, nullptr);
        glXDestroyContext(platform->display, platform->gl_context);
    }

    if (platform->display) {
        if (platform->arrow_cursor) {
            XFreeCursor(platform->display, platform->arrow_cursor);
        }
        if (platform->ibeam_cursor) {
            XFreeCursor(platform->display, platform->ibeam_cursor);
        }
    }

    if (platform->window) {
        XDestroyWindow(platform->display, platform->window);
    }

    if (platform->display) {
        XCloseDisplay(platform->display);
    }
}

// Set clipboard content
inline void platform_set_clipboard(Platform* platform, const char* text) {
    // If platform is null (testing mode), use test clipboard
    if (!platform || !platform->display) {
        strncpy(g_test_clipboard, text, sizeof(g_test_clipboard) - 1);
        g_test_clipboard[sizeof(g_test_clipboard) - 1] = '\0';
        return;
    }

    // Store clipboard content for SelectionRequest handling
    strncpy(g_clipboard_buffer, text, sizeof(g_clipboard_buffer) - 1);
    g_clipboard_buffer[sizeof(g_clipboard_buffer) - 1] = '\0';

    printf("[CLIPBOARD] Set clipboard: '%s' (len=%zu)\n", text, strlen(text));

    // Set the CLIPBOARD selection
    XSetSelectionOwner(platform->display, platform->clipboard_atom, platform->window, CurrentTime);

    // Also set PRIMARY for middle-click paste
    XSetSelectionOwner(platform->display, XA_PRIMARY, platform->window, CurrentTime);

    XFlush(platform->display);

    printf("[CLIPBOARD] Ownership set, buffer stored\n");
}

// Get clipboard content (simplified - returns empty string if unavailable)
// Note: Full implementation would handle SelectionRequest events properly
inline char* platform_get_clipboard(Platform* platform) {
    // If platform is null (testing mode), use test clipboard
    if (!platform || !platform->display) {
        if (g_test_clipboard[0] == '\0') {
            return nullptr;
        }
        char* result = new char[strlen(g_test_clipboard) + 1];
        strcpy(result, g_test_clipboard);
        return result;
    }

    // Check if we own the clipboard - if so, return our buffer directly
    Window owner = XGetSelectionOwner(platform->display, platform->clipboard_atom);
    if (owner == platform->window && g_clipboard_buffer[0] != '\0') {
        printf("[CLIPBOARD] We own the clipboard, returning local buffer\n");
        char* result = new char[strlen(g_clipboard_buffer) + 1];
        strcpy(result, g_clipboard_buffer);
        return result;
    }

    printf("[CLIPBOARD] Requesting clipboard from owner (window=%ld)\n", owner);

    // Request conversion of CLIPBOARD selection to UTF8_STRING
    XConvertSelection(platform->display, platform->clipboard_atom,
                     platform->utf8_string_atom, platform->clipboard_atom,
                     platform->window, CurrentTime);
    XFlush(platform->display);

    // Wait for SelectionNotify event (with timeout)
    XEvent event;
    int max_wait = 100;  // 100ms timeout
    while (max_wait-- > 0) {
        if (XCheckTypedWindowEvent(platform->display, platform->window, SelectionNotify, &event)) {
            if (event.xselection.property == None) {
                // Selection conversion failed
                return nullptr;
            }

            // Read the property
            Atom actual_type;
            int actual_format;
            unsigned long nitems, bytes_after;
            unsigned char* prop_data = nullptr;

            XGetWindowProperty(platform->display, platform->window,
                             platform->clipboard_atom, 0, (~0L),
                             False, AnyPropertyType,
                             &actual_type, &actual_format,
                             &nitems, &bytes_after, &prop_data);

            if (prop_data) {
                char* result = new char[nitems + 1];
                memcpy(result, prop_data, nitems);
                result[nitems] = '\0';
                XFree(prop_data);
                return result;
            }
            return nullptr;
        }
        usleep(1000);  // Wait 1ms
    }

    return nullptr;  // Timeout
}

#endif // ZED_PLATFORM_H
