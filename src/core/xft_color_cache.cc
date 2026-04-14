#include "xft_color_cache.hh"
#include "x11_safe.hh"

namespace vt::core {

XftColorCache& XftColorCache::Instance()
{
    static XftColorCache instance;
    return instance;
}

XftColor* XftColorCache::Get(Display* display, int screen_number, const XRenderColor& color)
{
    if (display == nullptr)
    {
        return nullptr;
    }

    ColorKey key{display, screen_number, color.red, color.green, color.blue, color.alpha};

    std::lock_guard<std::mutex> lk(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end())
    {
        return &it->second;
    }

    XftColor xft{};
    // Check allocation result; XftColorAllocValue may fail on some displays/drivers.
    Bool alloc_ok = XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &color, &xft);
    if (!alloc_ok) {
        // Allocation failed; do not insert an invalid XftColor into the cache.
        return nullptr;
    }

    auto res = cache_.emplace(std::make_pair(key, xft));
    return &res.first->second;
}

XftColorCache::~XftColorCache()
{
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto &entry : cache_)
    {
        const ColorKey &k = entry.first;
        XftColor* c = &entry.second;
        XftColorFreeSafe(k.display, DefaultVisual(k.display, k.screen), DefaultColormap(k.display, k.screen), c);
    }
    cache_.clear();
}

} // namespace vt::core
