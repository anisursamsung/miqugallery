#include "media_viewer.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

using namespace miqu;

namespace miqugallery {

MediaViewer::MediaViewer(std::vector<MediaItem> items, int initial_index)
    : m_items(std::move(items)), m_current_index(initial_index) {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    if (m_current_index < 0 || m_current_index >= static_cast<int>(m_items.size())) {
        m_current_index = 0;
    }

    auto config = Config::get();

    // 1. Central Image View (Fills canvas with Contain fit)
    m_image_view = ImageViewBuilder::create()
        ->fitMode(FitMode::Contain)
        ->build();
    m_image_view->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent),
        Gravity::Center
    ));
    m_image_view->set_quality_mode(ImageQuality::FullOriginal);
    add_view(m_image_view);

    // 2. Left Floating Navigation Button
    m_btn_prev = ButtonBuilder::create()
        ->text("◀")
        ->flat(true)
        ->textSize(22)
        ->bold(true)
        ->padding(12, 16)
        ->cornerRadius(24)
        ->onClick([this]() {
            prev_media();
        })
        ->build();
    m_btn_prev->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical | Gravity::Left
    ));
    m_btn_prev->set_margin(16, 0, 0, 0);
    if (m_items.size() <= 1) {
        m_btn_prev->set_visibility(Visibility::Gone);
    }
    add_view(m_btn_prev);

    // 3. Right Floating Navigation Button
    m_btn_next = ButtonBuilder::create()
        ->text("▶")
        ->flat(true)
        ->textSize(22)
        ->bold(true)
        ->padding(12, 16)
        ->cornerRadius(24)
        ->onClick([this]() {
            next_media();
        })
        ->build();
    m_btn_next->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical | Gravity::Right
    ));
    m_btn_next->set_margin(0, 0, 16, 0);
    if (m_items.size() <= 1) {
        m_btn_next->set_visibility(Visibility::Gone);
    }
    add_view(m_btn_next);

    // 4. Bottom Information & Actions Floating Card
    auto bottom_card = std::make_shared<CardView>();
    bottom_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::Bottom | Gravity::CenterHorizontal
    ));
    bottom_card->set_margin(0, 0, 0, 16);
    bottom_card->set_padding(18, 10);

    auto bottom_layout = std::make_shared<LinearLayout>(Orientation::Horizontal);
    bottom_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    text_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    text_col->set_margin(0, 0, 18, 0);

    m_info_name = TextViewBuilder::create()
        ->text("Filename")
        ->bold(true)
        ->build();
    m_info_name->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    m_info_meta = TextViewBuilder::create()
        ->text("0 KB • Date")
        ->caption(true)
        ->muted(true)
        ->build();
    m_info_meta->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    text_col->add_view(m_info_name);
    text_col->add_view(m_info_meta);
    bottom_layout->add_view(text_col);

    auto btn_rotate = ButtonBuilder::create()
        ->text("⟲ Rotate")
        ->flat(true)
        ->padding(10, 6)
        ->onClick([this]() {
            rotate_clockwise();
        })
        ->build();
    btn_rotate->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    btn_rotate->set_margin(0, 0, 8, 0);
    bottom_layout->add_view(btn_rotate);

    m_btn_play_external = ButtonBuilder::create()
        ->text("▶ Open Video")
        ->primary(true)
        ->padding(12, 6)
        ->bold(true)
        ->onClick([this]() {
            open_external();
        })
        ->build();
    m_btn_play_external->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    bottom_layout->add_view(m_btn_play_external);

    bottom_card->add_view(bottom_layout);
    add_view(bottom_card);

    update_display();
}

const MediaItem* MediaViewer::get_current_item() const {
    if (m_current_index >= 0 && m_current_index < static_cast<int>(m_items.size())) {
        return &m_items[m_current_index];
    }
    return nullptr;
}

void MediaViewer::next_media() {
    if (m_items.size() <= 1) return;
    m_current_index = (m_current_index + 1) % static_cast<int>(m_items.size());
    m_rotation = 0.0;
    update_display();
}

void MediaViewer::prev_media() {
    if (m_items.size() <= 1) return;
    m_current_index = (m_current_index - 1 + static_cast<int>(m_items.size())) % static_cast<int>(m_items.size());
    m_rotation = 0.0;
    update_display();
}

void MediaViewer::rotate_clockwise() {
    m_rotation = std::fmod(m_rotation + 90.0, 360.0);
    if (m_image_view) {
        m_image_view->set_rotation_angle(m_rotation);
        if (get_window()) get_window()->schedule_redraw();
    }
}

void MediaViewer::open_external() {
    const auto* item = get_current_item();
    if (!item) return;

    pid_t pid = fork();
    if (pid == 0) {
        if (fork() != 0) {
            _exit(0);
        }
        setsid();
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execlp("xdg-open", "xdg-open", item->path.c_str(), nullptr);
        _exit(127);
    } else if (pid > 0) {
        waitpid(pid, nullptr, 0);
    }
}

void MediaViewer::update_display() {
    const auto* item = get_current_item();
    if (!item) return;

    if (m_image_view) {
        m_image_view->set_rotation_angle(m_rotation);
        m_image_view->set_image_resource(item->path);
    }

    if (m_info_name) {
        m_info_name->set_text(item->filename);
    }

    if (m_info_meta) {
        std::string meta = item->formatted_size + " • " + item->formatted_date;
        if (m_items.size() > 1) {
            meta += " • (" + std::to_string(m_current_index + 1) + " of " + std::to_string(m_items.size()) + ")";
        }
        m_info_meta->set_text(meta);
    }

    if (m_btn_prev && m_btn_next) {
        auto nav_vis = (m_items.size() > 1) ? Visibility::Visible : Visibility::Gone;
        m_btn_prev->set_visibility(nav_vis);
        m_btn_next->set_visibility(nav_vis);
    }

    if (m_btn_play_external) {
        if (item->type == MediaType::Video) {
            m_btn_play_external->set_text("▶ Play Video");
            m_btn_play_external->set_visibility(Visibility::Visible);
        } else {
            m_btn_play_external->set_visibility(Visibility::Gone);
        }
    }

    if (m_on_index_changed) {
        m_on_index_changed(m_current_index, *item);
    }

    if (get_window()) {
        get_window()->schedule_redraw();
    }
}

bool MediaViewer::on_key(const KeyPressEvent& event) {
    if (!event.pressed) return false;

    if (m_items.size() > 1) {
        if (event.keysym == XKB_KEY_Left) {
            prev_media();
            return true;
        }
        if (event.keysym == XKB_KEY_Right) {
            next_media();
            return true;
        }
    }
    if (event.keysym == XKB_KEY_r || event.keysym == XKB_KEY_R) {
        rotate_clockwise();
        return true;
    }
    if (event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter) {
        const auto* item = get_current_item();
        if (item && item->type == MediaType::Video) {
            open_external();
            return true;
        }
    }

    return false;
}

} // namespace miqugallery
