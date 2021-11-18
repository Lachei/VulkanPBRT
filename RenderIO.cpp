#include "RenderIO.hpp"
#include <future>
#include <cctype>

std::vector<vsg::ref_ptr<OfflineGBuffer>> GBufferIO::importGBufferDepth(const std::string &depthFormat, const std::string &normalFormat, const std::string &materialFormat, const std::string &albedoFormat, int numFrames, int verbosity)
{
    if(verbosity > 0)
        std::cout << "Start loading GBuffer" << std::endl;
    std::vector<vsg::ref_ptr<OfflineGBuffer>> gBuffers(numFrames);
    auto options = vsg::Options::create(vsgXchange::openexr::create());
    auto execLoad = [&](int f){
        if(verbosity > 1)
            std::cout << "GBuffer: Loading frame " << f << std::endl << std::flush; 
        gBuffers[f] = OfflineGBuffer::create();
        char buff[200];
        std::string filename;
        // load depth image
        snprintf(buff, sizeof(buff), depthFormat.c_str(), f);
        filename = vsg::findFile(buff, options);

        if (gBuffers[f]->depth = vsg::read_cast<vsg::Data>(filename, options); !gBuffers[f]->depth.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return;
        }
        // load normal image
        snprintf(buff, sizeof(buff), normalFormat.c_str(), f);
        filename = vsg::findFile(buff, options);

        if (gBuffers[f]->normal = convertNormalToSpherical(vsg::read_cast<vsg::vec4Array2D>(filename, options)); !gBuffers[f]->normal.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return;
        }
        // load albedo image
        snprintf(buff, sizeof(buff), albedoFormat.c_str(), f);
        filename = vsg::findFile(buff, options);

        if (gBuffers[f]->albedo = vsg::read_cast<vsg::Data>(filename, options); !gBuffers[f]->albedo.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return;
        }
        gBuffers[f]->albedo = compressAlbedo(gBuffers[f]->albedo);
        if(verbosity > 1)
            std::cout << "GBuffer: Loaded frame " << f << std::endl << std::flush;
    };
    {   //automatic join at the end of threads scope
        std::vector<std::future<void>> threads(numFrames);
        for (int f = 0; f < numFrames; ++f) threads[f] = std::async(std::launch::async, execLoad, f);
    }
    if(verbosity > 0)
        std::cout << "Done loading GBuffer" << std::endl;
    return gBuffers;
}

