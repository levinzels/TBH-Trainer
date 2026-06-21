#pragma once
#include "includes.h"

#define IMGUI_DEFINE_MATH_OPERATORS

struct update_log_t
{
    std::string main, text;
};

struct games_t {
    std::string name;
    int active;
    ImTextureID image;
};

class c_widgets
{
public:
    bool tab_button(std::string name, int tab, bool autorization_check = false);
    bool button_bordered(std::string name);
    bool button(std::string name, ImVec2 size = { 0, 0 });
    bool text_button_colored(std::string default_text, std::string colored_text);
    void info(std::string users, std::string sells, std::string time, std::string features);
    bool enter_button(std::string name);
    bool text_field(std::string name, std::string icon, char* buf, int buf_size);
    bool games_search_field(std::string name, std::string icon, char* buf, int buf_size);
    bool game_button(std::string name, int active_products, ImTextureID image = nullptr, bool sssmall = false);
    bool cheat_button(std::string name, std::string last_update, std::string last_launch);
    bool dropdown(std::string name, int* callback, std::vector<std::string> variant);
    bool classic_button(std::string name, ImVec2 size = { 0, 0 });
    bool checkbox(std::string name, bool* callback);
    bool user_button(std::string username, ImTextureID avatar = nullptr);
    bool hue_slider(std::string name, float* callback);
    void update_log(std::string game, std::string hack, std::string time, std::string status, std::string last_update, std::string version, std::vector<update_log_t> log);
};

inline std::unique_ptr<c_widgets> widgets = std::make_unique<c_widgets>();

enum notify_type
{
    success = 0,
    warning = 1,
    error = 2
};

struct notify_state
{
    int notify_id;
    std::string_view text;
    notify_type type{ success };

    ImVec2 window_size{ 0, 0 };
    float notify_alpha{ 0 };
    bool active_notify{ true };
    float notify_timer{ 0 };
    float notify_pos{ 0 };
};

class c_notify
{
public:
    void setup_notify();

    void add_notify(std::string_view text, notify_type type);

private:
    ImVec2 render_notify(int cur_notify_value, float notify_alpha, float notify_percentage, float notify_pos, std::string_view text, notify_type type);

    float notify_time{ 15 };
    int notify_count{ 0 };

    float notify_spacing{ 20 };
    ImVec2 notify_padding{ 20, 20 };

    std::vector<notify_state> notifications;

};

inline std::unique_ptr<c_notify> notify = std::make_unique<c_notify>();
