#pragma once
// save all different type components and get component based on its type
//TODO
#include<vector>
class Component
{
public:
    Component(const std::string& name)
        : component_name(name) {}
    virtual ~Component() {}
private:
    std::string component_name;
};

template< typename T >
class TypedComponent : public Component
{
public:
    TypedComponent(const std::string& name, const T& data)
        : Component(name), component_data(data);
protected:
    T* component_data;
};

class ComponentsArray
{
public:
	template <typename T>
	void AddComponent(std::string& component_name, T& component);
	template <typename T>
	T* GetComponent();
private:
    std::vector<std::shared_ptr<Component>> components_array;
};
