# Zed Text Editor - Technical Specification

## Project Overview
A high-performance C++ visual multiline text editor with no frameworks, targeting 144+ fps with GB-sized files. Features an OpenGL rendering backend, FreeType font rendering, and a minimal, keyboard-driven interface.

## Core Design Principles
- **Zero dependencies on frameworks** - Build everything from scratch
- **Extreme performance** - 144+ fps even with GB-sized files
- **Minimal UI** - Text area only, no chrome, keyboard-driven
- **Code clarity over cleverness** - Readable, maintainable implementation
- **Keep dependencies minimal** - Vendor what we need, full control

## Performance Targets
- **Frame rate**: 144+ fps sustained
- **File size**: Support GB-sized text files
- **Startup**: Show window immediately, load files asynchronously
- **Main risk**: Lazy rope materialization complexity

---

## 1. Text Buffer Architecture

### 1.1 Rope Data Structure
**Implementation**: Tree of strings (rope) with AVL balancing

**Configuration**:
- Node size: 256-512 bytes (small nodes for cache efficiency)
- Balancing: AVL tree (height ≤ 1.44 log n)
- Provides O(log n) insertions, deletions, and lookups

**Memory Strategy**: Lazy loading with rope materialization
- Keep file on disk (mmap)
- Load rope nodes on-demand around viewport
- LRU cache for rope nodes with eviction policy
- Cannot hold entire GB file in memory
- Enables arbitrary file sizes

### 1.2 Line Metadata Tree
**Purpose**: Fast random access to line positions for viewport rendering

**Cached Information**:
- Line start byte offsets in rope
- Line lengths (character count)
- Visual line wrapping info (for soft-wrap mode)

**Integration**: Built lazily alongside rope, queried during rendering

**Benefits**:
- Jump to line N in O(log n) instead of scanning from start
- Supports fast viewport rendering
- Enables features like minimap (future)

---

## 2. Rendering System

### 2.1 OpenGL Configuration
**Version**: OpenGL 3.3 core profile
- Maximum compatibility across Linux systems
- Core profile only (no deprecated features)
- Buffer orphaning via `glBufferData(NULL)` for updates

### 2.2 Text Rendering Pipeline
**Font Rendering**: FreeType only (no text shaping)
- Simple Latin text focus
- No ligatures, no RTL, no complex scripts
- Direct glyph-to-quad mapping

**Glyph Atlas Management**: Dynamic atlas with LRU eviction
- Pack glyphs on-demand into texture atlas
- Evict least-recently-used glyphs when atlas fills
- Handles arbitrary font sizes and zoom levels
- Requires runtime texture updates

**Anti-aliasing**: Subpixel AA with gamma correction
- RGB subpixel rendering for LCD displays
- Gamma-correct blending for proper color
- Requires RGB texture format per glyph
- Highest quality text rendering

### 2.3 Geometry Submission
**Strategy**: Single draw call, instanced rendering
- `glDrawArraysInstanced` - one call per frame
- Minimal driver overhead
- Maximum throughput

**Instance Data Format**: Interleaved vertex attributes
```
struct GlyphInstance {
    float x, y;           // Screen position
    float u, v;           // Atlas texture coordinates
    float r, g, b, a;     // Color (for selections, cursor, themes)
}
```

**Vertex Buffer**: Dynamic buffer with orphaning
- Generate instances for visible viewport each frame
- Upload via `glBufferData` with orphaning
- Avoids stalls, maintains 144fps

### 2.4 Viewport Rendering
**Strategy**: Pure virtual scrolling with lazy line metadata tree
- Only traverse rope for visible viewport lines
- Use line metadata tree for O(log n) access to line N
- Regenerate geometry each frame
- Must complete rope traversal + layout in <7ms for 144fps target

**Culling**: Render only visible glyphs in viewport window

---

## 3. Animation System

### 3.1 Text Scale Animation
**Requirement**: Quickly animate the scale of all rendered text

**Implementation**: Regenerate atlas at target size
- On scale change, rebuild glyph atlas at new pixel size
- Crisp rendering at all sizes
- Trade-off: Atlas rebuild cost vs visual quality
- May cache common zoom levels to reduce regeneration

### 3.2 Cursor Animation
**Features**:
- Blinking animation with smooth fade (timer-based alpha)
- Multiple cursor highlighting (different visual treatment for non-primary cursors)
- No smooth position interpolation (instant movement)

