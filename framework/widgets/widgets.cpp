#include "../headers/includes.h"
#include "../headers/widgets.h"

bool c_widgets::tab_button(std::string name, int tab, bool autorization_check) {

	struct animstate_t {
		float alpha;
		ImVec4 text;
	};

	ImGuiWindow* window = gui->get_window();
	ImGuiID id = window->GetID(name.data());
	animstate_t* animstate = gui->anim_container(&animstate, id);

	ImVec2 text_size = gui->text_size(font->get(inter_semibold, 11), name.data());

	ImVec2 pos(window->DC.CursorPos);
	ImVec2 size(text_size.x, gui->content_avail().y);
	ImRect rect(pos, pos + size);

	gui->item_size(rect);
	gui->item_add(rect, id);

	bool selected = var->gui.tab_stored == tab;
	bool pressed = gui->button_behavior(rect, id, nullptr, nullptr);

	if (pressed) {
		if (autorization_check) {
			if (!var->gui.autorization) {
				var->gui.is_login = true;
			}
			else {
				var->gui.tab_stored = tab;
			}
		}
		else {
			var->gui.tab_stored = tab;
		}
	}

	gui->easing(animstate->text, selected ? clr->white.Value : clr->text_inactive.Value, 8.f, dynamic_easing);
	gui->easing(animstate->alpha, selected ? 1.f : 0.f, 5.f, static_easing);

	draw->rect_filled_multi_color(window->DrawList, ImVec2(rect.Min.x, rect.Max.y - (rect.GetHeight() / 2) * animstate->alpha), rect.Max, draw->get_clr(clr->tab_button_top, 0.f), draw->get_clr(clr->tab_button_top, 0.f), draw->get_clr(clr->tab_button_bot, 0.05f), draw->get_clr(clr->tab_button_bot, 0.05f));
	draw->rect_filled(window->DrawList, rect.GetBL() - SCALE(0, 1), rect.Max, draw->get_clr(clr->accent, animstate->alpha));

	draw->text_clipped(window->DrawList, font->get(inter_semibold, 11), rect.Min, rect.Max, draw->get_clr(animstate->text), name.data(), gui->text_end(name.data()), 0, { 0.5, 0.5 });

	return pressed;
}

void draw_corner_border(ImDrawList* drawlist, ImRect rect, ImVec2 size, ImU32 color, float rounding, std::vector<bool> corners = { true, true, true, true}) {

	if (corners[0]) {
		draw->push_clip_rect(drawlist, rect.Min, rect.Min + size, true);
		draw->rect(drawlist, rect.Min, rect.Max, color, rounding);
		draw->pop_clip_rect(drawlist);
	}

	if (corners[1]) {
		draw->push_clip_rect(drawlist, rect.Max - size, rect.Max, true);
		draw->rect(drawlist, rect.Min, rect.Max, color, rounding);
		draw->pop_clip_rect(drawlist);
	}

	if (corners[2]) {
		draw->push_clip_rect(drawlist, rect.GetBL() - ImVec2(0, size.y), rect.GetBL() + ImVec2(size.x, 0), true);
		draw->rect(drawlist, rect.Min, rect.Max, color, rounding);
		draw->pop_clip_rect(drawlist);
	}

	if (corners[3]) {
		draw->push_clip_rect(drawlist, rect.GetTR() - ImVec2(size.x, 0), rect.GetTR() + ImVec2(0, size.y), true);
		draw->rect(drawlist, rect.Min, rect.Max, color, rounding);
		draw->pop_clip_rect(drawlist);
	}

}

bool c_widgets::button_bordered(std::string name) {

	struct state_t
	{
		ImVec4 col;
	};

	ImGuiWindow* window = gui->get_window();
	ImGuiID id = window->GetID(name.data());
	state_t* state = gui->anim_container(&state, id);

	ImVec2 pos(window->DC.CursorPos);
	ImVec2 size(SCALE(elements->button_bordered.size));
	ImRect total(pos, pos + size);
	ImRect rect(total.Min + SCALE(elements->button_bordered.padding), total.Max - SCALE(elements->button_bordered.padding));

	gui->item_size(total);
	gui->item_add(total, id);

	bool pressed = gui->button_behavior(rect, id, nullptr, nullptr);

	gui->easing(state->col, total.Contains(GetMousePos()) ? clr->white.Value : clr->border.Value, 16.f, dynamic_easing);

	draw_corner_border(window->DrawList, total, SCALE(50, 11), draw->get_clr(state->col), SCALE(elements->button_bordered.rounding));
	draw->rect_filled(window->DrawList, rect.Min, rect.Max, draw->get_clr(clr->accent), SCALE(elements->button_bordered.rounding));
	draw->text_clipped(window->DrawList, font->get(inter_medium, 10), rect.Min, rect.Max, draw->get_clr(clr->black), name.data(), gui->text_end(name.data()), 0, { 0.5, 0.5 });

	return pressed;
}

bool c_widgets::button(std::string name, ImVec2 с_size) {

	ImGuiWindow* window = gui->get_window();
	ImGuiID id = window->GetID(name.data());

	ImVec2 pos(window->DC.CursorPos);
	ImVec2 size = с_size.x > 0 ? с_size : ImVec2(gui->content_avail().x, SCALE(elements->button.height));
	ImRect rect(pos, pos + size);

	gui->item_size(rect);
	gui->item_add(rect, id);

	bool pressed = gui->button_behavior(rect, id, nullptr, nullptr);

	draw->rect_filled(window->DrawList, rect.Min, rect.Max, draw->get_clr(clr->accent), SCALE(elements->button.rounding));
	draw->text_clipped(window->DrawList, font->get(inter_medium, 10), rect.Min, rect.Max, draw->get_clr(clr->black), name.data(), gui->text_end(name.data()), 0, { 0.5, 0.5 });

	return pressed;
}

