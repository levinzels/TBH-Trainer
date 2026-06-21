#pragma once
#include <string>
#include <vector>
#include "imgui.h"
#include "../headers/flags.h"
#include <Windows.h>

class c_variables
{
public:


	struct
	{
		float dpi = 1.f;
		int stored_dpi = 100;
		bool dpi_changed = true;

		bool autorization = false, login_window = false, inject_window = false;
		bool is_login = false, is_inject = false;

		bool disable_blackout_close = false;

		float blackout_alpha = 0.f, blackout_pos = 0.f;

		int tab = 0, tab_stored = 0;
		float tab_alpha = 1.f;

		std::string game_tab = "none", game_tab_stored = "none";
		float game_tab_alpha = 1.f;

		int lang = 0;

		int inject_type = 0;

	} gui;

	struct
	{
		HWND hwnd;
		RECT rc;
	} winapi;

	struct
	{
		std::string left{ "G" };
		std::string right{ "H" };
		std::string discord{ "A" };
		std::string door{ "B" };
		std::string arrow_line{ "C" };
		std::string user{ "D" };
		std::string key{ "E" };
		std::string arrow_filled{ "F" };
		std::string close{ "J" };
		std::string search{ "K" };
	} icon;

	struct 
	{
		ImTextureID decoration;
		ImTextureID logo;

		ImTextureID cs;
		ImTextureID pubg;
		ImTextureID apex;
		ImTextureID rdr;
		ImTextureID dbd;
		ImTextureID cod;
		ImTextureID marvel;
		ImTextureID valo;
		ImTextureID avatar;
	} texture;

	gui_style style;

};

inline std::unique_ptr<c_variables> var = std::make_unique<c_variables>();
