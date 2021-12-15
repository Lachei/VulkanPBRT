/* <editor-fold desc="MIT License">

Copyright(c) 2019 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Options.h>
#include <vsg/viewer/FlyNavigation.h>

#include <iostream>

using namespace vsg;

FlyNavigation::FlyNavigation(ref_ptr<Camera> camera) :
    _camera(camera)
{
    _lookAt = _camera->viewMatrix.cast<LookAt>();

    if (!_lookAt)
    {
        // TODO: need to work out how to map the original ViewMatrix to a LookAt and back, for now just fallback to assigning our own LookAt
        _lookAt = new LookAt;
    }

    addKeyViewpoint(KEY_Space, LookAt::create(*_lookAt), 1.0);
}

bool FlyNavigation::withinRenderArea(int32_t x, int32_t y) const
{
    auto renderArea = _camera->getRenderArea();

    return (x >= renderArea.offset.x && x < static_cast<int32_t>(renderArea.offset.x + renderArea.extent.width)) &&
           (y >= renderArea.offset.y && y < static_cast<int32_t>(renderArea.offset.y + renderArea.extent.height));
}

/// compute non dimensional window coordinate (-1,1) from event coords
dvec2 FlyNavigation::ndc(PointerEvent& event)
{
    auto renderArea = _camera->getRenderArea();

    double aspectRatio = static_cast<double>(renderArea.extent.width) / static_cast<double>(renderArea.extent.height);
    dvec2 v(
        (renderArea.extent.width > 0) ? (static_cast<double>(event.x - renderArea.offset.x) / static_cast<double>(renderArea.extent.width) * 2.0 - 1.0) * aspectRatio : 0.0,
        (renderArea.extent.height > 0) ? static_cast<double>(event.y - renderArea.offset.y) / static_cast<double>(renderArea.extent.height) * 2.0 - 1.0 : 0.0);
    return v;
}

void FlyNavigation::apply(KeyPressEvent& keyPress)
{
    if (keyPress.handled || !_lastPointerEventWithinRenderArea) return;

    if (auto itr = keyViewpoitMap.find(keyPress.keyBase); itr != keyViewpoitMap.end())
    {
        _previousTime = keyPress.time;

        setViewpoint(itr->second.lookAt, itr->second.duration);

        keyPress.handled = true;
    }

    if (keyPress.keyModifier & MODKEY_Alt){  //adding a key Viewpoint
        addKeyViewpoint(keyPress.keyBase, LookAt::create(*_lookAt), 1.0);
        keyPress.handled = true;
    }

    switch(keyPress.keyModified){
    case 'w':
        _walkDir.z = 1;
        _updateMode = MOVE;
        break;
    case 'a':
        _walkDir.x = -1;
        _updateMode = MOVE;
        break;
    case 's':
        _walkDir.z = -1;
        _updateMode = MOVE;
        break;
    case 'd':
        _walkDir.x = 1;
        _updateMode = MOVE;
        break;
    case 'q':
        _walkDir.y = -1;
        _updateMode = MOVE;
        break;
    case 'e':
        _walkDir.y = 1;
        _updateMode = MOVE;
        break;
    case 'W':
         _walkDir.z = 5;
        _updateMode = MOVE;
        break;
    case 'A':
        _walkDir.x = -5;
        _updateMode = MOVE;
        break;
    case 'S':
        _walkDir.z = -5;
        _updateMode = MOVE;
        break;
    case 'D':
        _walkDir.x = 5;
        _updateMode = MOVE;
        break;
    case 'Q':
        _walkDir.y = -5;
        _updateMode = MOVE;
        break;
    case 'E':
        _walkDir.y = 5;
        _updateMode = MOVE;
        break;
    default: break;
    }
}

void FlyNavigation::apply(KeyReleaseEvent& keyRelease){
    if (keyRelease.handled || !_lastPointerEventWithinRenderArea) return;

    switch(keyRelease.keyBase){
    case 'w':
    case 'W':
    case 's':
    case 'S':
        _walkDir.z = 0;
        keyRelease.handled = true;
        break;
    case 'a':
    case 'A':
    case 'd':
    case 'D':
        _walkDir.x = 0;
        keyRelease.handled = true;
        break;
    case 'q':
    case 'Q':
    case 'e':
    case 'E':
        _walkDir.y = 0;
        keyRelease.handled = true;
        break;
    default: break;
    }
}

void FlyNavigation::apply(ButtonPressEvent& buttonPress)
{
    if (buttonPress.handled) return;

    _hasFocus = withinRenderArea(buttonPress.x, buttonPress.y);
    _lastPointerEventWithinRenderArea = _hasFocus;

    if (_hasFocus) buttonPress.handled = true;

    _previousPointerEvent = &buttonPress;
}

void FlyNavigation::apply(ButtonReleaseEvent& buttonRelease)
{
    _lastPointerEventWithinRenderArea = withinRenderArea(buttonRelease.x, buttonRelease.y);
    _hasFocus = false;

    _previousPointerEvent = &buttonRelease;
}

void FlyNavigation::apply(MoveEvent& moveEvent)
{
    _lastPointerEventWithinRenderArea = withinRenderArea(moveEvent.x, moveEvent.y);

    if (moveEvent.handled || !_hasFocus) return;

    dvec2 new_ndc = ndc(moveEvent);

    if (!_previousPointerEvent) _previousPointerEvent = &moveEvent;

    dvec2 prev_ndc = ndc(*_previousPointerEvent);

    double dt = std::chrono::duration<double, std::chrono::seconds::period>(moveEvent.time - _previousPointerEvent->time).count();
    _previousDelta = dt;

    //_previousTime = moveEvent.time;

    if (moveEvent.mask & rotateButtonMask)
    {
        _updateMode = MOVE;

        moveEvent.handled = true;
        dvec2 del = new_ndc - prev_ndc;

        rotate(-del.x, del.y);
    }
    else if (moveEvent.mask & panButtonMask)
    {
        _updateMode = MOVE;

        moveEvent.handled = true;

        //panning does nothing
    }
    else if (moveEvent.mask & zoomButtonMask)
    {
        _updateMode = MOVE;

        moveEvent.handled = true;

        //zooming also does nothing
    }

    _thrown = false;

    _previousPointerEvent = &moveEvent;
}

void FlyNavigation::apply(ScrollWheelEvent& scrollWheel)
{
    if (scrollWheel.handled) return;

    scrollWheel.handled = true;
}

void FlyNavigation::apply(FrameEvent& frame)
{
    if (_endLookAt)
    {
        double timeSinceOfAnimation = std::chrono::duration<double, std::chrono::seconds::period>(frame.time - _startTime).count();
        if (timeSinceOfAnimation < _animationDuration)
        {
            double r = smoothstep(0.0, 1.0, timeSinceOfAnimation / _animationDuration);
            _lookAt->eye = mix(_startLookAt->eye, _endLookAt->eye, r);
            _lookAt->center = mix(_startLookAt->center, _endLookAt->center, r);
        }
        else
        {
            _lookAt->eye = _endLookAt->eye;
            _lookAt->center = _endLookAt->center;
            _lookAt->up = _endLookAt->up;

            _endLookAt = nullptr;
            _animationDuration = 0.0;
        }
    }
    else if(_updateMode != INACTIVE){
        double scale = std::chrono::duration<double, std::chrono::seconds::period>(frame.time - _previousTime).count();
        if(_updateMode == MOVE){
            if(length(_walkDir) == 0){
                _updateMode = INACTIVE;
            }
            else{
                walk(_walkDir * scale);
            }
        }
    }

    _previousTime = frame.time;
}

void FlyNavigation::rotate(double angleUp, double angleRight)
{
    dmat4 rotation = vsg::rotate(angleUp, _lookAt->up) * vsg::rotate(angleRight, vsg::normalize(vsg::cross(_lookAt->up, _lookAt->center - _lookAt->eye)));
    dvec3 cen = normalize(_lookAt->center - _lookAt->eye);
    _lookAt->center = rotation * cen + _lookAt->eye;
}

void FlyNavigation::walk(dvec3 dir)
{
    dvec3 lookVector = normalize(_lookAt->center - _lookAt->eye);
    dvec3 right = normalize(cross(lookVector, _lookAt->up));
    _lookAt->eye += lookVector * dir.z;                 //fly forward
    _lookAt->center += lookVector * dir.z;
    _lookAt->eye += right * dir.x;                      //fly right
    _lookAt->center += right * dir.x;
    _lookAt->eye += normalize(_lookAt->up) * dir.y;     //fly up
    _lookAt->center += normalize(_lookAt->up) * dir.y;
}

void FlyNavigation::addKeyViewpoint(KeySymbol key, ref_ptr<LookAt> lookAt, double duration)
{
    keyViewpoitMap[key].lookAt = lookAt;
    keyViewpoitMap[key].duration = duration;
}

void FlyNavigation::setViewpoint(ref_ptr<LookAt> lookAt, double duration)
{
    if (!lookAt) return;

    _thrown = false;

    if (duration == 0.0)
    {
        _lookAt->eye = lookAt->eye;
        _lookAt->center = lookAt->center;
        _lookAt->up = lookAt->up;

        _startLookAt = nullptr;
        _endLookAt = nullptr;
        _animationDuration = 0.0;

        //clampToGlobe();
    }
    else
    {
        _startTime = _previousTime;
        _startLookAt = vsg::LookAt::create(*_lookAt);
        _endLookAt = lookAt;
        _animationDuration = duration;
    }
}
