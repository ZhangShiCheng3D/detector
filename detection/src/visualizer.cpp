/**
 * @file visualizer.cpp
 * @brief Comprehensive visualization module implementation
 */

#include "visualizer.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace defect_detection {

Visualizer::Visualizer() : config_(VisualizationConfig::getDefault()) {}

Visualizer::Visualizer(const VisualizationConfig& config) : config_(config) {}

Visualizer::~Visualizer() {}

void Visualizer::setConfig(const VisualizationConfig& config) {
    config_ = config;
}

const VisualizationConfig& Visualizer::getConfig() const {
    return config_;
}

bool Visualizer::loadConfig(const std::string& filepath) {
    return config_.loadFromFile(filepath);
}

cv::Mat Visualizer::visualize(const cv::Mat& test_image,
                               const DetectionResult& result,
                               const std::vector<ROIRegion>& rois) {
    if (!config_.enabled || test_image.empty()) {
        return test_image.clone();
    }

    cv::Mat output = test_image.clone();

    // Draw ROI boxes
    if (config_.roi_boxes.enabled) {
        drawROIBoxes(output, rois, result.roi_results);
    }

    // Draw defect markers with enhanced annotations
    if (config_.defect_markers.enabled) {
        for (size_t i = 0; i < rois.size() && i < result.roi_results.size(); ++i) {
            drawDefectMarkers(output, rois[i], result.roi_results[i].defects);
        }
    }

    // Draw similarity scores
    if (config_.similarity_scores.enabled) {
        drawSimilarityScores(output, rois, result.roi_results);
    }

    // Draw legend
    if (config_.legend.enabled && result.total_defect_count > 0) {
        drawLegend(output, result);
    }

    // Draw processing info and defect summary
    if (config_.processing_info.enabled) {
        drawProcessingInfo(output, result);
        if (result.total_defect_count > 0) {
            drawDefectSummary(output, result, rois);
        }
    }

    return output;
}

cv::Mat Visualizer::createComparison(const cv::Mat& template_image,
                                      const cv::Mat& test_image,
                                      const DetectionResult& result,
                                      const std::vector<ROIRegion>& rois) {
    cv::Mat template_viz = template_image.clone();
    cv::Mat test_viz = visualize(test_image, result, rois);

    // Add labels
    cv::putText(template_viz, config_.side_by_side.template_label,
                cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0,
                cv::Scalar(0, 255, 0), 2);
    cv::putText(test_viz, config_.side_by_side.test_label,
                cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0,
                cv::Scalar(0, 255, 0), 2);

    // Ensure same size
    if (template_viz.size() != test_viz.size()) {
        cv::resize(template_viz, template_viz, test_viz.size());
    }

    cv::Mat combined;
    cv::hconcat(template_viz, test_viz, combined);
    return combined;
}

cv::Mat Visualizer::createDifferenceMap(const cv::Mat& template_roi,
                                         const cv::Mat& test_roi) {
    cv::Mat gray_template, gray_test;
    if (template_roi.channels() == 3) {
        cv::cvtColor(template_roi, gray_template, cv::COLOR_BGR2GRAY);
    } else {
        gray_template = template_roi;
    }
    if (test_roi.channels() == 3) {
        cv::cvtColor(test_roi, gray_test, cv::COLOR_BGR2GRAY);
    } else {
        gray_test = test_roi;
    }

    // Resize if needed
    if (gray_template.size() != gray_test.size()) {
        cv::resize(gray_test, gray_test, gray_template.size());
    }

    cv::Mat diff;
    cv::absdiff(gray_template, gray_test, diff);

    return applyColormap(diff);
}