std::vector<vsg::ref_ptr<OfflineGBuffer>> GBufferIO::importGBufferPosition(const std::string &positionFormat, const std::string &normalFormat, const std::string &materialFormat, const std::string &albedoFormat, const std::vector<DoubleMatrix> &matrices, int numFrames, int verbosity)
{
    if(verbosity > 0)
        std::cout << "Start loading GBuffer" << std::endl;
    auto options = vsg::Options::create(vsgXchange::openexr::create());
    std::vector<vsg::ref_ptr<OfflineGBuffer>> gBuffers(numFrames);
    auto execLoad = [&](int f){
        if(verbosity > 1)
            std::cout << "GBuffer: Loading frame " << f << std::endl << std::flush;
        gBuffers[f] = OfflineGBuffer::create();
        char buff[200];
        std::string filename;
        // position images
        snprintf(buff, sizeof(buff), positionFormat.c_str(), f);
        filename = vsg::findFile(buff, options);

        vsg::ref_ptr<vsg::Data> pos;
        if (pos = vsg::read_cast<vsg::Data>(filename, options); !pos.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return;
        }
        {// converting position to depth
            vsg::ref_ptr<vsg::vec4Array2D> posArray = pos.cast<vsg::vec4Array2D>();
            if(!posArray){
                std::cerr << "Unexpected position format" << std::endl;
                return;
            }
            float* depth = new float[posArray->valueCount()];
            auto toVec3 = [&](vsg::vec4 v){return vsg::vec3(v.x, v.y, v.z);};
            vsg::vec4 cameraPos = matrices[f].invView[2];
            cameraPos /= cameraPos.w;
            for(uint32_t i = 0; i < posArray->valueCount() ; ++i){
                vsg::vec3 p = toVec3(posArray->data()[i]);
                depth[i] = vsg::length(toVec3(cameraPos) - p);
            }
            gBuffers[f]->depth = vsg::floatArray2D::create(pos->width(), pos->height(), depth, vsg::Data::Layout{VK_FORMAT_R32_SFLOAT});
        }
        // load normal image
        snprintf(buff, sizeof(buff), normalFormat.c_str(), f);
        filename = vsg::findFile(buff, options);

        if (gBuffers[f]->normal = convertNormalToSpherical(vsg::read_cast<vsg::vec4Array2D>(filename, options)); !gBuffers[f]->normal.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return;
        }
        // load albedo image
        snprintf(buff, sizeof(buff), albedoFormat.c_str(), f);
        filename = vsg::findFile(buff, options);

        if (gBuffers[f]->albedo = vsg::read_cast<vsg::Data>(filename, options); !gBuffers[f]->albedo.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return;
        }
        gBuffers[f]->albedo = compressAlbedo(gBuffers[f]->albedo);
        if(verbosity > 1)
            std::cout << "GBuffer: Loaded frame " << f << std::endl << std::flush; 
    };
    {   //automatic join at the end of threads scope
        std::vector<std::future<void>> threads(numFrames);
        for (int f = 0; f < numFrames; ++f) threads[f] = std::async(std::launch::async, execLoad, f); 
    }
    if(verbosity > 0)
        std::cout << "Done loading GBuffer" << std::endl;
    return gBuffers;
}

vsg::ref_ptr<vsg::Data> GBufferIO::convertNormalToSpherical(vsg::ref_ptr<vsg::vec4Array2D> normals) 
{
    if(!normals) return {};
    vsg::vec2* res = new vsg::vec2[normals->valueCount()];
    for(uint32_t i = 0; i < normals->valueCount(); ++i){
        vsg::vec4 curNormal = normals->data()[i];
        // the normals are generally stored in correct full format
        //curNormal *= 2;  
        //curNormal -= vsg::vec4(1, 1, 1, 0);
        res[i].x = acos(curNormal.z);
        res[i].y = atan2(curNormal.y, curNormal.x);
    }
    return vsg::vec2Array2D::create(normals->width(), normals->height(), res, vsg::Data::Layout{VK_FORMAT_R32G32_SFLOAT});
}

vsg::ref_ptr<vsg::Data> GBufferIO::compressAlbedo(vsg::ref_ptr<vsg::Data> in){
    vsg::ubvec4* albedo = new vsg::ubvec4[in->valueCount()];
    if(vsg::ref_ptr<vsg::vec4Array2D> largeAlbedo = in.cast<vsg::vec4Array2D>())
        for(uint32_t i = 0; i < in->valueCount(); ++i) albedo[i] = largeAlbedo->data()[i] * 255.0f;
    else if(vsg::ref_ptr<vsg::uivec4Array2D> largeAlbedo = in.cast<vsg::uivec4Array2D>())
        for(uint32_t i = 0; i < in->valueCount(); ++i) albedo[i] = largeAlbedo->data()[i];
    else if(vsg::ref_ptr<vsg::usvec4Array2D> largeAlbedo = in.cast<vsg::usvec4Array2D>()){
        auto toFloat = [](uint16_t h){uint32_t t = ((h&0x8000)<<16) | (((h&0x7c00)+0x1C000)<<13) | ((h&0x03FF)<<13); return *reinterpret_cast<float*>(&t);};
        for(uint32_t i = 0; i < in->valueCount(); ++i){
            vsg::usvec4& cur = largeAlbedo->data()[i];
            albedo[i] = vsg::vec4(toFloat(cur.x), toFloat(cur.y), toFloat(cur.z), toFloat(cur.w)) * 255.0f;
        }    
    }
    return vsg::ubvec4Array2D::create(in->width(), in->height(), albedo, vsg::Data::Layout{VK_FORMAT_R8G8B8A8_UNORM});
}

