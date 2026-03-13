/**
 * @file visualization_config.cpp
 * @brief Visualization configuration implementation with simple JSON parsing
 */

#include "visualization_config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace defect_detection {

namespace {
    std::string extractString(const std::string& json, const std::string& key, const std::string& def = "") {
        std::string pattern = "\"" + key + "\"\\s*:\\s*\"([^\"]+)\"";
        std::regex re(pattern);
        std::smatch match;
        if (std::regex_search(json, match, re) && match.size() > 1) {
            return match[1].str();
        }
        return def;
    }

    bool extractBool(const std::string& json, const std::string& key, bool def = false) {
        std::string pattern = "\"" + key + "\"\\s*:\\s*(true|false)";
        std::regex re(pattern);
        std::smatch match;
        if (std::regex_search(json, match, re) && match.size() > 1) {
            return match[1].str() == "true";
        }
        return def;
    }

    int extractInt(const std::string& json, const std::string& key, int def = 0) {
        std::string pattern = "\"" + key + "\"\\s*:\\s*(-?\\d+)";
        std::regex re(pattern);
        std::smatch match;
        if (std::regex_search(json, match, re) && match.size() > 1) {
            return std::stoi(match[1].str());
        }
        return def;
    }

    double extractDouble(const std::string& json, const std::string& key, double def = 0.0) {
        std::string pattern = "\"" + key + "\"\\s*:\\s*(-?\\d+\\.?\\d*)";
        std::regex re(pattern);
        std::smatch match;
        if (std::regex_search(json, match, re) && match.size() > 1) {
            return std::stod(match[1].str());
        }
        return def;
    }

    cv::Scalar extractColor(const std::string& json, const std::string& key, cv::Scalar def = cv::Scalar(0,0,0)) {
        std::string pattern = "\"" + key + "\"\\s*:\\s*\\[\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*\\]";
        std::regex re(pattern);
        std::smatch match;
        if (std::regex_search(json, match, re) && match.size() > 3) {
            return cv::Scalar(std::stoi(match[1].str()),
                              std::stoi(match[2].str()),
                              std::stoi(match[3].str()));
        }
        return def;
    }

    std::string extractSection(const std::string& json, const std::string& key) {
        size_t start = json.find("\"" + key + "\"");
        if (start == std::string::npos) return "";
        start = json.find('{', start);
        if (start == std::string::npos) return "";
        int depth = 1;
        size_t end = start + 1;
        while (end < json.size() && depth > 0) {
            if (json[end] == '{') depth++;
            else if (json[end] == '}') depth--;
            end++;
        }
        return json.substr(start, end - start);
    }
}

bool VisualizationConfig::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadFromString(buffer.str());
}

