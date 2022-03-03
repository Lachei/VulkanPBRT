#include <io/RenderIO.hpp>
#include <future>
#include <cctype>
#include <nlohmann/json.hpp>

std::vector<vsg::ref_ptr<OfflineGBuffer>> GBufferIO::import_g_buffer_depth(const std::string& depthFormat,
    const std::string& normalFormat, const std::string& material_format, const std::string& albedoFormat,
    int num_frames, int verbosity)
{
    if (verbosity > 0)
    {
        std::cout << "Start loading GBuffer" << std::endl;
    }
    std::vector<vsg::ref_ptr<OfflineGBuffer>> g_buffers(num_frames);
    auto options = vsg::Options::create(vsgXchange::openexr::create());
    auto exec_load = [&](int f)
    {
        if (verbosity > 1)
        {
            std::cout << "GBuffer: Loading frame " << f << std::endl << std::flush;
        }
        g_buffers[f] = OfflineGBuffer::create();
        char buff[200];
        std::string filename;
        // load depth image
        snprintf(buff, sizeof(buff), depthFormat.c_str(), f);
        filename = findFile(buff, options);

        if (g_buffers[f]->depth = vsg::read_cast<vsg::Data>(filename, options); !g_buffers[f]->depth.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return;
        }
        // load normal image
        snprintf(buff, sizeof(buff), normalFormat.c_str(), f);
        filename = findFile(buff, options);

        if (g_buffers[f]->normal = convert_normal_to_spherical(vsg::read_cast<vsg::vec4Array2D>(filename, options));
            !g_buffers[f]->normal.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return;
        }
        // load albedo image
        snprintf(buff, sizeof(buff), albedoFormat.c_str(), f);
        filename = findFile(buff, options);

        if (g_buffers[f]->albedo = vsg::read_cast<vsg::Data>(filename, options); !g_buffers[f]->albedo.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return;
        }
        g_buffers[f]->albedo = compress_albedo(g_buffers[f]->albedo);
        if (verbosity > 1)
        {
            std::cout << "GBuffer: Loaded frame " << f << std::endl << std::flush;
        }
    };
    {  // automatic join at the end of threads scope
        std::vector<std::future<void>> threads(num_frames);
        for (int f = 0; f < num_frames; ++f)
        {
            threads[f] = std::async(std::launch::async, exec_load, f);
        }
    }
    if (verbosity > 0)
    {
        std::cout << "Done loading GBuffer" << std::endl;
    }
    return g_buffers;
}

std::vector<vsg::ref_ptr<OfflineGBuffer>> GBufferIO::import_g_buffer_position(const std::string& positionFormat,
    const std::string& normalFormat, const std::string& material_format, const std::string& albedoFormat,
    const std::vector<CameraMatrices>& matrices, int num_frames, int verbosity)
{
    if (verbosity > 0)
    {
        std::cout << "Start loading GBuffer" << std::endl;
    }
    auto options = vsg::Options::create(vsgXchange::openexr::create());
    std::vector<vsg::ref_ptr<OfflineGBuffer>> g_buffers(num_frames);
    auto exec_load = [&](int f)
    {
        if (verbosity > 1)
        {
            std::cout << "GBuffer: Loading frame " << f << std::endl << std::flush;
        }
        g_buffers[f] = OfflineGBuffer::create();
        char buff[200];
        std::string filename;
        // position images
        snprintf(buff, sizeof(buff), positionFormat.c_str(), f);
        filename = findFile(buff, options);

        vsg::ref_ptr<vsg::Data> pos;
        if (pos = vsg::read_cast<vsg::Data>(filename, options); !pos.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return;
        }
        {  // converting position to depth
            vsg::ref_ptr<vsg::vec4Array2D> pos_array = pos.cast<vsg::vec4Array2D>();
            if (!pos_array)
            {
                std::cerr << "Unexpected position format" << std::endl;
                return;
            }
            auto* depth = new float[pos_array->valueCount()];
            auto to_vec3 = [&](vsg::vec4 v) { return vsg::vec3(v.x, v.y, v.z); };
            vsg::vec4 camera_pos = matrices[f].inv_view[2];
            camera_pos /= camera_pos.w;
            for (uint32_t i = 0; i < pos_array->valueCount(); ++i)
            {
                vsg::vec3 p = to_vec3(pos_array->data()[i]);
                depth[i] = length(to_vec3(camera_pos) - p);
            }
            g_buffers[f]->depth = vsg::floatArray2D::create(
                pos->width(), pos->height(), depth, vsg::Data::Layout{VK_FORMAT_R32_SFLOAT});
        }
        // load normal image
        snprintf(buff, sizeof(buff), normalFormat.c_str(), f);
        filename = findFile(buff, options);

        if (g_buffers[f]->normal = convert_normal_to_spherical(vsg::read_cast<vsg::vec4Array2D>(filename, options));
            !g_buffers[f]->normal.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return;
        }
        // load albedo image
        snprintf(buff, sizeof(buff), albedoFormat.c_str(), f);
        filename = findFile(buff, options);

        if (g_buffers[f]->albedo = vsg::read_cast<vsg::Data>(filename, options); !g_buffers[f]->albedo.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return;
        }
        g_buffers[f]->albedo = compress_albedo(g_buffers[f]->albedo);
        if (verbosity > 1)
        {
            std::cout << "GBuffer: Loaded frame " << f << std::endl << std::flush;
        }
    };
    {  // automatic join at the end of threads scope
        std::vector<std::future<void>> threads(num_frames);
        for (int f = 0; f < num_frames; ++f)
        {
            threads[f] = std::async(std::launch::async, exec_load, f);
        }
    }
    if (verbosity > 0)
    {
        std::cout << "Done loading GBuffer" << std::endl;
    }
    return g_buffers;
}