bool c_widgets::text_button_colored(std::string default_text, std::string colored_text) {

	struct animstate_t {
		ImVec4 text;
	};

	ImGuiWindow* window = gui->get_window();
	ImGuiID id = window->GetID(default_text.data());
	animstate_t* animstate = gui->anim_container(&animstate, id);

	ImVec2 default_text_size = gui->text_size(font->get(inter_medium, 9), default_text.data());
	ImVec2 colored_text_size = gui->text_size(font->get(inter_medium, 9), colored_text.data());

	ImVec2 pos(window->DC.CursorPos);
	ImVec2 size(default_text_size.x + colored_text_size.x + SCALE(2), default_text_size.y);
	ImRect rect(pos, pos + size);

	gui->item_size(rect);
	gui->item_add(rect, id);

	bool hovered, pressed = gui->button_behavior(rect, id, &hovered, nullptr);

	gui->easing(animstate->text, hovered ? clr->white.Value : clr->accent.Value, 6.f, dynamic_easing);

	draw->text_clipped(window->DrawList, font->get(inter_medium, 9), rect.Min, rect.Max, draw->get_clr(clr->text_inactive), default_text.data(), gui->text_end(default_text.data()), 0, { 0, 0.5 });
	draw->text_clipped(window->DrawList, font->get(inter_medium, 9), rect.Min, rect.Max, draw->get_clr(animstate->text), colored_text.data(), gui->text_end(colored_text.data()), 0, { 1, 0.5 });

	return pressed;
}

bool c_widgets::enter_button(std::string name) {

	ImGuiWindow* window = gui->get_window();
	ImGuiID id = window->GetID(name.data());

	ImVec2 pos(window->DC.CursorPos);
	ImVec2 size(SCALE(elements->enter_button.size));
	ImRect rect(pos, pos + size);

	gui->item_size(rect);
	gui->item_add(rect, id);

	bool pressed = gui->button_behavior(rect, id, nullptr, nullptr);

	draw->text_clipped(window->DrawList, font->get(inter_semibold, 11), rect.Min, rect.Max, draw->get_clr(clr->white), name.data(), gui->text_end(name.data()), 0, { 0, 0.5 });
	draw->text_clipped(window->DrawList, font->get(icons, 12), rect.Min, rect.Max, draw->get_clr(clr->accent), var->icon.door.data(), 0, 0, {1, 0.5});

	return pressed;
}

void c_widgets::info(std::string users, std::string sells, std::string time, std::string features) {

	ImGuiWindow* window = gui->get_window();

	ImVec2 pos(window->DC.CursorPos);
	ImVec2 size(SCALE(elements->info.size));
	ImRect total(pos, pos + size);
	ImVec2 padding(SCALE(elements->info.departament_padding));

	gui->item_size(total);
	gui->item_add(total, 0);

	ImRect rect(pos, pos + SCALE(elements->info.departament_size) - padding);
	{
		draw_corner_border(window->DrawList, rect, SCALE(100, 20), draw->get_clr(clr->border), 0, {true, true, false, false});
		draw->text_clipped(window->DrawList, font->get(inter_medium, 9), rect.Min + SCALE(20, 0), rect.Max, draw->get_clr(clr->text_inactive), LANG("Наши пользователи", "Our users").data(), 0, 0, {0, 0.35});
		draw->text_clipped(window->DrawList, font->get(inter_medium, 9), rect.Min + SCALE(20, 0), rect.Max, draw->get_clr(clr->white), users.data(), gui->text_end(users.data()), 0, {0, 0.65});
		draw->text_clipped(window->DrawList, font->get(icons, 3), rect.Min, rect.Max - SCALE(20, 0), draw->get_clr(clr->white), var->icon.right.data(), 0, 0, {1, 0.5});
	}

	rect = ImRect(pos + SCALE(elements->info.departament_size.x, 0) + ImVec2(padding.x, 0), pos + SCALE(elements->info.departament_size.x, 0) + SCALE(elements->info.departament_size) - ImVec2(0, padding.y));
	{
		draw_corner_border(window->DrawList, rect, SCALE(100, 20), draw->get_clr(clr->border), 0, { false, false, true, true });
		draw->text_clipped(window->DrawList, font->get(inter_medium, 9), rect.Min, rect.Max - SCALE(20, 0), draw->get_clr(clr->text_inactive), LANG("Продаж за всё время", "Sales for all time").data(), 0, 0, {1, 0.35});
		draw->text_clipped(window->DrawList, font->get(inter_medium, 9), rect.Min, rect.Max - SCALE(20, 0), draw->get_clr(clr->white), sells.data(), gui->text_end(sells.data()), 0, { 1, 0.65 });
		draw->text_clipped(window->DrawList, font->get(icons, 3), rect.Min + SCALE(20, 0), rect.Max, draw->get_clr(clr->white), var->icon.left.data(), 0, 0, { 0, 0.5 });
	}

	rect = ImRect(pos + SCALE(0, elements->info.departament_size.y) + ImVec2(0, padding.y), pos + SCALE(0, elements->info.departament_size.y) + SCALE(elements->info.departament_size) - ImVec2(padding.x, 0));
	{
		draw_corner_border(window->DrawList, rect, SCALE(100, 20), draw->get_clr(clr->border), 0, { false, false, true, true });
		draw->text_clipped(window->DrawList, font->get(inter_medium, 9), rect.Min + SCALE(20, 0), rect.Max, draw->get_clr(clr->text_inactive), LANG("Гарантии", "Guarantees").data(), 0, 0, {0, 0.35});
		draw->text_clipped(window->DrawList, font->get(inter_medium, 9), rect.Min + SCALE(20, 0), rect.Max, draw->get_clr(clr->white), time.data(), gui->text_end(time.data()), 0, { 0, 0.65 });
		draw->text_clipped(window->DrawList, font->get(icons, 3), rect.Min, rect.Max - SCALE(20, 0), draw->get_clr(clr->white), var->icon.right.data(), 0, 0, { 1, 0.5 });
	}

	rect = ImRect(pos + SCALE(elements->info.departament_size) + padding, pos + SCALE(elements->info.departament_size) * 2);
	{
		draw_corner_border(window->DrawList, rect, SCALE(100, 20), draw->get_clr(clr->border), 0, { true, true, false, false });
		draw->text_clipped(window->DrawList, font->get(inter_medium, 9), rect.Min, rect.Max - SCALE(20, 0), draw->get_clr(clr->text_inactive), LANG("Почему мы?", "Why us?").data(), 0, 0, {1, 0.35});
		draw->text_clipped(window->DrawList, font->get(inter_medium, 9), rect.Min, rect.Max - SCALE(20, 0), draw->get_clr(clr->white), features.data(), gui->text_end(features.data()), 0, { 1, 0.65 });
		draw->text_clipped(window->DrawList, font->get(icons, 3), rect.Min + SCALE(20, 0), rect.Max, draw->get_clr(clr->white), var->icon.left.data(), 0, 0, { 0, 0.5 });
	}
}

