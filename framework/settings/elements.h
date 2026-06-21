#pragma once
#include <string>
#include "imgui.h"

class c_elements
{
public:

	struct
	{
		float rounding{ 12 };

		ImVec2 size{ 810, 450 };

		window_flags flags{ window_flags_no_decoration | window_flags_no_background | window_flags_no_focus_on_appearing | window_flags_no_bring_to_front_on_focus | window_flags_no_nav | window_flags_nav_flattened };

	} window;

	struct
	{

		float header_height{ 50 };
		float body_height{ 95 };
		float rounding{ 8 };

		ImVec2 size{ 310, 195 };
		ImVec2 header_padding{ 19, 19 };
		ImVec2 body_padding{ 80, 0 };
		ImVec2 body_spacing{ 0, 10 };
		ImVec2 end_padding{ 19, 22 };

	} login_window;

	struct 
	{
		std::string injection_cheat_name;

		float rounding{ 8 };
		float timer{ 0.f };
		float alpha{ 0.f };

		ImVec2 size{ 310, 72 };

	} inject_window;

	struct 
	{
		float height{ 12 };
		float button_width{ 22 };
		float circle_rad{ 3 };
			
	} checkbox;

	struct
	{
		float height{ 60 };
		float logo_width{ 198 };
		float tab_width{ 254 };
		float enter_width{ 198 };
		
		ImVec2 padding{ 80, 0 };
		ImVec2 spacing{ 0, 0 };
		ImVec2 tab_spacing{ 15, 0 };

	} titlebar;

	struct 
	{
		struct {

			float desc_spacing{ 50 };
			
			ImVec2 padding{ 80, 57 };
			ImVec2 size{ 330, 280 };
			ImVec2 spacing{ 15, 25 };
		} tab0;
		
		struct {

			float left_header_height{ 50 };

			ImVec2 left_size{ 310, 390 };
			ImVec2 left_padding{ 10, 10 };
			ImVec2 left_spacing{ 10, 10 };

			ImVec2 right_padding{ 0, 10 };
			ImVec2 right_spacing{ 0, 10 };

		} tab1;

		struct
		{
			ImVec2 left_size{ 82, 390 };
			ImVec2 left_padding{ 10, 10 };
			ImVec2 left_spacing{ 10, 10 };

			ImVec2 right_padding{ 0, 10 };
			ImVec2 right_spacing{ 0, 10 };

		} tab2;

	} content;

	struct
	{
		float rounding{ 1 };

		ImVec2 size{ 156, 39 };
		ImVec2 padding{ 5, 5 };

	} button_bordered;

	struct
	{
		float rounding{ 2 };
		float height{ 25 };

	} button;

	struct
	{
		ImVec2 size{ 117, 25 };
		ImVec2 window_size{ 255, 181 };
		ImVec2 padding{ 10, 10 };
	} user_button;

	struct
	{
		float height{ 21 };
		float button_height{ 5 };
	} slider;

	struct {

		float rounding{ 2 };
		float height{ 30 };

	} classic_button;

	struct {

		ImVec2 size{ 52, 15 };

	} enter_button;

	struct
	{

		ImVec2 size{ 328, 82 };
		ImVec2 departament_size{ 164, 41 };
		ImVec2 departament_padding{ 2, 2 };

	} info;

	struct {

		float height{ 45 };
		float rounding{ 4 };
		float image_rounding{ 4 };

		ImVec2 name_padding{ 45, 10 };
		ImVec2 project_padding{ 45, 24 };
		ImVec2 active_padding{ 161, 24 };
		ImVec2 line_padding{ 10, 14 };
		ImVec2 line_size{ 69, 2 };
		ImVec2 image_padding{ 10, 10 };
		ImVec2 image_size{ 25, 25 };

	} game_button;

	struct {

		float height{ 66 };
		float rounding{ 4 };
		float button_rounding{ 4 };
		float popup_rounding{ 4 };

		ImVec2 inner{ 20, 17 };
		ImVec2 button_size{ 35, 26 };

		ImVec2 popup_padding{ 10, 10 };
		ImVec2 popup_spacing{ 10, 10 };
		ImVec2 popup_size{ 250, 106 };

	} cheat_button;

	struct
	{
		float minimal_height{ 37 };
		float rounding{ 4 };
		
		ImVec2 content_padding{ 8, 6 };
		ImVec2 head_padding{ 8, 8 };

	} update_log;

	struct {
		float height{ 46 };
		float rounding{ 2 };
		float button_height{ 30 };
	} dropdown;

	struct {

		float height{ 30 };
		float rounding{ 2 };
		float padding{ 10 };

	} selectable; 
};

inline std::unique_ptr<c_elements> elements = std::make_unique<c_elements>();