std::string Visualizer::saveVisualization(const cv::Mat& image,
                                           const std::string& base_filename) {
    if (image.empty()) return "";

    // Create output directory if needed
    if (!fs::exists(config_.output_directory)) {
        fs::create_directories(config_.output_directory);
    }

    std::string filename = config_.save_options.filename_prefix + base_filename;
    if (config_.save_options.include_timestamp) {
        filename += "_" + generateTimestamp();
    }
    filename += "." + config_.save_options.format;

    std::string full_path = config_.output_directory + "/" + filename;

    std::vector<int> params;
    if (config_.save_options.format == "jpg" || config_.save_options.format == "jpeg") {
        params.push_back(cv::IMWRITE_JPEG_QUALITY);
        params.push_back(config_.save_options.quality);
    } else if (config_.save_options.format == "png") {
        params.push_back(cv::IMWRITE_PNG_COMPRESSION);
        params.push_back(9 - config_.save_options.quality / 12);
    }

    if (cv::imwrite(full_path, image, params)) {
        return full_path;
    }
    return "";
}

int Visualizer::displayVisualization(const cv::Mat& image) {
    if (image.empty()) return -1;

    cv::Mat display_img = resizeForDisplay(image);
    cv::imshow(config_.display_options.window_name, display_img);
    return cv::waitKey(config_.display_options.wait_key_ms);
}

cv::Mat Visualizer::processAndOutput(const cv::Mat& test_image,
                                      const DetectionResult& result,
                                      const std::vector<ROIRegion>& rois,
                                      const std::string& base_filename) {
    cv::Mat viz = visualize(test_image, result, rois);

    // Check if we should skip saving (save_failed_only mode)
    bool should_save = true;
    if (config_.save_options.save_failed_only && result.overall_passed) {
        should_save = false;
    }

    // Save to file if configured
    if (should_save && (config_.output_mode == OutputMode::FILE_ONLY ||
                        config_.output_mode == OutputMode::BOTH)) {
        saveVisualization(viz, base_filename);
    }

    // Display if configured
    if (config_.output_mode == OutputMode::DISPLAY_ONLY ||
        config_.output_mode == OutputMode::BOTH) {
        displayVisualization(viz);
    }

    return viz;
}

void Visualizer::drawROIBoxes(cv::Mat& image,
                               const std::vector<ROIRegion>& rois,
                               const std::vector<ROIResult>& results) {
    for (size_t i = 0; i < rois.size() && i < results.size(); ++i) {
        const auto& roi = rois[i];
        const auto& result = results[i];

        cv::Scalar color = result.passed ? config_.roi_boxes.pass_color
                                          : config_.roi_boxes.fail_color;
        cv::rectangle(image, roi.bounds.toCvRect(), color, config_.roi_boxes.line_thickness);
    }
}

void Visualizer::drawDefectMarkers(cv::Mat& image,
                                    const ROIRegion& roi,
                                    const std::vector<Defect>& defects) {
    cv::Point offset(roi.bounds.x, roi.bounds.y);

    for (size_t i = 0; i < defects.size(); ++i) {
        drawSingleDefect(image, defects[i], offset, static_cast<int>(i));
    }
}

