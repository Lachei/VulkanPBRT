#pragma once

#include<vsg/core/Object.h>
#include<vsg/rtx/RayTracingShaderGroup.h>
#include<vsg/rtx/RayTracingPipeline.h>

namespace vsg
{
    //forward declare Raytracing pipeline
    class RayTracingPipeline;

    struct ShaderBindingTableEntries{
        RayTracingShaderGroups raygenGroups;
        RayTracingShaderGroups raymissGroups;
        RayTracingShaderGroups hitGroups;
        RayTracingShaderGroups callableGroups;
    };

    class VSG_DECLSPEC RayTracingShaderBindingTable : public Inherit<Object, RayTracingShaderBindingTable>
    {
    public:
        RayTracingShaderBindingTable(){};
        RayTracingShaderBindingTable(RayTracingShaderGroups& shaderGroups, ShaderBindingTableEntries& bindingTableEntries): shaderGroups(shaderGroups), bindingTableEntries(bindingTableEntries){};
        
        void compile(Context& context);             //called by the ray tracing pipeline it is attached to

        RayTracingShaderGroups shaderGroups;
        ShaderBindingTableEntries bindingTableEntries;
        VkPipeline pipeline;        //set automatically by the ray tracing pipeline the binding table is assigned to
        BufferInfoList bindingTable;
    };
    VSG_type_name(vsg::RayTracingShaderBindingTable);
}