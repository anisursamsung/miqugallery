#include "media_scanner.hpp"
#include "media_viewer.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <iostream>
#include <memory>
#include <vector>

using namespace miqu;
using namespace miqugallery;

int main(int argc, char** argv) {
    auto engine = AppEngine::create();
    if (!engine) {
        std::cerr << "[miqugallery] Failed to initialize AppEngine.\n";
        return 1;
    }

    auto config = Config::get();

    // Check optional CLI search path or direct file target
    if (argc > 1) {
        std::error_code ec;
        std::filesystem::path input_p(argv[1]);
        if (std::filesystem::exists(input_p, ec) && std::filesystem::is_regular_file(input_p, ec)) {
            // =================================================================
            // STANDALONE SINGLE-MEDIA VIEWER MODE
            // Invoked from file manager ("Open With...") or CLI for a specific file.
            // Only view this single file. No folder scanning, no neighboring media.
            // =================================================================
            MediaItem item = MediaScanner::create_single_item(input_p.string());
            auto viewer = std::make_shared<MediaViewer>(std::vector<MediaItem>{item}, 0);

            auto window = WindowBuilder::create()
                ->title(item.filename + " - Gallery")
                ->appId("miqugallery")
                ->role(WindowRole::Toplevel)
                ->preferredSize(600, 680)
                ->closeOnEscape(true)
                ->contentView(viewer)
                ->onKey([&](const KeyPressEvent& ev) {
                    if (!ev.pressed) return;
                    if (ev.keysym == XKB_KEY_Escape || ev.keysym == XKB_KEY_q || ev.keysym == XKB_KEY_Q) {
                        engine->quit();
                    }
                })
                ->onClose([engine]() {
                    engine->quit();
                })
                ->build();

            if (!window) {
                std::cerr << "[miqugallery] Failed to initialize Wayland window.\n";
                return 1;
            }

            window->show();
            return engine->enter_loop();
        }
    }

    // =========================================================================
    // FULL GALLERY APPLICATION MODE
    // Multi-album navigation, gallery grids, and full media browser pipeline.
    // =========================================================================
    std::string initial_dir_target = "";
    std::vector<std::string> search_paths;

    if (argc > 1) {
        std::error_code ec;
        std::filesystem::path input_p(argv[1]);
        if (std::filesystem::exists(input_p, ec) && std::filesystem::is_directory(input_p, ec)) {
            initial_dir_target = std::filesystem::canonical(input_p, ec).string();
            search_paths.push_back(initial_dir_target);
        }
    }

    std::vector<Album> albums = MediaScanner::scan_albums(search_paths);

    // =========================================================================
    // NAVIGATION CONTAINER
    // =========================================================================
    auto nav_view = NavigationViewBuilder::create()
        ->autoBack(true)
        ->build();
    nav_view->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        0,
        1.0f
    ));

    // Forward declarations of view factories
    std::function<std::shared_ptr<View>()> create_albums_view;
    std::function<std::shared_ptr<View>(const Album&)> create_gallery_view;

    // --- LEVEL 0: ALBUMS OVERVIEW GRID ---
    create_albums_view = [&]() -> std::shared_ptr<View> {
        if (albums.empty()) {
            auto empty_card = std::make_shared<CardView>();
            empty_card->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::MatchParent)
            ));
            empty_card->set_padding(48);

            auto empty_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
            empty_layout->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::WrapContent),
                Gravity::Center
            ));

            auto empty_icon = TextViewBuilder::create()
                ->text("🖼️")
                ->textSize(48)
                ->textAlignment(TextAlignment::Center)
                ->build();
            empty_icon->set_margin(0, 0, 0, 16);

            auto empty_title = TextViewBuilder::create()
                ->text("No Media Folders Found")
                ->h1()
                ->textAlignment(TextAlignment::Center)
                ->build();
            empty_title->set_margin(0, 0, 0, 8);

            auto empty_desc = TextViewBuilder::create()
                ->text("Add images or videos to ~/Pictures or ~/Videos,\nor launch with: miqugallery /path/to/folder")
                ->caption()
                ->muted()
                ->textAlignment(TextAlignment::Center)
                ->build();

            empty_layout->add_view(empty_icon);
            empty_layout->add_view(empty_title);
            empty_layout->add_view(empty_desc);
            empty_card->add_view(empty_layout);
            return empty_card;
        }

        auto grid = std::make_shared<GridView>();
        grid->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::MatchParent)
        ));
        grid->set_auto_fit(200);
        grid->set_cell_height(205);
        grid->set_horizontal_spacing(16);
        grid->set_vertical_spacing(16);

        for (size_t i = 0; i < albums.size(); ++i) {
            const auto& alb = albums[i];

            auto card = std::make_shared<CardView>();
            card->set_padding(8);
            card->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::MatchParent)
            ));

            auto card_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
            card_layout->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::MatchParent)
            ));

            auto thumb = ImageViewBuilder::create()
                ->imageResource(!alb.cover_path.empty() ? alb.cover_path : "folder-pictures")
                ->fitMode(FitMode::Cover)
                ->cornerRadius(8)
                ->qualityMode(ImageQuality::ThumbnailFast)
                ->build();
            thumb->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                135
            ));
            thumb->set_margin(0, 0, 0, 8);

            auto name_lbl = TextViewBuilder::create()
                ->text(alb.name)
                ->bold(true)
                ->build();
            name_lbl->set_margin(4, 0, 4, 2);

            auto count_lbl = TextViewBuilder::create()
                ->text(alb.get_summary_text())
                ->caption()
                ->muted()
                ->build();
            count_lbl->set_margin(4, 0, 4, 4);

            card_layout->add_view(thumb);
            card_layout->add_view(name_lbl);
            card_layout->add_view(count_lbl);
            card->add_view(card_layout);

            grid->add_item(card);
        }

        grid->set_on_item_click_listener([&](size_t idx, std::shared_ptr<View>) {
            if (idx < albums.size()) {
                const auto& selected_album = albums[idx];
                nav_view->push(
                    create_gallery_view(selected_album),
                    selected_album.name,
                    selected_album.get_summary_text(),
                    "album_" + selected_album.name
                );
            }
        });

        return grid;
    };

    // --- LEVEL 1: GALLERY GRID (Media Items in Selected Album) ---
    create_gallery_view = [&](const Album& album) -> std::shared_ptr<View> {
        auto grid = std::make_shared<GridView>();
        grid->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::MatchParent)
        ));
        grid->set_auto_fit(150);
        grid->set_cell_height(165);
        grid->set_horizontal_spacing(12);
        grid->set_vertical_spacing(12);

        for (size_t i = 0; i < album.items.size(); ++i) {
            const auto& item = album.items[i];

            auto card = std::make_shared<CardView>();
            card->set_padding(6);
            card->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::MatchParent)
            ));

            auto card_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
            card_layout->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::MatchParent)
            ));

            auto thumb_container = std::make_shared<FrameLayout>();
            thumb_container->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                115
            ));
            thumb_container->set_margin(0, 0, 0, 6);

            auto thumb = ImageViewBuilder::create()
                ->imageResource(item.path)
                ->fitMode(FitMode::Cover)
                ->cornerRadius(6)
                ->qualityMode(ImageQuality::ThumbnailFast)
                ->build();
            thumb->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::MatchParent)
            ));
            thumb_container->add_view(thumb);

            if (item.type == MediaType::Video) {
                auto video_badge = TextViewBuilder::create()
                    ->text("▶ Video")
                    ->caption()
                    ->bold()
                    ->build();
                video_badge->set_layout_params(LayoutParams(
                    static_cast<int>(LayoutDimension::WrapContent),
                    static_cast<int>(LayoutDimension::WrapContent),
                    Gravity::Bottom | Gravity::Left
                ));
                video_badge->set_margin(6, 0, 0, 6);
                thumb_container->add_view(video_badge);
            }

            auto name_lbl = TextViewBuilder::create()
                ->text(item.filename)
                ->caption()
                ->bold()
                ->build();
            name_lbl->set_margin(2, 0, 2, 2);

            card_layout->add_view(thumb_container);
            card_layout->add_view(name_lbl);
            card->add_view(card_layout);

            grid->add_item(card);
        }

        grid->set_on_item_click_listener([&, album](size_t idx, std::shared_ptr<View>) {
            if (idx < album.items.size()) {
                auto viewer = std::make_shared<MediaViewer>(album.items, static_cast<int>(idx));
                const auto& item = album.items[idx];

                nav_view->push(
                    viewer,
                    item.filename,
                    item.formatted_size + " • " + item.formatted_date,
                    "viewer"
                );
            }
        });

        return grid;
    };

    // Calculate total count summary for root
    size_t total_items = 0;
    for (const auto& a : albums) total_items += a.items.size();
    std::string root_subtitle = std::to_string(albums.size()) + " Albums • " + std::to_string(total_items) + " Items";

    // Set Root View (AlbumGrid)
    nav_view->push(create_albums_view(), "Albums", root_subtitle, "root");

    // Automatically navigate to specific directory album if specified
    if (!initial_dir_target.empty()) {
        for (const auto& alb : albums) {
            std::error_code ec;
            if (std::filesystem::equivalent(alb.path, initial_dir_target, ec) || alb.path == initial_dir_target) {
                nav_view->push(
                    create_gallery_view(alb),
                    alb.name,
                    alb.get_summary_text(),
                    "album_" + alb.name
                );
                break;
            }
        }
    }

    // =========================================================================
    // PERSISTENT TOP HEADER BAR (Universal Toolbar)
    // =========================================================================
    std::shared_ptr<ImageButton> btn_header_home;

    auto toolbar = ToolbarBuilder::create()
        ->title(nav_view->get_current_title())
        ->subtitle(nav_view->get_current_subtitle())
        ->onBack([nav_view]() {
            nav_view->pop();
        })
        ->onRefresh([&, nav_view]() {
            albums = MediaScanner::scan_albums(search_paths);
            size_t new_total = 0;
            for (const auto& a : albums) new_total += a.items.size();
            std::string new_subtitle = std::to_string(albums.size()) + " Albums • " + std::to_string(new_total) + " Items";

            nav_view->pop_to_root();
            nav_view->replace_top(create_albums_view(), "Albums", new_subtitle);
        })
        ->onClose([engine]() {
            engine->quit();
        })
        ->build();
    toolbar->set_back_visible(false);
    toolbar->set_margin(0, 0, 0, 10);

    btn_header_home = toolbar->add_action(icons::HOME, [nav_view]() {
        nav_view->pop_to_root();
    });
    btn_header_home->set_visibility(Visibility::Invisible);

    // Synchronize Header bar whenever navigation transitions occur
    nav_view->set_on_navigation_listener([toolbar, btn_header_home](const NavigationPage& page, bool can_go_back) {
        toolbar->set_title(page.title);
        toolbar->set_subtitle(page.subtitle);
        toolbar->set_back_visible(can_go_back);
        if (btn_header_home) {
            btn_header_home->set_visibility(can_go_back ? Visibility::Visible : Visibility::Invisible);
        }
    });

    // =========================================================================
    // ROOT LAYOUT & WINDOW
    // =========================================================================
    auto root_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    root_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root_container->set_padding(14);

    root_container->add_view(toolbar);
    root_container->add_view(nav_view);

    auto window = WindowBuilder::create()
        ->title("Gallery - miqugallery")
        ->appId("miqugallery")
        ->role(WindowRole::Toplevel)
        ->preferredSize(600, 680)
        ->closeOnEscape(false) // Escape is handled by NavigationView; closes app only when at root
        ->contentView(root_container)
        ->onKey([&](const KeyPressEvent& ev) {
            if (!ev.pressed) return;
            if (ev.keysym == XKB_KEY_Escape && !nav_view->can_go_back()) {
                engine->quit();
            }
        })
        ->onClose([engine]() {
            engine->quit();
        })
        ->build();

    if (!window) {
        std::cerr << "[miqugallery] Failed to initialize Wayland window.\n";
        return 1;
    }

    window->show();
    return engine->enter_loop();
}