bool c_widgets::game_button(std::string name, int active_products, ImTextureID image, bool sssmall) {

	struct animstate_t {
		float alpha;
	};

	ImGuiWindow* window = gui->get_window();
	ImGuiID id = window->GetID(name.data());
	animstate_t* animstate = gui->anim_container(&animstate, id);

	ImVec2 pos(window->DC.CursorPos);
	ImVec2 size(gui->content_avail().x, SCALE(elements->game_button.height));
	ImRect rect(pos, pos + size);

	gui->item_size(rect);
	gui->item_add(rect, id);

	bool pressed = gui->button_behavior(rect, id, nullptr, nullptr);

	if (pressed) {
		var->gui.game_tab_stored = name;
	}

	gui->easing(animstate->alpha, var->gui.game_tab_stored == name ? 1.f : 0.f, 6.f, static_easing);

	draw->rect_filled(window->DrawList, rect.Min, rect.Max, draw->get_clr(clr->widget_layout), SCALE(elements->game_button.rounding));
	
	if (image == nullptr) {
		draw->rect_filled(window->DrawList, rect.Min + SCALE(elements->game_button.image_padding), rect.Min + SCALE(elements->game_button.image_padding + elements->game_button.image_size), draw->get_clr(clr->white, 0.1f), SCALE(elements->game_button.image_rounding));
	}
	else {
		draw->image_rounded(window->DrawList, image, rect.Min + SCALE(elements->game_button.image_padding), rect.Min + SCALE(elements->game_button.image_padding + elements->game_button.image_size), ImVec2(0, 0), ImVec2(1, 1), draw->get_clr(clr->white), SCALE(elements->game_button.image_rounding));
	}

	if (!sssmall) {

		draw->text_clipped(window->DrawList, font->get(inter_medium, 12), rect.Min + SCALE(elements->game_button.name_padding), rect.Max, draw->get_clr(clr->white), name.data(), 0, 0, { 0, 0 });
		std::string text = std::to_string(active_products) + LANG(" проектов", " projects");
		draw->text_clipped(window->DrawList, font->get(inter_medium, 11), rect.Min + SCALE(elements->game_button.project_padding), rect.Max, draw->get_clr(clr->text_inactive), text.data(), 0, 0, { 0, 0 });
		text = std::to_string(active_products) + "/" + std::to_string(20);
		draw->text_clipped(window->DrawList, font->get(inter_medium, 11), rect.Min + SCALE(elements->game_button.active_padding), rect.Max - SCALE(elements->game_button.line_padding.x * 1.5f + elements->game_button.line_size.x, 0), draw->get_clr(clr->text_inactive), text.data(), 0, 0, { 1, 0 });

		ImRect line(rect.Max - SCALE(elements->game_button.line_padding) - SCALE(elements->game_button.line_size), rect.Max - SCALE(elements->game_button.line_padding));

		const float normalized = (line.GetWidth() / float(20)) * active_products;

		draw->rect_filled(window->DrawList, line.Min, line.Max, draw->get_clr(clr->line_layout), SCALE(100));
		if (active_products > 0)
			draw->rect_filled(window->DrawList, line.Min, line.Min + ImVec2(normalized, line.GetHeight()), draw->get_clr(clr->accent), SCALE(100));
	}

	if (animstate->alpha > 0.01f) 
		draw->rect_filled(window->DrawList, ImVec2(rect.Min.x, rect.GetCenter().y - SCALE(7 * animstate->alpha)), ImVec2(rect.Min.x + SCALE(2), rect.GetCenter().y + SCALE(7 * animstate->alpha)), draw->get_clr(clr->accent, animstate->alpha), SCALE(100));
	
	return pressed;
}