vsg::ref_ptr<vsg::Data> GBufferIO::convert_normal_to_spherical(vsg::ref_ptr<vsg::vec4Array2D> normals)
{
    if (!normals)
    {
        return {};
    }
    auto* res = new vsg::vec2[normals->valueCount()];
    for (uint32_t i = 0; i < normals->valueCount(); ++i)
    {
        vsg::vec4 cur_normal = normals->data()[i];
        // the normals are generally stored in correct full format
        // curNormal *= 2;
        // curNormal -= vsg::vec4(1, 1, 1, 0);
        res[i].x = acos(cur_normal.z);
        res[i].y = atan2(cur_normal.y, cur_normal.x);
    }
    return vsg::vec2Array2D::create(
        normals->width(), normals->height(), res, vsg::Data::Layout{VK_FORMAT_R32G32_SFLOAT});
}

vsg::ref_ptr<vsg::Data> GBufferIO::compress_albedo(vsg::ref_ptr<vsg::Data> in)
{
    auto* albedo = new vsg::ubvec4[in->valueCount()];
    if (vsg::ref_ptr<vsg::vec4Array2D> large_albedo = in.cast<vsg::vec4Array2D>())
    {
        for (uint32_t i = 0; i < in->valueCount(); ++i)
        {
            albedo[i] = large_albedo->data()[i] * 255.0F;
        }
    }
    else if (vsg::ref_ptr<vsg::uivec4Array2D> large_albedo = in.cast<vsg::uivec4Array2D>())
    {
        for (uint32_t i = 0; i < in->valueCount(); ++i)
        {
            albedo[i] = large_albedo->data()[i];
        }
    }
    else if (vsg::ref_ptr<vsg::usvec4Array2D> large_albedo = in.cast<vsg::usvec4Array2D>())
    {
        auto to_float = [](uint16_t h)
        {
            uint32_t t = ((h & 0x8000) << 16) | (((h & 0x7c00) + 0x1C000) << 13) | ((h & 0x03FF) << 13);
            return *reinterpret_cast<float*>(&t);
        };
        for (uint32_t i = 0; i < in->valueCount(); ++i)
        {
            vsg::usvec4& cur = large_albedo->data()[i];
            albedo[i] = vsg::vec4(to_float(cur.x), to_float(cur.y), to_float(cur.z), to_float(cur.w)) * 255.0F;
        }
    }
    return vsg::ubvec4Array2D::create(in->width(), in->height(), albedo, vsg::Data::Layout{VK_FORMAT_R8G8B8A8_UNORM});
}