void Visualizer::drawSingleDefect(cv::Mat& image,
                                   const Defect& defect,
                                   const cv::Point& offset,
                                   int defect_index) {
    cv::Rect defect_rect = defect.location.toCvRect();
    defect_rect.x += offset.x;
    defect_rect.y += offset.y;

    // Ensure defect rect is within image bounds
    defect_rect.x = std::max(0, defect_rect.x);
    defect_rect.y = std::max(0, defect_rect.y);
    defect_rect.width = std::min(defect_rect.width, image.cols - defect_rect.x);
    defect_rect.height = std::min(defect_rect.height, image.rows - defect_rect.y);

    if (defect_rect.width <= 0 || defect_rect.height <= 0) return;

    // Get color based on defect type
    cv::Scalar color = getDefectColor(defect.type);

    // Draw filled rectangle with alpha overlay
    if (config_.defect_markers.fill_alpha > 0) {
        cv::Mat overlay = image.clone();
        cv::rectangle(overlay, defect_rect, color, cv::FILLED);
        cv::addWeighted(overlay, config_.defect_markers.fill_alpha,
                       image, 1 - config_.defect_markers.fill_alpha, 0, image);
    }

    // Draw outline with thicker border
    cv::rectangle(image, defect_rect, color, config_.defect_markers.line_thickness);

    // Draw corner markers for emphasis
    int corner_len = std::min(15, std::min(defect_rect.width, defect_rect.height) / 2);
    cv::Point tl = defect_rect.tl();
    cv::Point br = defect_rect.br();
    // Top-left corner
    cv::line(image, tl, cv::Point(tl.x + corner_len, tl.y), color, config_.defect_markers.line_thickness + 1);
    cv::line(image, tl, cv::Point(tl.x, tl.y + corner_len), color, config_.defect_markers.line_thickness + 1);
    // Top-right corner
    cv::line(image, cv::Point(br.x, tl.y), cv::Point(br.x - corner_len, tl.y), color, config_.defect_markers.line_thickness + 1);
    cv::line(image, cv::Point(br.x, tl.y), cv::Point(br.x, tl.y + corner_len), color, config_.defect_markers.line_thickness + 1);
    // Bottom-left corner
    cv::line(image, cv::Point(tl.x, br.y), cv::Point(tl.x + corner_len, br.y), color, config_.defect_markers.line_thickness + 1);
    cv::line(image, cv::Point(tl.x, br.y), cv::Point(tl.x, br.y - corner_len), color, config_.defect_markers.line_thickness + 1);
    // Bottom-right corner
    cv::line(image, br, cv::Point(br.x - corner_len, br.y), color, config_.defect_markers.line_thickness + 1);
    cv::line(image, br, cv::Point(br.x, br.y - corner_len), color, config_.defect_markers.line_thickness + 1);

    // Draw arrow pointing to defect
    if (config_.defect_markers.show_arrows) {
        drawDefectArrow(image, defect_rect, color);
    }

    // Draw label with defect information
    if (config_.defect_markers.show_labels) {
        std::vector<std::string> label_lines;

        // Line 1: Defect index and type
        std::stringstream ss1;
        ss1 << "#" << (defect_index + 1) << " ";
        if (defect.type == "black_spot") {
            ss1 << "Black Spot";
        } else if (defect.type == "white_spot") {
            ss1 << "White Spot";
        } else {
            ss1 << defect.type;
        }
        label_lines.push_back(ss1.str());

        // Line 2: Location
        std::stringstream ss2;
        ss2 << "Pos: (" << defect_rect.x << "," << defect_rect.y << ")";
        label_lines.push_back(ss2.str());

        // Line 3: Size info
        if (config_.defect_markers.show_size_info) {
            std::stringstream ss3;
            ss3 << "Area: " << static_cast<int>(defect.area) << "px";
            label_lines.push_back(ss3.str());
        }

        // Line 4: Severity
        if (config_.defect_markers.show_severity && defect.severity > 0) {
            std::stringstream ss4;
            ss4 << "Severity: " << getSeverityLabel(defect.severity);
            label_lines.push_back(ss4.str());
        }

        // Calculate label position (below the defect)
        int line_height = static_cast<int>(18 * config_.defect_markers.label_font_scale);
        int label_y = defect_rect.y + defect_rect.height + 5;

        // If label would go below image, place it above
        if (label_y + line_height * static_cast<int>(label_lines.size()) > image.rows) {
            label_y = defect_rect.y - line_height * static_cast<int>(label_lines.size()) - 5;
        }

        // Draw label background and text
        for (size_t i = 0; i < label_lines.size(); ++i) {
            int baseline;
            cv::Size text_size = cv::getTextSize(label_lines[i], cv::FONT_HERSHEY_SIMPLEX,
                                                  config_.defect_markers.label_font_scale, 1, &baseline);
            cv::Point text_pos(defect_rect.x, label_y + static_cast<int>(i) * line_height);

            // Ensure text is within bounds
            if (text_pos.x + text_size.width > image.cols) {
                text_pos.x = image.cols - text_size.width - 5;
            }
            if (text_pos.x < 5) text_pos.x = 5;
            if (text_pos.y < text_size.height + 5) text_pos.y = text_size.height + 5;
            if (text_pos.y > image.rows - 5) continue;

            // Draw background
            cv::rectangle(image,
                         cv::Point(text_pos.x - 2, text_pos.y - text_size.height - 2),
                         cv::Point(text_pos.x + text_size.width + 2, text_pos.y + baseline + 2),
                         cv::Scalar(0, 0, 0), cv::FILLED);

            // Draw text
            cv::putText(image, label_lines[i], text_pos, cv::FONT_HERSHEY_SIMPLEX,
                        config_.defect_markers.label_font_scale, color, 1);
        }
    }
}