bool c_widgets::cheat_button(std::string name, std::string last_update, std::string last_launch) {

	struct animstate_t {
		bool open;
		float alpha;
		int callback{ 0 };
		ImVec4 layout, button, text;
	};

	ImGuiWindow* window = gui->get_window();
	ImGuiID id = window->GetID(name.data());
	animstate_t* animstate = gui->anim_container(&animstate, id);

	ImVec2 pos(window->DC.CursorPos);
	ImVec2 size(window->ScrollbarY ? gui->content_avail().x : gui->content_avail().x - SCALE(10), SCALE(elements->cheat_button.height));
	ImRect rect(pos, pos + size);

	gui->item_size(rect);
	gui->item_add(rect, id);

	bool hovered, pressed = gui->button_behavior(rect, id, &hovered, nullptr);

	gui->easing(animstate->layout, hovered || animstate->open ? clr->dark_widget.Value : clr->widget_layout.Value, 10.f, dynamic_easing);
	gui->easing(animstate->button, hovered || animstate->open ? clr->accent.Value : clr->dark_widget.Value, 10.f, dynamic_easing);
	gui->easing(animstate->text, hovered || animstate->open ? clr->black.Value : clr->white.Value, 10.f, dynamic_easing);

	draw->rect_filled(window->DrawList, rect.Min, rect.Max, draw->get_clr(animstate->layout), SCALE(elements->cheat_button.rounding));

	ImRect text(rect.Min + SCALE(elements->cheat_button.inner), rect.Max - SCALE(elements->cheat_button.inner));
	
	std::string last_update_text = LANG("Последнее обновление: ", "Last update: ") + last_update;
	std::string last_launch_text = LANG("Последний запуск: ", "Last launch: ") + last_launch;
	ImVec2 last_update_text_size = gui->text_size(font->get(inter_medium, 12), last_update_text.data());

	draw->text_clipped(window->DrawList, font->get(inter_medium, 12), text.Min, text.Max, draw->get_clr(clr->white), name.data(), 0, 0, { 0, 0 });
	draw->text_clipped(window->DrawList, font->get(inter_medium, 12), text.Min, text.Max, draw->get_clr(clr->text_inactive), last_update_text.data(), 0, 0, { 0, 1 });	
	draw->text_clipped(window->DrawList, font->get(inter_medium, 12), text.Min + ImVec2(last_update_text_size.x + SCALE(10), 0), text.Max, draw->get_clr(clr->text_inactive), last_launch_text.data(), 0, 0, {0, 1});

	ImRect button(text.Max - SCALE(elements->cheat_button.button_size) - ImVec2(gui->text_size(font->get(inter_semibold, 10), LANG("Начать", "Launch").data()).x, 0) - SCALE(0, 3), text.Max - SCALE(0, 3));

	draw->rect_filled(window->DrawList, button.Min, button.Max, draw->get_clr(animstate->button), SCALE(elements->cheat_button.button_rounding));

	draw->text_clipped(window->DrawList, font->get(inter_semibold, 10), button.Min + SCALE(10, 0), button.Max, draw->get_clr(animstate->text), LANG("Начать", "Launch").data(), 0, 0, {0, 0.5});
	draw->text_clipped(window->DrawList, font->get(icons, 12), button.Min, button.Max - SCALE(7, 0), draw->get_clr(animstate->text), var->icon.arrow_line.data(), 0, 0, {1, 0.5});

	if (pressed && !animstate->open && animstate->alpha < 0.01f) {
		animstate->open = !animstate->open;
	}

	gui->easing(animstate->alpha, animstate->open ? 1.f : 0.f, 6.f, static_easing);

	if (animstate->alpha > 0.01f) {

		gui->set_next_window_pos(button.Max - SCALE(elements->cheat_button.popup_size.x, -10));
		gui->set_next_window_size(SCALE(elements->cheat_button.popup_size));
		gui->push_var(style_var_alpha, animstate->alpha);
		gui->push_var(style_var_window_padding, SCALE(elements->cheat_button.popup_padding));
		gui->push_var(style_var_item_spacing, SCALE(elements->cheat_button.popup_spacing));
		gui->begin("settings", nullptr, window_flags_always_auto_resize | window_flags_no_decoration | window_flags_no_saved_settings | window_flags_no_move | window_flags_no_scrollbar | window_flags_no_scroll_with_mouse | window_flags_no_background);
		{
			draw->rect_filled(gui->window_drawlist(), gui->window_pos() + SCALE(1, 1), gui->window_pos() + gui->window_size() - SCALE(1, 1), draw->get_clr(clr->widget_layout), SCALE(elements->cheat_button.popup_rounding));
			draw->rect(gui->window_drawlist(), gui->window_pos() + SCALE(1, 1), gui->window_pos() + gui->window_size() - SCALE(1, 1), draw->get_clr(clr->login_border), SCALE(elements->cheat_button.popup_rounding));
		
			widgets->dropdown(LANG("Способ инъекции", "Injection method"), &var->gui.inject_type, {LANG("Скрытый инжект", "Hidden injection"), LANG("Открытый инжект", "Open injection"), LANG("EFI Драйвер", "EFI Driver")});
			if (widgets->classic_button(LANG("Начать сессию", "Start a session"))) {
				animstate->open = false;
				elements->inject_window.injection_cheat_name = name;
				var->gui.is_inject = true;
			}

			auto g = GetCurrentContext();

			bool dropdown_hovered = g->HoveredWindow && strstr(g->HoveredWindow->Name, LANG("Способ инъекции", "Injection method").data());

			if (!ImRect(gui->window_pos(), gui->window_pos() + gui->window_size()).Contains(GetMousePos()) && gui->mouse_clicked(0) && !dropdown_hovered) {
				animstate->open = false;
			}

			if (IsKeyPressed(ImGuiKey_MouseWheelY)) {
				animstate->open = false;
			}
		}
		gui->end();
		gui->pop_var(3);
	}

	return pressed;
}

