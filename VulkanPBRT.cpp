
#include "PBRTPipeline.hpp"
#include "Denoiser/BFR.hpp"

#include "Denoiser/BFRBlender.hpp"
#include "Denoiser/BMFR.hpp"
#include "PipelineStructs.hpp"
#include "CountTrianglesVisitor.hpp"
#include "gui.hpp"
#include "RenderImporter.hpp"

#include <vsgXchange/models.h>
#include <vsgXchange/images.h>
#include <vsg/all.h>

#include <set>
#include <iostream>

#define _DEBUG

class RayTracingPushConstantsValue : public vsg::Inherit<vsg::Value<RayTracingPushConstants>, RayTracingPushConstantsValue>{
    public:
    RayTracingPushConstantsValue(){}
};

enum class DenoisingType{
    None,
    BMFR,
    BFR,
    SVG
};
DenoisingType denoisingType = DenoisingType::None;

enum class DenoisingBlockSize{
    x8,
    x16,
    x32,
    x64,
    x8x16x32
};
DenoisingBlockSize denoisingBlockSize = DenoisingBlockSize::x32;

class LoggingRedirectSentry
{
public:
    LoggingRedirectSentry(std::ostream* outStream, std::streambuf* originalBuffer)
    	: outStream(outStream), originalBuffer(originalBuffer)
    {
    }
    ~LoggingRedirectSentry()
    {
        //reset to standard output
        outStream->rdbuf(originalBuffer); 
    }
private:
    std::ostream* outStream;
    std::streambuf* originalBuffer;
};