bool GBufferIO::exportGBufferDepth(const std::string& depthFormat, const std::string& normalFormat, const std::string& materialFormat, const std::string& albedoFormat, int numFrames, const OfflineGBuffers& gBuffers, int verbosity) 
{
    if(verbosity > 0)
        std::cout << "Start exporting GBuffer" << std::endl;
    auto options = vsg::Options::create(vsgXchange::openexr::create());
    bool fine = true;
    auto execStore = [&](int f){
        if(verbosity > 1)
            std::cout << "GBuffer: Storing frame " << f << std::endl << std::flush;
        char buff[200];
        std::string filename;
        // depth images
        snprintf(buff, sizeof(buff), depthFormat.c_str(), f);
        filename = buff;
        if(!vsg::write(gBuffers[f]->depth, filename, options)){
            std::cerr << "Failed to store image: " << filename << std::endl;
            fine = false;
            return;
        }
        //normal images
        snprintf(buff, sizeof(buff), normalFormat.c_str(), f);
        filename = buff;
        if(!vsg::write(gBuffers[f]->normal, filename, options)){
            std::cerr << "Failed to store image: " << filename << std::endl;
            fine = false;
            return;
        }
        //material images
        snprintf(buff, sizeof(buff), materialFormat.c_str(), f);
        filename = buff;
        if(!vsg::write(gBuffers[f]->material, filename, options)){
            std::cerr << "Failed to store image: " << filename << std::endl;
            fine = false;
            return;
        }
        //albedo images
        snprintf(buff, sizeof(buff), albedoFormat.c_str(), f);
        filename = buff;
        if(!vsg::write(gBuffers[f]->albedo, filename, options)){
            std::cerr << "Failed to store image: " << filename << std::endl;
            fine = false;
            return;
        }
        if(verbosity > 1)
            std::cout << "GBuffer: Stored frame " << f << std::endl << std::flush;
    };
    {   //automatic join at the end of threads scope
        std::vector<std::future<void>> threads(numFrames);
        for (int f = 0; f < numFrames; ++f) threads[f] = std::async(std::launch::async, execStore, f); 
    }
    if(verbosity > 0)
        std::cout << "Done exporting GBuffer" << std::endl;
    return fine;
}

bool GBufferIO::exportGBufferPosition(const std::string& positionFormat, const std::string& normalFormat, const std::string& materialFormat, const std::string& albedoFormat, int numFrames, const OfflineGBuffers& gBuffers, const DoubleMatrices& matrices, int verbosity) 
{
    if(verbosity > 0)
        std::cout << "Start exporting GBuffer" << std::endl;
    auto options = vsg::Options::create(vsgXchange::openexr::create());
    bool fine = true;
    auto execStore = [&](int f){
        if(verbosity > 1)
            std::cout << "GBuffer: Storing frame " << f << std::endl << std::flush;
        char buff[200];
        std::string filename;
        // position images
        snprintf(buff, sizeof(buff), positionFormat.c_str(), f);
        vsg::ref_ptr<vsg::Data> position = depthToPosition(gBuffers[f]->depth.cast<vsg::floatArray2D>(), matrices[f]);
        filename = buff;
        if(!vsg::write(position, filename, options)){
            std::cerr << "Failed to store image: " << filename << std::endl;
            fine = false;
            return;
        }
        //normal images
        snprintf(buff, sizeof(buff), normalFormat.c_str(), f);
        filename = buff;
        if(!vsg::write(gBuffers[f]->normal, filename, options)){
            std::cerr << "Failed to store image: " << filename << std::endl;
            fine = false;
            return;
        }
        //material images
        snprintf(buff, sizeof(buff), materialFormat.c_str(), f);
        filename = buff;
        if(!vsg::write(gBuffers[f]->material, filename, options)){
            std::cerr << "Failed to store image: " << filename << std::endl;
            fine = false;
            return;
        }
        //albedo images
        snprintf(buff, sizeof(buff), albedoFormat.c_str(), f);
        filename = buff;
        if(!vsg::write(gBuffers[f]->albedo, filename, options)){
            std::cerr << "Failed to store image: " << filename << std::endl;
            fine = false;
            return;
        }
        if(verbosity > 1)
            std::cout << "GBuffer: Stored frame " << f << std::endl << std::flush;
    };
    {
        std::vector<std::future<void>> threads(numFrames);
        for (int f = 0; f < numFrames; ++f) threads[f] = std::async(std::launch::async, execStore, f); 
    }
    if(verbosity > 0)
        std::cout << "Done exporting GBuffer" << std::endl;
    return fine;
}