bool selectable(std::string name, bool selected) {

	struct anim_t {
		float alpha;
	};

	ImGuiWindow* window = GetCurrentWindow();
	ImGuiID id = window->GetID(name.data());
	anim_t* animstate = gui->anim_container(&animstate, id);

	ImVec2 pos = window->DC.CursorPos;
	ImVec2 size = ImVec2(gui->content_avail().x, SCALE(elements->selectable.height));
	ImRect rect(pos, pos + size);

	gui->item_size(rect);
	gui->item_add(rect, id);

	bool pressed = rect.Contains(GetMousePos()) && gui->mouse_clicked(0);

	gui->easing(animstate->alpha, selected ? 1.f : 0.f, 5.f, static_easing);

	draw->rect_filled(gui->foreground_drawlist(), rect.Min, rect.Max, draw->get_clr(clr->selectable_active, animstate->alpha), SCALE(elements->selectable.rounding));
	draw->circle_filled(gui->foreground_drawlist(), rect.Max - ImVec2(SCALE(13), size.y / 2), SCALE(2), draw->get_clr(clr->accent, animstate->alpha), SCALE(64));

	draw->text_clipped(gui->foreground_drawlist(), font->get(inter_medium, 10), rect.Min + SCALE(elements->selectable.padding, 0), rect.Max, draw->get_clr(clr->white), name.data(), 0, 0, {0, 0.5});

	return pressed;
}

bool begin_dropdown(std::string name, std::string preview, int item_count) {

	struct anim_t {
		float alpha, arrow_rad;
		bool open;
		ImVec4 text;
	};

	ImGuiWindow* window = GetCurrentWindow();
	ImGuiID id = window->GetID(name.data());
	anim_t* animstate = gui->anim_container(&animstate, id);

	ImVec2 pos = window->DC.CursorPos;
	ImVec2 size = ImVec2(gui->content_avail().x, SCALE(elements->dropdown.height));
	ImRect rect(pos, pos + size);
	ImRect button(rect.GetBL() - SCALE(0, elements->dropdown.button_height), rect.Max);

	gui->item_size(rect); 
	gui->item_add(rect, id);

	bool held, hovered, pressed = gui->button_behavior(button, id, &hovered, &held);

	if (pressed && !animstate->open && animstate->alpha < 0.01f) {
		animstate->open = !animstate->open;
	}

	gui->easing(animstate->alpha, animstate->open ? 1.f : 0.f, 6.f, static_easing);
	gui->easing(animstate->text, animstate->open ? clr->white.Value : clr->text_inactive.Value, 7.f, dynamic_easing);
	gui->easing(animstate->arrow_rad, animstate->open ? 270.f : 90.f, 7.f, dynamic_easing);

	draw->text_clipped(window->DrawList, font->get(inter_medium, 10), rect.Min, rect.Max, draw->get_clr(animstate->text), name.data(), gui->text_end(name.data()), 0, {0, 0});

	draw->rect_filled(window->DrawList, button.Min, button.Max, draw->get_clr(clr->dark_widget), SCALE(elements->dropdown.rounding));
	draw->text_clipped(window->DrawList, font->get(inter_medium, 10), button.Min + SCALE(10, 0), button.Max, draw->get_clr(clr->white), preview.data(), gui->text_end(preview.data()), 0, {0, 0.5});
	
	draw->rotate_start(window->DrawList);
	draw->text_clipped(window->DrawList, font->get(icons, 10), button.Min, button.Max - SCALE(10, 0), draw->get_clr(clr->white), var->icon.arrow_filled.data(), 0, 0, {1, 0.5});
	draw->rotate_end(window->DrawList, animstate->arrow_rad, ImVec2(button.Max.x - SCALE(15), button.GetCenter().y));

	if (animstate->alpha < 0.01f) {
		return false;
	}

	gui->set_next_window_pos(rect.GetBL() + SCALE(0, 10));
	gui->set_next_window_size(ImVec2(size.x, SCALE(elements->selectable.height) * item_count));

	gui->push_var(style_var_alpha, animstate->alpha);
	gui->push_var(style_var_window_padding, ImVec2(0, 0));
	gui->push_var(style_var_item_spacing, ImVec2(0, 0));

	bool ret = gui->begin(name, nullptr, window_flags_no_decoration | window_flags_no_saved_settings | window_flags_no_move | window_flags_no_background | window_flags_no_scrollbar);
	{
		gui->set_window_focus();

		window = gui->get_window();

		draw->push_clip_rect(gui->foreground_drawlist(), window->Pos, window->Pos + ImVec2(window->Size.x, window->Size.y * animstate->alpha));

		draw->rect_filled(gui->foreground_drawlist(), window->Pos, window->Pos + window->Size, draw->get_clr(clr->dark_widget), SCALE(elements->selectable.rounding));

		if (!IsWindowHovered() && IsMouseClicked(0)) {
			animstate->open = false;
		}

		if (IsKeyPressed(ImGuiKey_MouseWheelY)) {
			animstate->open = false;
		}
	}

	return ret;
}

