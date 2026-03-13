/**
 * @file roi_manager.cpp
 * @brief ROI区域管理器实现
 */

#include "roi_manager.h"
#include <algorithm>

namespace defect_detection {

ROIManager::ROIManager() : next_id_(0) {
}

ROIManager::~ROIManager() {
}

int ROIManager::addROI(const Rect& bounds, float threshold) {
    return addROI(bounds, threshold, DetectionParams());
}

int ROIManager::addROI(const Rect& bounds, float threshold, const DetectionParams& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ROIRegion roi;
    roi.id = next_id_++;
    roi.bounds = bounds;
    roi.similarity_threshold = threshold;
    roi.params = params;
    
    rois_.push_back(roi);
    id_to_index_[roi.id] = rois_.size() - 1;
    
    return roi.id;
}

bool ROIManager::removeROI(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = id_to_index_.find(id);
    if (it == id_to_index_.end()) {
        return false;
    }
    
    size_t index = it->second;
    rois_.erase(rois_.begin() + index);
    rebuildIndex();
    
    return true;
}

bool ROIManager::updateROI(int id, const Rect& new_bounds) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = id_to_index_.find(id);
    if (it == id_to_index_.end()) {
        return false;
    }
    
    rois_[it->second].bounds = new_bounds;
    return true;
}

bool ROIManager::setThreshold(int id, float threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = id_to_index_.find(id);
    if (it == id_to_index_.end()) {
        return false;
    }
    
    rois_[it->second].similarity_threshold = std::clamp(threshold, 0.0f, 1.0f);
    return true;
}

bool ROIManager::setParams(int id, const DetectionParams& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = id_to_index_.find(id);
    if (it == id_to_index_.end()) {
        return false;
    }
    
    rois_[it->second].params = params;
    return true;
}

std::vector<ROIRegion>& ROIManager::getAllROIs() {
    return rois_;
}

const std::vector<ROIRegion>& ROIManager::getAllROIs() const {
    return rois_;
}

ROIRegion* ROIManager::getROI(int id) {
    auto it = id_to_index_.find(id);
    if (it == id_to_index_.end()) {
        return nullptr;
    }
    return &rois_[it->second];
}

const ROIRegion* ROIManager::getROI(int id) const {
    auto it = id_to_index_.find(id);
    if (it == id_to_index_.end()) {
        return nullptr;
    }
    return &rois_[it->second];
}

size_t ROIManager::getROICount() const {
    return rois_.size();
}

void ROIManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    rois_.clear();
    id_to_index_.clear();
}

void ROIManager::applyTransform(const cv::Mat& affine_matrix) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (affine_matrix.empty() || affine_matrix.rows != 2 || affine_matrix.cols != 3) {
        return;
    }
    
    for (auto& roi : rois_) {
        // 变换四个角点
        std::vector<cv::Point2f> corners = {
            cv::Point2f((float)roi.bounds.x, (float)roi.bounds.y),
            cv::Point2f((float)(roi.bounds.x + roi.bounds.width), (float)roi.bounds.y),
            cv::Point2f((float)(roi.bounds.x + roi.bounds.width), (float)(roi.bounds.y + roi.bounds.height)),
            cv::Point2f((float)roi.bounds.x, (float)(roi.bounds.y + roi.bounds.height))
        };
        
        std::vector<cv::Point2f> transformed;
        cv::transform(corners, transformed, affine_matrix);
        
        // 计算新的边界框
        float min_x = transformed[0].x, max_x = transformed[0].x;
        float min_y = transformed[0].y, max_y = transformed[0].y;
        for (const auto& pt : transformed) {
            min_x = std::min(min_x, pt.x);
            max_x = std::max(max_x, pt.x);
            min_y = std::min(min_y, pt.y);
            max_y = std::max(max_y, pt.y);
        }
        
        roi.bounds.x = (int)min_x;
        roi.bounds.y = (int)min_y;
        roi.bounds.width = (int)(max_x - min_x);
        roi.bounds.height = (int)(max_y - min_y);
    }
}

void ROIManager::extractTemplates(const cv::Mat& template_image) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& roi : rois_) {
        cv::Rect cv_rect = roi.bounds.toCvRect();
        
        // 确保在图像范围内
        cv_rect.x = std::max(0, cv_rect.x);
        cv_rect.y = std::max(0, cv_rect.y);
        cv_rect.width = std::min(cv_rect.width, template_image.cols - cv_rect.x);
        cv_rect.height = std::min(cv_rect.height, template_image.rows - cv_rect.y);
        
        if (cv_rect.width > 0 && cv_rect.height > 0) {
            roi.template_image = template_image(cv_rect).clone();
        }
    }
}

bool ROIManager::validateBounds(int image_width, int image_height) const {
    for (const auto& roi : rois_) {
        if (roi.bounds.x < 0 || roi.bounds.y < 0 ||
            roi.bounds.x + roi.bounds.width > image_width ||
            roi.bounds.y + roi.bounds.height > image_height) {
            return false;
        }
    }
    return true;
}

void ROIManager::clipToImageBounds(int image_width, int image_height) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& roi : rois_) {
        roi.bounds.x = std::max(0, roi.bounds.x);
        roi.bounds.y = std::max(0, roi.bounds.y);
        roi.bounds.width = std::min(roi.bounds.width, image_width - roi.bounds.x);
        roi.bounds.height = std::min(roi.bounds.height, image_height - roi.bounds.y);
    }
}

void ROIManager::rebuildIndex() {
    id_to_index_.clear();
    for (size_t i = 0; i < rois_.size(); ++i) {
        id_to_index_[rois_[i].id] = i;
    }
}

} // namespace defect_detection

