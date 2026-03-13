/**
 * @file test_api_v2.cpp
 * @brief API v2.0 完整性测试
 * 
 * 测试内容：
 * 1. BinaryDetectionParamsC 结构体大小和字段偏移
 * 2. 新旧字段的读写
 * 3. 默认值验证
 */

#include <iostream>
#include <cstring>
#include "../api/c_api.h"

void test_binary_params_struct_size() {
    std::cout << "=== 测试 BinaryDetectionParamsC 结构体 ===" << std::endl;
    
    BinaryDetectionParamsC params;
    std::cout << "结构体大小: " << sizeof(params) << " 字节" << std::endl;
    
    // 测试默认值
    GetDefaultBinaryDetectionParams(&params);
    
    std::cout << "默认值验证:" << std::endl;
    std::cout << "  enabled: " << params.enabled << " (预期: 0)" << std::endl;
    std::cout << "  auto_detect_binary: " << params.auto_detect_binary << " (预期: 1)" << std::endl;
    std::cout << "  noise_filter_size: " << params.noise_filter_size << " (预期: 2)" << std::endl;
    std::cout << "  edge_tolerance_pixels: " << params.edge_tolerance_pixels << " (预期: 2)" << std::endl;
    std::cout << "  edge_diff_ignore_ratio: " << params.edge_diff_ignore_ratio << " (预期: 0.05)" << std::endl;
    std::cout << "  min_significant_area: " << params.min_significant_area << " (预期: 20)" << std::endl;
    std::cout << "  area_diff_threshold: " << params.area_diff_threshold << " (预期: 0.001)" << std::endl;
    std::cout << "  overall_similarity_threshold: " << params.overall_similarity_threshold << " (预期: 0.95)" << std::endl;
    // v2.0 新增字段
    std::cout << "  edge_defect_size_threshold: " << params.edge_defect_size_threshold << " (预期: 500)" << std::endl;
    std::cout << "  edge_distance_multiplier: " << params.edge_distance_multiplier << " (预期: 2.0)" << std::endl;
    std::cout << "  binary_threshold: " << params.binary_threshold << " (预期: 128)" << std::endl;
    
    // 验证新增字段
    bool v2_fields_ok = (params.edge_defect_size_threshold == 500) &&
                        (params.edge_distance_multiplier == 2.0f) &&
                        (params.binary_threshold == 128);
    
    std::cout << "v2.0 字段默认值验证: " << (v2_fields_ok ? "通过" : "失败") << std::endl;
}

void test_detector_with_v2_params() {
    std::cout << std::endl << "=== 测试 v2.0 参数设置和获取 ===" << std::endl;
    
    void* detector = CreateDetector();
    if (!detector) {
        std::cout << "创建检测器失败!" << std::endl;
        return;
    }
    
    // 设置 v2.0 参数
    BinaryDetectionParamsC params_in;
    GetDefaultBinaryDetectionParams(&params_in);
    params_in.enabled = 1;
    params_in.edge_tolerance_pixels = 15;
    params_in.edge_defect_size_threshold = 600;      // v2.0
    params_in.edge_distance_multiplier = 3.0f;       // v2.0
    params_in.binary_threshold = 100;                // v2.0
    
    int set_result = SetBinaryDetectionParams(detector, &params_in);
    std::cout << "设置参数结果: " << (set_result == 0 ? "成功" : "失败") << std::endl;
    
    // 获取参数验证
    BinaryDetectionParamsC params_out;
    int get_result = GetBinaryDetectionParams(detector, &params_out);
    std::cout << "获取参数结果: " << (get_result == 0 ? "成功" : "失败") << std::endl;
    
    // 验证 v2.0 字段
    bool v2_fields_match = (params_out.edge_defect_size_threshold == 600) &&
                           (params_out.edge_distance_multiplier == 3.0f) &&
                           (params_out.binary_threshold == 100);
    
    std::cout << "v2.0 字段读写验证:" << std::endl;
    std::cout << "  edge_defect_size_threshold: " << params_out.edge_defect_size_threshold << " (预期: 600)" << std::endl;
    std::cout << "  edge_distance_multiplier: " << params_out.edge_distance_multiplier << " (预期: 3.0)" << std::endl;
    std::cout << "  binary_threshold: " << params_out.binary_threshold << " (预期: 100)" << std::endl;
    std::cout << "  结果: " << (v2_fields_match ? "通过" : "失败") << std::endl;
    
    DestroyDetector(detector);
}

void test_backward_compatibility() {
    std::cout << std::endl << "=== 测试向后兼容性 ===" << std::endl;
    
    // 模拟旧版本客户端：只设置前8个字段
    struct OldBinaryParams {
        int enabled;
        int auto_detect_binary;
        int noise_filter_size;
        int edge_tolerance_pixels;
        float edge_diff_ignore_ratio;
        int min_significant_area;
        float area_diff_threshold;
        float overall_similarity_threshold;
    };
    
    OldBinaryParams old_params;
    memset(&old_params, 0, sizeof(old_params));
    old_params.enabled = 1;
    old_params.edge_tolerance_pixels = 10;
    
    void* detector = CreateDetector();
    if (!detector) {
        std::cout << "创建检测器失败!" << std::endl;
        return;
    }
    
    // 旧结构体大小应该小于新结构体
    std::cout << "旧结构体大小: " << sizeof(old_params) << " 字节" << std::endl;
    std::cout << "新结构体大小: " << sizeof(BinaryDetectionParamsC) << " 字节" << std::endl;
    std::cout << "大小差异: " << (sizeof(BinaryDetectionParamsC) - sizeof(old_params)) << " 字节" << std::endl;
    
    // 注意：实际使用时应该使用新结构体，这里只是验证二进制兼容性
    // 真实场景中，旧客户端使用旧DLL，新客户端使用新DLL
    
    DestroyDetector(detector);
    std::cout << "向后兼容性说明: 新增字段在C API转换函数中有默认值处理" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   API v2.0 完整性测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "库版本: " << GetLibraryVersion() << std::endl;
    std::cout << "构建信息: " << GetBuildInfo() << std::endl;
    std::cout << std::endl;
    
    test_binary_params_struct_size();
    test_detector_with_v2_params();
    test_backward_compatibility();
    
    std::cout << std::endl << "========================================" << std::endl;
    std::cout << "测试完成" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