void Visualizer::drawDefectArrow(cv::Mat& image,
                                  const cv::Rect& defect_rect,
                                  const cv::Scalar& color) {
    // Calculate arrow position (pointing from outside to center of defect)
    cv::Point center(defect_rect.x + defect_rect.width / 2,
                     defect_rect.y + defect_rect.height / 2);

    int arrow_len = config_.defect_markers.arrow_length;
    cv::Point arrow_start, arrow_end;

    // Determine best arrow direction based on defect position
    if (defect_rect.x > image.cols / 2) {
        // Defect on right side - arrow from left
        arrow_start = cv::Point(defect_rect.x - arrow_len, center.y);
        arrow_end = cv::Point(defect_rect.x - 3, center.y);
    } else {
        // Defect on left side - arrow from right
        arrow_start = cv::Point(defect_rect.x + defect_rect.width + arrow_len, center.y);
        arrow_end = cv::Point(defect_rect.x + defect_rect.width + 3, center.y);
    }

    // Ensure arrow is within image bounds
    arrow_start.x = std::max(5, std::min(arrow_start.x, image.cols - 5));
    arrow_start.y = std::max(5, std::min(arrow_start.y, image.rows - 5));
    arrow_end.x = std::max(5, std::min(arrow_end.x, image.cols - 5));
    arrow_end.y = std::max(5, std::min(arrow_end.y, image.rows - 5));

    // Draw arrow
    cv::arrowedLine(image, arrow_start, arrow_end, color, 2, cv::LINE_AA, 0, 0.3);
}

cv::Scalar Visualizer::getDefectColor(const std::string& defect_type) const {
    if (defect_type == "black_spot" || defect_type == "black_on_white") {
        return config_.defect_markers.black_spot_color;
    } else if (defect_type == "white_spot" || defect_type == "white_on_black") {
        return config_.defect_markers.white_spot_color;
    }
    return config_.defect_markers.color;
}

std::string Visualizer::getSeverityLabel(float severity) const {
    if (severity >= 0.8f) return "HIGH";
    if (severity >= 0.5f) return "MEDIUM";
    if (severity >= 0.2f) return "LOW";
    return "MINOR";
}

