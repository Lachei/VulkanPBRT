#pragma once

#include <vsg/all.h>
#include <vsgXchange/images.h>
#include <vector>
#include <string>

class DoubleMatrix{
    vsg::mat4 view, invView;
};

class OfflineGBuffer: public vsg::Inherit<vsg::Object, OfflineGBuffer>{
public:
    vsg::ref_ptr<vsg::Data> depth, normal, material, albedo;
};

class GBufferImporter{
public:
    static std::vector<vsg::ref_ptr<OfflineGBuffer>> importGBufferDepth(const std::string& depthFormat, const std::string& normalFormat, const std::string& materialFormat, const std::string& albedoFormat, int numFrames);
    static std::vector<vsg::ref_ptr<OfflineGBuffer>> importGBufferPosition(const std::string& positionFormat, const std::string& normalFormat, const std::string& materialFormat, const std::string& albedoFormat, const std::vector<DoubleMatrix>& matrices, int numFrames);
};

class OfflineIllumination: public vsg::Inherit<vsg::Object, OfflineIllumination>{
public:
    vsg::ref_ptr<vsg::Data> noisy;
};

class IlluminationBufferImporter{
public:
    static std::vector<vsg::ref_ptr<OfflineIllumination>> importIllumination(const std::string& illuminationFormat, int numFrames);
};

class MatrixImporter{
public:
    static std::vector<DoubleMatrix> importMatrices(const std::string& matrixPath);
};