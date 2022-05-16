#include "Scene.h"

#include "entity/Components.h"

#include <vsg/all.h>

#include <stack>

using namespace vkpbrt;
using namespace DirectX;

Scene::Scene(ResourceManager& resource_manager) : _resource_manager(resource_manager) {}
Scene::Scene(ResourceManager& resource_manager, aiScene* ai_scene) : Scene(resource_manager)
{
    // TODO: load meshes
    for (int i = 0; i < ai_scene->mNumMeshes; ++i)
    {
        aiMesh* mesh = ai_scene->mMeshes[i];
        auto vertices = vsg::vec3Array::create(mesh->mNumVertices);
        auto normals = vsg::vec3Array::create(mesh->mNumVertices);
        auto texcoords = vsg::vec2Array::create(mesh->mNumVertices);
        std::vector<unsigned int> indices;

        for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
        {
            vertices->at(j) = vsg::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);

            if (mesh->mNormals != nullptr)
            {
                normals->at(j) = vsg::vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
            }
            else
            {
                normals->at(j) = vsg::vec3(0, 0, 0);
            }

            if (mesh->mTextureCoords[0] != nullptr)
            {
                texcoords->at(j) = vsg::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y);
            }
            else
            {
                texcoords->at(j) = vsg::vec2(0, 0);
            }
        }

        for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
        {
            const auto& face = mesh->mFaces[j];

            for (unsigned int k = 0; k < face.mNumIndices; ++k)
            {
                indices.push_back(face.mIndices[k]);
            }
        }

        vsg::ref_ptr<vsg::Data> vsg_indices;

        /*if (indices.size() < std::numeric_limits<uint16_t>::max())
        {
            auto myindices = vsg::ushortArray::create(static_cast<uint16_t>(indices.size()));
            std::copy(indices.begin(), indices.end(), myindices->data());
            vsg_indices = myindices;
        }
        else*/
        {
            auto myindices = vsg::uintArray::create(static_cast<uint32_t>(indices.size()));
            std::copy(indices.begin(), indices.end(), myindices->data());
            vsg_indices = myindices;
        }

        // TODO: add mesh to resource manager
        /* stategroup->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{ vertices, normals, texcoords }));
         stategroup->addChild(vsg::BindIndexBuffer::create(vsg_indices));
         stategroup->addChild(vsg::DrawIndexed::create(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0));*/
    }

    // TODO: load materials

    std::stack<aiNode*> ai_node_stack;
    ai_node_stack.push(ai_scene->mRootNode);

    while (!ai_node_stack.empty())
    {
        aiNode* ai_node = ai_node_stack.top();

        if (ai_node != nullptr)
        {
            EntityHandle new_entity_handle = create_entity();

            aiMatrix4x4 m = ai_node->mTransformation;
            aiVector3t<float> scale;
            aiQuaterniont<float> rotation;
            aiVector3t<float> position;
            m.Decompose(scale, rotation, position);

            new_entity_handle.add_component<TransformComponent>();
            auto* transform_component = new_entity_handle.get_component<TransformComponent>();
            transform_component->position = SimpleMath::Vector3(position.x, position.y, position.z);
            transform_component->orientation = SimpleMath::Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);
            transform_component->scale = SimpleMath::Vector3(scale.x, scale.y, scale.z);

            // TODO: create one entity per reference mesh

            ai_node_stack.pop();

            for (int i = 0; i < ai_node->mNumChildren; ++i)
            {
                ai_node_stack.push(ai_node->mChildren[i]);
            }
        }
        else
        {
            ai_node_stack.pop();
        }
    }
}

EntityHandle Scene::create_entity()
{
    uint64_t entity_id = _entity_manager.create_entity();
    return EntityHandle(entity_id, _entity_manager);
}
