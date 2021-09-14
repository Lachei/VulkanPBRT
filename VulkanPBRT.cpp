#include <iostream>
#include <set>
#include "Defines.hpp"
#include <vsg/all.h>
#include <vsgXchange/models.h>
#include <vsgXchange/images.h>
#include "RayTracingVisitor.hpp"
#include "PBRTPipeline.hpp"
#include "IO/IO.hpp"
#include "Denoiser/BFR.hpp"
#include "TAA/taa.hpp"
#include "Denoiser/BFRBlender.hpp"
#include "Denoiser/BMFR.hpp"
#include "PipelineStructs.hpp"
#include "CountTrianglesVisitor.hpp"
 
#include "gui.hpp"

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
DenoisingType denoisingType = DenoisingType::BMFR;

enum class DenoisingBlockSize{
    x8,
    x16,
    x32,
    x64,
    x8x16x32
};
DenoisingBlockSize denoisingBlockSize = DenoisingBlockSize::x32;

int main(int argc, char** argv){
    try{
        // command line parsing
        vsg::CommandLine arguments(&argc, argv);

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "VulkanPBRT";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if(arguments.read({"--fullscreen", "-fs"})) windowTraits->fullscreen = true;
        if(arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) windowTraits->fullscreen = false;
        arguments.read("--screen", windowTraits->screenNum);

        auto numFrames = arguments.value(-1, "-f");
        auto filename = arguments.value(std::string(), "-i");
        if(arguments.read("m")) filename = "models/raytracing_scene.vsgt";
        if(arguments.errors()) return arguments.writeErrorMessages(std::cerr);

#ifdef _DEBUG
        // overwriting command line options for debug
        windowTraits->debugLayer = true;
        windowTraits->width = 1800;
        filename = getUserDirectory() + "Downloads/glTF-Sample-Models-master/2.0/Sponza/glTF/Sponza.gltf";//"/Downloads/teapot.obj";
#endif

        //viewer creation
        auto viewer = vsg::Viewer::create();
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
        vsg::ref_ptr<vsg::Device> device;
        try{
            device = window->getOrCreateDevice();
        }
        catch(const vsg::Exception& e){
            std::cout << e.message << " Vk Result = " << e.result << std::endl;
            return 0;
        }

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
            viewer->addWindow(window);
        }

        //creating camera matrices
        auto perspective = vsg::Perspective::create(60, static_cast<double>(windowTraits->width) / static_cast<double>(windowTraits->height), .1, 1000);
        vsg::ref_ptr<vsg::LookAt> lookAt;

        vsg::ref_ptr<vsg::TopLevelAccelerationStructure> tlas;
        vsg::ref_ptr<vsg::Node> loaded_scene;
        auto guiValues = Gui::Values::create();
        guiValues->width = windowTraits->width;
        guiValues->height = windowTraits->height;
        if(filename.empty()){
            //no extern geometry
            //acceleration structures
            auto vertices = vsg::vec3Array::create(
                {{-1.0f, -1.0f, 0.0f},
                {1.0f, -1.0f, 0.0f},
                {0.0f, 1.0f, 0.0f}}
            );
            auto indices = vsg::uintArray::create({0,1,2});

            auto accelGeometry = vsg::AccelerationGeometry::create();
            accelGeometry->verts = vertices;
            accelGeometry->indices = indices;

            auto blas = vsg::BottomLevelAccelerationStructure::create(device);
            blas->geometries.push_back(accelGeometry);

            tlas = vsg::TopLevelAccelerationStructure::create(device);

            auto geominstance = vsg::GeometryInstance::create();
            geominstance->accelerationStructure = blas;
            geominstance->transform = vsg::mat4();

            tlas->geometryInstances.push_back(geominstance);

            lookAt = vsg::LookAt::create(vsg::dvec3(0,0,-2.5), vsg::dvec3(0,0,0), vsg::dvec3(0,1,0));
            guiValues->triangleCount = 1;
        }
        else{
            auto options = vsg::Options::create(vsgXchange::assimp::create(), vsgXchange::dds::create(), vsgXchange::stbi::create()); //using the assimp loader
            loaded_scene = vsg::read_cast<vsg::Node>(filename, options);
            if(!loaded_scene){
                std::cout << "Scene not found: " << filename << std::endl;
                return 1;
            }
            vsg::BuildAccelerationStructureTraversal buildAccelStruct(device);
            loaded_scene->accept(buildAccelStruct);
            tlas = buildAccelStruct.tlas;

            lookAt = vsg::LookAt::create(vsg::dvec3(0.0, 1.0, -5.0), vsg::dvec3(0.0, 0.5, 0.0), vsg::dvec3(0.0, 0.0, 1.0));
            CountTrianglesVisitor counter;
            loaded_scene->accept(counter);
            guiValues->triangleCount = counter.triangleCount;
        }
        
        auto rayTracingPushConstantsValue = RayTracingPushConstantsValue::create();
        perspective->get_inverse(rayTracingPushConstantsValue->value().projInverse);
        lookAt->get_inverse(rayTracingPushConstantsValue->value().viewInverse);
        lookAt->get(rayTracingPushConstantsValue->value().prevView);
        rayTracingPushConstantsValue->value().frameNumber = 0;
        rayTracingPushConstantsValue->value().steadyCamFrame = 0;
        auto pushConstants = vsg::PushConstants::create(VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, rayTracingPushConstantsValue);
        auto computeConstants = vsg::PushConstants::create(VK_SHADER_STAGE_COMPUTE_BIT, 0, rayTracingPushConstantsValue);

        // raytracing pipelin setup
        uint maxRecursionDepth = 2;
        auto illuminationBuffer = IlluminationBufferFinalDemodulated::create(windowTraits->width, windowTraits->height);
        auto pbrtPipeline = PBRTPipeline::create(windowTraits->width, windowTraits->height, maxRecursionDepth, loaded_scene, illuminationBuffer);
        auto gBuffer = pbrtPipeline->gBuffer;
        auto accumulationBuffer = pbrtPipeline->accumulationBuffer;
        pbrtPipeline->setTlas(tlas);
        
        // -------------------------------------------------------------------------------------
        // creation of scene graph for ray tracing
        // -------------------------------------------------------------------------------------
        guiValues->raysPerPixel = maxRecursionDepth * 2; //for each depth recursion one next event estimate is done

        //state group to bind the pipeline and descriptorset
        auto scenegraph = vsg::Commands::create();
        scenegraph->addChild(pbrtPipeline->bindRayTracingPipeline);
        scenegraph->addChild(pushConstants);
        scenegraph->addChild(pbrtPipeline->bindRayTracingDescriptorSet);

        //ray trace setup
        auto traceRays = vsg::TraceRays::create();
        traceRays->bindingTable = pbrtPipeline->shaderBindingTable;
        traceRays->width = windowTraits->width;
        traceRays->height = windowTraits->height;
        traceRays->depth = 1;

        scenegraph->addChild(traceRays);

        // -------------------------------------------------------------------------------------
        // image layout conversions and correct binding of different denoising tequniques
        // -------------------------------------------------------------------------------------
        vsg::ref_ptr<vsg::CopyImageViewToWindow> copyImageViewToWindow;
        vsg::CompileTraversal imageLayoutCompile(window);
        pbrtPipeline->compile(imageLayoutCompile.context);
        pbrtPipeline->updateImageLayouts(imageLayoutCompile.context);
        switch(denoisingType){
        case DenoisingType::None:
            copyImageViewToWindow = vsg::CopyImageViewToWindow::create(illuminationBuffer->illuminationImages[0]->imageInfoList[0].imageView, window);
            break;
        case DenoisingType::BFR:
            switch(denoisingBlockSize){
            case DenoisingBlockSize::x8:
            {
                auto bfr8 = BFR::create(windowTraits->width, windowTraits->height, 8, 8, gBuffer, illuminationBuffer, accumulationBuffer);
                bfr8->compile(imageLayoutCompile.context);
                bfr8->updateImageLayouts(imageLayoutCompile.context);
                bfr8->addDispatchToCommandGraph(scenegraph, computeConstants);
                copyImageViewToWindow = vsg::CopyImageViewToWindow::create(bfr8->taaPipeline->finalImage->imageInfoList[0].imageView, window);
                break;
            }
            case DenoisingBlockSize::x16:
            {
                auto bfr16 = BFR::create(windowTraits->width, windowTraits->height, 16, 16, gBuffer, illuminationBuffer, accumulationBuffer);
                bfr16->compile(imageLayoutCompile.context);
                bfr16->updateImageLayouts(imageLayoutCompile.context);
                bfr16->addDispatchToCommandGraph(scenegraph, computeConstants);
                copyImageViewToWindow = vsg::CopyImageViewToWindow::create(bfr16->taaPipeline->finalImage->imageInfoList[0].imageView, window);
                break;
            }
            case DenoisingBlockSize::x32:
            {
                auto bfr32 = BFR::create(windowTraits->width, windowTraits->height, 32, 32, gBuffer, illuminationBuffer, accumulationBuffer);
                bfr32->compile(imageLayoutCompile.context);
                bfr32->updateImageLayouts(imageLayoutCompile.context);
                bfr32->addDispatchToCommandGraph(scenegraph, computeConstants);
                copyImageViewToWindow = vsg::CopyImageViewToWindow::create(bfr32->taaPipeline->finalImage->imageInfoList[0].imageView, window);
                break;
            }
            case DenoisingBlockSize::x8x16x32:
            {
                auto bfr8 = BFR::create(windowTraits->width, windowTraits->height, 8, 8, gBuffer, illuminationBuffer, accumulationBuffer);
                auto bfr16 = BFR::create(windowTraits->width, windowTraits->height, 16, 16, gBuffer, illuminationBuffer, accumulationBuffer);
                auto bfr32 = BFR::create(windowTraits->width, windowTraits->height, 32, 32, gBuffer, illuminationBuffer, accumulationBuffer);
                auto blender = BFRBlender::create(windowTraits->width, windowTraits->height, 
                                        illuminationBuffer->illuminationImages[1], illuminationBuffer->illuminationImages[2],
                                        bfr8->taaPipeline->finalImage, bfr16->taaPipeline->finalImage, bfr32->taaPipeline->finalImage);
                bfr8->compile(imageLayoutCompile.context);
                bfr8->updateImageLayouts(imageLayoutCompile.context);
                bfr16->compile(imageLayoutCompile.context);
                bfr16->updateImageLayouts(imageLayoutCompile.context);
                bfr32->compile(imageLayoutCompile.context);
                bfr32->updateImageLayouts(imageLayoutCompile.context);
                blender->compile(imageLayoutCompile.context);
                blender->updateImageLayouts(imageLayoutCompile.context);
                bfr8->addDispatchToCommandGraph(scenegraph, computeConstants);
                bfr16->addDispatchToCommandGraph(scenegraph, computeConstants);
                bfr32->addDispatchToCommandGraph(scenegraph, computeConstants);
                blender->addDispatchToCommandGraph(scenegraph);
                copyImageViewToWindow = vsg::CopyImageViewToWindow::create(blender->finalImage->imageInfoList[0].imageView, window);
                break;
            }
            }
            break;
        case DenoisingType::BMFR:
            switch(denoisingBlockSize){
            case DenoisingBlockSize::x8:
                std::cout << "Not yet implemented" << std::endl;
                break;
            case DenoisingBlockSize::x16:
                std::cout << "Not yet implemented" << std::endl;
                break;
            case DenoisingBlockSize::x32:
            {
                auto bmfr32 = BMFR::create(windowTraits->width, windowTraits->height, 32, 32, gBuffer, illuminationBuffer, accumulationBuffer);
                bmfr32->compileImages(imageLayoutCompile.context);
                bmfr32->updateImageLayouts(imageLayoutCompile.context);
                bmfr32->addDispatchToCommandGraph(scenegraph, computeConstants);
                copyImageViewToWindow = vsg::CopyImageViewToWindow::create(bmfr32->taaPipeline->finalImage->imageInfoList[0].imageView, window);
                break;
            }
            case DenoisingBlockSize::x8x16x32:
                std::cout << "Not yet implemented" << std::endl;
                break;
            }
            break;
        case DenoisingType::SVG:
            std::cout << "Not yet implemented" << std::endl;
            break;
        }

        imageLayoutCompile.context.record();

        pbrtPipeline->cmdCopyToAccImages(scenegraph);

        auto viewport = vsg::ViewportState::create(0, 0, windowTraits->width, windowTraits->height);
        auto camera = vsg::Camera::create(perspective, lookAt, viewport);

        auto commandGraph = vsg::CommandGraph::create(window);
        auto renderGraph = vsg::createRenderGraphForView(window, camera, vsgImGui::RenderImGui::create(window, Gui(guiValues))); // render graph for gui rendering
        renderGraph->clearValues.clear();   //removing clear values to avoid clearing the raytraced image

        commandGraph->addChild(scenegraph);
        commandGraph->addChild(copyImageViewToWindow);
        commandGraph->addChild(renderGraph);
        
        //close handler to close and imgui handler to forward to imgui
        viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));
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
            rayTracingPushConstantsValue->value().steadyCamFrame++;
            if(viewer->getEvents().size() > 1) rayTracingPushConstantsValue->value().steadyCamFrame = 0;
            guiValues->steadyCamFrame = rayTracingPushConstantsValue->value().steadyCamFrame;

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