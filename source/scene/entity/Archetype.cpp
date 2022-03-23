#include "Archetype.h"

using namespace vkpbrt;

Archetype::Archetype(TypeMask component_type_mask) : _entity_count(0) {}
void Archetype::add_entity(uint64_t entitiy_id) {}
void Archetype::move_entity(uint64_t entity_id, Archetype& target_archetype) {}
void Archetype::remove_entity(uint64_t entity_id) {}