void Visualizer::drawSimilarityScores(cv::Mat& image,
                                       const std::vector<ROIRegion>& rois,
                                       const std::vector<ROIResult>& results) {
    for (size_t i = 0; i < rois.size() && i < results.size(); ++i) {
        const auto& roi = rois[i];
        const auto& result = results[i];

        // Build multi-line annotation text
        std::vector<std::string> lines;

        // Line 1: ROI ID and similarity percentage
        std::stringstream ss1;
        ss1 << "ROI" << roi.id << ": " << std::fixed << std::setprecision(1)
           << (result.similarity * 100) << "%";
        lines.push_back(ss1.str());

        // Line 2: PASS/FAIL status and defect count
        std::stringstream ss2;
        ss2 << (result.passed ? "PASS" : "FAIL");
        if (!result.defects.empty()) {
            ss2 << " [" << result.defects.size() << " defects]";
        }
        lines.push_back(ss2.str());

        // Determine text color based on pass/fail
        cv::Scalar color = result.passed ? config_.roi_boxes.pass_color
                                          : config_.roi_boxes.fail_color;

        // Calculate text position - try above ROI first, then inside if no room
        int line_height = static_cast<int>(25 * config_.similarity_scores.font_scale);
        int total_height = line_height * static_cast<int>(lines.size());

        cv::Point text_pos(roi.bounds.x, roi.bounds.y - total_height - 5);

        // If text would go above image, place it inside the ROI at the top
        if (text_pos.y < 5) {
            text_pos.y = roi.bounds.y + line_height + 5;
        }

        // Ensure text doesn't go beyond right edge
        int max_text_width = 0;
        for (const auto& line : lines) {
            int baseline;
            cv::Size text_size = cv::getTextSize(line, cv::FONT_HERSHEY_SIMPLEX,
                                                  config_.similarity_scores.font_scale,
                                                  config_.similarity_scores.font_thickness,
                                                  &baseline);
            max_text_width = std::max(max_text_width, text_size.width);
        }
        if (text_pos.x + max_text_width > image.cols - 5) {
            text_pos.x = image.cols - max_text_width - 5;
        }
        if (text_pos.x < 5) text_pos.x = 5;

        // Draw each line with background
        for (size_t j = 0; j < lines.size(); ++j) {
            cv::Point line_pos(text_pos.x, text_pos.y + static_cast<int>(j) * line_height);

            // Draw background rectangle
            if (config_.similarity_scores.background_enabled) {
                int baseline;
                cv::Size text_size = cv::getTextSize(lines[j], cv::FONT_HERSHEY_SIMPLEX,
                                                      config_.similarity_scores.font_scale,
                                                      config_.similarity_scores.font_thickness,
                                                      &baseline);
                cv::rectangle(image,
                             cv::Point(line_pos.x - 2, line_pos.y - text_size.height - 2),
                             cv::Point(line_pos.x + text_size.width + 4, line_pos.y + baseline + 2),
                             config_.similarity_scores.background_color, cv::FILLED);
            }

            cv::putText(image, lines[j], line_pos, cv::FONT_HERSHEY_SIMPLEX,
                        config_.similarity_scores.font_scale, color,
                        config_.similarity_scores.font_thickness);
        }
    }
}