bool GBufferIO::export_g_buffer(const std::string& positionFormat, const std::string& depthFormat,
    const std::string& normalFormat, const std::string& materialFormat, const std::string& albedoFormat, int num_frames,
    const OfflineGBuffers& g_buffers, const CameraMatricesVec& matrices, int verbosity)
{
    if (verbosity > 0)
    {
        std::cout << "Start exporting GBuffer" << std::endl;
    }
    auto options = vsg::Options::create(vsgXchange::openexr::create());
    bool fine = true;
    auto exec_store = [&](int f)
    {
        if (verbosity > 1)
        {
            std::cout << "GBuffer: Storing frame " << f << std::endl << std::flush;
        }
        char buff[200];
        std::string filename;
        // depth images
        if (!depthFormat.empty() != 0u)
        {
            snprintf(buff, sizeof(buff), depthFormat.c_str(), f);
            filename = buff;
            if (!write(g_buffers[f]->depth, filename, options))
            {
                std::cerr << "Failed to store image: " << filename << std::endl;
                fine = false;
                return;
            }
        }
        // position images
        if (!positionFormat.empty() != 0u)
        {
            snprintf(buff, sizeof(buff), positionFormat.c_str(), f);
            vsg::ref_ptr<vsg::Data> position
                = depth_to_position(g_buffers[f]->depth.cast<vsg::floatArray2D>(), matrices[f]);
            filename = buff;
            if (!write(position, filename, options))
            {
                std::cerr << "Failed to store image: " << filename << std::endl;
                fine = false;
                return;
            }
        }
        // normal images
        if (!normalFormat.empty() != 0u)
        {
            snprintf(buff, sizeof(buff), normalFormat.c_str(), f);
            filename = buff;
            if (!write(spherical_to_cartesian(g_buffers[f]->normal.cast<vsg::vec2Array2D>()), filename, options))
            {
                std::cerr << "Failed to store image: " << filename << std::endl;
                fine = false;
                return;
            }
        }
        // material images
        if (!materialFormat.empty() != 0u)
        {
            snprintf(buff, sizeof(buff), materialFormat.c_str(), f);
            filename = buff;
            if (!write(unorm_to_float(g_buffers[f]->material.cast<vsg::ubvec4Array2D>()), filename, options))
            {
                std::cerr << "Failed to store image: " << filename << std::endl;
                fine = false;
                return;
            }
        }
        // albedo images
        if (!albedoFormat.empty() != 0u)
        {
            snprintf(buff, sizeof(buff), albedoFormat.c_str(), f);
            filename = buff;
            if (!write(unorm_to_float(g_buffers[f]->albedo.cast<vsg::ubvec4Array2D>()), filename, options))
            {
                std::cerr << "Failed to store image: " << filename << std::endl;
                fine = false;
                return;
            }
        }
        if (verbosity > 1)
        {
            std::cout << "GBuffer: Stored frame " << f << std::endl << std::flush;
        }
    };
    {
        std::vector<std::future<void>> threads(num_frames);
        for (int f = 0; f < num_frames; ++f)
        {
            threads[f] = std::async(std::launch::async, exec_store, f);
        }
    }
    if (verbosity > 0)
    {
        std::cout << "Done exporting GBuffer" << std::endl;
    }
    return fine;
}

vsg::ref_ptr<vsg::Data> GBufferIO::spherical_to_cartesian(vsg::ref_ptr<vsg::vec2Array2D> normals)
{
    if (!normals)
    {
        return {};
    }
    auto* res = new vsg::vec4[normals->valueCount()];
    for (uint32_t i = 0; i < normals->valueCount(); ++i)
    {
        vsg::vec2 cur_normal = normals->data()[i];
        res[i].x = cos(cur_normal.y) * sin(cur_normal.x);
        res[i].y = sin(cur_normal.y) * sin(cur_normal.x);
        res[i].z = cos(cur_normal.x);
        res[i].w = 1;
    }
    return vsg::vec4Array2D::create(
        normals->width(), normals->height(), res, vsg::Data::Layout{VK_FORMAT_R32G32B32A32_SFLOAT});
}

vsg::ref_ptr<vsg::Data> GBufferIO::unorm_to_float(vsg::ref_ptr<vsg::ubvec4Array2D> array)
{
    if (!array)
    {
        return {};
    }
    auto* res = new vsg::vec4[array->valueCount()];
    for (uint32_t i = 0; i < array->valueCount(); ++i)
    {
        vsg::ubvec4 col = array->data()[i];
        res[i] = {static_cast<float>(col.x) / 255.F, static_cast<float>(col.y) / 255.F,
            static_cast<float>(col.z) / 255.F, static_cast<float>(col.w) / 255.F};
    }
    return vsg::vec4Array2D::create(
        array->width(), array->height(), res, vsg::Data::Layout{VK_FORMAT_R32G32B32A32_SFLOAT});
}

vsg::ref_ptr<vsg::Data> GBufferIO::depth_to_position(
    vsg::ref_ptr<vsg::floatArray2D> depths, const CameraMatrices& matrix)
{
    if (!depths)
    {
        return {};
    }
    if (!matrix.proj)
    {
        std::cout << "GBufferIO::depthToPosition: Camera matrix in wrong layout. Expected camera matrix with separate "
                     "projection matrix"
                  << std::endl;
        return {};
    }
    auto* res = new vsg::vec4[depths->valueCount()];
    auto to_vec3 = [&](vsg::vec4 v) { return vsg::vec3(v.x, v.y, v.z); };
    vsg::vec4 camera_pos = matrix.inv_view[3];
    for (uint32_t i = 0; i < depths->valueCount(); ++i)
    {
        uint32_t x = i % depths->width();
        uint32_t y = i / depths->width();
        vsg::vec2 p{(x + .5F) / depths->width() * 2 - 1, (y + .5F) / depths->height() * 2 - 1};
        vsg::vec4 dir = matrix.inv_proj.value() * vsg::vec4{p.x, p.y, 1, 1};
        dir.w = 0;
        vsg::vec3 direction = to_vec3(matrix.inv_view * normalize(dir));
        direction *= depths->data()[i];
        vsg::vec3 pos = to_vec3(camera_pos) + direction;
        res[i].x = pos.x;
        res[i].y = pos.y;
        res[i].z = pos.z;
        res[i].w = 1;
    }
    return vsg::vec4Array2D::create(
        depths->width(), depths->height(), res, vsg::Data::Layout{VK_FORMAT_R32G32B32A32_SFLOAT});
}