void end_dropdown() {

	draw->pop_clip_rect(gui->foreground_drawlist());
	gui->end();
	gui->pop_var(3);
}

bool c_widgets::dropdown(std::string name, int* callback, std::vector<std::string> variants) {

	bool ret;

	if (ret = begin_dropdown(name, variants[*callback], variants.size())) 
	{
		for (int i = 0; i < variants.size(); ++i) 
		{
			if (selectable(variants[i], *callback == i)) {
				*callback = i;
			}
		}

		end_dropdown();
	}

	return ret;
}

bool c_widgets::classic_button(std::string name, ImVec2 с_size) {

	ImGuiWindow* window = gui->get_window();
	ImGuiID id = window->GetID(name.data());

	ImVec2 pos(window->DC.CursorPos);
	ImVec2 size = с_size.x > 0 ? с_size : ImVec2(gui->content_avail().x, SCALE(elements->classic_button.height));
	ImRect rect(pos, pos + size);

	gui->item_size(rect);
	gui->item_add(rect, id);

	bool pressed = gui->button_behavior(rect, id, nullptr, nullptr);

	draw->rect_filled(window->DrawList, rect.Min, rect.Max, draw->get_clr(clr->dark_widget), SCALE(elements->classic_button.rounding));
	draw->text_clipped(window->DrawList, font->get(inter_medium, 10), rect.Min, rect.Max, draw->get_clr(clr->white), name.data(), gui->text_end(name.data()), 0, { 0.5, 0.5 });

	return pressed;

}

bool c_widgets::checkbox(std::string name, bool* callback) {

	struct animstate_t {
		ImVec4 layout, circle, text;
		float pos;
	};

	ImGuiWindow* window = gui->get_window();
	ImGuiID id = window->GetID(name.data());
	animstate_t* animstate = gui->anim_container(&animstate, id);

	ImVec2 pos(window->DC.CursorPos);
	ImVec2 size(gui->content_avail().x, SCALE(elements->checkbox.height));
	ImRect rect(pos, pos + size);
	ImRect button(rect.GetTR() - SCALE(elements->checkbox.button_width, 0), rect.Max);

	gui->item_size(rect);
	gui->item_add(rect, id);

	bool pressed = gui->button_behavior(rect, id, nullptr, nullptr);

	if (pressed)
		*callback = !*callback;

	gui->easing(animstate->layout, *callback ? clr->accent.Value : clr->dark_widget.Value, 7.f, dynamic_easing);
	gui->easing(animstate->circle, *callback ? clr->black.Value : clr->checkbox_disabled_circle.Value, 7.f, dynamic_easing);
	gui->easing(animstate->text, *callback ? clr->white.Value : clr->text_inactive.Value, 7.f, dynamic_easing);
	gui->easing(animstate->pos, *callback ? SCALE(10) : 0.f, 7.f, dynamic_easing);

	draw->rect_filled(window->DrawList, button.Min, button.Max, draw->get_clr(animstate->layout), SCALE(100));
	draw->circle_filled(window->DrawList, ImVec2(button.Min.x + SCALE(6) + animstate->pos, button.GetCenter().y), SCALE(elements->checkbox.circle_rad), draw->get_clr(animstate->circle), SCALE(64));
	draw->text_clipped(window->DrawList, font->get(inter_medium, 10), rect.Min, rect.Max, draw->get_clr(animstate->text), name.data(), 0, 0, { 0, 0.5 });

	return pressed;
}

