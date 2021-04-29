 #include <iostream>
 #include <vsg/all.h>
 
 #include "gui.hpp"

 struct RayTracingUniform{
     vsg::mat4 viewInverse;
     vsg::mat4 projInverse;
 };

class RayTracingUniformValue : public vsg::Inherit<vsg::Value<RayTracingUniform>, RayTracingUniformValue>{
    public:
    RayTracingUniformValue(){}
};

int main(int argc, char** argv){
    try{
        // command line parsing
        vsg::CommandLine arguments(&argc, argv);

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "VulkanPBRT";
        windowTraits->debugLayer = true;//arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if(arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if(arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) windowTraits->fullscreen = false;
        arguments.read("--screen", windowTraits->screenNum);

        auto numFrames = arguments.value(-1, "-f");
        auto filename = arguments.value(std::string(), "-i");
        if(arguments.read("m")) filename = "models/raytracing_scene.vsgt";
        if(arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        //viewer creation
        auto viewer = vsg::Viewer::create();
        windowTraits->queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        windowTraits->imageAvailableSemaphoreWaitFlag = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        windowTraits->swapchainPreferences.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        //windowTraits->instanceExtensionNames.push_back("VK_VERSION_1_1");
        windowTraits->deviceExtensionNames = {VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, VK_NV_RAY_TRACING_EXTENSION_NAME};

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
        //setting a custom render pass
        vsg::AttachmentDescription  colorAttachment = vsg::defaultColorAttachment(window->surfaceFormat().format);
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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
        //window->clearColor() = {};
        viewer->addWindow(window);

        //shader creation
        const uint32_t shaderIndexRaygen = 0;
        const uint32_t shaderIndexMiss = 1;
        const uint32_t shaderIndexClosestHit = 2;
        std::string raygenPath = "shaders/simple_raygen.rgen.spv";
        std::string raymissPath = "shaders/simple_miss.rmiss.spv";
        std::string closesthitPath = "shaders/simple_closesthit.rchit.spv";

        auto raygenShader = vsg::ShaderStage::read(VK_SHADER_STAGE_RAYGEN_BIT_NV, "main", raygenPath);
        auto raymissShader = vsg::ShaderStage::read(VK_SHADER_STAGE_MISS_BIT_NV, "main", raymissPath);
        auto closesthitShader = vsg::ShaderStage::read(VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, "main", closesthitPath);
        if(!raygenShader || !raymissShader || !closesthitShader){
            std::cout << "Shader creation failed" << std::endl;
            return 1;
        }

        auto shaderStage = vsg::ShaderStages{raygenShader, raymissShader, closesthitShader};

        auto raygenShaderGroup = vsg::RayTracingShaderGroup::create();
        raygenShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
        raygenShaderGroup->generalShader = 0;
        auto raymissShaderGroup = vsg::RayTracingShaderGroup::create();
        raymissShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
        raymissShaderGroup->generalShader = 1;
        auto closesthitShaderGroup = vsg::RayTracingShaderGroup::create();
        closesthitShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
        closesthitShaderGroup->closestHitShader = 2;
        auto shaderGroups = vsg::RayTracingShaderGroups{raygenShaderGroup, raymissShaderGroup, closesthitShaderGroup};

        //creating camera things
        auto perspective = vsg::Perspective::create(60, static_cast<double>(windowTraits->width) / static_cast<double>(windowTraits->height), .1, 1000);
        vsg::ref_ptr<vsg::LookAt> lookAt;

        vsg::ref_ptr<vsg::TopLevelAccelerationStructure> tlas;
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
        }
        else{
            auto loaded_scene = vsg::read_cast<vsg::Node>(filename);
            if(!loaded_scene){
                std::cout << "Scene not found: " << filename << std::endl;
                return 1;
            }
            vsg::BuildAccelerationStructureTraversal buildAccelStruct(device);
            loaded_scene->accept(buildAccelStruct);
            tlas = buildAccelStruct.tlas;

            lookAt = vsg::LookAt::create(vsg::dvec3(0.0, 1.0, -5.0), vsg::dvec3(0.0, 0.5, 0.0), vsg::dvec3(0.0, 1.0, 0.0));
        }

        vsg::CompileTraversal compile(window);

        auto storageImage = vsg::Image::create();
        storageImage->imageType = VK_IMAGE_TYPE_2D;
        storageImage->format = VK_FORMAT_B8G8R8A8_UNORM;
        storageImage->extent.width = windowTraits->width;
        storageImage->extent.height = windowTraits->height;
        storageImage->extent.depth = 1;
        storageImage->mipLevels = 1;
        storageImage->arrayLayers = 1;
        storageImage->samples = VK_SAMPLE_COUNT_1_BIT;
        storageImage->tiling = VK_IMAGE_TILING_OPTIMAL;
        storageImage->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        storageImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        storageImage->flags = 0;
        storageImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vsg::ImageInfo storageImageInfo{nullptr, vsg::createImageView(compile.context, storageImage, VK_IMAGE_ASPECT_COLOR_BIT), VK_IMAGE_LAYOUT_GENERAL};
        
        auto raytracingUniformValues = new RayTracingUniformValue();
        perspective->get_inverse(raytracingUniformValues->value().projInverse);
        lookAt->get_inverse(raytracingUniformValues->value().viewInverse);
        vsg::ref_ptr<RayTracingUniformValue> raytracingUniform(raytracingUniformValues);

        //set up graphics pipeline
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV, nullptr},
            {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV, nullptr}
        };
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

        auto accelDescriptor = vsg::DescriptorAccelerationStructure::create(vsg::AccelerationStructures{tlas}, 0, 0);
        auto storageImageDescriptor = vsg::DescriptorImage::create(storageImageInfo,1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        auto raytracingUniformDescriptor = vsg::DescriptorBuffer::create(raytracingUniform, 2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        raytracingUniformDescriptor->copyDataListToBuffers();

        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{});
        auto raytracingPipeline = vsg::RayTracingPipeline::create(pipelineLayout, shaderStage, shaderGroups);
        auto bindRayTracingPipeline = vsg::BindRayTracingPipeline::create(raytracingPipeline);

        auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{accelDescriptor, storageImageDescriptor, raytracingUniformDescriptor});
        auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, raytracingPipeline->getPipelineLayout(), 0, vsg::DescriptorSets{descriptorSet});

        //state group to bind the pipeline and descriptorset
        auto scenegraph = vsg::Commands::create();
        scenegraph->addChild(bindRayTracingPipeline);
        scenegraph->addChild(bindDescriptorSets);

        //ray trace setup
        auto traceRays = vsg::TraceRays::create();
        traceRays->raygen = raygenShaderGroup;
        traceRays->missShader = raymissShaderGroup;
        traceRays->hitShader = closesthitShaderGroup;
        traceRays->width = windowTraits->width;
        traceRays->height = windowTraits->height;
        traceRays->depth = 1;

        scenegraph->addChild(traceRays);

        auto viewport = vsg::ViewportState::create(0, 0, windowTraits->width, windowTraits->height);
        auto camera = vsg::Camera::create(perspective, lookAt, viewport);

        auto commandGraph = vsg::CommandGraph::create(window);
        auto renderGraph = vsg::RenderGraph::create(window); // render graph for gui rendering
        renderGraph->clearValues.clear();   //removing clear values to avoid clearing the raytraced image
        auto guiValues = Gui::Values::create();
        auto copyImageViewToWindow = vsg::CopyImageViewToWindow::create(storageImageInfo.imageView, window);

        //renderGraph->addChild(scenegraph);
        //renderGraph->addChild(copyImageViewToWindow);
        renderGraph->addChild(vsgImGui::RenderImGui::create(window, Gui(guiValues)));
        commandGraph->addChild(renderGraph);
        
        //close handler to close and imgui handler to forward to imgui
        viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());
        viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
        viewer->compile();

        while(viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0)){
            viewer->handleEvents();
            viewer->update();
            viewer->recordAndSubmit();
            viewer->present();
        }
    }
    catch (const vsg::Exception& e){
        std::cout << e.message << " VkResult = " << e.result << std::endl;
        return 0;
    }
    return 0;
}