void OfflineIllumination::upload_to_illumination_buffer_command(
    vsg::ref_ptr<IlluminationBuffer>& illu_buffer, vsg::ref_ptr<vsg::Commands>& commands, vsg::Context& context)
{
    _staging_memory_buffer_pools = context.stagingMemoryBufferPools;
    if (!_noisy_staging)
    {
        setup_staging_buffer(illu_buffer->width, illu_buffer->height);
    }
    if (illu_buffer->illumination_images[0])
    {
        commands->addChild(
            CopyBufferToImage::create(_noisy_staging, illu_buffer->illumination_images[0]->imageInfoList.front(), 1));
        illu_buffer->illumination_images[0]->imageInfoList[0]->imageView->image->usage
            |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
}

void OfflineIllumination::download_from_illumination_buffer_command(
    vsg::ref_ptr<IlluminationBuffer>& illu_buffer, vsg::ref_ptr<vsg::Commands>& commands, vsg::Context& context)
{
    _staging_memory_buffer_pools = context.stagingMemoryBufferPools;
    if (!_noisy_staging)
    {
        setup_staging_buffer(illu_buffer->width, illu_buffer->height);
    }
    if (illu_buffer->illumination_images[0])
    {
        // transfer image layout for optimal transfer and memory barrier
        VkImageSubresourceRange resource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        auto mem_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0,
            illu_buffer->illumination_images[0]->imageInfoList.front()->imageView->image, resource_range);
        auto pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, mem_barrier);
        commands->addChild(pipeline_barrier);
        // copy image to buffer
        auto copy = vsg::CopyImageToBuffer::create();
        auto info = illu_buffer->illumination_images[0]->imageInfoList.front();
        copy->srcImage = info->imageView->image;
        copy->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copy->dstBuffer = _noisy_staging->buffer;
        copy->regions = {
            VkBufferImageCopy{_noisy_staging->offset, 0, 0,
                              VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, VkOffset3D{0, 0, 0},
                              info->imageView->image->extent}
        };
        commands->addChild(copy);
        // transfer image layout back
        mem_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0,
            illu_buffer->illumination_images[0]->imageInfoList.front()->imageView->image, resource_range);
        pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, mem_barrier);
        commands->addChild(pipeline_barrier);

        illu_buffer->illumination_images[0]->imageInfoList[0]->imageView->image->usage
            |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        _noisy_staging->buffer->usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
}

void OfflineIllumination::transfer_staging_data_to(vsg::ref_ptr<OfflineIllumination>& illu_buffer)
{
    if (!_staging_memory_buffer_pools)
    {
        std::cout << "Current offline illumination buffer has not been added to a command graph and is thus not able "
                     "to do transfer"
                  << std::endl;
        return;
    }
    auto device_id = _staging_memory_buffer_pools->device->deviceID;
    vsg::ref_ptr<vsg::Buffer> buffer(_noisy_staging->buffer);
    vsg::ref_ptr<vsg::DeviceMemory> memory(buffer->getDeviceMemory(device_id));
    if (!memory)
    {
        std::cout << "Error while transferring illumination staging memory data to offline illumination buffer."
                  << std::endl;
        return;
    }
    void* gpu_data;
    memory->map(buffer->getMemoryOffset(device_id) + _noisy_staging->offset, _noisy_staging->range, 0, &gpu_data);
    std::memcpy(illu_buffer->noisy->dataPointer(), gpu_data, static_cast<size_t>(_noisy_staging->range));
    memory->unmap();
}

void OfflineIllumination::transfer_staging_data_from(vsg::ref_ptr<OfflineIllumination>& illu_buffer)
{
    if (!_staging_memory_buffer_pools)
    {
        std::cout << "Current offline illumination buffer has not been added to a command graph and is thus not able "
                     "to do transfer"
                  << std::endl;
        return;
    }
    auto device_id = _staging_memory_buffer_pools->device->deviceID;
    vsg::ref_ptr<vsg::Buffer> buffer(_noisy_staging->buffer);
    vsg::ref_ptr<vsg::DeviceMemory> memory(buffer->getDeviceMemory(device_id));
    if (!memory)
    {
        std::cout << "Error while transferring illumination staging memory data from offline illumination buffer."
                  << std::endl;
        return;
    }
    memory->copy(buffer->getMemoryOffset(device_id) + _noisy_staging->offset, _noisy_staging->range,
        illu_buffer->noisy->dataPointer());
}

