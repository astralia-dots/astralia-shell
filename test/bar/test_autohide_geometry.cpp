#include <cassert>

#include "modules/bar/styles/geometry.h"

void test_bar_autohide_geometry() {
    using bar_detail::bar_autohide_geometry;
    using bar_detail::BarGeometry;

    BarGeometry shown = bar_autohide_geometry(false, false, 40, 10, 0);
    assert(shown.height == 40 && shown.margin_top == 10 && shown.exclusive_zone == 40);

    BarGeometry hugged = bar_autohide_geometry(false, false, 40, 10, 30);
    assert(hugged.height == 70 && hugged.margin_top == 10 && hugged.exclusive_zone == 40);

    BarGeometry revealed = bar_autohide_geometry(true, false, 40, 10, 30);
    assert(revealed.height == 80 && revealed.margin_top == 0 && revealed.exclusive_zone == 0);

    BarGeometry collapsed = bar_autohide_geometry(true, true, 40, 10, 30);
    assert(collapsed.height == 1 && collapsed.margin_top == 0 && collapsed.exclusive_zone == 0);
}