### 3.3 Scrolling
**Type**: Pixel-perfect scrolling
- Allow fractional line offsets
- Smoother than line-snapped scrolling
- Instant jump (no smooth animation for MVP)

---

## 4. Platform Layer

### 4.1 Target Platforms
**Primary**: Linux (X11)
- Raw X11/XCB for window creation
- GLX for OpenGL context
- Full control over window management

**Future**: Wayland support
- Add after X11 MVP is working
- Separate platform abstraction layer

**Not in scope**: Windows, macOS (Linux-first approach)

### 4.2 Window Management
**Creation**: Direct X11/XCB calls
- Create window with XCB
- Set up GLX context for OpenGL 3.3
- Handle resize, input events, clipboard

**HiDPI Support**: Query DPI and scale UI accordingly
- Read monitor DPI from X11
- Scale font size and UI elements
- Handle DPI changes (monitor switching)
- Support fractional scaling

---

## 5. Input System

### 5.1 Input Model
**Architecture**: Hybrid extensible command palette
- Normal editing: standard keybindings (Ctrl+C/V, arrows, etc.)
- Advanced operations: Ctrl+Shift+P command palette
- Approachable for beginners, powerful for experts

### 5.2 Keybinding System
**Configuration**: JSON config with modifier + key combos
```json
{
  "keybindings": {
    "Ctrl+S": "save",
    "Ctrl+F": "search",
    "Alt+Shift+F": "format"
  }
}
```
- Parse into lookup table at startup
- Dispatch to command handlers
- Manual reload via `:reload-config` command

### 5.3 Mouse Interaction
**Text Selection**:
- Click and drag: character-level selection
- Double-click: select word (whitespace/punctuation boundaries)
- Triple-click: select entire line
- Shift+click: extend selection from cursor

**Multi-cursor**:
- Alt+Click: add/remove cursor at mouse position
- Requires mouse coordinate → text position mapping

### 5.4 Multi-cursor Editing
**Interaction Model**: Both mouse and keyboard
- Mouse: Alt+Click to add cursor
- Keyboard: Ctrl+D to add cursor at next match (like Sublime)
- All cursors edit simultaneously
- Requires cursor state management and merged edit operations

---

## 6. Features

### 6.1 MVP Scope (Phase 1)
**Core Editing Only**:
- Text insertion and deletion
- Single cursor movement (arrows, home, end, page up/down)
- Undo/redo
- File open and save
- Basic selection (mouse and keyboard)

**Goal**: Get something working fast, validate performance targets

### 6.2 Phase 2 Features
After MVP is solid:
- Multi-cursor editing
- Search (find text)
- Command palette
- Theme system

### 6.3 Phase 3 Features
Later additions:
- Replace functionality (search & replace)
- Syntax highlighting (background thread)
- More advanced editing commands

### 6.4 Undo/Redo System
**Implementation**: Store edit operation deltas
- Stack of `(insert/delete, position, text)` operations
- Reverse operations to undo
- Minimal memory usage
- Trade-off: Replaying edits on lazy rope requires materializing affected nodes

### 6.5 Search & Replace
**Strategy**: Incremental search
- Search visible viewport first (instant feedback)
- Background thread scans rest of file
- Display results progressively
- Parallel scan: divide rope across threads for performance

**Find**: Regex support with match highlighting
**Replace**: Batch operations (Phase 3)

### 6.6 Selection Rendering
**Implementation**: Simple rectangle overlay per line
- Render colored quads behind selected text
- One rectangle per line segment
- Handles multi-line selections
- Works with multi-cursor

### 6.7 Long Line Handling
**Modes**: Switchable between two modes
1. **Horizontal scrolling**: Scroll horizontally to view off-screen content
2. **Soft wrap**: Break logical line into visual lines at window width

**Implementation**:
- Line metadata tree caches visual line wrapping info
- Complicates rope↔screen mapping
- Cursor movement must handle logical vs visual lines

---

## 7. UI/UX

### 7.1 Visual Design
**Layout**: Minimal - text area only
- No status bar
- No line numbers gutter
- No minimap
- Pure text canvas, maximum screen real estate
- Everything via keyboard shortcuts

### 7.2 Theme System
**Configuration**: Configurable theme system (dark/light)
- JSON/JSON5 theme files
- Support multiple themes
- Colors: background, foreground, cursor, selection, etc.
- Hot reload via `:reload-config` command

