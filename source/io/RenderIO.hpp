#pragma once

#include <optional>
#include <vsg/all.h>
#include <vsgXchange/images.h>
#include <vector>
#include <string>
#include <buffers/GBuffer.hpp>
#include <buffers/IlluminationBuffer.hpp>

// vk copy Buffer to image wrapper class
class CopyBufferToImage : public vsg::Inherit<vsg::Command, CopyBufferToImage>
{
public:
    CopyBufferToImage(
        vsg::ref_ptr<vsg::BufferInfo> src, vsg::ref_ptr<vsg::ImageInfo> dst, uint32_t num_mip_map_levels = 1)
        : copy_data(src, dst, num_mip_map_levels)
    {
        copy_data.width = dst->imageView->image->extent.width;
        copy_data.height = dst->imageView->image->extent.height;
        copy_data.depth = dst->imageView->image->extent.depth;
    }
    vsg::CopyAndReleaseImage::CopyData copy_data;
    void record(vsg::CommandBuffer& command_buffer) const override { copy_data.record(command_buffer); }
};

// Matrices ------------------------------------------------------------------------
class CameraMatrices
{
public:
    vsg::mat4 view, inv_view;  // if no projection matrix is available, the projection and view matricesa are combined
                               // in the view matrix
    std::optional<vsg::mat4> proj, inv_proj;
};
using CameraMatricesVec = std::vector<CameraMatrices>;

class MatrixIO
{
public:
    static std::vector<CameraMatrices> import_matrices(const std::string& matrix_path);
    static bool export_matrices(const std::string& matrix_path, const CameraMatricesVec& matrices);
};

// GBuffer --------------------------------------------------------------------------
class OfflineGBuffer : public vsg::Inherit<vsg::Object, OfflineGBuffer>
{
public:
    vsg::ref_ptr<vsg::Data> depth, normal, material, albedo;
    // automatically adds correct image usag eflags to the gBuffer images
    void upload_to_g_buffer_command(
        vsg::ref_ptr<GBuffer>& g_buffer, vsg::ref_ptr<vsg::Commands> commands, vsg::Context& context);
    // automatically adds correct image usag eflags to the gBuffer images
    void download_from_g_buffer_command(
        vsg::ref_ptr<GBuffer>& g_buffer, vsg::ref_ptr<vsg::Commands> commands, vsg::Context& context);
    void transfer_staging_data_to(vsg::ref_ptr<OfflineGBuffer> other);
    void transfer_staging_data_from(vsg::ref_ptr<OfflineGBuffer> other);

private:
    vsg::ref_ptr<vsg::BufferInfo> _depth_staging, _normal_staging, _material_staging, _albedo_staging;
    vsg::ref_ptr<vsg::MemoryBufferPools> _staging_memory_buffer_pools;
    void setup_staging_buffer(uint32_t width, uint32_t height);
};
using OfflineGBuffers = std::vector<vsg::ref_ptr<OfflineGBuffer>>;

class GBufferIO
{
public:
    static OfflineGBuffers import_g_buffer_depth(const std::string& depth_format, const std::string& normal_format,
        const std::string& material_format, const std::string& albedo_format, int num_frames, int verbosity = 1);
    static OfflineGBuffers import_g_buffer_position(const std::string& position_format,
        const std::string& normal_format, const std::string& material_format, const std::string& albedo_format,
        const std::vector<CameraMatrices>& matrices, int num_frames, int verbosity = 1);
    static bool export_g_buffer(const std::string& position_format, const std::string& depth_format,
        const std::string& normal_format, const std::string& material_format, const std::string& albedo_format,
        int num_frames, const OfflineGBuffers& g_buffers, const CameraMatricesVec& matrices, int verbosity = 1);

private:
    static vsg::ref_ptr<vsg::Data> convert_normal_to_spherical(vsg::ref_ptr<vsg::vec4Array2D> normals);
    static vsg::ref_ptr<vsg::Data> compress_albedo(vsg::ref_ptr<vsg::Data> in);
    static vsg::ref_ptr<vsg::Data> spherical_to_cartesian(vsg::ref_ptr<vsg::vec2Array2D> normals);
    static vsg::ref_ptr<vsg::Data> unorm_to_float(vsg::ref_ptr<vsg::ubvec4Array2D> array);
    static vsg::ref_ptr<vsg::Data> depth_to_position(
        vsg::ref_ptr<vsg::floatArray2D> depths, const CameraMatrices& matrix);
};

// IlluminationBuffer ---------------------------------------------------------------
class OfflineIllumination : public vsg::Inherit<vsg::Object, OfflineIllumination>
{
public:
    vsg::ref_ptr<vsg::Data> noisy;
    void upload_to_illumination_buffer_command(
        vsg::ref_ptr<IlluminationBuffer>& illu_buffer, vsg::ref_ptr<vsg::Commands>& commands, vsg::Context& context);
    void download_from_illumination_buffer_command(
        vsg::ref_ptr<IlluminationBuffer>& illu_buffer, vsg::ref_ptr<vsg::Commands>& commands, vsg::Context& context);
    void transfer_staging_data_to(vsg::ref_ptr<OfflineIllumination>& illu_buffer);
    void transfer_staging_data_from(vsg::ref_ptr<OfflineIllumination>& illu_buffer);

private:
    vsg::ref_ptr<vsg::BufferInfo> _noisy_staging;
    vsg::ref_ptr<vsg::MemoryBufferPools> _staging_memory_buffer_pools;
    void setup_staging_buffer(uint32_t width, uint32_t height);
};
using OfflineIlluminations = std::vector<vsg::ref_ptr<OfflineIllumination>>;

class IlluminationBufferIO
{
public:
    static OfflineIlluminations import_illumination(
        const std::string& illumination_format, int num_frames, int verbosity = 1);
    static bool export_illumination(
        const std::string& illumination_format, int num_frames, const OfflineIlluminations& illus, int verbosity = 1);
};
