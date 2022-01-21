#pragma once
#include "../Entity.h"

/**
 * \brief Base class for entity behavior logic.
 */
class EntityBehaviorBase
{
public:
    EntityBehaviorBase(Entity entity);
    virtual ~EntityBehaviorBase() = 0;

    /**
     * \brief Called after this behaviour is attached to an entity.
     */
    virtual void Initialize();

    /**
     * \brief Called once per frame.
     * \param frame_time The time between frames. Typically used to make behaviors independent of the framerate.
     */
    virtual void Update(float frame_time);

    /**
     * \brief Called before this behaviour is detached from an entity or when the entity is destroyed.
     */
    virtual void Terminate();
protected:
    /**
     * \brief The entity this behavior is attached to.
     */
    Entity _entity;
};

inline EntityBehaviorBase::EntityBehaviorBase(Entity entity)
    : _entity(entity)
{
}
inline EntityBehaviorBase::~EntityBehaviorBase()
{
}
inline void EntityBehaviorBase::Initialize()
{
}
inline void EntityBehaviorBase::Update(float frame_time)
{
}
inline void EntityBehaviorBase::Terminate()
{
}
