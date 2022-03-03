#include "scene/CountTrianglesVisitor.hpp"
#include "renderModules/PipelineStructs.hpp"
#include "renderModules/PBRTPipeline.hpp"
#include "renderModules/Accumulator.hpp"
#include "renderModules/FormatConverter.hpp"
#include "renderModules/denoisers/BFR.hpp"
#include "renderModules/denoisers/BFRBlender.hpp"
#include "renderModules/denoisers/BMFR.hpp"
#include "renderModules/Taa.hpp"
#include "io/RenderIO.hpp"
#include "Gui.hpp"

#include <vsg/all.h>
#include <vsgXchange/images.h>
#include <vsgXchange/models.h>

#include <nlohmann/json.hpp>

#include <iostream>

#include "../external/vsgXchange/src/assimp/3DFrontImporter.h"

#define _DEBUG

class RayTracingPushConstantsValue
    : public vsg::Inherit<vsg::Value<RayTracingPushConstants>, RayTracingPushConstantsValue>
{
public:
    RayTracingPushConstantsValue() = default;
};

enum class DenoisingType
{
    NONE,
    BMFR,
    BFR,
    SVG
};
DenoisingType denoising_type = DenoisingType::NONE;

enum class DenoisingBlockSize
{
    X8,
    X16,
    X32,
    X64,
    X8X16X32
};
DenoisingBlockSize denoising_block_size = DenoisingBlockSize::X32;

class LoggingRedirectSentry
{
public:
    LoggingRedirectSentry(std::ostream* out_stream, std::streambuf* original_buffer)
        : _out_stream(out_stream), _original_buffer(original_buffer)
    {
    }
    ~LoggingRedirectSentry()
    {
        // reset to standard output
        _out_stream->rdbuf(_original_buffer);
    }

private:
    std::ostream* _out_stream;
    std::streambuf* _original_buffer;
};

