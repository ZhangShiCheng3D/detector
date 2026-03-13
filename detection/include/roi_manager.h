/**
 * @file roi_manager.h
 * @brief ROI区域管理器
 */

#ifndef DEFECT_DETECTION_ROI_MANAGER_H
#define DEFECT_DETECTION_ROI_MANAGER_H

#include "types.h"
#include <vector>
#include <unordered_map>
#include <mutex>

namespace defect_detection {

/**
 * @brief ROI区域管理器类
 * 
 * 负责管理多个ROI区域，支持独立阈值设置和坐标变换
 */
class ROIManager {
public:
    ROIManager();
    ~ROIManager();
    
    /**
     * @brief 添加一个ROI区域
     * @param bounds 区域边界
     * @param threshold 相似度阈值
     * @return ROI的ID
     */
    int addROI(const Rect& bounds, float threshold = 0.85f);
    
    /**
     * @brief 添加一个ROI区域（带参数）
     * @param bounds 区域边界
     * @param threshold 相似度阈值
     * @param params 检测参数
     * @return ROI的ID
     */
    int addROI(const Rect& bounds, float threshold, const DetectionParams& params);
    
    /**
     * @brief 移除一个ROI区域
     * @param id ROI的ID
     * @return 是否成功
     */
    bool removeROI(int id);
    
    /**
     * @brief 更新ROI区域边界
     * @param id ROI的ID
     * @param new_bounds 新的边界
     * @return 是否成功
     */
    bool updateROI(int id, const Rect& new_bounds);
    
    /**
     * @brief 设置ROI的相似度阈值
     * @param id ROI的ID
     * @param threshold 阈值
     * @return 是否成功
     */
    bool setThreshold(int id, float threshold);
    
    /**
     * @brief 设置ROI的检测参数
     * @param id ROI的ID
     * @param params 检测参数
     * @return 是否成功
     */
    bool setParams(int id, const DetectionParams& params);
    
    /**
     * @brief 获取所有ROI区域
     * @return ROI区域列表
     */
    std::vector<ROIRegion>& getAllROIs();
    const std::vector<ROIRegion>& getAllROIs() const;
    
    /**
     * @brief 获取指定ROI
     * @param id ROI的ID
     * @return ROI区域指针，不存在返回nullptr
     */
    ROIRegion* getROI(int id);
    const ROIRegion* getROI(int id) const;
    
    /**
     * @brief 获取ROI数量
     * @return ROI数量
     */
    size_t getROICount() const;
    
    /**
     * @brief 清除所有ROI
     */
    void clear();
    
    /**
     * @brief 对所有ROI应用仿射变换
     * @param affine_matrix 仿射变换矩阵 (2x3)
     */
    void applyTransform(const cv::Mat& affine_matrix);
    
    /**
     * @brief 从模板图像提取所有ROI区域的模板
     * @param template_image 模板图像
     */
    void extractTemplates(const cv::Mat& template_image);
    
    /**
     * @brief 验证ROI边界是否在图像范围内
     * @param image_width 图像宽度
     * @param image_height 图像高度
     * @return 所有ROI是否有效
     */
    bool validateBounds(int image_width, int image_height) const;
    
    /**
     * @brief 裁剪ROI边界以适应图像大小
     * @param image_width 图像宽度
     * @param image_height 图像高度
     */
    void clipToImageBounds(int image_width, int image_height);

private:
    std::vector<ROIRegion> rois_;
    std::unordered_map<int, size_t> id_to_index_;
    int next_id_;
    mutable std::mutex mutex_;
    
    void rebuildIndex();
};

} // namespace defect_detection

#endif // DEFECT_DETECTION_ROI_MANAGER_H