void OfflineIllumination::setup_staging_buffer(uint32_t width, uint32_t height)
{
    VkDeviceSize image_total_size = sizeof(vsg::vec4) * width * height;
    VkDeviceSize alignment = 16;
    VkMemoryPropertyFlags memory_property_flags
        = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    _noisy_staging = _staging_memory_buffer_pools->reserveBuffer(image_total_size, alignment,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE,
        memory_property_flags);
}

std::vector<vsg::ref_ptr<OfflineIllumination>> IlluminationBufferIO::import_illumination(
    const std::string& illumination_format, int num_frames, int verbosity)
{
    if (verbosity > 0)
    {
        std::cout << "Start loading Illumination" << std::endl;
    }
    auto options = vsg::Options::create(vsgXchange::openexr::create());
    std::vector<vsg::ref_ptr<OfflineIllumination>> illuminations(num_frames);
    auto exec_load = [&](int f)
    {
        if (verbosity > 1)
        {
            std::cout << "Illumination: Loading frame " << f << std::endl << std::flush;
        }
        char buff[100];
        std::string filename;
        // position images
        snprintf(buff, sizeof(buff), illumination_format.c_str(), f);
        filename = findFile(buff, options);

        illuminations[f] = OfflineIllumination::create();

        if (illuminations[f]->noisy = vsg::read_cast<vsg::Data>(filename, options); !illuminations[f]->noisy.valid())
        {
            std::cerr << "Failed to load image: " << filename << " texPath = " << buff << std::endl;
            return;
        }
        if (verbosity > 1)
        {
            std::cout << "Illumination: Loaded frame " << f << std::endl << std::flush;
        }
    };
    {
        std::vector<std::future<void>> threads(num_frames);
        for (int f = 0; f < num_frames; ++f)
        {
            threads[f] = std::async(std::launch::async, exec_load, f);
        }
    }
    if (verbosity > 0)
    {
        std::cout << "Done loading Illumination" << std::endl;
    }
    return illuminations;
}

bool IlluminationBufferIO::export_illumination(
    const std::string& illumination_format, int num_frames, const OfflineIlluminations& illus, int verbosity)
{
    if (verbosity > 0)
    {
        std::cout << "Start exporting Illumination" << std::endl;
    }
    auto options = vsg::Options::create(vsgXchange::openexr::create());
    bool fine = true;
    auto exec_store = [&](int f)
    {
        if (verbosity > 1)
        {
            std::cout << "IlluminationBuffer: Storing frame" << f << std::endl << std::flush;
        }
        char buff[200];
        std::string filename;
        snprintf(buff, sizeof(buff), illumination_format.c_str(), f);
        filename = buff;
        if (!write(illus[f]->noisy, filename, options))
        {
            std::cout << "Faled to store image: " << filename << std::endl;
            fine = false;
            return;
        }
        if (verbosity > 1)
        {
            std::cout << "IlluminationBuffer: Stored frame" << f << std::endl << std::flush;
        }
    };
    {  // automatic join at the end of threads scope
        std::vector<std::future<void>> threads(num_frames);
        for (int f = 0; f < num_frames; ++f)
        {
            threads[f] = std::async(std::launch::async, exec_store, f);
        }
    }
    if (verbosity > 0)
    {
        std::cout << "Done exporting Illumination" << std::endl;
    }
    return fine;
}

CameraMatricesVec MatrixIO::import_matrices(const std::string& matrix_path)
{
    // TODO: temporary implementation to parse matrices from BMFRs dataset
    std::ifstream f(matrix_path);
    if (!f)
    {
        std::cout << "Matrix file " << matrix_path << " unable to open." << std::endl;
        return {};
    }

    CameraMatricesVec matrices;

    auto to_mat4 = [](const nlohmann::json& json)
    {
        vsg::mat4 ret;
        int c = 0;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                ret[i][j] = json.at(c++);
            }
        }
        return ret;
    };
    if (matrix_path.substr(matrix_path.find_last_of(".")) == ".json")
    {  // json parsing
        nlohmann::json json;
        f >> json;
        int amt_of_frames = json["amtOfFrames"];
        matrices.resize(amt_of_frames);
        for (int i = 0; i < amt_of_frames; ++i)
        {
            auto matrix = json["matrices"][i];
            matrices[i].view = to_mat4(matrix["view"]);
            matrices[i].inv_view = to_mat4(matrix["invView"]);
            if (matrix["type"] == "ModelView+Projection")
            {
                matrices[i].proj = to_mat4(matrix["proj"]);
                matrices[i].inv_proj = to_mat4(matrix["invProj"]);
            }
        }
    }
    else
    {
        std::string cur;
        uint32_t count = 0;
        vsg::mat4 tmp;
        while (!f.eof())
        {
            f >> cur;
            if (cur.back() == ',')
            {
                cur.pop_back();
            }
            if (cur.front() == '{')
            {
                cur = cur.substr(1, cur.size() - 1);
            }
            if ((!cur.empty() != 0u) && ((isdigit(cur[0]) != 0) || cur[0] == '-'))
            {
                tmp[count / 4][count % 4] = std::stof(cur);
                ++count;
                if (count == 16)
                {
                    matrices.push_back({tmp, inverse(tmp)});
                    count = 0;
                }
            }
        }
    }

    return matrices;
}

