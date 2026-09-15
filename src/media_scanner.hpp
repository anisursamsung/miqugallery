#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace miqugallery {

enum class MediaType {
    Image,
    Video,
    Unknown
};

struct MediaItem {
    std::string path;
    std::string filename;
    std::string parent_path;
    MediaType type = MediaType::Image;
    uintmax_t file_size = 0;
    std::string formatted_size;
    std::string formatted_date;
    std::filesystem::file_time_type modified_time;
};

struct Album {
    std::string path;
    std::string name;
    std::string cover_path;
    std::vector<MediaItem> items;
    size_t photo_count = 0;
    size_t video_count = 0;

    std::string get_summary_text() const;
};

class MediaScanner {
public:
    static bool is_image_extension(const std::string& ext);
    static bool is_video_extension(const std::string& ext);
    static MediaType detect_type(const std::string& path);

    static std::string format_file_size(uintmax_t bytes);
    static std::string format_time(std::filesystem::file_time_type ftime);

    // Scans given directory or default media directories (~/Pictures, ~/Videos, etc.)
    static std::vector<Album> scan_albums(const std::vector<std::string>& search_paths = {});

    // Scans a single directory for media items
    static Album scan_single_album(const std::string& folder_path);

    // Creates a single MediaItem directly from a specific file path
    static MediaItem create_single_item(const std::string& file_path);
};

} // namespace miqugallery
