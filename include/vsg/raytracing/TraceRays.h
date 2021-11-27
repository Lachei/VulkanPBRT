#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2019 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/Command.h>
#include <vsg/raytracing/RayTracingShaderGroup.h>

namespace vsg
{

    class VSG_DECLSPEC TraceRays : public Inherit<Command, TraceRays>
    {
    public:
        TraceRays();

        void record(CommandBuffer& commandBuffer) const override;

        ref_ptr<RayTracingShaderGroup> raygen;
        ref_ptr<RayTracingShaderGroup> missShader;
        ref_ptr<RayTracingShaderGroup> hitShader;
        ref_ptr<RayTracingShaderGroup> callableShader;

        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 0;
    };

} // namespace vsg