bool MatrixIO::export_matrices(const std::string& matrix_path, const CameraMatricesVec& matrices)
{
    std::ofstream f(matrix_path, std::ofstream::out);
    if (!f)
    {
        std::cout << "Matrix file " << matrix_path << " unable to open." << std::endl;
        return false;
    }

    auto to_json_array = [](const vsg::mat4& m)
    {
        nlohmann::json ret;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                ret.push_back(m[i][j]);
            }
        }
        return ret;
    };
    nlohmann::json json;
    nlohmann::json json_mat = nlohmann::json::array();  // each matrix is stored as a json object
    for (const auto& m : matrices)
    {
        std::string type = "ModelViewProjection";
        nlohmann::json obj;
        if (m.proj)
        {
            type = "ModelView+Projection";
        }
        obj["type"] = type;
        obj["storageType"] = "ColumnMajor";
        obj["view"] = to_json_array(m.view);
        obj["invView"] = to_json_array(m.inv_view);
        if (m.proj)
        {
            obj["proj"] = to_json_array(m.proj.value());
            obj["invProj"] = to_json_array(m.inv_proj.value());
        }
        json_mat.push_back(obj);
    }
    json["amtOfFrames"] = matrices.size();
    json["matrices"] = json_mat;

    f << json.dump(4);  // dump with pretty printing

    return true;
}

