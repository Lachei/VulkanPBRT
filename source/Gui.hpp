#pragma once

#include <vsg/all.h>
#include <vsgImGui/imgui.h>
#include <vsgImGui/RenderImGui.h>
#include <vsgImGui/SendEventsToImGui.h>

class Gui
{
public:
    // add storage in this struct for checkboxes, slider floats etc... and add widgets in the 
    // ()operator function and get the resluting values by reading out the values struct put into cration of the class
    struct Values : public vsg::Inherit<vsg::Object, Values>
    {
        bool testCheck;
        float testFloatSlider[3];
        float testColor[4];
        char testTextInput[200];
        int triangleCount;
        int raysPerPixel;
        int width;
        int height;
        uint32_t sampleNumber;
    };

    Gui(vsg::ref_ptr<Values> values): _values(values), _state({true})
    {
    }


    // this is called for rendering.
    bool operator()()
    {
        ImGui::Begin("Test window");

        //here goes all needed settings
        ImGui::Text("Your text here.");
        ImGui::Checkbox("test check", &_values->testCheck);
        ImGui::SliderFloat3("testFloatSlider", _values->testFloatSlider, 0, 100);
        ImGui::ColorEdit4("testColor", _values->testColor);
        ImGui::InputText("testTextInput", _values->testTextInput, 200);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS) for %d triangles", 1000.0f / ImGui::GetIO().Framerate,
                    ImGui::GetIO().Framerate, _values->triangleCount);
        ImGui::Text("Render size is %d by %d with %d rays/pixel resulting in %.3f mRays/second", _values->width, _values->height,
                    _values->raysPerPixel, ImGui::GetIO().Framerate * _values->raysPerPixel * _values->width * _values->height / 1.0e6);
        ImGui::Text("Samples per pixel: %d", _values->sampleNumber);

        ImGui::End();
        return _state.active;
    }

private:
    vsg::ref_ptr<Values> _values;

    // add private state variables here
    struct State
    {
        bool active;
    } _state;
};
