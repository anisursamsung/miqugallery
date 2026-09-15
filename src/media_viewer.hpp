#pragma once

#include "media_scanner.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <vector>
#include <memory>
#include <functional>

namespace miqugallery {

class MediaViewer : public miqu::FrameLayout {
public:
    MediaViewer(std::vector<MediaItem> items, int initial_index = 0);
    ~MediaViewer() override = default;

    void next_media();
    void prev_media();
    void rotate_clockwise();
    void open_external();

    int get_current_index() const { return m_current_index; }
    const MediaItem* get_current_item() const;

    void set_on_index_changed_listener(std::function<void(int index, const MediaItem& item)> cb) {
        m_on_index_changed = std::move(cb);
    }

    bool on_key(const miqu::KeyPressEvent& event) override;

private:
    void update_display();

    std::vector<MediaItem> m_items;
    int m_current_index = 0;
    double m_rotation = 0.0;

    std::shared_ptr<miqu::ImageView> m_image_view;
    std::shared_ptr<miqu::Button> m_btn_prev;
    std::shared_ptr<miqu::Button> m_btn_next;
    std::shared_ptr<miqu::TextView> m_info_name;
    std::shared_ptr<miqu::TextView> m_info_meta;
    std::shared_ptr<miqu::Button> m_btn_play_external;

    std::function<void(int, const MediaItem&)> m_on_index_changed;
};

} // namespace miqugallery
