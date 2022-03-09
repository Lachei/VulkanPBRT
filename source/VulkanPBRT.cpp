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
#include <util/VsgUtils.hpp>
#include <util/DenoiserUtils.hpp>

#include "Gui.hpp"

#include <vsg/all.h>
#include <vsgXchange/images.h>
#include <vsgXchange/models.h>

#include <nlohmann/json.hpp>

#include <iostream>

#include "../external/vsgXchange/src/assimp/3DFrontImporter.h"

// TODO: remove this
#include "scene/entity/Components.h"

#define _DEBUG

class RayTracingPushConstantsValue : public vsg::Inherit<vsg::Value<RayTracingPushConstants>, RayTracingPushConstantsValue>
{
public:
    RayTracingPushConstantsValue() {}
};

class LoggingRedirectSentry
{
public:
    LoggingRedirectSentry(std::ostream *outStream, std::streambuf *originalBuffer)
        : outStream(outStream), originalBuffer(originalBuffer)
    {
    }
    ~LoggingRedirectSentry()
    {
        //reset to standard output
        outStream->rdbuf(originalBuffer);
    }

private:
    std::ostream *outStream;
    std::streambuf *originalBuffer;
};

int main(int argc, char **argv)
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
        LoggingRedirectSentry coutSentry(&std::cout, std::cout.rdbuf());
        LoggingRedirectSentry cerrSenry(&std::cerr, std::cerr.rdbuf());
        std::ofstream out("out_log.txt");
        std::ofstream err_log("err_log.txt");
        if (arguments.read({"--log", "-l"}))
        {
            // redirect cout and cerr to log files
            std::cout.rdbuf(out.rdbuf());
            std::cerr.rdbuf(err_log.rdbuf());
        }

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "VulkanPBRT";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        windowTraits->fullscreen = arguments.read({"--fullscreen", "-fs"});
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height))
            windowTraits->fullscreen = false;
        arguments.read("--screen", windowTraits->screenNum);

        auto numFrames = arguments.value(-1, "-f");
        auto samplesPerPixel = arguments.value(1, "--spp");
        auto depthPath = arguments.value(std::string(), "--depths");
        auto exportDepthPath = arguments.value(std::string(), "--exportDepth");
        auto positionPath = arguments.value(std::string(), "--positions");
        auto exportPositionPath = arguments.value(std::string(), "--exportPosition");
        auto normalPath = arguments.value(std::string(), "--normals");
        auto exportNormalPath = arguments.value(std::string(), "--exportNormal");
        auto albedoPath = arguments.value(std::string(), "--albedos");
        auto exportAlbedoPath = arguments.value(std::string(), "--exportAlbedo");
        auto materialPath = arguments.value(std::string(), "--materials");
        auto exportMaterialPath = arguments.value(std::string(), "--exportMaterial");
        auto illuminationPath = arguments.value(std::string(), "--illuminations");
        auto exportIlluminationPath = arguments.value(std::string(), "--exportIllumination");
        auto matricesPath = arguments.value(std::string(), "--matrices");
        auto exportMatricesPath = arguments.value(std::string(), "--exportMatrices");
        auto sceneFilename = arguments.value(std::string(), "-i");
        bool use_external_buffers = normalPath.size();
        bool exportIllumination = exportIlluminationPath.size();
        bool exportGBuffer = exportNormalPath.size() || exportDepthPath.size() || exportPositionPath.size() || exportAlbedoPath.size() || exportMaterialPath.size();
        bool storeMatrices = exportGBuffer || exportMatricesPath.size();
        if (sceneFilename.empty() && !use_external_buffers)
        {
            std::cout << "Missing input parameter \"-i <path_to_model>\"." << std::endl;
        }
        if (arguments.read("m"))
            sceneFilename = "models/raytracing_scene.vsgt";
        if (arguments.errors())
            return arguments.writeErrorMessages(std::cerr);

        DenoisingType denoisingType = DenoisingType::None;
        DenoisingBlockSize denoisingBlockSize = DenoisingBlockSize::x32;
        std::string denoisingTypeStr;
        if (arguments.read("--denoiser", denoisingTypeStr))
        {
            if (denoisingTypeStr == "bmfr")
                denoisingType = DenoisingType::BMFR;
            else if (denoisingTypeStr == "bfr")
                denoisingType = DenoisingType::BFR;
            else if (denoisingTypeStr == "svgf")
                denoisingType = DenoisingType::SVG;
            else if (denoisingTypeStr == "none")
            {
            }
            else
                std::cout << "Unknown denoising type: " << denoisingTypeStr << std::endl;
        }
        bool useTaa = arguments.read("--taa");
        bool useFlyNavigation = arguments.read("--fly");
