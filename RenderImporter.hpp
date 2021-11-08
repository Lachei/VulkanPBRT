#pragma once

#include <vsg/all.h>
#include <vsgXchange/images.h>
#include <vector>
#include <string>
#include "GBuffer.hpp"
#include "IlluminationBuffer.hpp"

// vk copy Buffer to image wrapper class
class CopyBufferToImage: public vsg::Inherit<vsg::Command, CopyBufferToImage>{
public:
    CopyBufferToImage(vsg::BufferInfo src, vsg::ImageInfo dst, uint32_t numMipMapLevels = 1):
        copyData(src, dst, numMipMapLevels){}
    vsg::CopyAndReleaseImage::CopyData copyData;
    void record(vsg::CommandBuffer& commandBuffer) const override{
        copyData.record(commandBuffer);
    }
};

// Matrices ------------------------------------------------------------------------
class DoubleMatrix{
public:
    vsg::mat4 view, invView;
};
using DoubleMatrices = std::vector<DoubleMatrix>;

// GBuffer --------------------------------------------------------------------------
class OfflineGBuffer: public vsg::Inherit<vsg::Object, OfflineGBuffer>{
public:
    vsg::ref_ptr<vsg::Data> depth, normal, material, albedo;
    // automatically adds correct image usag eflags to the gBuffer images
    void uploadToGBufferCommand(vsg::ref_ptr<GBuffer>& gBuffer, vsg::ref_ptr<vsg::Commands> commands, vsg::Context& context);
    // automatically adds correct image usag eflags to the gBuffer images
    void downloadFromGBufferCommand(vsg::ref_ptr<GBuffer>& gBuffer, vsg::ref_ptr<vsg::Commands> commands, vsg::Context& context);
    void transferStagingDataTo(vsg::ref_ptr<OfflineGBuffer> other);
    void transferStagingDataFrom(vsg::ref_ptr<OfflineGBuffer> other);
private:
    vsg::BufferInfo depthStaging, normalStaging, materialStaging, albedoStaging;
    vsg::ref_ptr<vsg::MemoryBufferPools> stagingMemoryBufferPools;
    void setupStagingBuffer(uint32_t width, uint32_t height);
};
using OfflineGBuffers = std::vector<vsg::ref_ptr<OfflineGBuffer>>;

class GBufferImporter{
public:
    static OfflineGBuffers importGBufferDepth(const std::string& depthFormat, const std::string& normalFormat, const std::string& materialFormat, const std::string& albedoFormat, int numFrames);
    static OfflineGBuffers importGBufferPosition(const std::string& positionFormat, const std::string& normalFormat, const std::string& materialFormat, const std::string& albedoFormat, const std::vector<DoubleMatrix>& matrices, int numFrames);
private:
    static vsg::ref_ptr<vsg::Data> convertNormalToSpherical(vsg::ref_ptr<vsg::vec4Array2D> normals);
    static vsg::ref_ptr<vsg::Data> compressAlbedo(vsg::ref_ptr<vsg::Data> in);
};

class GBufferExporter{
public:
    static bool exportGBufferDepth(const std::string& depthFormat, const std::string& normalFormat, const std::string& materialFormat, const std::string& albedoFormat, int numFrames, const OfflineGBuffers& gBuffers);
    static bool exportGBufferPosition(const std::string& positionFormat, const std::string& normalFormat, const std::string& materialFormat, const std::string& albedoFormat, int numFrames, const OfflineGBuffers& gBuffers, const DoubleMatrices& matrices);
private:
    static vsg::ref_ptr<vsg::Data> sphericalToCartesian(vsg::ref_ptr<vsg::vec2Array2D> normals);
    static vsg::ref_ptr<vsg::Data> depthToPosition(vsg::ref_ptr<vsg::floatArray2D> depths, const DoubleMatrix& matrix);
};

// IlluminationBuffer ---------------------------------------------------------------
class OfflineIllumination: public vsg::Inherit<vsg::Object, OfflineIllumination>{
public:
    vsg::ref_ptr<vsg::Data> noisy;
    void uploadToIlluminationBufferCommand(vsg::ref_ptr<IlluminationBuffer>& illuBuffer, vsg::ref_ptr<vsg::Commands>& commands, vsg::Context& context);
    void downloadFromIlluminationBufferCommand(vsg::ref_ptr<IlluminationBuffer>& illuBuffer, vsg::ref_ptr<vsg::Commands>& commands, vsg::Context& context);
    void transferStagingDataTo(vsg::ref_ptr<OfflineIllumination>& illuBuffer);
    void transferStagingDataFrom(vsg::ref_ptr<OfflineIllumination>& illuBuffer);
private:
    vsg::BufferInfo noisyStaging;
    vsg::ref_ptr<vsg::MemoryBufferPools> stagingMemoryBufferPools;
    void setupStagingBuffer(uint32_t widht, uint32_t height);
};
using OfflineIlluminations = std::vector<vsg::ref_ptr<OfflineIllumination>>;

class IlluminationBufferImporter{
public:
    static OfflineIlluminations importIllumination(const std::string& illuminationFormat, int numFrames);
};

class IlluminationBufferEdxporter{
public:
    static bool exportIllumination(const std::string& illuminationFormat, int numFrames, const OfflineIlluminations& illus);
};

class MatrixIO{
public:
    static std::vector<DoubleMatrix> importMatrices(const std::string& matrixPath);
    static bool exportMatrices(const std::string& matrixPath, const DoubleMatrices& matrices);
};