vsg::ref_ptr<vsg::Data> GBufferIO::sphericalToCartesian(vsg::ref_ptr<vsg::vec2Array2D> normals)
{
    if(!normals) return {};
    vsg::vec4* res = new vsg::vec4[normals->valueCount()];
    for(uint32_t i = 0; i < normals->valueCount(); ++i){
        vsg::vec2 curNormal = normals->data()[i];
        res[i].x = cos(curNormal.y) * sin(curNormal.x);
        res[i].y = sin(curNormal.y) * sin(curNormal.x);
        res[i].z = cos(curNormal.x);
        res[i].w = 1;
    }
    return vsg::vec4Array2D::create(normals->width(), normals->height(), res, vsg::Data::Layout{VK_FORMAT_R32G32B32A32_SFLOAT});
}

vsg::ref_ptr<vsg::Data> GBufferIO::depthToPosition(vsg::ref_ptr<vsg::floatArray2D> depths, const DoubleMatrix& matrix)
{
    if(!depths) return {};
    vsg::vec4* res = new vsg::vec4[depths->valueCount()];
    auto toVec3 = [&](vsg::vec4 v){return vsg::vec3(v.x, v.y, v.z);};
    vsg::vec4 cameraPos = matrix.invView[2];
    cameraPos /= cameraPos.w;
    for(uint32_t i = 0; i < depths->valueCount(); ++i){
        uint32_t x = i % depths->width();
        uint32_t y = i / depths->width();
        vsg::vec2 p{(x + .5f) / depths->width() * 2 - 1, (y + .5f) / depths->height() * 2 - 1};
        vsg::vec4 dir = matrix.invView * vsg::vec4{p.x, p.y, 1, 1};
        dir /= dir.w + 1e-9;
        vsg::vec3 direction = vsg::normalize(vsg::vec3{dir.x, dir.y, dir.z});
        direction *= depths->data()[i];
        vsg::vec3 pos = toVec3(cameraPos) - direction;
        res[i].x = pos.x;
        res[i].y = pos.y;
        res[i].z = pos.z;
        res[i].w = 1;
    }
    return vsg::vec4Array2D::create(depths->width(), depths->height(), res, vsg::Data::Layout{VK_FORMAT_R32G32B32A32_SFLOAT});
}

