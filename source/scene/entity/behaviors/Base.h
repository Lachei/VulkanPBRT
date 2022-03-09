#pragma once
#include "../Entity.h"

namespace vkpbrt
{
/**
 * \brief Base class for entity behavior logic.
 */
class EntityBehaviorBase
{
public:
    explicit EntityBehaviorBase(Entity entity);
    virtual ~EntityBehaviorBase() = 0;

    /**
     * \brief Called after this behaviour is attached to an entity.
     */
    virtual void initialize();

    /**
     * \brief Called once per frame.
     * \param frame_time The time between frames. Typically used to make behaviors independent of the framerate.
     */
    virtual void update(float frame_time);

    /**
     * \brief Called before this behaviour is detached from an entity or when the entity is destroyed.
     */
    virtual void terminate();

protected:
    /**
     * \brief The entity this behavior is attached to.
     */
    Entity _entity;
};

inline EntityBehaviorBase::EntityBehaviorBase(Entity entity) : _entity(entity) {}
inline EntityBehaviorBase::~EntityBehaviorBase() = default;
inline void EntityBehaviorBase::initialize() {}
inline void EntityBehaviorBase::update(float frame_time) {}
inline void EntityBehaviorBase::terminate() {}
}  // namespace vkpbrt