int main(int argc, char** argv){
    try{
        // command line parsing
        vsg::CommandLine arguments(&argc, argv);

        // ensure that cout and cerr are reset to their standard output when main() is exited
        LoggingRedirectSentry coutSentry(&std::cout, std::cout.rdbuf());
        LoggingRedirectSentry cerrSenry(&std::cerr, std::cerr.rdbuf());
        std::ofstream out("out_log.txt");
        std::ofstream err_log("err_log.txt");
        if (arguments.read({ "--log", "-l" }))
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
        if(arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) windowTraits->fullscreen = false;
        arguments.read("--screen", windowTraits->screenNum);	

        auto numFrames = arguments.value(-1, "-f");
        auto depthImages = arguments.value(std::string(), "--depths");
        auto positionImages = arguments.value(std::string(), "--positions");
        auto normalImages = arguments.value(std::string(), "--normals");
        auto albedoImages = arguments.value(std::string(), "--albedos");
        auto materialImages = arguments.value(std::string(), "--materials");
        auto illuminationImages = arguments.value(std::string(), "--illuminations");
        auto matricesPath = arguments.value(std::string(), "--matrices");
        auto sceneFilename = arguments.value(std::string(), "-i");
        if (sceneFilename.empty() && normalImages.empty())
        {
            std::cout << "Missing input parameter \"-i <path_to_model>\"." << std::endl;
        }
        if(arguments.read("m")) sceneFilename = "models/raytracing_scene.vsgt";
        if(arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        std::string denoisingTypeStr;
        if(arguments.read("--denoiser", denoisingTypeStr)){
            if(denoisingTypeStr == "bmfr")     denoisingType = DenoisingType::BMFR;
            else if(denoisingTypeStr == "bfr") denoisingType = DenoisingType::BFR;
            else if(denoisingTypeStr == "svgf")denoisingType = DenoisingType::SVG;
            else if(denoisingTypeStr == "none"){}
            else std::cout << "Unknown denoising type: " << denoisingTypeStr << std::endl;
        }
        bool useTaa = arguments.read("--taa");
        bool useFlyNavigation = arguments.read("--fly");

#ifdef _DEBUG
        // overwriting command line options for debug
        windowTraits->debugLayer = true;
        windowTraits->width = 1800;
#endif
        windowTraits->queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        windowTraits->imageAvailableSemaphoreWaitFlag = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        windowTraits->swapchainPreferences.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        windowTraits->deviceExtensionNames = {VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME
        , VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME};
        windowTraits->vulkanVersion = VK_API_VERSION_1_2;
        auto& enabledAccelerationStructureFeatures = windowTraits->deviceFeatures->get<VkPhysicalDeviceAccelerationStructureFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR>();
        auto& enabledRayTracingPipelineFeatures = windowTraits->deviceFeatures->get<VkPhysicalDeviceRayTracingPipelineFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR>();
        enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
        enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
        auto& enabledPhysicalDeviceVk12Feature = windowTraits->deviceFeatures->get<VkPhysicalDeviceVulkan12Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES>();
        enabledPhysicalDeviceVk12Feature.runtimeDescriptorArray = VK_TRUE;
        enabledPhysicalDeviceVk12Feature.bufferDeviceAddress = VK_TRUE;
        enabledPhysicalDeviceVk12Feature.descriptorIndexing = VK_TRUE;
        enabledPhysicalDeviceVk12Feature.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
        enabledPhysicalDeviceVk12Feature.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

        auto window = vsg::Window::create(windowTraits);
        if(!window){
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }
        auto viewer = vsg::Viewer::create();
        viewer->addWindow(window);

        vsg::ref_ptr<vsg::Device> device(window->getOrCreateDevice());

        //setting a custom render pass for imgui non clear rendering
        {
            vsg::AttachmentDescription  colorAttachment = vsg::defaultColorAttachment(window->surfaceFormat().format);
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            vsg::AttachmentDescription depthAttachment = vsg::defaultDepthAttachment(window->depthFormat());
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            vsg::RenderPass::Attachments attachments{
                colorAttachment,
                depthAttachment};
    
            VkAttachmentReference colorAttachmentRef = {};
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
            VkAttachmentReference depthAttachmentRef = {};
            depthAttachmentRef.attachment = 1;
            depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
            vsg::SubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachments.emplace_back(colorAttachmentRef);
            subpass.depthStencilAttachments.emplace_back(depthAttachmentRef);
    
            vsg::RenderPass::Subpasses subpasses{subpass};
    
            // image layout transition
            VkSubpassDependency colorDependency = {};
            colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            colorDependency.dstSubpass = 0;
            colorDependency.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            colorDependency.srcAccessMask = 0;
            colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            colorDependency.dependencyFlags = 0;
    
            // depth buffer is shared between swap chain images
            VkSubpassDependency depthDependency = {};
            depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            depthDependency.dstSubpass = 0;
            depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            depthDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depthDependency.dependencyFlags = 0;
    
            vsg::RenderPass::Dependencies dependencies{colorDependency, depthDependency};
    
            auto renderPass = vsg::RenderPass::create(device, attachments, subpasses, dependencies);
            window->setRenderPass(renderPass);
        }

        vsg::ref_ptr<vsg::TopLevelAccelerationStructure> tlas;
        vsg::ref_ptr<vsg::Node> loaded_scene;

    	// load scene or images
        std::vector<vsg::ref_ptr<OfflineGBuffer>> offlineGBuffer;
        std::vector<vsg::ref_ptr<OfflineIllumination>> offlineIllumination;
        std::vector<DoubleMatrix> cameraMatrices;
        if(sceneFilename.size()){
            auto options = vsg::Options::create(vsgXchange::assimp::create(), vsgXchange::dds::create(), vsgXchange::stbi::create()); //using the assimp loader
            loaded_scene = vsg::read_cast<vsg::Node>(sceneFilename, options);
            if(!loaded_scene){
                std::cout << "Scene not found: " << sceneFilename << std::endl;
                return 1;
            }
            vsg::BuildAccelerationStructureTraversal buildAccelStruct(device);
            loaded_scene->accept(buildAccelStruct);
            tlas = buildAccelStruct.tlas;
        }
        else{
            if(numFrames <= 0){
                std::cout << "No number of frames given. For usage of external GBuffer and Illumination information use \"-f\" to inform about the number of frames." << std::endl;
                return 1;
            }
            if(matricesPath.empty()){
                std::cout << "Camera matrices are missing. Insert location of file with camera information via \"--matrices\"." << std::endl;
                return 1;
            }
            cameraMatrices = MatrixImporter::importMatrices(matricesPath);
            if(cameraMatrices.empty()){
                std::cout << "Camera matrices could not be loaded" << std::endl;
                return 1;
            }
            if(positionImages.size()){
                offlineGBuffer = GBufferImporter::importGBufferPosition(positionImages, normalImages, materialImages, albedoImages, cameraMatrices, numFrames);
            }
            else{
                offlineGBuffer = GBufferImporter::importGBufferDepth(depthImages, normalImages, materialImages, albedoImages, numFrames);
            }
            offlineIllumination = IlluminationBufferImporter::importIllumination(illuminationImages, numFrames);
        }

        //create camera matrices
        auto perspective = vsg::Perspective::create(60, static_cast<double>(windowTraits->width) / static_cast<double>(windowTraits->height), .1, 1000);
        auto lookAt = vsg::LookAt::create(vsg::dvec3(0.0, -3, 1), vsg::dvec3(0.0, 0.0, 1), vsg::dvec3(0.0, 0.0, 1.0));

        // set push constants
        auto rayTracingPushConstantsValue = RayTracingPushConstantsValue::create();
        perspective->get_inverse(rayTracingPushConstantsValue->value().projInverse);
        lookAt->get_inverse(rayTracingPushConstantsValue->value().viewInverse);
        lookAt->get(rayTracingPushConstantsValue->value().prevView);
        rayTracingPushConstantsValue->value().frameNumber = 0;
        rayTracingPushConstantsValue->value().sampleNumber = 0;
        auto pushConstants = vsg::PushConstants::create(VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, rayTracingPushConstantsValue);
        auto computeConstants = vsg::PushConstants::create(VK_SHADER_STAGE_COMPUTE_BIT, 0, rayTracingPushConstantsValue);

        vsg::ref_ptr<GBuffer> gBuffer;
        vsg::ref_ptr<AccumulationBuffer> accumulationBuffer;
        bool writeGBuffer;
        IlluminationBufferType illuminationBufferType;
        if (denoisingType != DenoisingType::None)
        {
            gBuffer = GBuffer::create(windowTraits->width, windowTraits->height);
            accumulationBuffer = AccumulationBuffer::create(windowTraits->width, windowTraits->height);
            writeGBuffer = true;
            illuminationBufferType = IlluminationBufferType::FINAL_DEMODULATED;
        }
        else
        {
            writeGBuffer = false;
            illuminationBufferType = IlluminationBufferType::FINAL;
        }
        if (useTaa && !accumulationBuffer)
        {
            // TODO: need the velocity buffer
        }
        
        // raytracing pipeline setup
        uint32_t maxRecursionDepth = 2;
        auto pbrtPipeline = PBRTPipeline::create(windowTraits->width, windowTraits->height, maxRecursionDepth, loaded_scene, gBuffer, accumulationBuffer,
                illuminationBufferType, writeGBuffer, RayTracingRayOrigin::CAMERA);

        // setup tlas
        vsg::BuildAccelerationStructureTraversal buildAccelStruct(device);
        loaded_scene->accept(buildAccelStruct);
        pbrtPipeline->setTlas(buildAccelStruct.tlas);      

        // -------------------------------------------------------------------------------------
        // image layout conversions and correct binding of different denoising tequniques
        // -------------------------------------------------------------------------------------
        vsg::CompileTraversal imageLayoutCompile(window);
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
        pbrtPipeline->compile(imageLayoutCompile.context);
        pbrtPipeline->updateImageLayouts(imageLayoutCompile.context);

        auto commands = vsg::Commands::create();
        pbrtPipeline->addTraceRaysToCommandGraph(commands, pushConstants);
        
        vsg::ref_ptr<vsg::DescriptorImage> finalDescriptorImage;
        vsg::ref_ptr<IlluminationBuffer> illuminationBuffer = pbrtPipeline->getIlluminationBuffer();
        switch(denoisingType){
        case DenoisingType::None:
            finalDescriptorImage = illuminationBuffer->illuminationImages[0];
            break;
        case DenoisingType::BFR:
            switch(denoisingBlockSize){
            case DenoisingBlockSize::x8:
            {
                auto bfr8 = BFR::create(windowTraits->width, windowTraits->height, 8, 8, gBuffer, illuminationBuffer, accumulationBuffer);
                bfr8->compile(imageLayoutCompile.context);
                bfr8->updateImageLayouts(imageLayoutCompile.context);
                bfr8->addDispatchToCommandGraph(commands, computeConstants);
                finalDescriptorImage = bfr8->getFinalDescriptorImage();
                break;
            }
            case DenoisingBlockSize::x16:
            {
                auto bfr16 = BFR::create(windowTraits->width, windowTraits->height, 16, 16, gBuffer, illuminationBuffer, accumulationBuffer);
                bfr16->compile(imageLayoutCompile.context);
                bfr16->updateImageLayouts(imageLayoutCompile.context);
                bfr16->addDispatchToCommandGraph(commands, computeConstants);
                finalDescriptorImage = bfr16->getFinalDescriptorImage();
                break;
            }
            case DenoisingBlockSize::x32:
            {
                auto bfr32 = BFR::create(windowTraits->width, windowTraits->height, 32, 32, gBuffer, illuminationBuffer, accumulationBuffer);
                bfr32->compile(imageLayoutCompile.context);
                bfr32->updateImageLayouts(imageLayoutCompile.context);
                bfr32->addDispatchToCommandGraph(commands, computeConstants);
                finalDescriptorImage = bfr32->getFinalDescriptorImage();
                break;
            }
            case DenoisingBlockSize::x8x16x32:
            {
                auto bfr8 = BFR::create(windowTraits->width, windowTraits->height, 8, 8, gBuffer, illuminationBuffer, accumulationBuffer);
                auto bfr16 = BFR::create(windowTraits->width, windowTraits->height, 16, 16, gBuffer, illuminationBuffer, accumulationBuffer);
                auto bfr32 = BFR::create(windowTraits->width, windowTraits->height, 32, 32, gBuffer, illuminationBuffer, accumulationBuffer);
                auto blender = BFRBlender::create(windowTraits->width, windowTraits->height, 
                                        illuminationBuffer->illuminationImages[1], illuminationBuffer->illuminationImages[2],
                                        bfr8->getFinalDescriptorImage(), bfr16->getFinalDescriptorImage(), bfr32->getFinalDescriptorImage());
                bfr8->compile(imageLayoutCompile.context);
                bfr8->updateImageLayouts(imageLayoutCompile.context);
                bfr16->compile(imageLayoutCompile.context);
                bfr16->updateImageLayouts(imageLayoutCompile.context);
                bfr32->compile(imageLayoutCompile.context);
                bfr32->updateImageLayouts(imageLayoutCompile.context);
                blender->compile(imageLayoutCompile.context);
                blender->updateImageLayouts(imageLayoutCompile.context);
                bfr8->addDispatchToCommandGraph(commands, computeConstants);
                bfr16->addDispatchToCommandGraph(commands, computeConstants);
                bfr32->addDispatchToCommandGraph(commands, computeConstants);
                blender->addDispatchToCommandGraph(commands);
                finalDescriptorImage = blender->getFinalDescriptorImage();
                break;
            }
            }
            break;
        case DenoisingType::BMFR:
            switch(denoisingBlockSize){
            case DenoisingBlockSize::x8:
            {
                auto bmfr8 = BMFR::create(windowTraits->width, windowTraits->height, 8, 8, gBuffer, illuminationBuffer, accumulationBuffer, 64);
                bmfr8->compile(imageLayoutCompile.context);
                bmfr8->updateImageLayouts(imageLayoutCompile.context);
                bmfr8->addDispatchToCommandGraph(commands, computeConstants);
                finalDescriptorImage = bmfr8->getFinalDescriptorImage();
                break;
            }
            case DenoisingBlockSize::x16:
            {
                auto bmfr16 = BMFR::create(windowTraits->width, windowTraits->height, 16, 16, gBuffer, illuminationBuffer, accumulationBuffer);
                bmfr16->compile(imageLayoutCompile.context);
                bmfr16->updateImageLayouts(imageLayoutCompile.context);
                bmfr16->addDispatchToCommandGraph(commands, computeConstants);
                finalDescriptorImage = bmfr16->getFinalDescriptorImage();
                break;
            }
            case DenoisingBlockSize::x32:
            {
                auto bmfr32 = BMFR::create(windowTraits->width, windowTraits->height, 32, 32, gBuffer, illuminationBuffer, accumulationBuffer);
                bmfr32->compile(imageLayoutCompile.context);
                bmfr32->updateImageLayouts(imageLayoutCompile.context);
                bmfr32->addDispatchToCommandGraph(commands, computeConstants);
                finalDescriptorImage = bmfr32->getFinalDescriptorImage();
                break;
            }
            case DenoisingBlockSize::x8x16x32:
                auto bmfr8 = BMFR::create(windowTraits->width, windowTraits->height, 8, 8, gBuffer, illuminationBuffer, accumulationBuffer, 64);
                auto bmfr16 = BMFR::create(windowTraits->width, windowTraits->height, 16, 16, gBuffer, illuminationBuffer, accumulationBuffer);
                auto bmfr32 = BMFR::create(windowTraits->width, windowTraits->height, 32, 32, gBuffer, illuminationBuffer, accumulationBuffer);
                auto blender = BFRBlender::create(windowTraits->width, windowTraits->height, 
                                        illuminationBuffer->illuminationImages[1], illuminationBuffer->illuminationImages[2],
                                        bmfr8->getFinalDescriptorImage(), bmfr16->getFinalDescriptorImage(), bmfr32->getFinalDescriptorImage());
                bmfr8->compile(imageLayoutCompile.context);
                bmfr8->updateImageLayouts(imageLayoutCompile.context);
                bmfr16->compile(imageLayoutCompile.context);
                bmfr16->updateImageLayouts(imageLayoutCompile.context);
                bmfr32->compile(imageLayoutCompile.context);
                bmfr32->updateImageLayouts(imageLayoutCompile.context);
                blender->compile(imageLayoutCompile.context);
                blender->updateImageLayouts(imageLayoutCompile.context);
                bmfr8->addDispatchToCommandGraph(commands, computeConstants);
                bmfr16->addDispatchToCommandGraph(commands, computeConstants);
                bmfr32->addDispatchToCommandGraph(commands, computeConstants);
                blender->addDispatchToCommandGraph(commands);
                finalDescriptorImage = blender->getFinalDescriptorImage();
                break;
            }
            break;
        case DenoisingType::SVG:
            std::cout << "Not yet implemented" << std::endl;
            break;
        }

        if(useTaa){
            auto taa = Taa::create(windowTraits->width, windowTraits->height, 16, 16, gBuffer, accumulationBuffer, finalDescriptorImage);
            taa->compile(imageLayoutCompile.context);
            taa->updateImageLayouts(imageLayoutCompile.context);
            taa->addDispatchToCommandGraph(commands);
            finalDescriptorImage = taa->getFinalDescriptorImage();
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
        loaded_scene->accept(counter);
        guiValues->triangleCount = counter.triangleCount;
        guiValues->raysPerPixel = maxRecursionDepth * 2; //for each depth recursion one next event estimate is done

        auto viewport = vsg::ViewportState::create(0, 0, windowTraits->width, windowTraits->height);
        auto camera = vsg::Camera::create(perspective, lookAt, viewport);
        auto renderGraph = vsg::createRenderGraphForView(window, camera, vsgImGui::RenderImGui::create(window, Gui(guiValues))); // render graph for gui rendering
        renderGraph->clearValues.clear();   //removing clear values to avoid clearing the raytraced image

        auto commandGraph = vsg::CommandGraph::create(window);
        commandGraph->addChild(commands);
        commandGraph->addChild(vsg::CopyImageViewToWindow::create(finalDescriptorImage->imageInfoList[0].imageView, window));
        commandGraph->addChild(renderGraph);
        
        //close handler to close and imgui handler to forward to imgui
        viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));
        if(useFlyNavigation)
            viewer->addEventHandler(vsg::FlyNavigation::create(camera));
        else
            viewer->addEventHandler(vsg::Trackball::create(camera));
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
        viewer->compile();

        //waiting for image layout transitions
        imageLayoutCompile.context.waitForCompletion();

        while(viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0)){
            
            viewer->handleEvents();

            //update push constants
            lookAt->get_inverse(rayTracingPushConstantsValue->value().viewInverse);

            rayTracingPushConstantsValue->value().frameNumber++;
            rayTracingPushConstantsValue->value().sampleNumber++;
            if((vsg::mat4)vsg::lookAt(lookAt->eye, lookAt->center, lookAt->up) != rayTracingPushConstantsValue->value().prevView) rayTracingPushConstantsValue->value().sampleNumber = 0;
            guiValues->sampleNumber = rayTracingPushConstantsValue->value().sampleNumber;

            viewer->update();
            viewer->recordAndSubmit();
            viewer->present();

            lookAt->get(rayTracingPushConstantsValue->value().prevView);
        }
    }
    catch (const vsg::Exception& e){
        std::cout << e.message << " VkResult = " << e.result << std::endl;
        return 0;
    }
    return 0;
}