bool c_widgets::user_button(std::string username, ImTextureID avatar)
{
	struct animstate_t {
		bool open, init;
		float alpha;
	};

	ImGuiWindow* window = gui->get_window();
	ImGuiID id = window->GetID(username.data());
	animstate_t* animstate = gui->anim_container(&animstate, id);

	ImVec2 pos(window->DC.CursorPos);
	ImVec2 size(SCALE(elements->user_button.size));
	ImRect rect(pos, pos + size);

	gui->item_size(rect);
	gui->item_add(rect, id);

	bool pressed = gui->button_behavior(rect, id, nullptr, nullptr);

	if (avatar != nullptr) 
	{
		draw->image_rounded(window->DrawList, avatar, rect.Min, rect.Min + SCALE(25, 25), ImVec2(0, 0), ImVec2(1, 1), draw->get_clr(clr->white), SCALE(100));
	}

	draw->text_clipped(window->DrawList, font->get(inter_semibold, 14), rect.Min + SCALE(40, 0), rect.Max, draw->get_clr(clr->white), username.data(), 0, 0, { 0, 0.5 });
	draw->text_clipped(window->DrawList, font->get(icons, 14), rect.Min, rect.Max, draw->get_clr(clr->text_inactive), var->icon.arrow_line.data(), 0, 0, {1, 0.5});

	if (pressed && !animstate->open && animstate->alpha < 0.01f) {
		animstate->open = !animstate->open;
	}

	gui->easing(animstate->alpha, animstate->open ? 1.f : 0.f, 6.f, static_easing);

	if (animstate->alpha > 0.01f) {

		gui->set_next_window_pos(rect.Max - SCALE(elements->user_button.window_size.x, -10));
		gui->set_next_window_size(SCALE(elements->user_button.window_size));
		gui->push_var(style_var_alpha, animstate->alpha);
		gui->push_var(style_var_window_padding, SCALE(elements->cheat_button.popup_padding));
		gui->push_var(style_var_item_spacing, SCALE(elements->cheat_button.popup_spacing));
		gui->begin("settings", nullptr, window_flags_no_decoration | window_flags_no_background | window_flags_no_nav | window_flags_nav_flattened);
		{
			draw->rect_filled(gui->window_drawlist(), gui->window_pos() + SCALE(1, 1), gui->window_pos() + gui->window_size() - SCALE(1, 1), draw->get_clr(clr->widget_layout), SCALE(elements->cheat_button.popup_rounding));
			draw->rect(gui->window_drawlist(), gui->window_pos() + SCALE(1, 1), gui->window_pos() + gui->window_size() - SCALE(1, 1), draw->get_clr(clr->login_border), SCALE(elements->cheat_button.popup_rounding));

			widgets->dropdown(LANG("Язык приложения", "Application Language"), &var->gui.lang, {LANG("Русский", "Russian"), LANG("Английский", "English")});
			static bool auto_close = false;
			static bool auto_load = false;
			static float hue = 0.f;
			static float saturation = 0.f;
			static float brightness = 0.f;
			if (!animstate->init) 
			{
				ColorConvertRGBtoHSV(clr->accent.Value.x, clr->accent.Value.y, clr->accent.Value.z, hue, saturation, brightness);
				animstate->init = true;
			}
			widgets->hue_slider(LANG("Акцентированный цвет", "Accentuated color"), &hue);
			clr->accent = ImColor::HSV(hue, saturation, brightness, 1.f);
			widgets->checkbox(LANG("Автоматически закрывать лаунчер", "Automatically close the launcher"), &auto_close);
			widgets->checkbox(LANG("Добавить в авто-загрузку", "Add to auto-upload"), &auto_load);
			if (widgets->classic_button(LANG("Выйти из учетной записи.", "Log out of your account."))) {
				animstate->open = false;
				animstate->alpha = 0.f;
				var->gui.tab_stored = 0;
				var->gui.autorization = false;
			}

			if (!ImRect(gui->window_pos(), gui->window_pos() + gui->window_size()).Contains(GetMousePos()) && gui->mouse_clicked(0)) {
				animstate->open = false;
			}

			if (IsKeyPressed(ImGuiKey_MouseWheelY)) {
				animstate->open = false;
			}
		}
		gui->end();
		gui->pop_var(3);
	}

	return pressed;
}

bool c_widgets::hue_slider(std::string name, float* callback) {

	struct animstate_t {
		float position;
	};

	ImGuiWindow* window = gui->get_window();
	ImGuiID id = window->GetID(name.data());
	animstate_t* animstate = gui->anim_container(&animstate, id);

	ImVec2 pos(window->DC.CursorPos);
	ImVec2 size(gui->content_avail().x, SCALE(elements->slider.height));
	ImRect rect(pos, pos + size);
	ImRect button(rect.GetBL() - SCALE(0, elements->slider.button_height), rect.Max);

	gui->item_size(rect);
	gui->item_add(rect, id);

	bool held, pressed = gui->button_behavior(button, id, nullptr, &held);

	if (held) {
		*callback = ImSaturate((GetMousePos().x - pos.x) / button.GetWidth());
	}

	gui->easing(animstate->position, *callback, 10.f, dynamic_easing);

	const int num_colors = 7;
	ImColor col_hues[num_colors];
	for (int i = 0; i < num_colors; ++i) {
		float current_hue = i / float(num_colors - 1);
		float hue, saturation, brightness;
		ColorConvertRGBtoHSV(clr->accent.Value.x, clr->accent.Value.y, clr->accent.Value.z, hue, saturation, brightness);
		col_hues[i] = ImColor::HSV(current_hue, saturation, brightness, 1.f);
	}

	for (int i = 0; i < num_colors - 1; ++i) {
		float rounding = (i == 0) || (i == num_colors - 2) ? SCALE(100) : 0;
		draw_flags flags = (i == 0) ? draw_flags_round_corners_left : (i == num_colors - 2) ? draw_flags_round_corners_right : 0;

		float x1 = roundf(button.Min.x + i * (button.GetWidth() / (num_colors - 1)));
		float x2 = roundf(button.Min.x + (i + 1) * (button.GetWidth() / (num_colors - 1)));

		draw->rect_filled_multi_color(window->DrawList, ImVec2(x1, button.Min.y), ImVec2(x2, button.Max.y), draw->get_clr(col_hues[i]), draw->get_clr(col_hues[i + 1]), draw->get_clr(col_hues[i + 1]), draw->get_clr(col_hues[i]), rounding, flags);
	}

	float x = button.Min.x + button.GetWidth() * animstate->position;

	x = ImClamp(x, rect.Min.x + SCALE(4), rect.Max.x - SCALE(4));

	ImRect grab(ImVec2(x - SCALE(4), button.GetCenter().y - SCALE(4)), ImVec2(x + SCALE(4), button.GetCenter().y + SCALE(4)));

	draw->rect_filled(window->DrawList, grab.Min, grab.Max, draw->get_clr(clr->accent), SCALE(1));
	
	
	draw->text_clipped(window->DrawList, font->get(inter_medium, 10), rect.Min - SCALE(0, 2), rect.Max, draw->get_clr(clr->white), name.data(), 0, 0, {0, 0});

	return held;
}

