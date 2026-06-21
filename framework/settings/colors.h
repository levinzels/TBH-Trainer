#pragma once
#include "imgui.h"
#include <memory>

class c_colors
{
public:

	ImColor layout{ 21, 23, 31 };
	ImColor titlebar_layout{ 23, 25, 34 };
	ImColor white{ 255, 255, 255 };
	ImColor text_inactive{ 83, 86, 101 };
	ImColor tab_button_top{ 24, 36, 35 };
	ImColor tab_button_bot{ 211, 220, 255 };
	ImColor accent{ 160, 183, 227 };
	ImColor black{ 0, 0, 0 };
	ImColor border{ 45, 48, 63 };
	ImColor login_border{ 33, 36, 50 };
	ImColor widget_layout{ 25, 27, 37 };
	ImColor dark_widget{ 29, 31, 42 };
	ImColor line_layout{ 32, 35, 48 };
	ImColor selectable_active{ 35, 37, 50 };
	ImColor checkbox_disabled_circle{ 66, 69, 90 };
};

inline std::unique_ptr<c_colors> clr = std::make_unique<c_colors>();