int main(int argc, char** argv)
{
    try
    {
        vsg::CommandLine arguments(&argc, argv);

        // load config
        nlohmann::json config_json;
        {
            auto config_path = arguments.value(std::string(), "--config");
            if (!config_path.empty())
            {
                std::ifstream scene_file(config_path);
                if (!scene_file)
                {
                    std::cout << "Failed to load config file " << config_path << "." << std::endl;
                    return 1;
                }
                scene_file >> config_json;
            }
        }

        // ensure that cout and cerr are reset to their standard output when main() is exited
        LoggingRedirectSentry cout_sentry(&std::cout, std::cout.rdbuf());
        LoggingRedirectSentry cerr_sentry(&std::cerr, std::cerr.rdbuf());
        std::ofstream out("out_log.txt");
        std::ofstream err_log("err_log.txt");
        if (arguments.read({"--log", "-l"}))
        {
            // redirect cout and cerr to log files
            std::cout.rdbuf(out.rdbuf());
            std::cerr.rdbuf(err_log.rdbuf());
        }

        auto window_traits = vsg::WindowTraits::create();
        window_traits->windowTitle = "VulkanPBRT";
        window_traits->debugLayer = arguments.read({"--debug", "-d"});
        window_traits->apiDumpLayer = arguments.read({"--api", "-a"});
        window_traits->fullscreen = arguments.read({"--fullscreen", "-fs"});
        if (arguments.read({"--window", "-w"}, window_traits->width, window_traits->height))
        {
            window_traits->fullscreen = false;
        }
        arguments.read("--screen", window_traits->screenNum);

        auto num_frames = arguments.value(-1, "-f");
        auto samples_per_pixel = arguments.value(1, "--spp");
        auto depth_path = arguments.value(std::string(), "--depths");
        auto export_depth_path = arguments.value(std::string(), "--exportDepth");
        auto position_path = arguments.value(std::string(), "--positions");
        auto export_position_path = arguments.value(std::string(), "--exportPosition");
        auto normal_path = arguments.value(std::string(), "--normals");
        auto export_normal_path = arguments.value(std::string(), "--exportNormal");
        auto albedo_path = arguments.value(std::string(), "--albedos");
        auto export_albedo_path = arguments.value(std::string(), "--exportAlbedo");
        auto material_path = arguments.value(std::string(), "--materials");
        auto export_material_path = arguments.value(std::string(), "--exportMaterial");
        auto illumination_path = arguments.value(std::string(), "--illuminations");
        auto export_illumination_path = arguments.value(std::string(), "--exportIllumination");
        auto matrices_path = arguments.value(std::string(), "--matrices");
        auto export_matrices_path = arguments.value(std::string(), "--exportMatrices");
        auto scene_filename = arguments.value(std::string(), "-i");
        bool use_external_buffers = !normal_path.empty();
        bool export_illumination = static_cast<unsigned int>(!export_illumination_path.empty()) != 0U;
        bool export_g_buffer = !export_normal_path.empty() || !export_depth_path.empty()
                               || !export_position_path.empty() || !export_albedo_path.empty()
                               || !export_material_path.empty();
        bool store_matrices = export_g_buffer || !export_matrices_path.empty();
        if (scene_filename.empty() && !use_external_buffers)
        {
            std::cout << "Missing input parameter \"-i <path_to_model>\"." << std::endl;
        }
        if (arguments.read("m"))
        {
            scene_filename = "models/raytracing_scene.vsgt";
        }
        if (arguments.errors())
        {
            return arguments.writeErrorMessages(std::cerr);
        }

        std::string denoising_type_str;
        if (arguments.read("--denoiser", denoising_type_str))
        {
            if (denoising_type_str == "bmfr")
            {
                denoising_type = DenoisingType::BMFR;
            }
            else if (denoising_type_str == "bfr")
            {
                denoising_type = DenoisingType::BFR;
            }
            else if (denoising_type_str == "svgf")
            {
                denoising_type = DenoisingType::SVG;
            }
            else if (denoising_type_str == "none")
            {
            }
            else
            {
                std::cout << "Unknown denoising type: " << denoising_type_str << std::endl;
            }
        }
        bool use_taa = arguments.read("--taa");
        bool use_fly_navigation = arguments.read("--fly");
#ifdef _DEBUG
        // overwriting command line options for debug
        window_traits->debugLayer = true;
        window_traits->width = 1800;
        window_traits->height = 990;
#endif
        window_traits->queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        window_traits->imageAvailableSemaphoreWaitFlag = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        window_traits->swapchainPreferences.imageUsage
            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        window_traits->deviceExtensionNames
            = {VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
                VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
                VK_KHR_SPIRV_1_4_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME};
        window_traits->vulkanVersion = VK_API_VERSION_1_2;
        auto& enabled_acceleration_structure_features
            = window_traits->deviceFeatures->get<VkPhysicalDeviceAccelerationStructureFeaturesKHR,
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR>();
        auto& enabled_ray_tracing_pipeline_features
            = window_traits->deviceFeatures->get<VkPhysicalDeviceRayTracingPipelineFeaturesKHR,
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR>();
        enabled_acceleration_structure_features.accelerationStructure = VK_TRUE;
        enabled_ray_tracing_pipeline_features.rayTracingPipeline = VK_TRUE;
        auto& enabled_physical_device_vk12_feature
            = window_traits->deviceFeatures
                  ->get<VkPhysicalDeviceVulkan12Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES>();
        enabled_physical_device_vk12_feature.runtimeDescriptorArray = VK_TRUE;
        enabled_physical_device_vk12_feature.bufferDeviceAddress = VK_TRUE;
        enabled_physical_device_vk12_feature.descriptorIndexing = VK_TRUE;
        enabled_physical_device_vk12_feature.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
        enabled_physical_device_vk12_feature.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

        // load scene or images
        vsg::ref_ptr<vsg::Node> loaded_scene;
        std::vector<vsg::ref_ptr<OfflineGBuffer>> offline_g_buffers;
        std::vector<vsg::ref_ptr<OfflineIllumination>> offline_illuminations;
        std::vector<CameraMatrices> camera_matrices;
        if (!use_external_buffers)
        {
            AI3DFrontImporter::ReadConfig(config_json);
            auto options = vsg::Options::create(vsgXchange::assimp::create(), vsgXchange::dds::create(),
                vsgXchange::stbi::create());  // using the assimp loader
            loaded_scene = vsg::read_cast<vsg::Node>(scene_filename, options);
            if (!loaded_scene)
            {
                std::cout << "Scene not found: " << scene_filename << std::endl;
                return 1;
            }
        }
        else
        {
            if (num_frames <= 0)
            {
                std::cout << "No number of frames given. For usage of external GBuffer and Illumination information "
                             "use \"-f\" to inform about the number of frames."
                          << std::endl;
                return 1;
            }
            if (matrices_path.empty())
            {
                std::cout << "Camera matrices are missing. Insert location of file with camera information via "
                             "\"--matrices\"."
                          << std::endl;
                return 1;
            }
            camera_matrices = MatrixIO::import_matrices(matrices_path);
            if (camera_matrices.empty())
            {
                std::cout << "Camera matrices could not be loaded" << std::endl;
                return 1;
            }
            if (!position_path.empty())
            {
                offline_g_buffers = GBufferIO::import_g_buffer_position(
                    position_path, normal_path, material_path, albedo_path, camera_matrices, num_frames);
            }
            else
            {
                offline_g_buffers
                    = GBufferIO::import_g_buffer_depth(depth_path, normal_path, material_path, albedo_path, num_frames);
            }
            offline_illuminations = IlluminationBufferIO::import_illumination(illumination_path, num_frames);
            window_traits->width = offline_g_buffers[0]->depth->width();
            window_traits->height = offline_g_buffers[0]->depth->height();
        }
        if (export_illumination)
        {
            if (num_frames <= 0)
            {
                std::cout << "No number of frames given. For usage of Illumination export use \"-f\" to inform about "
                             "the number of frames."
                          << std::endl;
                return 1;
            }
            if (offline_illuminations.empty())
            {
                offline_illuminations.resize(num_frames);
                for (auto& i : offline_illuminations)
                {
                    i = OfflineIllumination::create();
                    i->noisy = vsg::vec4Array2D::create(window_traits->width, window_traits->height);
                }
            }
        }
        if (export_g_buffer)
        {
            if (num_frames <= 0)
            {
                std::cout << "No number of frames given. For usage of GBuffer export use \"-f\" to inform about the "
                             "number of frames."
                          << std::endl;
                return 1;
            }
            if (offline_g_buffers.empty())
            {
                offline_g_buffers.resize(num_frames);
                for (auto& i : offline_g_buffers)
                {
                    i = OfflineGBuffer::create();
                    i->depth = vsg::floatArray2D::create(window_traits->width, window_traits->height);
                    i->normal = vsg::vec2Array2D::create(window_traits->width, window_traits->height);
                    i->albedo = vsg::ubvec4Array2D::create(window_traits->width, window_traits->height);
                    i->material = vsg::ubvec4Array2D::create(window_traits->width, window_traits->height);
                }
            }
        }
        if (store_matrices)
        {
            camera_matrices.resize(num_frames);
            for (auto& matrix : camera_matrices)
            {
                matrix.proj = vsg::mat4();
                matrix.inv_proj = vsg::mat4();
            }
        }

        auto window = vsg::Window::create(window_traits);
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }
        auto viewer = vsg::Viewer::create();
        viewer->addWindow(window);

        vsg::ref_ptr<vsg::Device> device(window->getOrCreateDevice());

        // setting a custom render pass for imgui non clear rendering
        {
            vsg::AttachmentDescription color_attachment = vsg::defaultColorAttachment(window->surfaceFormat().format);
            color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color_attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            vsg::AttachmentDescription depth_attachment = vsg::defaultDepthAttachment(window->depthFormat());
            depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            vsg::RenderPass::Attachments attachments{color_attachment, depth_attachment};

            VkAttachmentReference color_attachment_ref = {};
            color_attachment_ref.attachment = 0;
            color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentReference depth_attachment_ref = {};
            depth_attachment_ref.attachment = 1;
            depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            vsg::SubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachments.emplace_back(color_attachment_ref);
            subpass.depthStencilAttachments.emplace_back(depth_attachment_ref);

            vsg::RenderPass::Subpasses subpasses{subpass};

            // image layout transition
            VkSubpassDependency color_dependency = {};
            color_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            color_dependency.dstSubpass = 0;
            color_dependency.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            color_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            color_dependency.srcAccessMask = 0;
            color_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            color_dependency.dependencyFlags = 0;

            // depth buffer is shared between swap chain images
            VkSubpassDependency depth_dependency = {};
            depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            depth_dependency.dstSubpass = 0;
            depth_dependency.srcStageMask
                = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            depth_dependency.dstStageMask
                = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            depth_dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depth_dependency.dstAccessMask
                = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depth_dependency.dependencyFlags = 0;

            vsg::RenderPass::Dependencies dependencies{color_dependency, depth_dependency};

            auto render_pass = vsg::RenderPass::create(device, attachments, subpasses, dependencies);
            window->setRenderPass(render_pass);
        }

        // create camera matrices
        auto perspective = vsg::Perspective::create(
            60, static_cast<double>(window_traits->width) / static_cast<double>(window_traits->height), .1, 1000);
        auto look_at = vsg::LookAt::create(vsg::dvec3(0.0, -3, 1), vsg::dvec3(0.0, 0.0, 1), vsg::dvec3(0.0, 0.0, 1.0));

        // set push constants
        auto ray_tracing_push_constants_value = RayTracingPushConstantsValue::create();
        ray_tracing_push_constants_value->value().proj_inverse = perspective->inverse();
        ray_tracing_push_constants_value->value().view_inverse = look_at->inverse();
        ray_tracing_push_constants_value->value().prev_view = look_at->transform();
        ray_tracing_push_constants_value->value().frame_number = 0;
        ray_tracing_push_constants_value->value().sample_number = 0;
        auto push_constants
            = vsg::PushConstants::create(VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, ray_tracing_push_constants_value);
        auto compute_constants
            = vsg::PushConstants::create(VK_SHADER_STAGE_COMPUTE_BIT, 0, ray_tracing_push_constants_value);

        vsg::ref_ptr<GBuffer> g_buffer;
        vsg::ref_ptr<IlluminationBuffer> illumination_buffer;
        vsg::ref_ptr<AccumulationBuffer> accumulation_buffer;
        bool write_g_buffer;
        if (denoising_type != DenoisingType::NONE)
        {
            write_g_buffer = true;
            g_buffer = GBuffer::create(window_traits->width, window_traits->height);
            illumination_buffer
                = IlluminationBufferDemodulatedFloat::create(window_traits->width, window_traits->height);
        }
        else
        {
            write_g_buffer = false;
            illumination_buffer = IlluminationBufferFinalFloat::create(window_traits->width, window_traits->height);
        }
        if (export_illumination && !g_buffer)
        {
            write_g_buffer = true;
            g_buffer = GBuffer::create(window_traits->width, window_traits->height);
        }
        if (use_taa && !accumulation_buffer)
        {
            // TODO: need the velocity buffer
        }

        // raytracing pipeline setup
        uint32_t max_recursion_depth = 2;
        vsg::ref_ptr<PBRTPipeline> pbrt_pipeline;
        if (!use_external_buffers)
        {
            pbrt_pipeline = PBRTPipeline::create(
                loaded_scene, g_buffer, illumination_buffer, write_g_buffer, RayTracingRayOrigin::CAMERA);

            // setup tlas
            vsg::BuildAccelerationStructureTraversal build_accel_struct(device);
            loaded_scene->accept(build_accel_struct);
            pbrt_pipeline->set_tlas(build_accel_struct.tlas);
        }
        else
        {
            if (!g_buffer)
            {
                g_buffer = GBuffer::create(offline_g_buffers[0]->depth->width(), offline_g_buffers[0]->depth->height());
            }
            switch (offline_illuminations[0]->noisy->getLayout().format)
            {
            case VK_FORMAT_R16G16B16A16_SFLOAT:
                illumination_buffer = IlluminationBufferDemodulated::create(
                    offline_illuminations[0]->noisy->width(), offline_illuminations[0]->noisy->height());
                break;
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                illumination_buffer = IlluminationBufferDemodulatedFloat::create(
                    offline_illuminations[0]->noisy->width(), offline_illuminations[0]->noisy->height());
                break;
            default:
                std::cout << "Offline illumination buffer image format not compatible" << std::endl;
                return 1;
            }
        }
        // -------------------------------------------------------------------------------------
        // image layout conversions and correct binding of different denoising tequniques
        // -------------------------------------------------------------------------------------
        vsg::CompileTraversal image_layout_compile(window);
        auto commands = vsg::Commands::create();
        auto offline_g_buffer_stager = OfflineGBuffer::create();
        auto offline_illumination_buffer_stager = OfflineIllumination::create();
        vsg::ref_ptr<vsg::QueryPool> query_pool;
        if (pbrt_pipeline)
        {
            query_pool = vsg::QueryPool::create();  // standard init has 1 timestamp place
            query_pool->queryCount = 2;
            auto reset_query = vsg::ResetQueryPool::create(query_pool);
            auto write1 = vsg::WriteTimestamp::create(query_pool, 0, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
            auto write2 = vsg::WriteTimestamp::create(query_pool, 1, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
            commands->addChild(reset_query);
            commands->addChild(write1);
            pbrt_pipeline->add_trace_rays_to_command_graph(commands, push_constants);
            commands->addChild(write2);
            illumination_buffer = pbrt_pipeline->get_illumination_buffer();
        }
        else
        {
            if (offline_g_buffers.size() < num_frames || offline_illuminations.size() < num_frames)
            {
                std::cout << "Missing offline GBuffer or offline Illumination Buffer info" << std::endl;
                return 1;
            }
            offline_g_buffer_stager->upload_to_g_buffer_command(g_buffer, commands, image_layout_compile.context);
            offline_illumination_buffer_stager->upload_to_illumination_buffer_command(
                illumination_buffer, commands, image_layout_compile.context);
        }

        vsg::ref_ptr<Accumulator> accumulator;
        if (denoising_type != DenoisingType::NONE)
        {
            accumulator = Accumulator::create(g_buffer, illumination_buffer, !use_external_buffers);
            accumulator->add_dispatch_to_command_graph(commands);
            accumulation_buffer = accumulator->accumulation_buffer;
            illumination_buffer->compile(image_layout_compile.context);
            illumination_buffer->update_image_layouts(image_layout_compile.context);
            illumination_buffer
                = accumulator->accumulated_illumination;  // swap illumination buffer to accumulated illumination for
                                                          // correct use in the following pipelines
        }

        vsg::ref_ptr<vsg::DescriptorImage> final_descriptor_image;
        switch (denoising_type)
        {
        case DenoisingType::NONE:
            final_descriptor_image = illumination_buffer->illumination_images[0];
            break;
        case DenoisingType::BFR:
            switch (denoising_block_size)
            {
            case DenoisingBlockSize::X8:
                {
                    auto bfr8 = BFR::create(window_traits->width, window_traits->height, 8, 8, g_buffer,
                        illumination_buffer, accumulation_buffer);
                    bfr8->compile(image_layout_compile.context);
                    bfr8->update_image_layouts(image_layout_compile.context);
                    bfr8->add_dispatch_to_command_graph(commands, compute_constants);
                    final_descriptor_image = bfr8->get_final_descriptor_image();
                    break;
                }
            case DenoisingBlockSize::X16:
                {
                    auto bfr16 = BFR::create(window_traits->width, window_traits->height, 16, 16, g_buffer,
                        illumination_buffer, accumulation_buffer);
                    bfr16->compile(image_layout_compile.context);
                    bfr16->update_image_layouts(image_layout_compile.context);
                    bfr16->add_dispatch_to_command_graph(commands, compute_constants);
                    final_descriptor_image = bfr16->get_final_descriptor_image();
                    break;
                }
            case DenoisingBlockSize::X32:
                {
                    auto bfr32 = BFR::create(window_traits->width, window_traits->height, 32, 32, g_buffer,
                        illumination_buffer, accumulation_buffer);
                    bfr32->compile(image_layout_compile.context);
                    bfr32->update_image_layouts(image_layout_compile.context);
                    bfr32->add_dispatch_to_command_graph(commands, compute_constants);
                    final_descriptor_image = bfr32->get_final_descriptor_image();
                    break;
                }
            case DenoisingBlockSize::X8X16X32:
                {
                    auto bfr8 = BFR::create(window_traits->width, window_traits->height, 8, 8, g_buffer,
                        illumination_buffer, accumulation_buffer);
                    auto bfr16 = BFR::create(window_traits->width, window_traits->height, 16, 16, g_buffer,
                        illumination_buffer, accumulation_buffer);
                    auto bfr32 = BFR::create(window_traits->width, window_traits->height, 32, 32, g_buffer,
                        illumination_buffer, accumulation_buffer);
                    auto blender = BFRBlender::create(window_traits->width, window_traits->height,
                        illumination_buffer->illumination_images[0], illumination_buffer->illumination_images[1],
                        bfr8->get_final_descriptor_image(), bfr16->get_final_descriptor_image(),
                        bfr32->get_final_descriptor_image());
                    bfr8->compile(image_layout_compile.context);
                    bfr8->update_image_layouts(image_layout_compile.context);
                    bfr16->compile(image_layout_compile.context);
                    bfr16->update_image_layouts(image_layout_compile.context);
                    bfr32->compile(image_layout_compile.context);
                    bfr32->update_image_layouts(image_layout_compile.context);
                    blender->compile(image_layout_compile.context);
                    blender->update_image_layouts(image_layout_compile.context);
                    bfr8->add_dispatch_to_command_graph(commands, compute_constants);
                    bfr16->add_dispatch_to_command_graph(commands, compute_constants);
                    bfr32->add_dispatch_to_command_graph(commands, compute_constants);
                    blender->add_dispatch_to_command_graph(commands);
                    final_descriptor_image = blender->get_final_descriptor_image();
                    break;
                }
            }
            break;
        case DenoisingType::BMFR:
            switch (denoising_block_size)
            {
            case DenoisingBlockSize::X8:
                {
                    auto bmfr8 = BMFR::create(window_traits->width, window_traits->height, 8, 8, g_buffer,
                        illumination_buffer, accumulation_buffer, 64);
                    bmfr8->compile(image_layout_compile.context);
                    bmfr8->update_image_layouts(image_layout_compile.context);
                    bmfr8->add_dispatch_to_command_graph(commands, compute_constants);
                    final_descriptor_image = bmfr8->get_final_descriptor_image();
                    break;
                }
            case DenoisingBlockSize::X16:
                {
                    auto bmfr16 = BMFR::create(window_traits->width, window_traits->height, 16, 16, g_buffer,
                        illumination_buffer, accumulation_buffer);
                    bmfr16->compile(image_layout_compile.context);
                    bmfr16->update_image_layouts(image_layout_compile.context);
                    bmfr16->add_dispatch_to_command_graph(commands, compute_constants);
                    final_descriptor_image = bmfr16->get_final_descriptor_image();
                    break;
                }
            case DenoisingBlockSize::X32:
                {
                    auto bmfr32 = BMFR::create(window_traits->width, window_traits->height, 32, 32, g_buffer,
                        illumination_buffer, accumulation_buffer);
                    bmfr32->compile(image_layout_compile.context);
                    bmfr32->update_image_layouts(image_layout_compile.context);
                    bmfr32->add_dispatch_to_command_graph(commands, compute_constants);
                    final_descriptor_image = bmfr32->get_final_descriptor_image();
                    break;
                }
            case DenoisingBlockSize::X8X16X32:
                auto bmfr8 = BMFR::create(window_traits->width, window_traits->height, 8, 8, g_buffer,
                    illumination_buffer, accumulation_buffer, 64);
                auto bmfr16 = BMFR::create(window_traits->width, window_traits->height, 16, 16, g_buffer,
                    illumination_buffer, accumulation_buffer);
                auto bmfr32 = BMFR::create(window_traits->width, window_traits->height, 32, 32, g_buffer,
                    illumination_buffer, accumulation_buffer);
                auto blender = BFRBlender::create(window_traits->width, window_traits->height,
                    illumination_buffer->illumination_images[1], illumination_buffer->illumination_images[2],
                    bmfr8->get_final_descriptor_image(), bmfr16->get_final_descriptor_image(),
                    bmfr32->get_final_descriptor_image());
                bmfr8->compile(image_layout_compile.context);
                bmfr8->update_image_layouts(image_layout_compile.context);
                bmfr16->compile(image_layout_compile.context);
                bmfr16->update_image_layouts(image_layout_compile.context);
                bmfr32->compile(image_layout_compile.context);
                bmfr32->update_image_layouts(image_layout_compile.context);
                blender->compile(image_layout_compile.context);
                blender->update_image_layouts(image_layout_compile.context);
                bmfr8->add_dispatch_to_command_graph(commands, compute_constants);
                bmfr16->add_dispatch_to_command_graph(commands, compute_constants);
                bmfr32->add_dispatch_to_command_graph(commands, compute_constants);
                blender->add_dispatch_to_command_graph(commands);
                final_descriptor_image = blender->get_final_descriptor_image();
                break;
            }
            break;
        case DenoisingType::SVG:
            std::cout << "Not yet implemented" << std::endl;
            break;
        }

        if (use_taa && accumulation_buffer)
        {
            auto taa = Taa::create(window_traits->width, window_traits->height, 16, 16, g_buffer, accumulation_buffer,
                final_descriptor_image);
            taa->compile(image_layout_compile.context);
            taa->update_image_layouts(image_layout_compile.context);
            taa->add_dispatch_to_command_graph(commands);
            final_descriptor_image = taa->get_final_descriptor_image();
        }
        if (export_g_buffer)
        {
            if (!g_buffer)
            {
                std::cout << "GBuffer information not available, export not possible" << std::endl;
                return 1;
            }
            offline_g_buffer_stager->download_from_g_buffer_command(g_buffer, commands, image_layout_compile.context);
        }
        if (export_illumination)
        {
            if (final_descriptor_image->imageInfoList[0]->imageView->image->format != VK_FORMAT_R32G32B32A32_SFLOAT)
            {
                std::cout << "Final image layout is not compatible illumination buffer export" << std::endl;
                return 1;
            }
            offline_illumination_buffer_stager->download_from_illumination_buffer_command(
                illumination_buffer, commands, image_layout_compile.context);
        }
        if (final_descriptor_image->imageInfoList[0]->imageView->image->format != VK_FORMAT_B8G8R8A8_UNORM)
        {
            auto converter = FormatConverter::create(
                final_descriptor_image->imageInfoList[0]->imageView, VK_FORMAT_B8G8R8A8_UNORM);
            converter->compile_images(image_layout_compile.context);
            converter->update_image_layouts(image_layout_compile.context);
            converter->add_dispatch_to_command_graph(commands);
            final_descriptor_image = converter->final_image;
        }
        if (g_buffer)
        {
            g_buffer->compile(image_layout_compile.context);
            g_buffer->update_image_layouts(image_layout_compile.context);
        }
        if (accumulation_buffer)
        {
            accumulation_buffer->compile(image_layout_compile.context);
            accumulation_buffer->update_image_layouts(image_layout_compile.context);
        }
        if (illumination_buffer)
        {
            illumination_buffer->compile(image_layout_compile.context);
            illumination_buffer->update_image_layouts(image_layout_compile.context);
        }
        image_layout_compile.context.record();

        if (accumulation_buffer)
        {
            accumulation_buffer->copy_to_back_images(commands, g_buffer, illumination_buffer);
        }

        // set GUI values
        auto gui_values = Gui::Values::create();
        gui_values->width = window_traits->width;
        gui_values->height = window_traits->height;
        CountTrianglesVisitor counter;
        if (loaded_scene)
        {
            loaded_scene->accept(counter);
        }
        gui_values->triangle_count = counter.triangle_count;
        gui_values->triangle_count
            = max_recursion_depth * 2;  // for each depth recursion one next event estimate is done

        auto viewport = vsg::ViewportState::create(0, 0, window_traits->width, window_traits->height);
        auto camera = vsg::Camera::create(perspective, look_at, viewport);
        auto render_graph = createRenderGraphForView(
            window, camera, vsgImGui::RenderImGui::create(window, Gui(gui_values)));  // render graph for gui rendering
        render_graph->clearValues.clear();  // removing clear values to avoid clearing the raytraced image

        auto command_graph = vsg::CommandGraph::create(window);
        command_graph->addChild(commands);
        command_graph->addChild(
            vsg::CopyImageViewToWindow::create(final_descriptor_image->imageInfoList[0]->imageView, window));
        command_graph->addChild(render_graph);

        // close handler to close and imgui handler to forward to imgui
        viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));
        if (use_fly_navigation)
        {
            viewer->addEventHandler(vsg::FlyNavigation::create(camera));
        }
        else
        {
            viewer->addEventHandler(vsg::Trackball::create(camera));
        }
        viewer->assignRecordAndSubmitTaskAndPresentation({command_graph});
        viewer->compile();

        // waiting for image layout transitions
        image_layout_compile.context.waitForCompletion();

        int frame_index = 0;
        int sample_index = 0;
        while (viewer->advanceToNextFrame() && (num_frames < 0 || frame_index < num_frames))
        {
            viewer->handleEvents();
            if (static_cast<vsg::mat4>(lookAt(look_at->eye, look_at->center, look_at->up))
                != ray_tracing_push_constants_value->value().prev_view)
            {
                // clear samples when the camera has moved
                sample_index = 0;
            }

            ray_tracing_push_constants_value->value().view_inverse = look_at->inverse();
            ray_tracing_push_constants_value->value().frame_number = frame_index;
            ray_tracing_push_constants_value->value().sample_number = sample_index;
            gui_values->sample_number = sample_index;

            if (use_external_buffers)
            {
                offline_g_buffer_stager->transfer_staging_data_from(offline_g_buffers[frame_index]);
                offline_illumination_buffer_stager->transfer_staging_data_from(offline_illuminations[frame_index]);
                if (accumulator)
                {
                    accumulator->set_camera_matrices(frame_index, camera_matrices[frame_index],
                        camera_matrices[frame_index != 0 ? frame_index - 1 : frame_index]);
                }
            }
            else if (accumulator)
            {
                CameraMatrices a{};
                CameraMatrices b{};
                a.inv_view = look_at->inverse();
                a.inv_proj = perspective->inverse();
                a.proj = perspective->transform();
                b.view = ray_tracing_push_constants_value->value().prev_view;
                accumulator->set_camera_matrices(ray_tracing_push_constants_value->value().frame_number, a, b);
            }

            viewer->update();
            viewer->recordAndSubmit();
            viewer->present();

            ray_tracing_push_constants_value->value().prev_view = look_at->transform();

            if (sample_index + 1 >= samples_per_pixel)
            {
                if (export_g_buffer || export_illumination)
                {
                    viewer->deviceWaitIdle();
                    if (export_illumination)
                    {
                        offline_illumination_buffer_stager->transfer_staging_data_to(
                            offline_illuminations[frame_index]);
                    }
                    if (export_g_buffer)
                    {
                        offline_g_buffer_stager->transfer_staging_data_to(offline_g_buffers[frame_index]);
                    }
                }
                if (store_matrices)
                {
                    camera_matrices[frame_index].view = look_at->transform();
                    camera_matrices[frame_index].inv_view = look_at->inverse();
                    camera_matrices[frame_index].proj.value() = perspective->transform();
                    camera_matrices[frame_index].inv_proj.value() = perspective->inverse();
                }
                frame_index++;
            }
            sample_index++;
        }

        // exporting all images
        if (export_g_buffer)
        {
            GBufferIO::export_g_buffer(export_position_path, export_depth_path, export_normal_path,
                export_material_path, export_albedo_path, num_frames, offline_g_buffers, camera_matrices);
        }
        if (export_illumination)
        {
            IlluminationBufferIO::export_illumination(export_illumination_path, num_frames, offline_illuminations);
        }
        if (!export_matrices_path.empty())
        {
            MatrixIO::export_matrices(export_matrices_path, camera_matrices);
        }
    }
    catch (const vsg::Exception& e)
    {
        std::cout << e.message << " VkResult = " << e.result << std::endl;
        return 0;
    }
    return 0;
}