void c_widgets::update_log(std::string game, std::string hack, std::string time, std::string status, std::string last_update, std::string version, std::vector<update_log_t> log) {

	ImGuiWindow* window = gui->get_window();
	ImGuiID id = window->GetID((game + hack + "##updatelog").data());

	float height = 0.f;

	for (int i = 0; i < log.size(); ++i)
	{
		height += SCALE(elements->update_log.content_padding.y) * 3;
		height += gui->text_size(font->get(inter_medium, 10), log[i].main.data()).y;
		height += gui->text_size(font->get(inter_medium, 10), log[i].text.data()).y;
	}

	ImVec2 pos(window->DC.CursorPos);
	ImVec2 size(window->ScrollbarY ? gui->content_avail().x : gui->content_avail().x - SCALE(10), SCALE(elements->update_log.minimal_height) + height);
	ImRect rect(pos, pos + size);

	gui->item_size(rect);
	gui->item_add(rect, id);

	draw->rect_filled(window->DrawList, rect.Min, rect.Max, draw->get_clr(clr->widget_layout), SCALE(elements->update_log.rounding));

	float header_content_width = size.x / 4;

	draw->line(window->DrawList, rect.Min + SCALE(0, elements->update_log.minimal_height), rect.Min + ImVec2(size.x, SCALE(elements->update_log.minimal_height)), draw->get_clr(clr->line_layout));

	ImVec2 padding = SCALE(elements->update_log.head_padding);
	ImRect content(rect.Min, rect.Min + ImVec2(header_content_width, SCALE(elements->update_log.minimal_height)));
	{
		draw->line(window->DrawList, content.GetTR(), content.GetBR(), draw->get_clr(clr->line_layout));
		draw->text_clipped(window->DrawList, font->get(inter_medium, 10), content.Min + padding, content.Max - padding, draw->get_clr(clr->white), game.data(), 0, 0, { 0, 0 });
		draw->text_clipped(window->DrawList, font->get(inter_medium, 10), content.Min + padding, content.Max - padding, draw->get_clr(clr->text_inactive), hack.data(), 0, 0, { 0, 1 });
	}

	content = ImRect(content.GetTR(), content.GetTR() + ImVec2(header_content_width, SCALE(elements->update_log.minimal_height)));
	{
		draw->line(window->DrawList, content.GetTR(), content.GetBR(), draw->get_clr(clr->line_layout));
		draw->text_clipped(window->DrawList, font->get(inter_medium, 10), content.Min + padding, content.Max - padding, draw->get_clr(clr->white), time.data(), 0, 0, { 0, 0 });
		draw->text_clipped(window->DrawList, font->get(inter_medium, 10), content.Min + padding, content.Max - padding, draw->get_clr(clr->text_inactive), status.data(), 0, 0, { 0, 1 });
	}

	content = ImRect(content.GetTR(), content.GetTR() + ImVec2(header_content_width, SCALE(elements->update_log.minimal_height)));
	{
		draw->line(window->DrawList, content.GetTR(), content.GetBR(), draw->get_clr(clr->line_layout));
		draw->text_clipped(window->DrawList, font->get(inter_medium, 10), content.Min + padding, content.Max - padding, draw->get_clr(clr->white), LANG("Последнее обновление", "Last update").data(), 0, 0, {0, 0});
		draw->text_clipped(window->DrawList, font->get(inter_medium, 10), content.Min + padding, content.Max - padding, draw->get_clr(clr->text_inactive), last_update.data(), 0, 0, { 0, 1 });
	}

	content = ImRect(content.GetTR(), content.GetTR() + ImVec2(header_content_width, SCALE(elements->update_log.minimal_height)));
	{
		draw->text_clipped(window->DrawList, font->get(inter_medium, 10), content.Min + padding, content.Max - padding, draw->get_clr(clr->white), LANG("Нынешная версия продукта", "Current version of the product").data(), 0, 0, {0, 0});
		draw->text_clipped(window->DrawList, font->get(inter_medium, 10), content.Min + padding, content.Max - padding, draw->get_clr(clr->text_inactive), version.data(), 0, 0, { 0, 1 });
	}

	for (int i = 0; i < log.size(); ++i)
	{
		float spacing = SCALE(elements->update_log.minimal_height);

		for (int j = 0; j < i; ++j)
		{
			spacing += SCALE(elements->update_log.content_padding.y) * 3;
			spacing += gui->text_size(font->get(inter_medium, 10), log[j].main.data()).y;
			spacing += gui->text_size(font->get(inter_medium, 10), log[j].text.data()).y;
		}

		float log_height = (SCALE(elements->update_log.content_padding.y) * 3) + gui->text_size(font->get(inter_medium, 10), log[i].main.data()).y + gui->text_size(font->get(inter_medium, 10), log[i].text.data()).y;

		ImRect log_rect(pos + ImVec2(0, spacing), pos + ImVec2(size.x, spacing + log_height));
		
		draw->text_clipped(window->DrawList, font->get(inter_medium, 10), log_rect.Min + SCALE(elements->update_log.content_padding), log_rect.Max, draw->get_clr(clr->white), log[i].main.data(), 0, 0, { 0, 0 });
		draw->text_clipped(window->DrawList, font->get(inter_medium, 10), log_rect.Min + ImVec2(SCALE(elements->update_log.content_padding.x), SCALE(elements->update_log.content_padding.y * 1.65) + gui->text_size(font->get(inter_medium, 10), log[i].main.data()).y), log_rect.Max, draw->get_clr(clr->text_inactive), log[i].text.data(), 0, 0, { 0, 0 });
	
		if (i + 1 != log.size()) {
			draw->line(window->DrawList, log_rect.GetBL(), log_rect.GetBR(), draw->get_clr(clr->line_layout));
		}

	}
}