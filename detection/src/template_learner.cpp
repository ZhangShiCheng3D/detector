/**
 * @file template_learner.cpp
 * @brief 一键学习模块实现
 */

#include "template_learner.h"
#include "template_matcher.h"

namespace defect_detection {

TemplateLearner::TemplateLearner()
    : roi_manager_(nullptr)
    , localization_(nullptr)
    , mode_(LearningMode::REPLACE)
    , fusion_alpha_(0.3f)
    , min_quality_threshold_(0.5f)
    , min_similarity_threshold_(0.7f)
{
}

TemplateLearner::~TemplateLearner() {
}

void TemplateLearner::setROIManager(ROIManager* roi_manager) {
    roi_manager_ = roi_manager;
}

void TemplateLearner::setLocalization(Localization* localization) {
    localization_ = localization;
}

LearningResult TemplateLearner::learnFromImage(const cv::Mat& new_image) {
    LearningResult result;
    
    if (new_image.empty()) {
        result.success = false;
        result.message = "Empty image provided";
        return result;
    }
    
    if (!roi_manager_) {
        result.success = false;
        result.message = "ROI manager not set";
        return result;
    }
    
    // Step 1: 验证图像
    result = validateImage(new_image);
    if (!result.success) {
        return result;
    }
    
    // Step 2: 定位校正（如果有定位模块）
    cv::Mat corrected_image = new_image;
    if (localization_ && !original_template_.empty()) {
        result.localization = localization_->autoDetectLocalization(original_template_, new_image);
        
        if (!result.localization.success) {
            result.success = false;
            result.message = "Localization failed - image rejected for learning";
            return result;
        }
        
        corrected_image = localization_->applyCorrection(new_image, result.localization);
    }
    
    // Step 3: 更新所有ROI的模板
    auto& rois = roi_manager_->getAllROIs();
    for (auto& roi : rois) {
        cv::Rect cv_rect = roi.bounds.toCvRect();
        
        // 边界检查
        cv_rect.x = std::max(0, cv_rect.x);
        cv_rect.y = std::max(0, cv_rect.y);
        cv_rect.width = std::min(cv_rect.width, corrected_image.cols - cv_rect.x);
        cv_rect.height = std::min(cv_rect.height, corrected_image.rows - cv_rect.y);
        
        if (cv_rect.width <= 0 || cv_rect.height <= 0) {
            continue;
        }
        
        cv::Mat new_template = corrected_image(cv_rect).clone();
        
        // 根据学习模式更新模板
        switch (mode_) {
            case LearningMode::REPLACE:
                roi.template_image = new_template;
                break;
            case LearningMode::INCREMENTAL:
                if (!roi.template_image.empty()) {
                    roi.template_image = fuseTemplates(roi.template_image, new_template, fusion_alpha_);
                } else {
                    roi.template_image = new_template;
                }
                break;
            case LearningMode::AVERAGE:
                if (!roi.template_image.empty()) {
                    roi.template_image = fuseTemplates(roi.template_image, new_template, 0.5f);
                } else {
                    roi.template_image = new_template;
                }
                break;
        }
    }
    
    result.success = true;
    result.message = "Template learned successfully";
    return result;
}

void TemplateLearner::setLearningMode(LearningMode mode) {
    mode_ = mode;
}

LearningMode TemplateLearner::getLearningMode() const {
    return mode_;
}

void TemplateLearner::setFusionAlpha(float alpha) {
    fusion_alpha_ = std::clamp(alpha, 0.0f, 1.0f);
}

float TemplateLearner::getFusionAlpha() const {
    return fusion_alpha_;
}

void TemplateLearner::setMinQualityThreshold(float threshold) {
    min_quality_threshold_ = std::clamp(threshold, 0.0f, 1.0f);
}

void TemplateLearner::setMinSimilarityThreshold(float threshold) {
    min_similarity_threshold_ = std::clamp(threshold, 0.0f, 1.0f);
}

LearningResult TemplateLearner::validateImage(const cv::Mat& new_image) {
    LearningResult result;
    
    // 检查图像质量
    result.quality_score = evaluateImageQuality(new_image);
    
    if (result.quality_score < min_quality_threshold_) {
        result.success = false;
        result.message = "Image quality too low: " + std::to_string(result.quality_score);
        return result;
    }
    
    // 如果有原始模板，检查相似度
    if (!original_template_.empty() && roi_manager_) {
        TemplateMatcher matcher;
        float max_similarity = 0.0f;
        
        auto& rois = roi_manager_->getAllROIs();
        for (const auto& roi : rois) {
            if (roi.template_image.empty()) continue;
            
            cv::Rect cv_rect = roi.bounds.toCvRect();
            if (cv_rect.x < 0 || cv_rect.y < 0 ||
                cv_rect.x + cv_rect.width > new_image.cols ||
                cv_rect.y + cv_rect.height > new_image.rows) {
                continue;
            }

            cv::Mat test_roi = new_image(cv_rect);
            float similarity = matcher.computeSimilarity(roi.template_image, test_roi);
            max_similarity = std::max(max_similarity, similarity);
        }

        if (max_similarity < min_similarity_threshold_ && !rois.empty()) {
            result.success = false;
            result.message = "New image too different from template: " + std::to_string(max_similarity);
            return result;
        }
    }

    result.success = true;
    result.message = "Image validation passed";
    return result;
}

float TemplateLearner::evaluateImageQuality(const cv::Mat& image) {
    float sharpness = computeSharpness(image);
    float brightness = computeBrightness(image);

    // 亮度太暗或太亮都会降低质量分数
    float brightness_score = 1.0f - std::abs(brightness - 0.5f) * 2.0f;

    // 组合分数
    return 0.6f * sharpness + 0.4f * brightness_score;
}

void TemplateLearner::reset() {
    if (roi_manager_ && !original_template_.empty()) {
        roi_manager_->extractTemplates(original_template_);
    }
}

void TemplateLearner::setOriginalTemplate(const cv::Mat& template_image) {
    original_template_ = template_image.clone();
}

cv::Mat TemplateLearner::fuseTemplates(const cv::Mat& old_template,
                                        const cv::Mat& new_template,
                                        float alpha) {
    cv::Mat result;

    // 确保尺寸匹配
    cv::Mat new_resized;
    if (old_template.size() != new_template.size()) {
        cv::resize(new_template, new_resized, old_template.size());
    } else {
        new_resized = new_template;
    }

    // 加权融合
    cv::addWeighted(old_template, alpha, new_resized, 1.0 - alpha, 0, result);

    return result;
}

float TemplateLearner::computeSharpness(const cv::Mat& image) {
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }

    // 使用拉普拉斯算子计算清晰度
    cv::Mat laplacian;
    cv::Laplacian(gray, laplacian, CV_64F);

    cv::Scalar mean, stddev;
    cv::meanStdDev(laplacian, mean, stddev);

    // 标准差越大，图像越清晰
    // 归一化到[0, 1]范围
    float variance = static_cast<float>(stddev[0] * stddev[0]);
    return std::min(1.0f, variance / 500.0f);
}

float TemplateLearner::computeBrightness(const cv::Mat& image) {
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }

    cv::Scalar mean = cv::mean(gray);
    return static_cast<float>(mean[0]) / 255.0f;
}

} // namespace defect_detection