**Example theme**:
```json
{
  "theme": {
    "background": "#1e1e1e",
    "foreground": "#d4d4d4",
    "cursor": "#00ff00",
    "selection": "#264f78"
  }
}
```

### 7.3 Command Palette
**Implementation**: Overlay with fuzzy search
- Render command list over editor (modal overlay)
- Fuzzy match as user types (fzf-style)
- Requires separate UI rendering pass
- Fuzzy scoring algorithm (simple substring match for MVP)

---

## 8. Text Handling

### 8.1 Character Encoding
**Support**: Auto-detect UTF-8/UTF-16/Latin1
- Detect encoding from BOM or heuristics
- Convert to UTF-8 internally
- All rope operations work on UTF-8

**Wide Characters**: Measure each glyph width properly
- Query actual glyph advance from FreeType
- Handle double-width CJK characters
- Handle emoji (potentially multi-codepoint)
- Handle zero-width combining marks

### 8.2 Line Endings
**Strategy**: Auto-detect, preserve on save
- Detect LF/CRLF/CR from file content
- Normalize internally to LF (simpler rope operations)
- Write back in original format on save
- Per-file line ending tracking

### 8.3 Tab Handling
**Configuration**: Configurable tab width + spaces/tabs
```json
{
  "tab_width": 4,
  "use_spaces": true
}
```
- Tab key inserts spaces or `\t` based on config
- Display `\t` characters as N spaces
- Configurable tab width (default: 4)

### 8.4 Word Boundaries
**Definition**: Traditional editor (whitespace boundaries)
- Ctrl+Left/Right stops at whitespace
- Treats `snake_case` and `camelCase` as single words
- Used for word selection and cursor movement

---

## 9. Threading Model

### 9.1 Thread Architecture
**Threads**:
1. **Main thread**: UI, rendering, input handling
2. **I/O thread**: Async file loading and rope building
3. **Search thread**: Background search across file
4. **Worker threads**: Parallel rope operations (search, bulk edits)

### 9.2 Thread Safety
**Rope Access**: Requires synchronization for parallel operations
- Read locks for search
- Write locks for edits
- Main thread owns rope, workers request access

### 9.3 Async Operations
**File Loading**:
- Show window immediately with "Loading..." message
- Build rope on I/O thread
- Update UI when ready
- Responsive startup even for GB files

**Background Syntax Highlighting** (Phase 3):
- Tokenize on worker thread
- Don't block rendering
- Update highlighting incrementally

---

## 10. File Operations

### 10.1 File Loading
**UX**: Show window immediately, load async
- Create window and OpenGL context instantly
- Display "Loading..." message
- Build rope structure on background thread
- Render text when ready

**Strategy**:
- `mmap` file for reading
- Build rope nodes referencing mmap'd regions
- Lazy materialization via LRU cache

### 10.2 File Saving
**Strategy**: Atomic write with temp file
- Write to `.tmp` file
- `fsync()` to ensure data on disk
- Rename over original (atomic operation)
- Prevents corruption on crash/power loss

**Line Ending Preservation**: Write in detected format

### 10.3 External Changes
**Watching**: Yes, prompt to reload on change
- Use `inotify` to watch open file
- Detect modifications by other programs
- Show dialog: "File changed on disk. Reload?"
- Prevent accidental overwrites

---

## 11. Memory Management

### 11.1 Allocation Strategy
**Custom allocators**:
- **Pool allocator** for rope nodes (fixed size)
  - Pre-allocate blocks of 256-512 byte nodes
  - Fast allocation/deallocation
  - Better cache locality
- **Arena allocator** for temporary/frame allocations
  - Per-frame scratch buffer
  - Reset after each frame
  - Zero fragmentation

**LRU Cache**: For rope node materialization
- Hash map of loaded rope nodes
- Doubly-linked list for LRU ordering
- Evict least-recently-used when cache limit reached
- Configurable cache size (e.g., 100MB of rope nodes)

### 11.2 GPU Memory
**Glyph Atlas**:
- Texture atlas for glyph cache (e.g., 2048x2048 RGBA)
- LRU eviction when atlas fills
- Subpixel AA requires RGB texture (3x memory vs grayscale)

