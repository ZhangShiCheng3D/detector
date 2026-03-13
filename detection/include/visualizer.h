/**
 * @file visualizer.h
 * @brief Comprehensive visualization module for defect detection results
 */

#ifndef DEFECT_DETECTION_VISUALIZER_H
#define DEFECT_DETECTION_VISUALIZER_H

#include "types.h"
#include "visualization_config.h"
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace defect_detection {

/**
 * @brief Comprehensive visualizer for detection results
 */
class Visualizer {
public:
    Visualizer();
    explicit Visualizer(const VisualizationConfig& config);
    ~Visualizer();
    
    /**
     * @brief Set visualization configuration
     * @param config Configuration settings
     */
    void setConfig(const VisualizationConfig& config);
    
    /**
     * @brief Get current configuration
     * @return Current config
     */
    const VisualizationConfig& getConfig() const;
    
    /**
     * @brief Load configuration from file
     * @param filepath Path to config JSON file
     * @return true if successful
     */
    bool loadConfig(const std::string& filepath);
    
    /**
     * @brief Visualize detection results on image
     * @param test_image Original test image
     * @param result Detection result
     * @param rois ROI regions for reference
     * @return Visualized image
     */
    cv::Mat visualize(const cv::Mat& test_image,
                      const DetectionResult& result,
                      const std::vector<ROIRegion>& rois);
    
    /**
     * @brief Create side-by-side comparison
     * @param template_image Template image
     * @param test_image Test image
     * @param result Detection result
     * @param rois ROI regions
     * @return Combined comparison image
     */
    cv::Mat createComparison(const cv::Mat& template_image,
                             const cv::Mat& test_image,
                             const DetectionResult& result,
                             const std::vector<ROIRegion>& rois);
    
    /**
     * @brief Create difference map for ROI
     * @param template_roi Template ROI image
     * @param test_roi Test ROI image
     * @return Colorized difference map
     */
    cv::Mat createDifferenceMap(const cv::Mat& template_roi,
                                const cv::Mat& test_roi);
    
    /**
     * @brief Save visualization to file
     * @param image Visualization image
     * @param base_filename Base filename (without extension)
     * @return Full path of saved file, empty if failed
     */
    std::string saveVisualization(const cv::Mat& image,
                                  const std::string& base_filename);
    
    /**
     * @brief Display visualization in window
     * @param image Visualization image
     * @return Key pressed (for waitKey)
     */
    int displayVisualization(const cv::Mat& image);
    
    /**
     * @brief Process and output visualization based on config
     * @param test_image Test image
     * @param result Detection result
     * @param rois ROI regions
     * @param base_filename Base filename for saving
     * @return Visualization image
     */
    cv::Mat processAndOutput(const cv::Mat& test_image,
                             const DetectionResult& result,
                             const std::vector<ROIRegion>& rois,
                             const std::string& base_filename);

private:
    VisualizationConfig config_;

    // Draw ROI boundaries with pass/fail colors
    void drawROIBoxes(cv::Mat& image,
                      const std::vector<ROIRegion>& rois,
                      const std::vector<ROIResult>& results);

    // Draw defect markers with enhanced annotations
    void drawDefectMarkers(cv::Mat& image,
                           const ROIRegion& roi,
                           const std::vector<Defect>& defects);

    // Draw a single defect with full annotations
    void drawSingleDefect(cv::Mat& image,
                          const Defect& defect,
                          const cv::Point& offset,
                          int defect_index);

    // Draw arrow pointing to defect location
    void drawDefectArrow(cv::Mat& image,
                         const cv::Rect& defect_rect,
                         const cv::Scalar& color);

    // Draw similarity scores on ROIs
    void drawSimilarityScores(cv::Mat& image,
                              const std::vector<ROIRegion>& rois,
                              const std::vector<ROIResult>& results);

    // Draw processing info overlay
    void drawProcessingInfo(cv::Mat& image,
                            const DetectionResult& result);

    // Draw legend explaining color coding
    void drawLegend(cv::Mat& image,
                    const DetectionResult& result);

    // Draw defect summary panel
    void drawDefectSummary(cv::Mat& image,
                           const DetectionResult& result,
                           const std::vector<ROIRegion>& rois);

    // Get color for defect type
    cv::Scalar getDefectColor(const std::string& defect_type) const;

    // Get severity label
    std::string getSeverityLabel(float severity) const;

    // Apply colormap to difference image
    cv::Mat applyColormap(const cv::Mat& diff_image);

    // Generate timestamp string
    std::string generateTimestamp();

    // Resize image for display if needed
    cv::Mat resizeForDisplay(const cv::Mat& image);
};

} // namespace defect_detection

#endif // DEFECT_DETECTION_VISUALIZER_H

