#include "RenderImporter.hpp"

std::vector<vsg::ref_ptr<OfflineGBuffer>> GBufferImporter::importGBufferDepth(const std::string &depthFormat, const std::string &normalFormat, const std::string &materialFormat, const std::string &albedoFormat, int numFrames)
{
    std::vector<vsg::ref_ptr<OfflineGBuffer>> gBuffers(numFrames);
    auto options = vsg::Options::create(vsgXchange::openexr::create());
    for (int f = 0; f < numFrames; ++f)
    {
        gBuffers[f] = OfflineGBuffer::create();
        char buff[200];
        std::string filename;
        // load depth image
        snprintf(buff, sizeof(buff), depthFormat.c_str(), f);
        filename = vsg::findFile(buff, options);

        if (gBuffers[f]->depth = vsg::read_cast<vsg::Data>(filename, options); !gBuffers[f]->depth.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return {};
        }
        // load normal image
        snprintf(buff, sizeof(buff), normalFormat.c_str(), f);
        filename = vsg::findFile(buff, options);

        if (gBuffers[f]->normal = vsg::read_cast<vsg::Data>(filename, options); !gBuffers[f]->normal.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return {};
        }
        // load albedo image
        snprintf(buff, sizeof(buff), albedoFormat.c_str(), f);
        filename = vsg::findFile(buff, options);

        if (gBuffers[f]->albedo = vsg::read_cast<vsg::Data>(filename, options); !gBuffers[f]->albedo.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return {};
        }
    }
    return gBuffers;
}

std::vector<vsg::ref_ptr<OfflineGBuffer>> GBufferImporter::importGBufferPosition(const std::string &positionFormat, const std::string &normalFormat, const std::string &materialFormat, const std::string &albedoFormat, const std::vector<DoubleMatrix> &matrices, int numFrames)
{
    auto options = vsg::Options::create(vsgXchange::openexr::create());
    std::vector<vsg::ref_ptr<OfflineGBuffer>> gBuffers(numFrames);
    for (int f = 0; f < numFrames; ++f)
    {
        char buff[200];
        std::string filename;
        // position images
        snprintf(buff, sizeof(buff), positionFormat.c_str(), f);
        filename = vsg::findFile(buff, options);

        vsg::ref_ptr<vsg::Data> pos;
        if (pos = vsg::read_cast<vsg::Data>(filename, options); !pos.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return {};
        }
        {// converting position to depth
            vsg::ref_ptr<vsg::vec4Array2D> posArray = pos.cast<vsg::vec4Array2D>();
            if(!posArray){
                std::cerr << "Unexpected position format" << std::endl;
                return {};
            }
            float* depth = new float[posArray->valueCount()];
            auto toVec3 = [&](vsg::vec4 v){return vsg::vec3(v.x, v.y, v.z);};
            vsg::vec3 cameraPos = toVec3(matrices[f].view[3]);
            for(uint32_t i = 0; i < posArray->valueCount() ; ++i){
                vsg::vec3 p = toVec3(posArray->data()[i]);
                depth[i] = vsg::length(cameraPos + p);  //note: camera pos has to be inverted, thus plus
            }
            gBuffers[f]->depth = vsg::floatArray2D::create(pos->width(), pos->height(), depth, vsg::Data::Layout{VK_FORMAT_R32_SFLOAT});
        }
        // load normal image
        snprintf(buff, sizeof(buff), normalFormat.c_str(), f);
        filename = vsg::findFile(buff, options);

        if (gBuffers[f]->normal = vsg::read_cast<vsg::Data>(filename, options); !gBuffers[f]->normal.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return {};
        }
        // load albedo image
        snprintf(buff, sizeof(buff), albedoFormat.c_str(), f);
        filename = vsg::findFile(buff, options);

        if (gBuffers[f]->albedo = vsg::read_cast<vsg::Data>(filename, options); !gBuffers[f]->albedo.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return {};
        }
    }
    return gBuffers;
}

std::vector<vsg::ref_ptr<OfflineIllumination>> IlluminationBufferImporter::importIllumination(const std::string &illuminationFormat, int numFrames)
{
    auto options = vsg::Options::create(vsgXchange::openexr::create());
    std::vector<vsg::ref_ptr<OfflineIllumination>> illuminations(numFrames);
    for (int f = 0; f < numFrames; ++f)
    {
        char buff[100];
        std::string filename;
        // position images
        snprintf(buff, sizeof(buff), illuminationFormat.c_str(), f);
        filename = vsg::findFile(buff, options);

        illuminations[f] = OfflineIllumination::create();

        if (illuminations[f]->noisy = vsg::read_cast<vsg::Data>(filename, options); !illuminations[f]->noisy.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return {};
        }
    }
    return illuminations;
}

std::vector<DoubleMatrix> MatrixImporter::importMatrices(const std::string &matrixPath)
{
    return {};
}