**Vertex Buffers**:
- Dynamic VBO for glyph instances
- Sized for maximum viewport (e.g., 100k glyphs)
- Orphaning prevents stalls

---

## 12. Build System

### 12.1 Build Configuration
**System**: Makefile only
- Simple, no external build tools required
- Single Makefile in root

**Dependencies**: Vendored
- Include FreeType source in tree
- No external package managers
- Full control over build

**Compilation**: Unity build for fast iteration
- Optionally compile all .cpp into single translation unit
- Faster compile times during development
- Can split for release builds

### 12.2 Project Structure
**Organization**: Header-only with impl in headers
```
zed/
├── Makefile
├── src/
│   ├── main.cpp          # Entry point
│   ├── rope.h            # Rope data structure
│   ├── renderer.h        # OpenGL rendering
│   ├── platform.h        # X11 window/input
│   ├── editor.h          # Editor state
│   ├── config.h          # Configuration loading
│   ├── commands.h        # Command system
│   └── shaders.h         # GLSL shaders (embedded strings)
├── vendor/
│   └── freetype/         # FreeType source
├── tests/
│   ├── rope_test.cpp     # Rope unit tests
│   └── render_test.cpp   # Rendering integration tests
└── assets/
    ├── default_theme.json
    └── default_config.json
```

### 12.3 Testing
**Unit Tests**: Rope operations
- Test insert/delete at various positions
- Verify AVL balancing properties
- Fuzz test with random operations
- Validate lazy loading correctness

**Integration Tests**: Rendering
- Snapshot tests: render text, compare pixels to reference
- Brittle across GPU drivers, but catches visual regressions

**Benchmarks**: Performance validation
- Measure frame times with large files
- Rope operation latency
- Memory usage profiling
- Continuous monitoring to prevent regressions

---

## 13. Configuration System

### 13.1 Config File Format
**Format**: JSON or JSON5
- Standard, easy to parse
- Readable and editable
- Many libraries available

**Location**: `~/.config/zed/config.json`

**Structure**:
```json
{
  "font": {
    "path": "/usr/share/fonts/truetype/FiraCode-Regular.ttf",
    "size": 14
  },
  "theme": {
    "background": "#1e1e1e",
    "foreground": "#d4d4d4",
    "cursor": "#00ff00",
    "selection": "#264f78"
  },
  "editor": {
    "tab_width": 4,
    "use_spaces": true,
    "line_wrap": "none"  // "none" | "soft"
  },
  "keybindings": {
    "Ctrl+S": "save",
    "Ctrl+O": "open",
    "Ctrl+F": "search",
    "Ctrl+Shift+P": "command_palette"
  }
}
```

### 13.2 Config Reloading
**Method**: Manual reload command
- `:reload-config` command in command palette
- Re-reads config file
- Applies new theme, keybindings, settings
- No automatic file watching (simpler)

---

## 14. Clipboard Integration

### 14.1 X11 Clipboard Support
**Selections**:
- **PRIMARY**: Mouse select = copy, middle-click = paste
  - Traditional X11 behavior
  - Requires handling `XA_PRIMARY` atom
- **CLIPBOARD**: Ctrl+C/V
  - Modern clipboard
  - Handle `XA_CLIPBOARD` atom
  - Format negotiation for different data types

**Implementation**:
- Listen for selection events
- Respond to `SelectionRequest` events
- Convert rope content to requested format
- Handle UTF-8 text format

---

## 15. Performance & Profiling

### 15.1 Performance Monitoring
**Frame Time Graph Overlay**:
- Real-time graph of frame times
- Overlaid on editor (toggle with key)
- Instant feedback on performance
- Visual indication of hitches

**Implementation**:
- Ring buffer of frame times (last 60-120 frames)
- Render as line graph in corner
- Color-coded: green <7ms, yellow <16ms, red >16ms

### 15.2 Optimization Strategy
**Compile Time**: Header-only design, unity builds
- Forward declarations to minimize includes
- Fast iteration during development

**Runtime**:
- Profile-guided optimization
- Focus on hot paths (rope traversal, glyph layout)
- Custom allocators for cache locality
- Minimize allocations in render loop

---

## 16. Logging & Debugging

### 16.1 Logging System
**Implementation**: Runtime log levels
- Levels: `DEBUG`, `INFO`, `WARN`, `ERROR`
- Filter by level at runtime
- Log to stderr or file