#ifdef _DEBUG
        // overwriting command line options for debug
        windowTraits->debugLayer = true;
        windowTraits->width = 1800;
        windowTraits->height = 990;
#endif
        windowTraits->queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        windowTraits->imageAvailableSemaphoreWaitFlag = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        windowTraits->swapchainPreferences.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        windowTraits->deviceExtensionNames = {VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME};
        windowTraits->vulkanVersion = VK_API_VERSION_1_2;
        auto &enabledAccelerationStructureFeatures = windowTraits->deviceFeatures->get<VkPhysicalDeviceAccelerationStructureFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR>();
        auto &enabledRayTracingPipelineFeatures = windowTraits->deviceFeatures->get<VkPhysicalDeviceRayTracingPipelineFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR>();
        enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
        enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
        auto &enabledPhysicalDeviceVk12Feature = windowTraits->deviceFeatures->get<VkPhysicalDeviceVulkan12Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES>();
        enabledPhysicalDeviceVk12Feature.runtimeDescriptorArray = VK_TRUE;
        enabledPhysicalDeviceVk12Feature.bufferDeviceAddress = VK_TRUE;
        enabledPhysicalDeviceVk12Feature.descriptorIndexing = VK_TRUE;
        enabledPhysicalDeviceVk12Feature.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
        enabledPhysicalDeviceVk12Feature.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

        // load scene or images
        vsg::ref_ptr<vsg::Node> loaded_scene;
        std::vector<vsg::ref_ptr<OfflineGBuffer>> offlineGBuffers;
        std::vector<vsg::ref_ptr<OfflineIllumination>> offlineIlluminations;
        std::vector<CameraMatrices> cameraMatrices;
        if(!use_external_buffers){
            AI3DFrontImporter::ReadConfig(config_json);
            auto options = vsg::Options::create(vsgXchange::assimp::create(), vsgXchange::dds::create(), vsgXchange::stbi::create()); //using the assimp loader
            loaded_scene = vsg::read_cast<vsg::Node>(sceneFilename, options);
            if (!loaded_scene)
            {
                std::cout << "Scene not found: " << sceneFilename << std::endl;
                return 1;
            }
        }
        else
        {
            if (numFrames <= 0)
            {
                std::cout << "No number of frames given. For usage of external GBuffer and Illumination information use \"-f\" to inform about the number of frames." << std::endl;
                return 1;
            }
            if (matricesPath.empty())
            {
                std::cout << "Camera matrices are missing. Insert location of file with camera information via \"--matrices\"." << std::endl;
                return 1;
            }
            cameraMatrices = MatrixIO::importMatrices(matricesPath);
            if (cameraMatrices.empty())
            {
                std::cout << "Camera matrices could not be loaded" << std::endl;
                return 1;
            }
            if (positionPath.size())
            {
                offlineGBuffers = GBufferIO::importGBufferPosition(positionPath, normalPath, materialPath, albedoPath, cameraMatrices, numFrames);
            }
            else
            {
                offlineGBuffers = GBufferIO::importGBufferDepth(depthPath, normalPath, materialPath, albedoPath, numFrames);
            }
            offlineIlluminations = IlluminationBufferIO::importIllumination(illuminationPath, numFrames);
            windowTraits->width = offlineGBuffers[0]->depth->width();
            windowTraits->height = offlineGBuffers[0]->depth->height();
        }
        if (exportIllumination)
        {
            if (numFrames <= 0)
            {
                std::cout << "No number of frames given. For usage of Illumination export use \"-f\" to inform about the number of frames." << std::endl;
                return 1;
            }
            if (offlineIlluminations.empty())
            {
                offlineIlluminations.resize(numFrames);
                for (auto &i : offlineIlluminations)
                {
                    i = OfflineIllumination::create();
                    i->noisy = vsg::vec4Array2D::create(windowTraits->width, windowTraits->height);
                }
            }
        }
        if (exportGBuffer)
        {
            if (numFrames <= 0)
            {
                std::cout << "No number of frames given. For usage of GBuffer export use \"-f\" to inform about the number of frames." << std::endl;
                return 1;
            }
            if (offlineGBuffers.empty())
            {
                offlineGBuffers.resize(numFrames);
                for (auto &i : offlineGBuffers)
                {
                    i = OfflineGBuffer::create();
                    i->depth = vsg::floatArray2D::create(windowTraits->width, windowTraits->height);
                    i->normal = vsg::vec2Array2D::create(windowTraits->width, windowTraits->height);
                    i->albedo = vsg::ubvec4Array2D::create(windowTraits->width, windowTraits->height);
                    i->material = vsg::ubvec4Array2D::create(windowTraits->width, windowTraits->height);
                }
            }
        }
        if (storeMatrices)
        {
            cameraMatrices.resize(numFrames);
            for (auto &matrix : cameraMatrices)
            {
                matrix.proj = vsg::mat4();
                matrix.invProj = vsg::mat4();
            }
        }

        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }
        auto viewer = vsg::Viewer::create();
        viewer->addWindow(window);

        vsg::ref_ptr<vsg::Device> device(window->getOrCreateDevice());

        //setting a custom render pass for imgui non clear rendering 
        window->setRenderPass(vkpbrt::createNonClearRenderPass(window->surfaceFormat().format, window->depthFormat(), device));

        //create camera matrices
        auto perspective = vsg::Perspective::create(60, static_cast<double>(windowTraits->width) / static_cast<double>(windowTraits->height), .1, 1000);
        auto lookAt = vsg::LookAt::create(vsg::dvec3(0.0, -3, 1), vsg::dvec3(0.0, 0.0, 1), vsg::dvec3(0.0, 0.0, 1.0));

        // create push constants
        auto rayTracingPushConstantsValue = RayTracingPushConstantsValue::create();
        rayTracingPushConstantsValue->value().projInverse = perspective->inverse();
        rayTracingPushConstantsValue->value().viewInverse = lookAt->inverse();
        rayTracingPushConstantsValue->value().prevView = lookAt->transform();
        rayTracingPushConstantsValue->value().frameNumber = 0;
        rayTracingPushConstantsValue->value().sampleNumber = 0;
        auto pushConstants = vsg::PushConstants::create(VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, rayTracingPushConstantsValue);
        auto computeConstants = vsg::PushConstants::create(VK_SHADER_STAGE_COMPUTE_BIT, 0, rayTracingPushConstantsValue);

        vsg::ref_ptr<GBuffer> gBuffer;
        vsg::ref_ptr<IlluminationBuffer> illuminationBuffer;
        vsg::ref_ptr<AccumulationBuffer> accumulationBuffer;
        bool writeGBuffer;
        if (denoisingType != DenoisingType::None)
        {
            writeGBuffer = true;
            gBuffer = GBuffer::create(windowTraits->width, windowTraits->height);
            illuminationBuffer = IlluminationBufferDemodulatedFloat::create(windowTraits->width, windowTraits->height);
        }
        else
        {
            writeGBuffer = false;
            illuminationBuffer = IlluminationBufferFinalFloat::create(windowTraits->width, windowTraits->height);
        }
        if (exportIllumination && !gBuffer)
        {
            writeGBuffer = true;
            gBuffer = GBuffer::create(windowTraits->width, windowTraits->height);
        }
        if (useTaa && !accumulationBuffer)
        {
            // TODO: need the velocity buffer
        }

        // raytracing pipeline setup
        uint32_t maxRecursionDepth = 2;
        vsg::ref_ptr<PBRTPipeline> pbrtPipeline;
        if(!use_external_buffers)
        {
            pbrtPipeline = PBRTPipeline::create(loaded_scene, gBuffer, illuminationBuffer, writeGBuffer, RayTracingRayOrigin::CAMERA);

            // setup tlas
            vsg::BuildAccelerationStructureTraversal buildAccelStruct(device);
            loaded_scene->accept(buildAccelStruct);
            pbrtPipeline->setTlas(buildAccelStruct.tlas);
        }
        else
        {
            if (!gBuffer)
                gBuffer = GBuffer::create(offlineGBuffers[0]->depth->width(), offlineGBuffers[0]->depth->height());
            switch (offlineIlluminations[0]->noisy->getLayout().format)
            {
            case VK_FORMAT_R16G16B16A16_SFLOAT:
                illuminationBuffer = IlluminationBufferDemodulated::create(offlineIlluminations[0]->noisy->width(), offlineIlluminations[0]->noisy->height());
                break;
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                illuminationBuffer = IlluminationBufferDemodulatedFloat::create(offlineIlluminations[0]->noisy->width(), offlineIlluminations[0]->noisy->height());
                break;
            default:
                std::cout << "Offline illumination buffer image format not compatible" << std::endl;
                return 1;
            }
        }
        // -------------------------------------------------------------------------------------
        // image layout conversions and correct binding of different denoising tequniques
        // -------------------------------------------------------------------------------------
        vsg::CompileTraversal imageLayoutCompile(window);
        auto commands = vsg::Commands::create();
        auto offlineGBufferStager = OfflineGBuffer::create();
        auto offlineIlluminationBufferStager = OfflineIllumination::create();
        vsg::ref_ptr<vsg::QueryPool> queryPool;
        if (pbrtPipeline)
        {
            queryPool = vsg::QueryPool::create(); //standard init has 1 timestamp place
            queryPool->queryCount = 2;
            auto resetQuery = vsg::ResetQueryPool::create(queryPool);
            auto write1 = vsg::WriteTimestamp::create(queryPool, 0, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
            auto write2 = vsg::WriteTimestamp::create(queryPool, 1, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
            commands->addChild(resetQuery);
            commands->addChild(write1);
            pbrtPipeline->addTraceRaysToCommandGraph(commands, pushConstants);
            commands->addChild(write2);
            illuminationBuffer = pbrtPipeline->getIlluminationBuffer();
        }
        else
        {
            if (offlineGBuffers.size() < numFrames || offlineIlluminations.size() < numFrames)
            {
                std::cout << "Missing offline GBuffer or offline Illumination Buffer info" << std::endl;
                return 1;
            }
            offlineGBufferStager->uploadToGBufferCommand(gBuffer, commands, imageLayoutCompile.context);
            offlineIlluminationBufferStager->uploadToIlluminationBufferCommand(illuminationBuffer, commands, imageLayoutCompile.context);
        }

        vsg::ref_ptr<Accumulator> accumulator;
        if(denoisingType != DenoisingType::None){
            accumulator = Accumulator::create(gBuffer, illuminationBuffer, !use_external_buffers);
            accumulator->addDispatchToCommandGraph(commands);
            accumulationBuffer = accumulator->accumulationBuffer;
            illuminationBuffer->compile(imageLayoutCompile.context);
            illuminationBuffer->updateImageLayouts(imageLayoutCompile.context);
            illuminationBuffer = accumulator->accumulatedIllumination; //swap illumination buffer to accumulated illumination for correct use in the following pipelines
        }

        vsg::ref_ptr<vsg::DescriptorImage> finalDescriptorImage;
        if(denoisingType == DenoisingType::None) 
            finalDescriptorImage = illuminationBuffer->illuminationImages[0];
        else
            vkpbrt::addDenoiserToCommands(denoisingType, denoisingBlockSize, commands, imageLayoutCompile, windowTraits->width, windowTraits->height, computeConstants, gBuffer, illuminationBuffer, accumulationBuffer, finalDescriptorImage);

        if (useTaa && accumulationBuffer)
        {
            auto taa = Taa::create(windowTraits->width, windowTraits->height, 16, 16, gBuffer, accumulationBuffer, finalDescriptorImage);
            taa->compile(imageLayoutCompile.context);
            taa->updateImageLayouts(imageLayoutCompile.context);
            taa->addDispatchToCommandGraph(commands);
            finalDescriptorImage = taa->getFinalDescriptorImage();
        }
        if (exportGBuffer)
        {
            if (!gBuffer)
            {
                std::cout << "GBuffer information not available, export not possible" << std::endl;
                return 1;
            }
            offlineGBufferStager->downloadFromGBufferCommand(gBuffer, commands, imageLayoutCompile.context);
        }
        if (exportIllumination)
        {
            if (finalDescriptorImage->imageInfoList[0]->imageView->image->format != VK_FORMAT_R32G32B32A32_SFLOAT)
            {
                std::cout << "Final image layout is not compatible illumination buffer export" << std::endl;
                return 1;
            }
            offlineIlluminationBufferStager->downloadFromIlluminationBufferCommand(illuminationBuffer, commands, imageLayoutCompile.context);
        }
        if (finalDescriptorImage->imageInfoList[0]->imageView->image->format != VK_FORMAT_B8G8R8A8_UNORM)
        {
            auto converter = FormatConverter::create(finalDescriptorImage->imageInfoList[0]->imageView, VK_FORMAT_B8G8R8A8_UNORM);
            converter->compileImages(imageLayoutCompile.context);
            converter->updateImageLayouts(imageLayoutCompile.context);
            converter->addDispatchToCommandGraph(commands);
            finalDescriptorImage = converter->finalImage;
        }
        if (gBuffer)
        {
            gBuffer->compile(imageLayoutCompile.context);
            gBuffer->updateImageLayouts(imageLayoutCompile.context);
        }
        if (accumulationBuffer)
        {
            accumulationBuffer->compile(imageLayoutCompile.context);
            accumulationBuffer->updateImageLayouts(imageLayoutCompile.context);
        }
        if (illuminationBuffer)
        {
            illuminationBuffer->compile(imageLayoutCompile.context);
            illuminationBuffer->updateImageLayouts(imageLayoutCompile.context);
        }
        imageLayoutCompile.context.record();

        if (accumulationBuffer)
        {
            accumulationBuffer->copyToBackImages(commands, gBuffer, illuminationBuffer);
        }

        // set GUI values
        auto guiValues = Gui::Values::create();
        guiValues->width = windowTraits->width;
        guiValues->height = windowTraits->height;
        CountTrianglesVisitor counter;
        if (loaded_scene)
            loaded_scene->accept(counter);
        guiValues->triangleCount = counter.triangleCount;
        guiValues->raysPerPixel = maxRecursionDepth * 2; //for each depth recursion one next event estimate is done

        auto viewport = vsg::ViewportState::create(0, 0, windowTraits->width, windowTraits->height);
        auto camera = vsg::Camera::create(perspective, lookAt, viewport);
        auto renderGraph = vsg::createRenderGraphForView(window, camera, vsgImGui::RenderImGui::create(window, Gui(guiValues))); // render graph for gui rendering
        renderGraph->clearValues.clear();                                                                                        //removing clear values to avoid clearing the raytraced image

        auto commandGraph = vsg::CommandGraph::create(window);
        commandGraph->addChild(commands);
        commandGraph->addChild(vsg::CopyImageViewToWindow::create(finalDescriptorImage->imageInfoList[0]->imageView, window));
        commandGraph->addChild(renderGraph);

        //close handler to close and imgui handler to forward to imgui
        viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));
        if (useFlyNavigation)
            viewer->addEventHandler(vsg::FlyNavigation::create(camera));
        else
            viewer->addEventHandler(vsg::Trackball::create(camera));
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
        viewer->compile();

        // waiting for image layout transitions
        imageLayoutCompile.context.waitForCompletion();

        int frame_index = 0;
        int sample_index = 0;
        while(viewer->advanceToNextFrame() && (numFrames < 0 || frame_index < numFrames))
        {
            viewer->handleEvents();
            if ((vsg::mat4)vsg::lookAt(lookAt->eye, lookAt->center, lookAt->up) != rayTracingPushConstantsValue->value().prevView)
            {
                // clear samples when the camera has moved
                sample_index = 0;
            }

            rayTracingPushConstantsValue->value().viewInverse = lookAt->inverse();
            rayTracingPushConstantsValue->value().frameNumber = frame_index;
            rayTracingPushConstantsValue->value().sampleNumber = sample_index;
            guiValues->sampleNumber = sample_index;
            
            if (use_external_buffers)
            {
                offlineGBufferStager->transferStagingDataFrom(offlineGBuffers[frame_index]);
                offlineIlluminationBufferStager->transferStagingDataFrom(offlineIlluminations[frame_index]);
                if (accumulator)
                   accumulator->setCameraMatrices(frame_index, cameraMatrices[frame_index], cameraMatrices[frame_index ? frame_index - 1 : frame_index]);
            }
            else if (accumulator)
            {
                CameraMatrices a{}, b{};
                a.invView = lookAt->inverse();
                a.invProj = perspective->inverse();
                a.proj = perspective->transform();
                b.view = rayTracingPushConstantsValue->value().prevView;
                accumulator->setCameraMatrices(rayTracingPushConstantsValue->value().frameNumber, a, b);
            }

            viewer->update();
            viewer->recordAndSubmit();
            viewer->present();

            rayTracingPushConstantsValue->value().prevView = lookAt->transform();

            if (sample_index + 1 >= samplesPerPixel) {
                if (exportGBuffer || exportIllumination) {
                    viewer->deviceWaitIdle();
                    if (exportIllumination) {
                        offlineIlluminationBufferStager->transferStagingDataTo(offlineIlluminations[frame_index]);
                    }
                    if (exportGBuffer) {
                        offlineGBufferStager->transferStagingDataTo(offlineGBuffers[frame_index]);
                    }
                }
                if (storeMatrices) {
                    cameraMatrices[frame_index].view = lookAt->transform();
                    cameraMatrices[frame_index].invView = lookAt->inverse();
                    cameraMatrices[frame_index].proj.value() = perspective->transform();
                    cameraMatrices[frame_index].invProj.value() = perspective->inverse();
                }
                frame_index++;
            }
            sample_index++;
        }

        // exporting all images
        if (exportGBuffer)
            GBufferIO::exportGBuffer(exportPositionPath, exportDepthPath, exportNormalPath, exportMaterialPath, exportAlbedoPath, numFrames, offlineGBuffers, cameraMatrices);
        if (exportIllumination)
            IlluminationBufferIO::exportIllumination(exportIlluminationPath, numFrames, offlineIlluminations);
        if (exportMatricesPath.size())
            MatrixIO::exportMatrices(exportMatricesPath, cameraMatrices);
    }
    catch (const vsg::Exception &e)
    {
        std::cout << e.message << " VkResult = " << e.result << std::endl;
        return 0;
    }
    return 0;
}