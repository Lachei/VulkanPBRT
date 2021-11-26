#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/raytracing/AccelerationStructure.h>
#include <vsg/state/Descriptor.h>

namespace vsg
{

    class VSG_DECLSPEC DescriptorAccelerationStructure : public Inherit<Descriptor, DescriptorAccelerationStructure>
    {
    public:
        DescriptorAccelerationStructure();

        explicit DescriptorAccelerationStructure(const AccelerationStructures& accelerationStructures, uint32_t dstBinding = 0, uint32_t dstArrayElement = 0, VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);

        AccelerationStructures& getAccelerationStructures() { return _accelerationStructures; }
        const AccelerationStructures& getAccelerationStructures() const { return _accelerationStructures; }

        void read(Input& input) override;
        void write(Output& output) const override;

        void compile(Context& context) override;

        void assignTo(Context& context, VkWriteDescriptorSet& wds) const override;

        uint32_t getNumDescriptors() const override;

    protected:
        AccelerationStructures _accelerationStructures;

        // populated by compile()
        std::vector<VkAccelerationStructureKHR> _vkAccelerationStructures;
    };
    VSG_type_name(vsg::DescriptorAccelerationStructure)

} // namespace vsg