**Example**:
```cpp
LOG(INFO, "Loading file: %s", path);
LOG(ERROR, "Failed to open file: %s", strerror(errno));
```

**Configuration**:
- Set log level via command-line flag: `--log-level=debug`
- Or in config file

### 16.2 Error Handling
**Philosophy**: Minimal - basic error checks only
- Handle file I/O errors
- Handle allocation failures
- Trust internal code correctness
- Smaller code footprint

**Not doing**:
- Extensive validation of internal invariants
- Graceful degradation on programmer errors
- Deep error recovery logic

---

## 17. Shader Architecture

### 17.1 Vertex Shader
**Responsibilities**:
- Transform glyph quad instances to screen space
- Pass texture coordinates to fragment shader
- Pass color to fragment shader

**Inputs**:
- Per-instance: position (x, y), texcoords (u, v), color (rgba)
- Uniforms: projection matrix, viewport size

### 17.2 Fragment Shader
**Responsibilities**:
- Sample glyph atlas texture
- Apply subpixel AA with gamma correction
- Output final color

**Subpixel Rendering**:
- Sample RGB channels from atlas
- Apply gamma correction (e.g., pow(color, 1/2.2))
- Blend with background using correct gamma

---

## 18. Future Considerations

### 18.1 Not in Current Scope
- Syntax highlighting (architecture supports, not in MVP)
- Minimap
- Split views
- Plugin system
- Network editing (collaborative)
- Git integration
- LSP integration

### 18.2 Potential Enhancements
- Tree-sitter integration for syntax highlighting
- Wayland native support
- macOS/Windows ports
- More sophisticated fuzzy matching
- Persistent undo (serialize undo history to disk)
- Session management (remember open files)

---

## 19. Risk Mitigation

### 19.1 Identified Risks
**Primary Risk**: Lazy rope complexity
- Complex interactions between rope cache, line metadata, viewport
- Potential for bugs, crashes, data corruption
- Memory leaks in LRU eviction

**Mitigation**:
- Extensive unit testing of rope operations
- Careful memory management with custom allocators
- Clear ownership and lifetime semantics
- Start simple: implement full in-memory rope first, add lazy loading later

**Secondary Risks**:
- Meeting 144fps target with GB files
  - Mitigation: Early benchmarking, profile-guided optimization
- X11/Wayland platform differences
  - Mitigation: X11 first, abstract platform later
- Scope creep
  - Mitigation: Strict MVP definition, phased development

---

## 20. Success Criteria

### 20.1 MVP Success
- Opens and renders GB-sized text files
- Maintains 144fps during scrolling and editing
- Supports basic editing: insert, delete, undo
- Smooth mouse selection and cursor movement
- Loads config and applies theme correctly
- Stable (no crashes in normal use)

### 20.2 Performance Benchmarks
- File loading: <1s to show first viewport for 1GB file
- Frame time: <7ms (144fps) for viewport scrolling
- Memory: <200MB overhead for editor (excluding file mmap)
- Rope operations: <1ms for insert/delete on GB file

### 20.3 Code Quality
- Clean, readable code (focus on clarity)
- Well-tested rope implementation
- Minimal dependencies (vendored only)
- Fast compile times (<5s clean build)

---

## Appendix A: Technology Stack Summary

| Component | Technology |
|-----------|------------|
| Language | C++ (C++17 or later) |
| Build System | Makefile |
| Windowing | X11/XCB (raw) |
| OpenGL | 3.3 Core |
| Font Rendering | FreeType |
| Text Buffer | Rope (AVL-balanced tree) |
| Config Format | JSON/JSON5 |
| Threading | pthreads |
| File I/O | mmap + POSIX |
| Clipboard | X11 selections |

## Appendix B: Key Architectural Decisions

1. **Rope over Gap Buffer**: Chosen for O(log n) operations, essential for multi-cursor and GB files
2. **Lazy Materialization**: Necessary to support GB files without proportional RAM
3. **OpenGL 3.3**: Balance of performance and compatibility
4. **No Text Shaping**: Simplifies pipeline, acceptable for Latin-only text
5. **Subpixel AA**: Highest quality, worth the complexity
6. **Header-only**: Fast compilation, easier to see full implementation
7. **Minimal UI**: Maximum simplicity, keyboard-driven workflow
8. **Phased Development**: MVP first to validate performance, features later

---

**End of Specification**