void OfflineGBuffer::upload_to_g_buffer_command(
    vsg::ref_ptr<GBuffer>& g_buffer, vsg::ref_ptr<vsg::Commands> commands, vsg::Context& context)
{
    _staging_memory_buffer_pools = context.stagingMemoryBufferPools;
    if (!_depth_staging || !_normal_staging || !_albedo_staging || !_material_staging)
    {
        setup_staging_buffer(g_buffer->width, g_buffer->height);
    }
    if (g_buffer->depth)
    {
        commands->addChild(CopyBufferToImage::create(_depth_staging, g_buffer->depth->imageInfoList.front(), 1));
        g_buffer->depth->imageInfoList[0]->imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if (g_buffer->normal)
    {
        commands->addChild(CopyBufferToImage::create(_normal_staging, g_buffer->normal->imageInfoList.front(), 1));
        g_buffer->normal->imageInfoList[0]->imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if (g_buffer->albedo)
    {
        commands->addChild(CopyBufferToImage::create(_albedo_staging, g_buffer->albedo->imageInfoList.front(), 1));
        g_buffer->albedo->imageInfoList[0]->imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if (g_buffer->material)
    {
        commands->addChild(CopyBufferToImage::create(_material_staging, g_buffer->material->imageInfoList.front(), 1));
        g_buffer->material->imageInfoList[0]->imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
}

void OfflineGBuffer::download_from_g_buffer_command(
    vsg::ref_ptr<GBuffer>& g_buffer, vsg::ref_ptr<vsg::Commands> commands, vsg::Context& context)
{
    _staging_memory_buffer_pools = context.stagingMemoryBufferPools;
    if (!_depth_staging || !_normal_staging || !_albedo_staging || !_material_staging)
    {
        setup_staging_buffer(g_buffer->width, g_buffer->height);
    }
    if (g_buffer->depth)
    {
        // transfer image layout for optimal transfer and memory barrier
        VkImageSubresourceRange resource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        auto mem_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0,
            g_buffer->depth->imageInfoList.front()->imageView->image, resource_range);
        auto pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, mem_barrier);
        commands->addChild(pipeline_barrier);
        auto copy = vsg::CopyImageToBuffer::create();
        auto info = g_buffer->depth->imageInfoList.front();
        copy->srcImage = info->imageView->image;
        copy->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copy->dstBuffer = _depth_staging->buffer;
        copy->regions = {
            VkBufferImageCopy{_depth_staging->offset, 0, 0,
                              VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, VkOffset3D{0, 0, 0},
                              info->imageView->image->extent}
        };
        commands->addChild(copy);
        // transfer image layout back
        mem_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0,
            g_buffer->depth->imageInfoList.front()->imageView->image, resource_range);
        pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, mem_barrier);
        commands->addChild(pipeline_barrier);
        g_buffer->depth->imageInfoList[0]->imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if (g_buffer->normal)
    {
        VkImageSubresourceRange resource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        auto mem_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0,
            g_buffer->normal->imageInfoList.front()->imageView->image, resource_range);
        auto pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, mem_barrier);
        commands->addChild(pipeline_barrier);
        auto copy = vsg::CopyImageToBuffer::create();
        auto info = g_buffer->normal->imageInfoList.front();
        copy->srcImage = info->imageView->image;
        copy->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copy->dstBuffer = _normal_staging->buffer;
        copy->regions = {
            VkBufferImageCopy{_normal_staging->offset, 0, 0,
                              VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, VkOffset3D{0, 0, 0},
                              info->imageView->image->extent}
        };
        commands->addChild(copy);
        // transfer image layout back
        mem_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0,
            g_buffer->normal->imageInfoList.front()->imageView->image, resource_range);
        pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, mem_barrier);
        commands->addChild(pipeline_barrier);
        g_buffer->normal->imageInfoList[0]->imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if (g_buffer->albedo)
    {
        VkImageSubresourceRange resource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        auto mem_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0,
            g_buffer->albedo->imageInfoList.front()->imageView->image, resource_range);
        auto pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, mem_barrier);
        commands->addChild(pipeline_barrier);
        auto copy = vsg::CopyImageToBuffer::create();
        auto info = g_buffer->albedo->imageInfoList.front();
        copy->srcImage = info->imageView->image;
        copy->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copy->dstBuffer = _albedo_staging->buffer;
        copy->regions = {
            VkBufferImageCopy{_albedo_staging->offset, 0, 0,
                              VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, VkOffset3D{0, 0, 0},
                              info->imageView->image->extent}
        };
        commands->addChild(copy);
        // transfer image layout back
        mem_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0,
            g_buffer->albedo->imageInfoList.front()->imageView->image, resource_range);
        pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, mem_barrier);
        commands->addChild(pipeline_barrier);
        g_buffer->albedo->imageInfoList[0]->imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if (g_buffer->material)
    {
        VkImageSubresourceRange resource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        auto mem_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0,
            g_buffer->material->imageInfoList.front()->imageView->image, resource_range);
        auto pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, mem_barrier);
        commands->addChild(pipeline_barrier);
        auto copy = vsg::CopyImageToBuffer::create();
        auto info = g_buffer->material->imageInfoList.front();
        copy->srcImage = info->imageView->image;
        copy->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copy->dstBuffer = _material_staging->buffer;
        copy->regions = {
            VkBufferImageCopy{_material_staging->offset, 0, 0,
                              VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, VkOffset3D{0, 0, 0},
                              info->imageView->image->extent}
        };
        commands->addChild(copy);
        // transfer image layout back
        mem_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0,
            g_buffer->material->imageInfoList.front()->imageView->image, resource_range);
        pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, mem_barrier);
        commands->addChild(pipeline_barrier);
        g_buffer->material->imageInfoList[0]->imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
}

void OfflineGBuffer::transfer_staging_data_to(vsg::ref_ptr<OfflineGBuffer> other)
{
    if (!_staging_memory_buffer_pools)
    {
        std::cout << "Current offline buffer has not been added to a command graph and is thus not able to do transfer"
                  << std::endl;
        return;
    }
    auto device_id = _staging_memory_buffer_pools->device->deviceID;
    vsg::ref_ptr<vsg::Buffer> buffer(_depth_staging->buffer);
    vsg::ref_ptr<vsg::DeviceMemory> memory(buffer->getDeviceMemory(device_id));
    if (!memory)
    {
        std::cout << "Error while transferring depth staging memory data to offline gBuffer." << std::endl;
        return;
    }
    void* gpu_data;
    memory->map(buffer->getMemoryOffset(device_id) + _depth_staging->offset, _depth_staging->range, 0, &gpu_data);
    std::memcpy(other->depth->dataPointer(), gpu_data, static_cast<size_t>(_depth_staging->range));
    memory->unmap();

    buffer = vsg::ref_ptr<vsg::Buffer>(_normal_staging->buffer);
    memory = vsg::ref_ptr<vsg::DeviceMemory>(buffer->getDeviceMemory(device_id));
    if (!memory)
    {
        std::cout << "Error while transferring normal staging memory data to offline gBuffer." << std::endl;
        return;
    }
    memory->map(buffer->getMemoryOffset(device_id) + _normal_staging->offset, _normal_staging->range, 0, &gpu_data);
    std::memcpy(other->normal->dataPointer(), gpu_data, static_cast<size_t>(_normal_staging->range));
    memory->unmap();

    buffer = vsg::ref_ptr<vsg::Buffer>(_albedo_staging->buffer);
    memory = vsg::ref_ptr<vsg::DeviceMemory>(buffer->getDeviceMemory(device_id));
    if (!memory)
    {
        std::cout << "Error while transferring albedo staging memory data to offline gBuffer." << std::endl;
        return;
    }
    memory->map(buffer->getMemoryOffset(device_id) + _albedo_staging->offset, _albedo_staging->range, 0, &gpu_data);
    std::memcpy(other->albedo->dataPointer(), gpu_data, static_cast<size_t>(_albedo_staging->range));
    memory->unmap();

    buffer = vsg::ref_ptr<vsg::Buffer>(_material_staging->buffer);
    memory = vsg::ref_ptr<vsg::DeviceMemory>(buffer->getDeviceMemory(device_id));
    if (!memory)
    {
        std::cout << "Error while transferring material staging memory data to offline gBuffer." << std::endl;
        return;
    }
    memory->map(buffer->getMemoryOffset(device_id) + _material_staging->offset, _material_staging->range, 0, &gpu_data);
    std::memcpy(other->material->dataPointer(), gpu_data, static_cast<size_t>(_material_staging->range));
    memory->unmap();
}

