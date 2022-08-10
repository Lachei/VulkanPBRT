/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Allocator.h>
#include <vsg/core/Auxiliary.h>
#include <vsg/core/ConstVisitor.h>
#include <vsg/core/Object.h>
#include <vsg/core/Visitor.h>

#include <vsg/traversals/RecordTraversal.h>

#include <vsg/io/Input.h>
#include <vsg/io/Logger.h>
#include <vsg/io/Options.h>
#include <vsg/io/Output.h>

using namespace vsg;

Object::Object() :
    _referenceCount(0),
    _auxiliary(nullptr)
{
}

Object::Object(const Object& rhs) :
    Object()
{
    if (rhs._auxiliary && rhs._auxiliary->getConnectedObject() == &rhs)
    {
        // the rhs's rhs._auxiliary is uniquely attached to it, so we need to create our own and copy it's ObjectMap across
        auto& userObjects = getOrCreateAuxiliary()->userObjects;
        userObjects = rhs._auxiliary->userObjects;
    }
}

Object& Object::operator=(const Object& rhs)
{
    //debug("Object& operator=(const Object&)");

    if (&rhs == this) return *this;

    if (rhs._auxiliary)
    {
        // the rhs's rhs._auxiliary is uniquely attached to it, so we need to create our own and copy it's ObjectMap across
        auto& userObjects = getOrCreateAuxiliary()->userObjects;
        userObjects = rhs._auxiliary->userObjects;
    }

    return *this;
}

Object::~Object()
{
    //debug("Object::~Object() ", this);

    if (_auxiliary)
    {
        _auxiliary->unref();
    }
}

void Object::_attemptDelete() const
{
    // what should happen when _delete is called on an Object with ref() of zero?  Need to decide whether this buggy application usage should be tested for.

    // if there is an auxiliary attached signal to it we wish to delete, and give it an opportunity to decide whether a delete is appropriate.
    // if no auxiliary is attached then go straight ahead and delete.
    if (_auxiliary == nullptr || _auxiliary->signalConnectedObjectToBeDeleted())
    {
        //debug("Object::_delete() ", this, " calling delete");

        delete this;
    }
    else
    {
        debug("Object::_delete() ", this, " choosing not to delete");
    }
}

void Object::accept(Visitor& visitor)
{
    visitor.apply(*this);
}

void Object::accept(ConstVisitor& visitor) const
{
    visitor.apply(*this);
}

void Object::accept(RecordTraversal& visitor) const
{
    visitor.apply(*this);
}

void Object::read(Input& input)
{
    auto numObjects = input.readValue<uint32_t>("userObjects");
    if (numObjects > 0)
    {
        auto& objectMap = getOrCreateAuxiliary()->userObjects;
        for (; numObjects > 0; --numObjects)
        {
            std::string key = input.readValue<std::string>("key");
            input.readObject("object", objectMap[key]);
        }
    }
}

void Object::write(Output& output) const
{
    if (_auxiliary)
    {
        // we have a unique auxiliary, need to write out it's ObjectMap entries
        auto& userObjects = _auxiliary->userObjects;
        output.writeValue<uint32_t>("userObjects", userObjects.size());
        for (auto& entry : userObjects)
        {
            output.write("key", entry.first);
            output.writeObject("object", entry.second.get());
        }
    }
    else
    {
        output.writeValue<uint32_t>("userObjects", 0);
    }
}

void Object::setObject(const std::string& key, Object* object)
{
    getOrCreateAuxiliary()->setObject(key, object);
}

Object* Object::getObject(const std::string& key)
{
    if (!_auxiliary) return nullptr;
    return _auxiliary->getObject(key);
}

const Object* Object::getObject(const std::string& key) const
{
    if (!_auxiliary) return nullptr;
    return _auxiliary->getObject(key);
}

void Object::removeObject(const std::string& key)
{
    if (_auxiliary)
    {
        _auxiliary->userObjects.erase(key);
    }
}

void Object::setAuxiliary(Auxiliary* auxiliary)
{
    if (_auxiliary)
    {
        _auxiliary->resetConnectedObject();
        _auxiliary->unref();
    }

    _auxiliary = auxiliary;

    if (auxiliary)
    {
        auxiliary->ref();
    }
}

Auxiliary* Object::getOrCreateAuxiliary()
{
    //debug("Object::getOrCreateAuxiliary() _auxiliary=",  _auxiliary);
    if (!_auxiliary)
    {
        _auxiliary = new Auxiliary(this);
        _auxiliary->ref();
    }
    return _auxiliary;
}

void* Object::operator new(std::size_t count)
{
    return vsg::allocate(count, vsg::ALLOCATOR_AFFINITY_OBJECTS);
}

void Object::operator delete(void* ptr)
{
    vsg::deallocate(ptr);
}
