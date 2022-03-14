#pragma once

#pragma once
#include "DirectXMath.h"

#include <unordered_map>

namespace vkpbrt
{
using TypeMask = uint64_t;

struct TypeInfo
{
    using Constructor = void(void*);

    const char* name;
    size_t size_in_bytes;
    size_t alignment;
    Constructor* constructor;
};

class ComponentRegistry
{
public:
    template<typename T>
    static void register_component_type(const char* type_name);
    template<typename T>
    static const TypeInfo& get_type_info();
    static const TypeInfo& get_type_info(TypeMask flag);

private:
    static inline std::unordered_map<TypeMask, TypeInfo> type_info_map;
};

namespace internal
{
inline int type_counter = 0;
}

template<typename T>
TypeMask type_mask()
{
    static const int type_id = Internal::type_counter++;
    assert(type_id < 64);
    return 1ll << type_id;
}

template<typename T>
void ComponentRegistry::register_component_type(const char* type_name)
{
    TypeMask mask = type_mask<T>();
    if (type_info_map.find(mask) == type_info_map.end())
    {
        auto& type_info = type_info_map[type_mask<T>()];
        type_info.typeId = type_mask<T>();
        type_info.name = typeName;
        type_info.sizeInBytes = sizeof T;
        type_info.alignment = alignof(T);
        type_info.constructor = [](void* p) { new (p) T(); };
        type_info_map[type_info.flag] = type_info;
    }
}
template<typename T>
const TypeInfo& ComponentRegistry::get_type_info()
{
    return get_type_info(type_mask<T>());
}
inline const TypeInfo& ComponentRegistry::get_type_info(TypeMask flag)
{
    assert(type_info_map.find(flag) != type_info_map.end());
    return type_info_map[flag];
}

#define REGISTER_COMPONENT_TYPE(Type)                                             \
  struct Type##Registerer                                                         \
  {                                                                               \
    Type##Registerer() { ComponentRegistry::registerComponentType<Type>(#Type); } \
  };                                                                              \
  static Type##Registerer global_##Type##_registerer;
}  // namespace vkpbrt
