#include "media_scanner.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <set>
#include <chrono>

namespace miqugallery {

namespace fs = std::filesystem;

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return s;
}

bool MediaScanner::is_image_extension(const std::string& ext) {
    static const std::set<std::string> exts = {
        ".png", ".jpg", ".jpeg", ".webp", ".bmp", ".gif", ".svg", ".tiff", ".tif", ".avif", ".heic"
    };
    return exts.find(to_lower(ext)) != exts.end();
}

bool MediaScanner::is_video_extension(const std::string& ext) {
    static const std::set<std::string> exts = {
        ".mp4", ".mkv", ".webm", ".avi", ".mov", ".flv", ".wmv", ".m4v", ".3gp", ".ts"
    };
    return exts.find(to_lower(ext)) != exts.end();
}

MediaType MediaScanner::detect_type(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    if (is_image_extension(ext)) return MediaType::Image;
    if (is_video_extension(ext)) return MediaType::Video;
    return MediaType::Unknown;
}

std::string MediaScanner::format_file_size(uintmax_t bytes) {
    if (bytes == 0) return "0 B";
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double d_size = static_cast<double>(bytes);
    while (d_size >= 1024.0 && unit_idx < 4) {
        d_size /= 1024.0;
        unit_idx++;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << d_size << " " << units[unit_idx];
    return oss.str();
}

std::string MediaScanner::format_time(std::filesystem::file_time_type ftime) {
    try {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
        std::tm tm_buf;
        localtime_r(&tt, &tm_buf);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm_buf);
        return std::string(buf);
    } catch (...) {
        return "";
    }
}

std::string Album::get_summary_text() const {
    if (photo_count == 0 && video_count == 0) return "0 items";
    std::string s;
    if (photo_count > 0) {
        s += std::to_string(photo_count) + (photo_count == 1 ? " photo" : " photos");
    }
    if (video_count > 0) {
        if (!s.empty()) s += ", ";
        s += std::to_string(video_count) + (video_count == 1 ? " video" : " videos");
    }
    return s;
}

Album MediaScanner::scan_single_album(const std::string& folder_path) {
    Album album;
    album.path = folder_path;
    album.name = fs::path(folder_path).filename().string();
    if (album.name.empty()) album.name = folder_path;

    std::error_code ec;
    if (!fs::exists(folder_path, ec) || !fs::is_directory(folder_path, ec)) {
        return album;
    }

    for (const auto& entry : fs::directory_iterator(folder_path, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;

        std::string p = entry.path().string();
        MediaType type = detect_type(p);
        if (type == MediaType::Unknown) continue;

        MediaItem item;
        item.path = p;
        item.filename = entry.path().filename().string();
        item.parent_path = folder_path;
        item.type = type;
        item.file_size = entry.file_size(ec);
        item.formatted_size = format_file_size(item.file_size);
        item.modified_time = entry.last_write_time(ec);
        item.formatted_date = format_time(item.modified_time);

        if (type == MediaType::Image) album.photo_count++;
        else if (type == MediaType::Video) album.video_count++;

        album.items.push_back(std::move(item));
    }

    // Sort items newest first
    std::sort(album.items.begin(), album.items.end(), [](const MediaItem& a, const MediaItem& b) {
        return a.modified_time > b.modified_time;
    });

    if (!album.items.empty()) {
        album.cover_path = album.items.front().path;
        for (const auto& item : album.items) {
            if (item.type == MediaType::Image) {
                album.cover_path = item.path;
                break;
            }
        }
    }

    return album;
}

std::vector<Album> MediaScanner::scan_albums(const std::vector<std::string>& search_paths) {
    std::vector<std::string> roots = search_paths;
    if (roots.empty()) {
        const char* home = getenv("HOME");
        if (home) {
            std::string home_str = home;
            roots.push_back(home_str + "/Pictures");
            roots.push_back(home_str + "/Videos");
            roots.push_back(home_str + "/.config/theme/wallpapers");
        }
    }

    std::set<std::string> visited_folders;
    std::vector<Album> albums;

    for (const auto& root : roots) {
        std::error_code ec;
        if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) continue;

        // Check root itself
        if (visited_folders.find(root) == visited_folders.end()) {
            Album alb = scan_single_album(root);
            if (!alb.items.empty()) {
                visited_folders.insert(root);
                albums.push_back(std::move(alb));
            }
        }

        // Recursively inspect subdirectories (max depth 3 to stay fast)
        try {
            for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
                 it != fs::recursive_directory_iterator(); ++it) {
                if (ec) break;
                if (it.depth() > 3) {
                    it.pop();
                    continue;
                }
                if (!it->is_directory(ec)) continue;

                std::string sub_path = it->path().string();
                std::string fname = it->path().filename().string();
                if (!fname.empty() && fname[0] == '.' && fname != ".config") {
                    it.disable_recursion_pending();
                    continue;
                }

                if (visited_folders.find(sub_path) == visited_folders.end()) {
                    Album alb = scan_single_album(sub_path);
                    if (!alb.items.empty()) {
                        visited_folders.insert(sub_path);
                        albums.push_back(std::move(alb));
                    }
                }
            }
        } catch (...) {
            // Safe fallback on filesystem permission exceptions
        }
    }

    // Sort albums: albums with most items or newest items first
    std::sort(albums.begin(), albums.end(), [](const Album& a, const Album& b) {
        return a.items.size() > b.items.size();
    });

    return albums;
}

MediaItem MediaScanner::create_single_item(const std::string& file_path) {
    MediaItem item;
    std::error_code ec;
    fs::path p = fs::canonical(file_path, ec);
    if (ec) p = file_path;

    item.path = p.string();
    item.filename = p.filename().string();
    item.parent_path = p.parent_path().string();
    item.type = detect_type(item.path);
    item.file_size = fs::file_size(p, ec);
    item.formatted_size = format_file_size(item.file_size);
    item.modified_time = fs::last_write_time(p, ec);
    item.formatted_date = format_time(item.modified_time);
    return item;
}

} // namespace miqugallery
