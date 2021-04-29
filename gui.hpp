#pragma once

#include <vsgImGui/RenderImGui.h>
#include <vsgImGui/SendEventsToImGui.h>
#include <imgui.h>

#include <vsg/all.h>

class Gui{
public:
    // add storage in this struct for checkboxes, slider floats etc... and add widgets in the 
    // ()operator function and get the resluting values by reading out the values struct put into cration of the class
    struct Values : public vsg::Inherit<vsg::Object, Values>{
        bool testCheck;
        float testFloatSlider[3];
        float testColor[4];
        char testTextInput[200];
    };
    Gui(vsg::ref_ptr<Values> values): _values(values), _state({true}){}


    // this is called for rendering.
    bool operator()(){
        ImGui::Begin("Test window");

        //here goes all needed settings
        ImGui::Text("Your text here.");
        ImGui::Checkbox("test check", &_values->testCheck);
        ImGui::SliderFloat3("testFloatSlider", _values->testFloatSlider, 0, 100);
        ImGui::ColorEdit4("testColor", _values->testColor);
        ImGui::InputText("testTextInput", _values->testTextInput, 200);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        ImGui::End();
        return _state.active;
    }

private:
    vsg::ref_ptr<Values> _values;

    // add private state variables here
    struct State{
        bool active;
    }_state;
};