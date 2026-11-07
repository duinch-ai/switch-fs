#!/usr/bin/env python3
"""Patch Borealis sidebarItemXML to place active indicator bar on the right of the icon."""
import pathlib

sidebar_cpp = pathlib.Path(__file__).resolve().parent.parent / "library/borealis/library/lib/views/sidebar.cpp"
content = sidebar_cpp.read_text(encoding="utf-8")

new_xml = """const std::string sidebarItemXML = R"xml(
    <brls:Box
        width="auto"
        height="@style/brls/sidebar/item_height"
        focusable="true" >

        <brls:Label
            id="brls/sidebar/item_label"
            width="auto"
            height="auto"
            grow="1.0"
            horizontalAlign="center"
            fontSize="@style/brls/sidebar/item_font_size"
            marginTop="@style/brls/sidebar/item_accent_margin_top_bottom"
            marginBottom="@style/brls/sidebar/item_accent_margin_top_bottom"
            marginLeft="@style/brls/sidebar/item_accent_margin_sides" />

        <brls:Rectangle
            id="brls/sidebar/item_accent"
            width="@style/brls/sidebar/item_accent_rect_width"
            height="auto"
            visibility="invisible"
            color="@theme/brls/sidebar/active_item"
            marginTop="@style/brls/sidebar/item_accent_margin_top_bottom"
            marginBottom="@style/brls/sidebar/item_accent_margin_top_bottom"
            marginLeft="@style/brls/sidebar/item_accent_margin_sides"
            marginRight="@style/brls/sidebar/item_accent_margin_sides" />

    </brls:Box>
)xml";"""

start_marker = 'const std::string sidebarItemXML = R"xml('
end_marker = ')xml";'

start_idx = content.find(start_marker)
end_idx = content.find(end_marker, start_idx)
if start_idx != -1 and end_idx != -1:
    updated = content[:start_idx] + new_xml + content[end_idx + len(end_marker):]
    sidebar_cpp.write_text(updated, encoding="utf-8")
    print("Patched sidebar.cpp successfully (indicator on right).")
else:
    raise RuntimeError("Could not locate sidebarItemXML in sidebar.cpp")

thread_cpp = pathlib.Path(__file__).resolve().parent.parent / "library/borealis/library/lib/core/thread.cpp"
t_content = thread_cpp.read_text(encoding="utf-8")
old_loop = """        for (auto task : m_tasks_copy)
        {
            task();
        }

        retro_sleep(500);"""
new_loop = """        for (auto task : m_tasks_copy)
        {
            if (!task_loop_active) break;
            task();
        }

        if (task_loop_active) retro_sleep(50);"""
if old_loop in t_content:
    thread_cpp.write_text(t_content.replace(old_loop, new_loop), encoding="utf-8")
    print("Patched thread.cpp successfully (fast exit on stop).")
elif "if (!task_loop_active) break;" in t_content:
    print("thread.cpp already patched.")
else:
    raise RuntimeError("Could not locate task_loop in thread.cpp")
