// 显示坐标换算：不同屏幕比例下居中显示 3:2 画面，黑边不能触发菜单按钮。
#pragma once
#include <algorithm>
namespace game {
struct Viewport {
    int x, y, width, height;
    bool mapPoint(int px, int py, int& logicalX, int& logicalY) const {
        // 先排除画布外的鼠标，再按实际内容区域映射到 960×640。
        if (px < x || py < y || px >= x + width || py >= y + height) return false;
        logicalX = (px - x) * 960 / width;
        logicalY = (py - y) * 640 / height;
        return true;
    }
};
inline Viewport fitViewport(int clientWidth, int clientHeight) {
    // 使用整数 3:2 单元，保持角色和按钮比例一致，剩余区域留作居中边框。
    const int unit = std::max(1, std::min(clientWidth / 3, clientHeight / 2));
    return {(clientWidth - unit * 3) / 2, (clientHeight - unit * 2) / 2, unit * 3, unit * 2};
}
}