void OfflineGBuffer::transfer_staging_data_from(vsg::ref_ptr<OfflineGBuffer> other)
{
    if (!_staging_memory_buffer_pools)
    {
        std::cout << "Current offline buffer has not been added to a command graph and is thus not able to do transfer"
                  << std::endl;
        return;
    }
    auto device_id = _staging_memory_buffer_pools->device->deviceID;
    vsg::ref_ptr<vsg::Buffer> buffer(_depth_staging->buffer);
    vsg::ref_ptr<vsg::DeviceMemory> memory(buffer->getDeviceMemory(device_id));
    if (!memory)
    {
        std::cout << "Error while transferring depth staging memory data to offline gBuffer." << std::endl;
        return;
    }
    memory->copy(buffer->getMemoryOffset(device_id) + _depth_staging->offset, _depth_staging->range,
        other->depth->dataPointer());

    buffer = vsg::ref_ptr<vsg::Buffer>(_normal_staging->buffer);
    memory = vsg::ref_ptr<vsg::DeviceMemory>(buffer->getDeviceMemory(device_id));
    if (!memory)
    {
        std::cout << "Error while transferring normal staging memory data to offline gBuffer." << std::endl;
        return;
    }
    memory->copy(buffer->getMemoryOffset(device_id) + _normal_staging->offset, _normal_staging->range,
        other->normal->dataPointer());

    buffer = vsg::ref_ptr<vsg::Buffer>(_albedo_staging->buffer);
    memory = vsg::ref_ptr<vsg::DeviceMemory>(buffer->getDeviceMemory(device_id));
    if (!memory)
    {
        std::cout << "Error while transferring albedo staging memory data to offline gBuffer." << std::endl;
        return;
    }
    memory->copy(buffer->getMemoryOffset(device_id) + _albedo_staging->offset, _albedo_staging->range,
        other->albedo->dataPointer());

    buffer = vsg::ref_ptr<vsg::Buffer>(_material_staging->buffer);
    memory = vsg::ref_ptr<vsg::DeviceMemory>(buffer->getDeviceMemory(device_id));
    if (!memory)
    {
        std::cout << "Error while transferring material staging memory data to offline gBuffer." << std::endl;
        return;
    }
    if (other->material)
    {
        memory->copy(buffer->getMemoryOffset(device_id) + _material_staging->offset, _material_staging->range,
            other->material->dataPointer());
    }
}

void OfflineGBuffer::setup_staging_buffer(uint32_t width, uint32_t height)
{
    VkDeviceSize image_total_size = sizeof(float) * width * height;
    VkDeviceSize alignment = 4;
    VkMemoryPropertyFlags memory_property_flags
        = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    _depth_staging = _staging_memory_buffer_pools->reserveBuffer(image_total_size, alignment,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE,
        memory_property_flags);

    image_total_size = sizeof(vsg::vec2) * width * height;
    alignment = 8;  // sizeof vsg::vec2
    _normal_staging = _staging_memory_buffer_pools->reserveBuffer(image_total_size, alignment,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE,
        memory_property_flags);

    image_total_size = sizeof(vsg::ubvec4) * width * height;
    alignment = 4;
    _albedo_staging = _staging_memory_buffer_pools->reserveBuffer(image_total_size, alignment,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE,
        memory_property_flags);

    _material_staging = _staging_memory_buffer_pools->reserveBuffer(image_total_size, alignment,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE,
        memory_property_flags);
}
