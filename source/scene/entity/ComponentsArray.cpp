#include"ComponentsArray.h"
#include <string>
#include <type_traits>

template <typename T>
void ComponentsArray::AddComponent(std::string& component_name, T& component)
{
	this->components_array.push_back(new Component(component_name, component));
}

template <typename T>
T* ComponentsArray::GetComponent()
{
	for (std::vector<Component*>::iterator it = this->components_array.begin(); it != this->components_array.end(); ++it)
	{
		if (std::is_same_v<T, decltype(it->component_data)>)
		{
			return it->component_data;
		}
	}
}
