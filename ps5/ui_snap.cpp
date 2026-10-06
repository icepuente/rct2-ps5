/*
 * D-pad navigation for OpenRCT2's interface on PS5.
 *
 * Collects the clickable widgets of the open windows (or the rows of an open
 * dropdown) and moves the cursor to the nearest one in the pressed direction.
 * The virtual mouse (platform/sdl/ps5_virtual_mouse.c) calls ps5_snap_hook on
 * D-pad presses; this file installs it. Compiled with OpenRCT2's own flags
 * because it reads OpenRCT2's window structures (see package-openrct2.sh).
 */
#include <openrct2-ui/interface/Widget.h>
#include <openrct2/config/Config.h>
#include <openrct2/interface/WindowBase.h>

#include <cmath>
#include <cstdlib>
#include <vector>

using namespace OpenRCT2;

namespace
{
    struct Target
    {
        int x, y;               // centre, in game (unscaled) coordinates
        int left, top, right, bottom;
    };

    constexpr int kDropdownItemHeight = 12; // OpenRCT2's default dropdown row height
    constexpr int kDropdownItemHeightLarge = 24;
    constexpr int kScrollStep = 12; // a list row, for stepping inside scroll areas

    bool isClickable(WidgetType type)
    {
        switch (type)
        {
            case WidgetType::imgBtn:
            case WidgetType::colourBtn:
            case WidgetType::trnBtn:
            case WidgetType::tab:
            case WidgetType::flatBtn:
            case WidgetType::button:
            case WidgetType::tableHeader:
            case WidgetType::dropdownMenu:
            case WidgetType::closeBox:
            case WidgetType::checkbox:
            case WidgetType::textBox:
            case WidgetType::scroll:
                return true;
            default:
                return false;
        }
    }

    bool isLive(const WindowBase& w)
    {
        return w.isVisible && !w.flags.has(WindowFlag::dead);
    }

    // Whether a window above `index` in the window list covers the point.
    bool isCovered(size_t index, int x, int y)
    {
        for (size_t i = index + 1; i < gWindowList.size(); i++)
        {
            const auto& w = *gWindowList[i];
            if (isLive(w) && x >= w.windowPos.x && x < w.windowPos.x + w.width && y >= w.windowPos.y
                && y < w.windowPos.y + w.height)
            {
                return true;
            }
        }
        return false;
    }

    // Rows of the topmost open dropdown, or nothing.
    bool collectDropdown(std::vector<Target>& out)
    {
        for (auto it = gWindowList.rbegin(); it != gWindowList.rend(); ++it)
        {
            const auto& w = **it;
            if (!isLive(w) || w.classification != WindowClass::dropdown)
            {
                continue;
            }
            const int rowHeight = Config::Get().interface.enlargedUi ? kDropdownItemHeightLarge : kDropdownItemHeight;
            const int rows = (w.height - 3) / rowHeight;
            for (int r = 0; r < rows; r++)
            {
                const int top = w.windowPos.y + 2 + r * rowHeight;
                out.push_back({ w.windowPos.x + w.width / 2, top + rowHeight / 2, w.windowPos.x, top,
                                w.windowPos.x + w.width - 1, top + rowHeight - 1 });
            }
            return true;
        }
        return false;
    }

    void collectWidgets(std::vector<Target>& out)
    {
        for (size_t i = 0; i < gWindowList.size(); i++)
        {
            const auto& w = *gWindowList[i];
            if (!isLive(w))
            {
                continue;
            }
            for (size_t wi = 0; wi < w.widgets.size(); wi++)
            {
                const auto& widget = w.widgets[wi];
                if (!isClickable(widget.type) || widget.isHidden()
                    || Ui::widgetIsDisabled(w, static_cast<WidgetIndex>(wi)))
                {
                    continue;
                }
                const int left = w.windowPos.x + widget.left;
                const int top = w.windowPos.y + widget.top;
                const int right = w.windowPos.x + widget.right;
                const int bottom = w.windowPos.y + widget.bottom;
                // Aim at the top of a list (its first row), the centre of anything else.
                const int cx = (left + right) / 2;
                const int cy = widget.type == WidgetType::scroll ? top + kScrollStep / 2 : (top + bottom) / 2;
                if (!isCovered(i, cx, cy))
                {
                    out.push_back({ cx, cy, left, top, right, bottom });
                }
            }
        }
    }

    // Step through the rows of a list the cursor is in, if any.
    bool stepInsideList(const std::vector<Target>& targets, int dir, int x, int y, int& nx, int& ny)
    {
        if (dir != 1 && dir != 3)
        {
            return false;
        }
        for (const auto& t : targets)
        {
            const bool isList = t.bottom - t.top > 3 * kScrollStep; // lists are tall; buttons are not
            if (!isList || x < t.left || x > t.right || y < t.top || y > t.bottom)
            {
                continue;
            }
            const int next = y + (dir == 1 ? -kScrollStep : kScrollStep);
            if (next >= t.top + kScrollStep / 2 && next <= t.bottom - kScrollStep / 2)
            {
                nx = x;
                ny = next;
                return true;
            }
        }
        return false;
    }

    /*
     * dir: 0 left, 1 up, 2 right, 3 down. x, y and the result are in window
     * pixels; OpenRCT2's interface is drawn at windowScale times that.
     */
    int snap(int dir, int x, int y, int* outX, int* outY)
    {
        const float scale = Config::Get().general.windowScale;
        const int gx = static_cast<int>(x / scale);
        const int gy = static_cast<int>(y / scale);

        std::vector<Target> targets;
        if (!collectDropdown(targets))
        {
            collectWidgets(targets);
        }

        static constexpr int kDx[] = { -1, 0, 1, 0 };
        static constexpr int kDy[] = { 0, -1, 0, 1 };

        int nx = 0, ny = 0;
        if (!stepInsideList(targets, dir, gx, gy, nx, ny))
        {
            const Target* best = nullptr;
            double bestScore = 0;
            for (const auto& t : targets)
            {
                const int vx = t.x - gx, vy = t.y - gy;
                const int along = vx * kDx[dir] + vy * kDy[dir];
                const int across = std::abs(vx * kDy[dir] - vy * kDx[dir]);
                if (along <= 2)
                {
                    continue; // not in that direction
                }
                // Prefer targets in line with the cursor over ones off to the side.
                const double score = along + 2.0 * across;
                if (best == nullptr || score < bestScore)
                {
                    best = &t;
                    bestScore = score;
                }
            }
            if (best == nullptr)
            {
                return 0;
            }
            nx = best->x;
            ny = best->y;
        }

        *outX = static_cast<int>(nx * scale + scale / 2);
        *outY = static_cast<int>(ny * scale + scale / 2);
        return 1;
    }
} // namespace

extern "C" int (*ps5_snap_hook)(int dir, int x, int y, int* outX, int* outY);

namespace
{
    struct InstallSnapHook
    {
        InstallSnapHook()
        {
            ps5_snap_hook = snap;
        }
    } installSnapHook;
} // namespace
