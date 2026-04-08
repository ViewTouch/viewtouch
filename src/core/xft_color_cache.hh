#pragma once

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <mutex>
#include <unordered_map>
#include <cstdint>

namespace vt::core {

struct ColorKey {
    Display* display;
    int screen;
    unsigned short red;
    unsigned short green;
    unsigned short blue;
    unsigned short alpha;

    bool operator==(ColorKey const& o) const noexcept
    {
        return display == o.display && screen == o.screen && red == o.red && green == o.green && blue == o.blue && alpha == o.alpha;
    }
};

struct ColorKeyHash {
    std::size_t operator()(ColorKey const& k) const noexcept
    {
        std::size_t h1 = std::hash<std::uintptr_t>()(reinterpret_cast<std::uintptr_t>(k.display));
        std::size_t h2 = std::hash<int>()(k.screen);
        std::uint64_t colors = (static_cast<std::uint64_t>(k.red)) |
                               (static_cast<std::uint64_t>(k.green) << 16) |
                               (static_cast<std::uint64_t>(k.blue)  << 32) |
                               (static_cast<std::uint64_t>(k.alpha) << 48);
        std::size_t h3 = std::hash<std::uint64_t>()(colors);
        // combine hashes
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1<<6) + (h1>>2)) ^ (h3 + 0x9e3779b97f4a7c15ULL + (h2<<6) + (h2>>2));
    }
};

class XftColorCache {
public:
    static XftColorCache& Instance();

    // Returns a pointer to an XftColor owned by the cache. Pointer remains valid
    // until the cache is destroyed (typically at program exit).
    XftColor* Get(Display* display, int screen_number, const XRenderColor& color);

private:
    XftColorCache() = default;
    ~XftColorCache();

    XftColorCache(XftColorCache const&) = delete;
    XftColorCache& operator=(XftColorCache const&) = delete;

    std::mutex mutex_;
    std::unordered_map<ColorKey, XftColor, ColorKeyHash> cache_;
};

} // namespace vt::core