bool VisualizationConfig::loadFromString(const std::string& json_str) {
    try {
        std::string viz = extractSection(json_str, "visualization");
        if (viz.empty()) return false;

        enabled = extractBool(viz, "enabled", true);
        output_directory = extractString(viz, "output_directory", "output");
        std::string mode = extractString(viz, "output_mode", "both");
        if (mode == "file") output_mode = OutputMode::FILE_ONLY;
        else if (mode == "display") output_mode = OutputMode::DISPLAY_ONLY;
        else output_mode = OutputMode::BOTH;

        std::string roi_sec = extractSection(viz, "roi_boxes");
        roi_boxes.enabled = extractBool(roi_sec, "enabled", true);
        roi_boxes.pass_color = extractColor(roi_sec, "pass_color", cv::Scalar(0,255,0));
        roi_boxes.fail_color = extractColor(roi_sec, "fail_color", cv::Scalar(0,0,255));
        roi_boxes.line_thickness = extractInt(roi_sec, "line_thickness", 2);

        std::string def_sec = extractSection(viz, "defect_markers");
        defect_markers.enabled = extractBool(def_sec, "enabled", true);
        defect_markers.color = extractColor(def_sec, "color", cv::Scalar(255,0,255));
        defect_markers.line_thickness = extractInt(def_sec, "line_thickness", 1);
        defect_markers.fill_alpha = (float)extractDouble(def_sec, "fill_alpha", 0.3);

        std::string sim_sec = extractSection(viz, "similarity_scores");
        similarity_scores.enabled = extractBool(sim_sec, "enabled", true);
        similarity_scores.font_scale = extractDouble(sim_sec, "font_scale", 0.6);
        similarity_scores.font_thickness = extractInt(sim_sec, "font_thickness", 1);
        similarity_scores.text_color = extractColor(sim_sec, "text_color", cv::Scalar(255,255,255));
        similarity_scores.background_enabled = extractBool(sim_sec, "background_enabled", true);
        similarity_scores.background_color = extractColor(sim_sec, "background_color", cv::Scalar(0,0,0));

        std::string proc_sec = extractSection(viz, "processing_info");
        processing_info.enabled = extractBool(proc_sec, "enabled", true);
        processing_info.show_time = extractBool(proc_sec, "show_time", true);
        processing_info.show_overall_status = extractBool(proc_sec, "show_overall_status", true);
        processing_info.show_defect_count = extractBool(proc_sec, "show_defect_count", true);
        processing_info.position = extractString(proc_sec, "position", "top_left");

        std::string diff_sec = extractSection(viz, "difference_map");
        difference_map.enabled = extractBool(diff_sec, "enabled", false);
        difference_map.colormap = extractString(diff_sec, "colormap", "jet");
        difference_map.alpha = (float)extractDouble(diff_sec, "alpha", 0.5);

        std::string sbs_sec = extractSection(viz, "side_by_side");
        side_by_side.enabled = extractBool(sbs_sec, "enabled", false);
        side_by_side.template_label = extractString(sbs_sec, "template_label", "Template");
        side_by_side.test_label = extractString(sbs_sec, "test_label", "Test Image");

        std::string save_sec = extractSection(viz, "save_options");
        save_options.format = extractString(save_sec, "format", "jpg");
        save_options.quality = extractInt(save_sec, "quality", 95);
        save_options.filename_prefix = extractString(save_sec, "filename_prefix", "result_");
        save_options.include_timestamp = extractBool(save_sec, "include_timestamp", true);
        save_options.save_failed_only = extractBool(save_sec, "save_failed_only", false);

        std::string disp_sec = extractSection(viz, "display_options");
        display_options.window_name = extractString(disp_sec, "window_name", "Defect Detection Result");
        display_options.auto_resize = extractBool(disp_sec, "auto_resize", true);
        display_options.max_display_width = extractInt(disp_sec, "max_display_width", 1920);
        display_options.max_display_height = extractInt(disp_sec, "max_display_height", 1080);
        display_options.wait_key_ms = extractInt(disp_sec, "wait_key_ms", 0);

        // 解析二值图像检测配置（从detection部分）
        std::string det_sec = extractSection(json_str, "detection");
        std::string bin_sec = extractSection(det_sec, "binary_detection");
        if (!bin_sec.empty()) {
            binary_detection.enabled = extractBool(bin_sec, "enabled", false);
            binary_detection.auto_detect_binary = extractBool(bin_sec, "auto_detect_binary", true);
            binary_detection.noise_filter_size = extractInt(bin_sec, "noise_filter_size", 2);
            binary_detection.edge_tolerance_pixels = extractInt(bin_sec, "edge_tolerance_pixels", 2);
            binary_detection.edge_diff_ignore_ratio = (float)extractDouble(bin_sec, "edge_diff_ignore_ratio", 0.05);
            binary_detection.min_significant_area = extractInt(bin_sec, "min_significant_area", 20);
            binary_detection.area_diff_threshold = (float)extractDouble(bin_sec, "area_diff_threshold", 0.001);
            binary_detection.overall_similarity_threshold = (float)extractDouble(bin_sec, "overall_similarity_threshold", 0.95);
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool VisualizationConfig::saveToFile(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    auto colorToStr = [](const cv::Scalar& c) {
        return "[" + std::to_string((int)c[0]) + ", " +
               std::to_string((int)c[1]) + ", " +
               std::to_string((int)c[2]) + "]";
    };

    file << "{\n";
    file << "    \"visualization\": {\n";
    file << "        \"enabled\": " << (enabled ? "true" : "false") << ",\n";
    std::string mode_str = output_mode == OutputMode::FILE_ONLY ? "file" :
                           output_mode == OutputMode::DISPLAY_ONLY ? "display" : "both";
    file << "        \"output_mode\": \"" << mode_str << "\",\n";
    file << "        \"output_directory\": \"" << output_directory << "\",\n";
    file << "        \"elements\": {\n";
    file << "            \"roi_boxes\": { \"enabled\": " << (roi_boxes.enabled ? "true" : "false");
    file << ", \"pass_color\": " << colorToStr(roi_boxes.pass_color);
    file << ", \"fail_color\": " << colorToStr(roi_boxes.fail_color);
    file << ", \"line_thickness\": " << roi_boxes.line_thickness << " },\n";
    file << "            \"defect_markers\": { \"enabled\": " << (defect_markers.enabled ? "true" : "false");
    file << ", \"color\": " << colorToStr(defect_markers.color) << " },\n";
    file << "            \"similarity_scores\": { \"enabled\": " << (similarity_scores.enabled ? "true" : "false") << " },\n";
    file << "            \"processing_info\": { \"enabled\": " << (processing_info.enabled ? "true" : "false") << " },\n";
    file << "            \"difference_map\": { \"enabled\": " << (difference_map.enabled ? "true" : "false") << " },\n";
    file << "            \"side_by_side\": { \"enabled\": " << (side_by_side.enabled ? "true" : "false") << " }\n";
    file << "        }\n";
    file << "    }\n";
    file << "}\n";
    return true;
}

VisualizationConfig VisualizationConfig::getDefault() {
    return VisualizationConfig();
}

} // namespace defect_detection