void OfflineIllumination::uploadToIlluminationBufferCommand(vsg::ref_ptr<IlluminationBuffer>& illuBuffer, vsg::ref_ptr<vsg::Commands>& commands, vsg::Context& context)
{
    stagingMemoryBufferPools = context.stagingMemoryBufferPools;
    if(!noisyStaging.buffer)
        setupStagingBuffer(illuBuffer->width, illuBuffer->height);
    if(illuBuffer->illuminationImages[0]){
        commands->addChild(CopyBufferToImage::create(noisyStaging, illuBuffer->illuminationImages[0]->imageInfoList.front(), 1));
        illuBuffer->illuminationImages[0]->imageInfoList[0].imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
}

void OfflineIllumination::downloadFromIlluminationBufferCommand(vsg::ref_ptr<IlluminationBuffer>& illuBuffer, vsg::ref_ptr<vsg::Commands>& commands, vsg::Context& context){
    stagingMemoryBufferPools = context.stagingMemoryBufferPools;
    if(!noisyStaging.buffer)
        setupStagingBuffer(illuBuffer->width, illuBuffer->height);
    if(illuBuffer->illuminationImages[0])
    {
        auto copy = vsg::CopyImageToBuffer::create();
        vsg::ImageInfo info = illuBuffer->illuminationImages[0]->imageInfoList.front();
        copy->srcImage = info.imageView->image;
        copy->srcImageLayout = info.imageLayout;
        copy->dstBuffer = noisyStaging.buffer;
        copy->regions = {VkBufferImageCopy{0, static_cast<uint32_t>(noisyStaging.buffer->size), 1, VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, VkOffset3D{0,0,0}, info.imageView->image->extent}};
        commands->addChild(copy);
        illuBuffer->illuminationImages[0]->imageInfoList[0].imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        noisyStaging.buffer->usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
}

void OfflineIllumination::transferStagingDataTo(vsg::ref_ptr<OfflineIllumination>& illuBuffer)
{
    if(!stagingMemoryBufferPools){
        std::cout << "Current offline illumination buffer has not been added to a command graph and is thus not able to do transfer" << std::endl;
        return;
    }
    auto deviceID = stagingMemoryBufferPools->device->deviceID;
    vsg::ref_ptr<vsg::Buffer> buffer(noisyStaging.buffer);
    vsg::ref_ptr<vsg::DeviceMemory> memory(buffer->getDeviceMemory(deviceID));
    if(!memory){
        std::cout << "Error while transferring illumination staging memory data to offline illumination buffer." << std::endl;
        return;
    }
    void* gpu_data;
    memory->map(buffer->getMemoryOffset(deviceID) + noisyStaging.offset, buffer->size, 0, &gpu_data);
    std::memcpy(illuBuffer->noisy->dataPointer(), gpu_data, (size_t)buffer->size);
    memory->unmap();
}

void OfflineIllumination::transferStagingDataFrom(vsg::ref_ptr<OfflineIllumination>& illuBuffer)
{
    if(!stagingMemoryBufferPools){
        std::cout << "Current offline illumination buffer has not been added to a command graph and is thus not able to do transfer" << std::endl;
        return;
    }
    auto deviceID = stagingMemoryBufferPools->device->deviceID;
    vsg::ref_ptr<vsg::Buffer> buffer(noisyStaging.buffer);
    vsg::ref_ptr<vsg::DeviceMemory> memory(buffer->getDeviceMemory(deviceID));
    if(!memory){
        std::cout << "Error while transferring illumination staging memory data from offline illumination buffer." << std::endl;
        return;
    }
    memory->copy(buffer->getMemoryOffset(deviceID) + noisyStaging.offset, noisyStaging.range, illuBuffer->noisy->dataPointer());
}

void OfflineIllumination::setupStagingBuffer(uint32_t width, uint32_t height){
    VkDeviceSize imageTotalSize = sizeof(vsg::vec4) * width * height;
    VkDeviceSize alignment = 16;
    VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    noisyStaging = stagingMemoryBufferPools->reserveBuffer(imageTotalSize, alignment, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, memoryPropertyFlags);
}

std::vector<vsg::ref_ptr<OfflineIllumination>> IlluminationBufferIO::importIllumination(const std::string &illuminationFormat, int numFrames, int verbosity)
{
    if(verbosity > 0)
        std::cout << "Start loading Illumination" << std::endl;
    auto options = vsg::Options::create(vsgXchange::openexr::create());
    std::vector<vsg::ref_ptr<OfflineIllumination>> illuminations(numFrames);
    auto execLoad = [&](int f){
        if(verbosity > 1)
            std::cout << "Illumination: Loading frame " << f << std::endl << std::flush;
        char buff[100];
        std::string filename;
        // position images
        snprintf(buff, sizeof(buff), illuminationFormat.c_str(), f);
        filename = vsg::findFile(buff, options);

        illuminations[f] = OfflineIllumination::create();

        if (illuminations[f]->noisy = vsg::read_cast<vsg::Data>(filename, options); !illuminations[f]->noisy.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return;
        }
        if(verbosity > 1)
            std::cout << "Illumination: Loaded frame " << f << std::endl << std::flush;
    };
    {
        std::vector<std::future<void>> threads(numFrames);
        for (int f = 0; f < numFrames; ++f) threads[f] = std::async(std::launch::async, execLoad, f); 
    }
    if(verbosity > 0)
        std::cout << "Done loading Illumination" << std::endl;
    return illuminations;
}

DoubleMatrices MatrixIO::importMatrices(const std::string &matrixPath)
{
    //TODO: temporary implementation to parse matrices from BMFRs dataset
    std::ifstream f(matrixPath);
    if (!f) {
        std::cout << "Matrix file " << matrixPath << " unable to open." << std::endl;
        exit(-1);
    }

    std::string cur;
    uint32_t count = 0;
    vsg::mat4 tmp;
    DoubleMatrices matrices;
    while (!f.eof()) {
        f >> cur;
        if (cur.back() == ',') cur.pop_back();
        if (cur.front() == '{') cur = cur.substr(1, cur.size() - 1);
        if (cur.size() && (isdigit(cur[0]) || cur[0] == '-')) {
            tmp[count / 4][count % 4] = std::stof(cur);
            ++count;
            if (count == 16) {
                matrices.push_back({tmp, vsg::inverse(tmp)});
                count = 0;
            }
        }
    }

    return matrices;
}

bool MatrixIO::exportMatrices(const std::string &matrixPath, const DoubleMatrices& matrices)
{
    std::ifstream f(matrixPath);
    if (!f) {
        std::cout << "Matrix file " << matrixPath << " unable to open." << std::endl;
        exit(-1);
    }

    //TODO: implement

    return true;
}

void OfflineGBuffer::uploadToGBufferCommand(vsg::ref_ptr<GBuffer>& gBuffer, vsg::ref_ptr<vsg::Commands> commands, vsg::Context& context) 
{
    stagingMemoryBufferPools = context.stagingMemoryBufferPools;
    if(!depthStaging.buffer || !normalStaging.buffer || !albedoStaging.buffer || !materialStaging.buffer)
        setupStagingBuffer(gBuffer->width, gBuffer->height);
    if(gBuffer->depth){
        commands->addChild(CopyBufferToImage::create(depthStaging, gBuffer->depth->imageInfoList.front(), 1));
        gBuffer->depth->imageInfoList[0].imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if(gBuffer->normal){
        commands->addChild(CopyBufferToImage::create(normalStaging, gBuffer->normal->imageInfoList.front(), 1));
        gBuffer->normal->imageInfoList[0].imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if(gBuffer->albedo){
        commands->addChild(CopyBufferToImage::create(albedoStaging, gBuffer->albedo->imageInfoList.front(), 1));
        gBuffer->albedo->imageInfoList[0].imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if(gBuffer->material){
        commands->addChild(CopyBufferToImage::create(materialStaging, gBuffer->material->imageInfoList.front(), 1));
        gBuffer->material->imageInfoList[0].imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
}

void OfflineGBuffer::downloadFromGBufferCommand(vsg::ref_ptr<GBuffer>& gBuffer, vsg::ref_ptr<vsg::Commands> commands, vsg::Context& context)
{
    stagingMemoryBufferPools = context.stagingMemoryBufferPools;
    if(!depthStaging.buffer || !normalStaging.buffer || !albedoStaging.buffer || !materialStaging.buffer)
        setupStagingBuffer(gBuffer->width, gBuffer->height);
    if(gBuffer->depth)
    {
        auto copy = vsg::CopyImageToBuffer::create();
        vsg::ImageInfo info = gBuffer->depth->imageInfoList.front();
        copy->srcImage = info.imageView->image;
        copy->srcImageLayout = info.imageLayout;
        copy->dstBuffer = depthStaging.buffer;
        copy->regions = {VkBufferImageCopy{0, static_cast<uint32_t>(depthStaging.buffer->size), 1, VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, VkOffset3D{0,0,0}, info.imageView->image->extent}};
        commands->addChild(copy);
        gBuffer->depth->imageInfoList[0].imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if(gBuffer->normal)
    {
        auto copy = vsg::CopyImageToBuffer::create();
        vsg::ImageInfo info = gBuffer->normal->imageInfoList.front();
        copy->srcImage = info.imageView->image;
        copy->srcImageLayout = info.imageLayout;
        copy->dstBuffer = normalStaging.buffer;
        copy->regions = {VkBufferImageCopy{0, static_cast<uint32_t>(normalStaging.buffer->size), 1, VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, VkOffset3D{0,0,0}, info.imageView->image->extent}};
        commands->addChild(copy);
        gBuffer->normal->imageInfoList[0].imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if(gBuffer->albedo)
    {
        auto copy = vsg::CopyImageToBuffer::create();
        vsg::ImageInfo info = gBuffer->albedo->imageInfoList.front();
        copy->srcImage = info.imageView->image;
        copy->srcImageLayout = info.imageLayout;
        copy->dstBuffer = albedoStaging.buffer;
        copy->regions = {VkBufferImageCopy{0, static_cast<uint32_t>(normalStaging.buffer->size), 1, VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, VkOffset3D{0,0,0}, info.imageView->image->extent}};
        commands->addChild(copy);
        gBuffer->albedo->imageInfoList[0].imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if(gBuffer->material)
    {
        auto copy = vsg::CopyImageToBuffer::create();
        vsg::ImageInfo info = gBuffer->material->imageInfoList.front();
        copy->srcImage = info.imageView->image;
        copy->srcImageLayout = info.imageLayout;
        copy->dstBuffer = materialStaging.buffer;
        copy->regions = {VkBufferImageCopy{0, static_cast<uint32_t>(normalStaging.buffer->size), 1, VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, VkOffset3D{0,0,0}, info.imageView->image->extent}};
        commands->addChild(copy);
        gBuffer->material->imageInfoList[0].imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
}

void OfflineGBuffer::transferStagingDataTo(vsg::ref_ptr<OfflineGBuffer> other)
{
    if(!stagingMemoryBufferPools){
        std::cout << "Current offline buffer has not been added to a command graph and is thus not able to do transfer" << std::endl;
        return;
    }
    auto deviceID = stagingMemoryBufferPools->device->deviceID;
    vsg::ref_ptr<vsg::Buffer> buffer(depthStaging.buffer);
    vsg::ref_ptr<vsg::DeviceMemory> memory(buffer->getDeviceMemory(deviceID));
    if(!memory){
        std::cout << "Error while transferring depth staging memory data to offline gBuffer." << std::endl;
        return;
    }
    void* gpu_data;
    memory->map(buffer->getMemoryOffset(deviceID) + depthStaging.offset, buffer->size, 0, &gpu_data);
    std::memcpy(other->depth->dataPointer(), gpu_data, (size_t)buffer->size);
    memory->unmap();

    buffer = vsg::ref_ptr<vsg::Buffer>(normalStaging.buffer);
    memory = vsg::ref_ptr<vsg::DeviceMemory>(buffer->getDeviceMemory(deviceID));
    if(!memory){
        std::cout << "Error while transferring normal staging memory data to offline gBuffer." << std::endl;
        return;
    }
    memory->map(buffer->getMemoryOffset(deviceID) + normalStaging.offset, buffer->size, 0, &gpu_data);
    std::memcpy(other->normal->dataPointer(), gpu_data, (size_t)buffer->size);
    memory->unmap();

    buffer = vsg::ref_ptr<vsg::Buffer>(albedoStaging.buffer);
    memory = vsg::ref_ptr<vsg::DeviceMemory>(buffer->getDeviceMemory(deviceID));
    if(!memory){
        std::cout << "Error while transferring albedo staging memory data to offline gBuffer." << std::endl;
        return;
    }
    memory->map(buffer->getMemoryOffset(deviceID) + albedoStaging.offset, buffer->size, 0, &gpu_data);
    std::memcpy(other->albedo->dataPointer(), gpu_data, (size_t)buffer->size);
    memory->unmap();

    buffer = vsg::ref_ptr<vsg::Buffer>(materialStaging.buffer);
    memory = vsg::ref_ptr<vsg::DeviceMemory>(buffer->getDeviceMemory(deviceID));
    if(!memory){
        std::cout << "Error while transferring material staging memory data to offline gBuffer." << std::endl;
        return;
    }
    memory->map(buffer->getMemoryOffset(deviceID) + materialStaging.offset, buffer->size, 0, &gpu_data);
    std::memcpy(other->material->dataPointer(), gpu_data, (size_t)buffer->size);
    memory->unmap();
}

void OfflineGBuffer::transferStagingDataFrom(vsg::ref_ptr<OfflineGBuffer> other)
{
    if(!stagingMemoryBufferPools){
        std::cout << "Current offline buffer has not been added to a command graph and is thus not able to do transfer" << std::endl;
        return;
    }
    auto deviceID = stagingMemoryBufferPools->device->deviceID;
    vsg::ref_ptr<vsg::Buffer> buffer(depthStaging.buffer);
    vsg::ref_ptr<vsg::DeviceMemory> memory(buffer->getDeviceMemory(deviceID));
    if(!memory){
        std::cout << "Error while transferring depth staging memory data to offline gBuffer." << std::endl;
        return;
    }
    memory->copy(buffer->getMemoryOffset(deviceID) + depthStaging.offset, depthStaging.range, other->depth->dataPointer());

    buffer = vsg::ref_ptr<vsg::Buffer>(normalStaging.buffer);
    memory = vsg::ref_ptr<vsg::DeviceMemory>(buffer->getDeviceMemory(deviceID));
    if(!memory){
        std::cout << "Error while transferring normal staging memory data to offline gBuffer." << std::endl;
        return;
    }
    memory->copy(buffer->getMemoryOffset(deviceID) + normalStaging.offset, normalStaging.range, other->normal->dataPointer());

    buffer = vsg::ref_ptr<vsg::Buffer>(albedoStaging.buffer);
    memory = vsg::ref_ptr<vsg::DeviceMemory>(buffer->getDeviceMemory(deviceID));
    if(!memory){
        std::cout << "Error while transferring albedo staging memory data to offline gBuffer." << std::endl;
        return;
    }
    memory->copy(buffer->getMemoryOffset(deviceID) + albedoStaging.offset, albedoStaging.range, other->albedo->dataPointer());

    buffer = vsg::ref_ptr<vsg::Buffer>(materialStaging.buffer);
    memory = vsg::ref_ptr<vsg::DeviceMemory>(buffer->getDeviceMemory(deviceID));
    if(!memory){
        std::cout << "Error while transferring material staging memory data to offline gBuffer." << std::endl;
        return;
    }
    if(other->material)
        memory->copy(buffer->getMemoryOffset(deviceID) + materialStaging.offset, materialStaging.range, other->material->dataPointer());
}

void OfflineGBuffer::setupStagingBuffer(uint32_t width, uint32_t height)
{
    VkDeviceSize imageTotalSize = sizeof(float) * width * height;
    VkDeviceSize alignment = 4;
    VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    depthStaging = stagingMemoryBufferPools->reserveBuffer(imageTotalSize, alignment, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, memoryPropertyFlags);
    
    imageTotalSize = sizeof(vsg::vec2) * width * height;
    alignment = 8; //sizeof vsg::vec2
    normalStaging = stagingMemoryBufferPools->reserveBuffer(imageTotalSize, alignment, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, memoryPropertyFlags);

    imageTotalSize = sizeof(vsg::ubvec4) * width * height;
    alignment = 4;
    albedoStaging = stagingMemoryBufferPools->reserveBuffer(imageTotalSize, alignment, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, memoryPropertyFlags);

    materialStaging = stagingMemoryBufferPools->reserveBuffer(imageTotalSize, alignment, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, memoryPropertyFlags);
}