void Visualizer::drawProcessingInfo(cv::Mat& image, const DetectionResult& result) {
    std::vector<std::string> lines;

    if (config_.processing_info.show_overall_status) {
        lines.push_back(result.overall_passed ? "Status: PASS" : "Status: FAIL");
    }

    if (config_.processing_info.show_time) {
        std::stringstream ss;
        ss << "Time: " << std::fixed << std::setprecision(2) << result.processing_time_ms << "ms";
        lines.push_back(ss.str());
    }

    if (config_.processing_info.show_defect_count) {
        lines.push_back("Defects: " + std::to_string(result.total_defect_count));
    }

    // Determine position
    int x = 10, y = 30;
    if (config_.processing_info.position == "top_right") {
        x = image.cols - 200;
    } else if (config_.processing_info.position == "bottom_left") {
        y = image.rows - 30 * (int)lines.size();
    } else if (config_.processing_info.position == "bottom_right") {
        x = image.cols - 200;
        y = image.rows - 30 * (int)lines.size();
    }

    cv::Scalar status_color = result.overall_passed ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
    for (size_t i = 0; i < lines.size(); ++i) {
        cv::Scalar color = (i == 0) ? status_color : cv::Scalar(255, 255, 255);
        cv::putText(image, lines[i], cv::Point(x, y + (int)i * 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.8, color, 2);
    }
}

void Visualizer::drawLegend(cv::Mat& image, const DetectionResult& result) {
    // Legend items
    std::vector<std::pair<std::string, cv::Scalar>> items;
    items.push_back({"ROI Pass", config_.roi_boxes.pass_color});
    items.push_back({"ROI Fail", config_.roi_boxes.fail_color});

    // Check if we have any defects to show type colors
    bool has_black_spots = false;
    bool has_white_spots = false;
    for (const auto& roi_result : result.roi_results) {
        for (const auto& defect : roi_result.defects) {
            if (defect.type == "black_spot" || defect.type == "black_on_white") {
                has_black_spots = true;
            } else if (defect.type == "white_spot" || defect.type == "white_on_black") {
                has_white_spots = true;
            }
        }
    }

    if (has_black_spots) {
        items.push_back({"Black Spot", config_.defect_markers.black_spot_color});
    }
    if (has_white_spots) {
        items.push_back({"White Spot", config_.defect_markers.white_spot_color});
    }

    // Calculate legend dimensions
    int padding = 10;
    int item_height = 25;
    int color_box_size = 15;
    int max_text_width = 0;

    for (const auto& item : items) {
        int baseline;
        cv::Size text_size = cv::getTextSize(item.first, cv::FONT_HERSHEY_SIMPLEX,
                                              config_.legend.font_scale, 1, &baseline);
        max_text_width = std::max(max_text_width, text_size.width);
    }

    int legend_width = max_text_width + color_box_size + padding * 3;
    int legend_height = static_cast<int>(items.size()) * item_height + padding * 2;

    // Determine position
    int x, y;
    if (config_.legend.position == "top_left") {
        x = padding;
        y = padding;
    } else if (config_.legend.position == "top_right") {
        x = image.cols - legend_width - padding;
        y = padding;
    } else if (config_.legend.position == "bottom_left") {
        x = padding;
        y = image.rows - legend_height - padding;
    } else { // bottom_right (default)
        x = image.cols - legend_width - padding;
        y = image.rows - legend_height - padding;
    }

    // Draw legend background
    cv::Mat overlay = image.clone();
    cv::rectangle(overlay, cv::Point(x, y),
                  cv::Point(x + legend_width, y + legend_height),
                  config_.legend.background_color, cv::FILLED);
    cv::addWeighted(overlay, config_.legend.background_alpha,
                   image, 1 - config_.legend.background_alpha, 0, image);

    // Draw border
    cv::rectangle(image, cv::Point(x, y),
                  cv::Point(x + legend_width, y + legend_height),
                  cv::Scalar(255, 255, 255), 1);

    // Draw legend title
    cv::putText(image, "Legend", cv::Point(x + padding, y + 18),
                cv::FONT_HERSHEY_SIMPLEX, config_.legend.font_scale + 0.1,
                cv::Scalar(255, 255, 255), 1);

    // Draw each item
    for (size_t i = 0; i < items.size(); ++i) {
        int item_y = y + padding + 20 + static_cast<int>(i) * item_height;

        // Draw color box
        cv::rectangle(image,
                      cv::Point(x + padding, item_y),
                      cv::Point(x + padding + color_box_size, item_y + color_box_size),
                      items[i].second, cv::FILLED);
        cv::rectangle(image,
                      cv::Point(x + padding, item_y),
                      cv::Point(x + padding + color_box_size, item_y + color_box_size),
                      cv::Scalar(255, 255, 255), 1);

        // Draw text
        cv::putText(image, items[i].first,
                    cv::Point(x + padding + color_box_size + 8, item_y + 12),
                    cv::FONT_HERSHEY_SIMPLEX, config_.legend.font_scale,
                    cv::Scalar(255, 255, 255), 1);
    }
}

void Visualizer::drawDefectSummary(cv::Mat& image,
                                    const DetectionResult& result,
                                    const std::vector<ROIRegion>& rois) {
    // Count defects by type
    int black_spots = 0;
    int white_spots = 0;
    int other_defects = 0;
    double total_area = 0;

    for (size_t i = 0; i < result.roi_results.size(); ++i) {
        for (const auto& defect : result.roi_results[i].defects) {
            if (defect.type == "black_spot" || defect.type == "black_on_white") {
                black_spots++;
            } else if (defect.type == "white_spot" || defect.type == "white_on_black") {
                white_spots++;
            } else {
                other_defects++;
            }
            total_area += defect.area;
        }
    }

    // Build summary lines
    std::vector<std::string> lines;
    lines.push_back("=== Defect Summary ===");
    lines.push_back("Total: " + std::to_string(result.total_defect_count) + " defects");

    if (black_spots > 0) {
        lines.push_back("  Black spots: " + std::to_string(black_spots));
    }
    if (white_spots > 0) {
        lines.push_back("  White spots: " + std::to_string(white_spots));
    }
    if (other_defects > 0) {
        lines.push_back("  Other: " + std::to_string(other_defects));
    }

    std::stringstream ss;
    ss << "Total area: " << std::fixed << std::setprecision(0) << total_area << " px";
    lines.push_back(ss.str());

    // Count failed ROIs
    int failed_rois = 0;
    for (const auto& roi_result : result.roi_results) {
        if (!roi_result.passed) failed_rois++;
    }
    lines.push_back("Failed ROIs: " + std::to_string(failed_rois) + "/" + std::to_string(rois.size()));

    // Draw summary panel in top-right corner
    int padding = 10;
    int line_height = 22;
    int panel_height = static_cast<int>(lines.size()) * line_height + padding * 2;
    int panel_width = 200;
    int x = image.cols - panel_width - padding;
    int y = padding;

    // Draw background
    cv::Mat overlay = image.clone();
    cv::rectangle(overlay, cv::Point(x, y),
                  cv::Point(x + panel_width, y + panel_height),
                  cv::Scalar(40, 40, 40), cv::FILLED);
    cv::addWeighted(overlay, 0.85, image, 0.15, 0, image);

    // Draw border
    cv::rectangle(image, cv::Point(x, y),
                  cv::Point(x + panel_width, y + panel_height),
                  cv::Scalar(0, 0, 255), 2);

    // Draw lines
    for (size_t i = 0; i < lines.size(); ++i) {
        cv::Scalar color = (i == 0) ? cv::Scalar(0, 165, 255) : cv::Scalar(255, 255, 255);
        if (lines[i].find("Black") != std::string::npos) {
            color = config_.defect_markers.black_spot_color;
        } else if (lines[i].find("White") != std::string::npos) {
            color = config_.defect_markers.white_spot_color;
        }
        cv::putText(image, lines[i],
                    cv::Point(x + padding, y + padding + static_cast<int>(i) * line_height + 15),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
    }
}

cv::Mat Visualizer::applyColormap(const cv::Mat& diff_image) {
    cv::Mat colored;
    int colormap = cv::COLORMAP_JET;
    if (config_.difference_map.colormap == "hot") {
        colormap = cv::COLORMAP_HOT;
    } else if (config_.difference_map.colormap == "cool") {
        colormap = cv::COLORMAP_COOL;
    } else if (config_.difference_map.colormap == "rainbow") {
        colormap = cv::COLORMAP_RAINBOW;
    }
    cv::applyColorMap(diff_image, colored, colormap);
    return colored;
}

std::string Visualizer::generateTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    return ss.str();
}

cv::Mat Visualizer::resizeForDisplay(const cv::Mat& image) {
    if (!config_.display_options.auto_resize) {
        return image;
    }

    int max_w = config_.display_options.max_display_width;
    int max_h = config_.display_options.max_display_height;

    if (image.cols <= max_w && image.rows <= max_h) {
        return image;
    }

    double scale = std::min((double)max_w / image.cols, (double)max_h / image.rows);
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(), scale, scale);
    return resized;
}

} // namespace defect_detection

