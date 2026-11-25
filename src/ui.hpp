// saved appearances menu
#pragma once

#include "engine.hpp"

#include <string>

namespace cap::ui
{
    enum class ButtonEvent
    {
        Click,
        Press,
        Release,
    };

    void set_data_dir(const std::wstring& dir);
    bool is_open();
    void open();
    void close();
    // esc fallback
    void on_escape();
    void tick(double t);
    bool knows(ue::UObject* button);
    void on_button(ue::UObject* button, ButtonEvent event);
    void on_dialog_answer(ue::UObject* dialog, bool yes);
    void forget();
}
