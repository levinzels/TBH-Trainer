#include "headers/includes.h"
#include <algorithm>

void move_window(HWND hwnd, RECT rc)
{
	if (var->gui.blackout_alpha < 0.01f) {

		bool hovered = IsMouseHoveringRect(GetWindowPos(), GetWindowPos() + ImVec2(GetWindowWidth(), SCALE(elements->titlebar.height)));
		static bool clicked = false;

		if (hovered && GetIO().MouseClicked[0])
			clicked = true;

		if (!GetIO().MouseDown[0])
			clicked = false;

		if (clicked)
		{
			GetWindowRect(hwnd, &rc);
			MoveWindow(hwnd, rc.left + GetMouseDragDelta().x, rc.top + GetMouseDragDelta().y, GetWindowWidth(), GetWindowHeight(), FALSE);
		}

		while (rc.right - rc.left != GetWindowWidth() || rc.bottom - rc.top != GetWindowHeight())
		{
			GetWindowRect(hwnd, &rc);
			MoveWindow(hwnd, rc.left, rc.top, GetWindowWidth(), GetWindowHeight(), FALSE);
		}

	}

}

void c_gui::render()
{
	gui->initialize();
	
	gui->easing(var->gui.blackout_alpha, var->gui.is_login || var->gui.is_inject ? 1.f : 0.f, 6.f, static_easing);
	gui->easing(var->gui.tab_alpha, var->gui.tab != var->gui.tab_stored ? 0.f : 1.f, 6.f, static_easing);
	gui->easing(var->gui.game_tab_alpha, var->gui.game_tab != var->gui.game_tab_stored ? 0.f : 1.f, 6.f, static_easing);

	if (var->gui.is_login) {
		var->gui.login_window = true;
	}

	if (var->gui.is_inject) {
		var->gui.disable_blackout_close = true;
		var->gui.inject_window = true;
	}

	if (var->gui.blackout_alpha > 0.01f) {
		gui->easing(var->gui.blackout_pos, 1.f, 6.f, dynamic_easing);
	}
	else {
		var->gui.blackout_pos = 10.f;
		var->gui.login_window = false;
		var->gui.inject_window = false;
		elements->inject_window.timer = 0.f;
		elements->inject_window.alpha = 0.f;
	}

	if (var->gui.tab_alpha < 0.01f) {
		var->gui.tab = var->gui.tab_stored;
	}
	if (var->gui.game_tab_alpha < 0.01f) {
		var->gui.game_tab = var->gui.game_tab_stored;
	}

	if (IsKeyPressed(ImGuiKey_F1)) {
		var->gui.stored_dpi += 10;
		var->gui.dpi_changed = true;
	}

	if (IsKeyPressed(ImGuiKey_F2)) {
		var->gui.stored_dpi -= 10;
		var->gui.dpi_changed = true;
	}

	gui->set_next_window_pos(ImVec2(0, 0));
	gui->set_next_window_size(SCALE(elements->window.size));
	gui->begin("menu", nullptr, elements->window.flags);
	{
		gui->set_style();
		gui->draw_decorations();

		window_flags no_scroll = window_flags_no_scrollbar | window_flags_no_scroll_with_mouse;

		gui->begin_content("titlebar", ImVec2(gui->content_avail().x, SCALE(elements->titlebar.height)), SCALE(elements->titlebar.padding), SCALE(elements->titlebar.spacing), no_scroll);
		{
			gui->begin_content("logo", ImVec2(SCALE(elements->titlebar.logo_width), gui->content_avail().y), ImVec2(0, 0), ImVec2(0, 0), no_scroll);
			{
				draw->image(gui->window_drawlist(), var->texture.logo, gui->window_pos() + SCALE(0, 19), gui->window_pos() + SCALE(25, 41), ImVec2(0, 0), ImVec2(1, 1), draw->get_clr(clr->white));
				draw->text_clipped(gui->window_drawlist(), font->get(saira, 20), gui->window_pos() + SCALE(45, 0), gui->window_pos() + gui->window_size(), draw->get_clr(clr->white), "MOUNTAIN", 0, 0, { 0, 0.35 });
				draw->text_clipped(gui->window_drawlist(), font->get(inter_semibold, 11), gui->window_pos() + SCALE(45, 0), gui->window_pos() + gui->window_size(), draw->get_clr(clr->text_inactive), "L A U N C H E R", 0, 0, { 0, 0.65 });
			}
			gui->end_content();

			gui->sameline();

			gui->begin_content("tabs", ImVec2(SCALE(elements->titlebar.tab_width), gui->content_avail().y), ImVec2(0, 0), SCALE(elements->titlebar.tab_spacing), no_scroll);
			{
				if (var->gui.lang == 1) {
					gui->dummy(SCALE(10, 10));
					gui->sameline();
				}

				widgets->tab_button(LANG("Главная", "General"), 0);
				gui->sameline();
				widgets->tab_button(LANG("Продукты", "Products"), 1, true);
				gui->sameline();
				widgets->tab_button(LANG("Обновления", "Updates"), 2, true);
				gui->sameline();
				widgets->tab_button(LANG("Контакты", "Contacts"), 3, true);
			}
			gui->end_content();

			gui->sameline();

			gui->begin_content("enter", ImVec2(SCALE(elements->titlebar.enter_width), gui->content_avail().y), ImVec2(0, 0), ImVec2(0, 0), no_scroll);
			{
				if (!var->gui.autorization)
				{
					gui->set_pos(SCALE(146, 22), pos_all);
					if (widgets->enter_button(LANG("Войти", "Log in"))) {
						var->gui.is_login = true;
					}
				}
				else
				{
					gui->set_pos(SCALE(82, 17), pos_all);
					widgets->user_button("Past Owl", var->texture.avatar);
				}
			}
			gui->end_content();
		}
		gui->end_content();

		gui->push_var(style_var_alpha, var->gui.tab_alpha);
		gui->begin_content("content", gui->content_avail(), ImVec2(0, 0), ImVec2(0, 0), no_scroll | window_flags_no_move);
		{
			if (var->gui.tab == 0) {

				gui->begin_content("content_fill", gui->content_avail(), SCALE(elements->content.tab0.padding), ImVec2(0, 0), no_scroll | window_flags_no_move);
				{
					gui->begin_content("content_main", SCALE(elements->content.tab0.size), ImVec2(0, 0), SCALE(elements->content.tab0.spacing), no_scroll);
					{
						draw->image(gui->window_drawlist(), var->texture.logo, gui->window_pos(), gui->window_pos() + SCALE(29, 25), ImVec2(0, 0), ImVec2(1, 1), draw->get_clr(clr->white));
						draw->text_clipped(gui->window_drawlist(), font->get(saira, 23), gui->window_pos() + SCALE(55, -5), gui->window_pos() + SCALE(140, 25), draw->get_clr(clr->white), "MOUNTAIN", 0, 0, { 0, 0 });
						draw->text_clipped(gui->window_drawlist(), font->get(inter_semibold, 12), gui->window_pos() + SCALE(55, 0), gui->window_pos() + SCALE(140, 27), draw->get_clr(clr->text_inactive), "L A U N C H E R", 0, 0, { 0, 1 });

						std::string description_text = LANG("Мы - команда разработчиков, специализирующихся на создании\nуникальных игровых модификаций на платформе RAGE\nMultiplayer. Эта платформа позволяет нам упрощать игровые\nмеханики и добавлять новые элементы, что делает игру более\nувлекательной и интересной для игроков.", "We are a team of developers specializing in creating\nunique game modifications on the RAGE Multiplayer\nplatform. This platform allows us to simplify game\nmechanics and add new elements, which makes the game more\nengaging and interesting for players.");

						draw->text_clipped(gui->window_drawlist(), font->get(inter_medium, 12), gui->window_pos() + SCALE(0, elements->content.tab0.desc_spacing), gui->window_pos() + gui->window_size(), draw->get_clr(clr->text_inactive), description_text.data(), gui->text_end(description_text.data()), 0, { 0, 0 });

						gui->dummy(SCALE(0, 105));

						if (widgets->button_bordered(LANG("Перейти к товарам", "Go to products"))) {
							if (!var->gui.autorization) {
								var->gui.is_login = true;
							}
							else {
								var->gui.tab_stored = 1;
							}
						}
						gui->sameline();
						if (widgets->button_bordered(LANG("4kn1", "4kn1"))) {
							ImGui::SetClipboardText("4kn1");
						}

						widgets->info(LANG("6300 Активных", "6300 Active users"), LANG("Более 12.300 продаж", "More than 12.300 sales"), LANG("4 года на рынке", "4 years on the market"), LANG("Скорость и качество", "Speed and quality"));

					}
					gui->end_content();
				}
				gui->end_content();

			} 
			else if (var->gui.tab == 1)
			{
				static float p = 1.f;
				gui->easing(p, 1.f, 2.f, static_easing);

				gui->begin_content("content_fill", gui->content_avail(), ImVec2(0, 0), ImVec2(0, 0), no_scroll | window_flags_no_move);
				{
					gui->begin_content("left", SCALE(elements->content.tab1.left_size), ImVec2(0, 0), ImVec2(0, 0), no_scroll | window_flags_no_move);
					{
						static char search_buf[64];
						gui->begin_content("header", ImVec2(gui->content_avail().x, SCALE(elements->content.tab1.left_header_height) + gui->content_avail().y * (1.f - p)), SCALE(elements->content.tab1.left_padding), SCALE(elements->content.tab1.left_spacing), no_scroll);
						{
							widgets->games_search_field(LANG("Поиск игр", "Search games"), var->icon.search, search_buf, sizeof(search_buf));
						}
						gui->end_content();

						gui->push_var(style_var_alpha, p* var->style.alpha);

						gui->begin_content("body", gui->content_avail(), SCALE(elements->content.tab1.left_padding), SCALE(elements->content.tab1.left_spacing), window_flags_no_move);
						{
							std::vector<games_t> games{
								{"Counter strike: Global offensive", 10, var->texture.cs},
								{"PUBG: Battlegrounds", 10, var->texture.pubg},
								{"Apex Legends", 10, var->texture.apex},
								{"Red Dead Redemption II", 10, var->texture.rdr},
								{"Dead by daylight", 10, var->texture.dbd},
								{"Call of Duty: WARZONE", 10, var->texture.cod},
								{"Valorant", 10, var->texture.valo},
								{"Marvel", 10, var->texture.marvel}
							};

							static std::string stored_search_str;

							if (strlen(search_buf) != 0) {
								std::string search_str = search_buf;

								if (search_str != stored_search_str)
								{
									p = 0.f;
									stored_search_str = search_str;
								}

								std::transform(search_str.begin(), search_str.end(), search_str.begin(), ::tolower);
								for (auto& game : games) {
									std::string name = game.name;
									std::transform(name.begin(), name.end(), name.begin(), ::tolower);
									if (name.find(search_str) != std::string::npos)
										widgets->game_button(game.name, game.active, game.image);
								}
							}
							else {
								for (auto& game : games) {
									widgets->game_button(game.name, game.active, game.image);
								}
							}
						}
						gui->end_content();

						gui->pop_var();
					}
					gui->end_content();

					gui->sameline();

					gui->push_var(style_var_alpha, var->gui.game_tab_alpha * var->gui.tab_alpha);
					gui->push_var(style_var_scrollbar_content_padding, SCALE(10));

					gui->begin_content("right", gui->content_avail(), SCALE(elements->content.tab1.right_padding), SCALE(elements->content.tab1.right_spacing));
					{
						widgets->cheat_button("Lunar.Shift", "01.01.2025", LANG("Вчера", "Yesterday"));
						widgets->cheat_button("Winner.Software", "15.10.2020", LANG("Никогда", "Never"));
						widgets->cheat_button("Internal.Codes", "10.05.2023", LANG("2 дня назад", "Two days ago"));
						widgets->cheat_button("Etherial.Base", "01.01.2025", LANG("Сегодня", "Today"));
						widgets->cheat_button("Oracle.Engine", "01.01.2025", LANG("Вчера", "Yesterday"));
						widgets->cheat_button("Solar.Nexus", "15.10.2020", LANG("Никогда", "Never"));
						widgets->cheat_button("Titan.Forge", "10.05.2023", LANG("2 дня назад", "Two days ago"));
						widgets->cheat_button("Omega.Point", "01.01.2025", LANG("Сегодня", "Today"));
						widgets->cheat_button("Alpha.Core", "01.01.2025", LANG("Вчера", "Yesterday"));
						widgets->cheat_button("Silent.Protocol", "15.10.2020", LANG("Никогда", "Never"));
						widgets->cheat_button("Binary.Stream", "10.05.2023", LANG("2 дня назад", "Two days ago"));
						widgets->cheat_button("Quantum.Leap", "01.01.2025", LANG("Сегодня", "Today"));
					}
					gui->end_content();

					gui->pop_var(2);
				}
				gui->end_content();
			}
			else if (var->gui.tab == 2)
			{
				gui->begin_content("left", SCALE(elements->content.tab2.left_size), SCALE(elements->content.tab2.left_padding), SCALE(elements->content.tab2.left_spacing), window_flags_no_move);
				{
					widgets->game_button("Counter strike: Global offensive", 10, var->texture.cs, true);
					widgets->game_button("PUBG: Battlegrounds", 10, var->texture.pubg, true);
					widgets->game_button("Apex Legends", 10, var->texture.apex, true);
					widgets->game_button("Red Dead Redemption II", 10, var->texture.rdr, true);
					widgets->game_button("Dead by daylight", 10, var->texture.dbd, true);
					widgets->game_button("Call of Duty: WARZONE", 10, var->texture.cod, true);
					widgets->game_button("Valorant", 10, var->texture.valo, true);
					widgets->game_button("Marvel", 10, var->texture.marvel, true);
				}
				gui->end_content();

				gui->sameline();

				gui->push_var(style_var_alpha, var->gui.game_tab_alpha* var->gui.tab_alpha);
				gui->push_var(style_var_scrollbar_content_padding, SCALE(10));

				gui->begin_content("right", gui->content_avail(), SCALE(elements->content.tab2.right_padding), SCALE(elements->content.tab2.right_spacing), window_flags_no_move);
				{
					widgets->update_log("Counter strike: Global offensive", "Lunar.Shift", "01.06.2024-01.01.2025", LANG("Полностью безопасен", "Completely safe"), "01.09.2025", "0.24.150", {{"Innovations in aimbot", "Aim assist functionality has been added to our software project for the game Counter-Strike: Global Offensive. This feature significantly simplifies aiming, helping users to more\naccurately target their crosshair on opponents. Aim assist makes the gameplay more comfortable and intuitive, providing players with an edge in situations requiring quick and\nprecise reactions."}, {"Improvements in wallhack", "Additionally, the project has been enhanced with improved ESP features. Now, information about player positions, health, and weapons is displayed with high accuracy and in\nreal-time. These enhancements allow users to have a comprehensive understanding of the situation on the map, improving their strategic decisions and boosting the\neffectiveness of their gameplay."}, {"Innovations in Miscellaneous", "In Miscellaneous, new features enhance gameplay. Bunnyhop automates jumping for faster movement. Strafe allows quick sideways moves to dodge attacks. Thunder adds\nstorm effects for realism. These updates enrich the gaming experience."}});
					widgets->update_log("Counter strike: Global offensive", "Winner.Software", "01.02.2024-01.01.2025", LANG("Активен", "Active"), "01.05.2024", "0.10.65", {{"Updates to the spoofer", "The new Spoofer tool successfully bypasses the security measures of both BattleEye and OneGuard by altering the hardware identifiers of the device, making it significantly\nhard-er for anti-cheat systems to detect unauthorized modifications or exploits."}});
					widgets->update_log("Counter strike: Global offensive", "Internal.Codes", "01.02.2024-01.01.2025", LANG("В Обновлении", "On update"), "01.10.2024", "0.0.45", {{"What's new in Sapphire hack", "The new Spoofer tool successfully bypasses the security measures of both BattleEye and OneGuard by altering the hardware identifiers of the device, making it significantly\nhard-er for anti-cheat systems to detect unauthorized modifications or exploits."}});
					widgets->update_log("Counter strike: Global offensive", "Etherial.Base", "01.06.2024-01.01.2025", LANG("Полностью безопасен", "Completely safe"), "01.09.2025", "0.24.150", { {"Innovations in aimbot", "Aim assist functionality has been added to our software project for the game Counter-Strike: Global Offensive. This feature significantly simplifies aiming, helping users to more\naccurately target their crosshair on opponents. Aim assist makes the gameplay more comfortable and intuitive, providing players with an edge in situations requiring quick and\nprecise reactions."}, {"Improvements in wallhack", "Additionally, the project has been enhanced with improved ESP features. Now, information about player positions, health, and weapons is displayed with high accuracy and in\nreal-time. These enhancements allow users to have a comprehensive understanding of the situation on the map, improving their strategic decisions and boosting the\neffectiveness of their gameplay."}, {"Innovations in Miscellaneous", "In Miscellaneous, new features enhance gameplay. Bunnyhop automates jumping for faster movement. Strafe allows quick sideways moves to dodge attacks. Thunder adds\nstorm effects for realism. These updates enrich the gaming experience."} });
					widgets->update_log("Counter strike: Global offensive", "Oracle.Engine", "01.02.2024-01.01.2025", LANG("Активен", "Active"), "01.05.2024", "0.10.65", { {"Updates to the spoofer", "The new Spoofer tool successfully bypasses the security measures of both BattleEye and OneGuard by altering the hardware identifiers of the device, making it significantly\nhard-er for anti-cheat systems to detect unauthorized modifications or exploits."} });
					widgets->update_log("Counter strike: Global offensive", "Solar.Nexus", "01.02.2024-01.01.2025", LANG("В Обновлении", "On update"), "01.10.2024", "0.0.45", { {"What's new in Sapphire hack", "The new Spoofer tool successfully bypasses the security measures of both BattleEye and OneGuard by altering the hardware identifiers of the device, making it significantly\nhard-er for anti-cheat systems to detect unauthorized modifications or exploits."} });
				}
				gui->end_content();

				gui->pop_var(2);
			}
		}
		gui->end_content();
		gui->pop_var();

		if (var->gui.blackout_alpha > 0.01f) 
		{
			gui->push_var(style_var_alpha, var->gui.blackout_alpha);
			gui->set_pos(SCALE(0, 0), pos_all);

			gui->begin_content("blackout", SCALE(elements->window.size), ImVec2(0, 0), ImVec2(0, 0), no_scroll | window_flags_no_move);
			{
				if (IsWindowHovered() && gui->mouse_clicked(0) && !var->gui.disable_blackout_close) 
				{
					var->gui.is_login = false;
					var->gui.is_inject = false;
				}

				draw->rect_filled(gui->window_drawlist(), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->layout, 0.8f), SCALE(elements->window.rounding));

				if (var->gui.login_window) 
				{
					gui->set_pos(SCALE(250, 127 * var->gui.blackout_pos), pos_all);
					gui->begin_content("login_window", SCALE(elements->login_window.size), ImVec2(0, 0), ImVec2(0, 0), no_scroll);
					{
						draw->rect_filled(gui->window_drawlist(), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->layout), SCALE(elements->login_window.rounding));
						draw->rect(gui->window_drawlist(), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->login_border), SCALE(elements->login_window.rounding));

						gui->begin_content("login_header", ImVec2(gui->content_avail().x, SCALE(elements->login_window.header_height)), SCALE(elements->login_window.header_padding), ImVec2(0, 0), no_scroll);
						{
							ImVec2 pos = gui->get_window()->DC.CursorPos;
							draw->image(gui->window_drawlist(), var->texture.logo, pos, pos + SCALE(14, 12), ImVec2(0, 0), ImVec2(1, 1), draw->get_clr(clr->white));
							draw->text_clipped(gui->window_drawlist(), font->get(saira, 17), pos + SCALE(24, -3), pos + gui->content_avail(), draw->get_clr(clr->white), "MOUNTAIN", 0, 0, { 0, 0 });
						}
						gui->end_content();

						gui->begin_content("login_body", ImVec2(gui->content_avail().x, SCALE(elements->login_window.body_height)), SCALE(elements->login_window.body_padding), SCALE(elements->login_window.body_spacing), no_scroll);
						{
							static char buf1[64];
							static char buf2[64];
							widgets->text_field(LANG("Логин", "Name"), var->icon.user, buf1, sizeof(buf1));
							widgets->text_field(LANG("Пароль", "Password"), var->icon.key, buf2, sizeof(buf2));
							if (widgets->button(LANG("Авторизоваться", "Log in")))
							{
								var->gui.autorization = true;
								var->gui.is_login = false;
							}
						}
						gui->end_content();

						gui->begin_content("login_end", gui->content_avail(), SCALE(elements->login_window.end_padding), ImVec2(0, 0), no_scroll);
						{
							gui->set_pos(SCALE(20), pos_y);
							widgets->text_button_colored(LANG("Не имеете аккаунта?", "Don't have an account?"), LANG("Регистрация", "Registration"));

							gui->set_pos(SCALE(var->gui.lang == 0 ? 180 : 200, 20), pos_all);
							widgets->text_button_colored(LANG("Забыли пароль?", "Forgot password?"), LANG("Восстановить", "Reset"));
						}
						gui->end_content();
					}
					gui->end_content();
				}
				else if (var->gui.inject_window) 
				{
					gui->set_pos(SCALE(250, 189 * var->gui.blackout_pos - SCALE(15 * elements->inject_window.alpha)), pos_all);
					gui->begin_content("inject_window", SCALE(elements->inject_window.size) + SCALE(0, 30 * elements->inject_window.alpha), ImVec2(0, 0), ImVec2(0, 0), no_scroll);
					{
						draw->rect_filled(gui->window_drawlist(), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->layout), SCALE(elements->inject_window.rounding));
						draw->rect(gui->window_drawlist(), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->login_border), SCALE(elements->inject_window.rounding));

						ImVec2 pos = gui->get_window()->DC.CursorPos;
						ImVec2 line_pos = SCALE(15, 52 - 15 * elements->inject_window.alpha);
						ImVec2 line_size = SCALE(280, 5);
						ImRect line_rect(pos + line_pos, pos + line_pos + line_size);

						elements->inject_window.timer += fixed_speed(0.25f);

						if (elements->inject_window.timer > 1.f) {
							elements->inject_window.timer = 1.f;
							gui->easing(elements->inject_window.alpha, 1.f, 7.f, dynamic_easing);
							var->gui.disable_blackout_close = false;
						}
						else {
							draw->text_clipped(gui->window_drawlist(), font->get(inter_medium, 12), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->white), "Counter strike: Global offensive", 0, 0, {0.5, 0.2});
							draw->text_clipped(gui->window_drawlist(), font->get(inter_medium, 12), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->text_inactive), elements->inject_window.injection_cheat_name.data(), 0, 0, {0.5, 0.45});

						}

						gui->push_var(style_var_alpha, elements->inject_window.alpha * var->gui.blackout_alpha);
						{
							draw->text_clipped(gui->window_drawlist(), font->get(inter_medium, 12), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->white), LANG("Успешная сессия!", "Successful session!").data(), 0, 0, { 0.5, 0.15 });

							gui->set_pos(SCALE(15, 57), pos_all);
							if (widgets->button(LANG("Вернуться назад", "Go back"), SCALE(132, 30))) {
								var->gui.is_inject = false;
							}
							gui->set_pos(SCALE(162, 57), pos_all);
							if (widgets->classic_button(LANG("Закрыть лаунчер", "Close the launcher"), SCALE(132, 30)))
								exit(-1);
						}
						gui->pop_var();

						draw->rect_filled(gui->window_drawlist(), line_rect.Min, line_rect.Max, draw->get_clr(clr->line_layout), SCALE(100));
						draw->rect_filled(gui->window_drawlist(), line_rect.Min, line_rect.Min + ImVec2(line_rect.GetWidth() * elements->inject_window.timer, line_rect.GetHeight()), draw->get_clr(clr->accent), SCALE(100));

					}
					gui->end_content();
				}
			}
			gui->end_content();

			gui->pop_var();

		}

		move_window(var->winapi.hwnd, var->winapi.rc);

	}
	gui->end();
}