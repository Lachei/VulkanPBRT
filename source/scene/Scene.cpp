#include "Scene.h"

#include "entity/Components.h"
#include "resources/Mesh.h"

#include <vsg/all.h>

#include <stack>

using namespace vkpbrt;
using namespace DirectX;

Scene::Scene(ResourceManager& resource_manager) : _resource_manager(resource_manager) {}

EntityHandle Scene::create_entity()
{
    uint64_t entity_id = _entity_manager.create_entity();
    return EntityHandle(entity_id, _entity_manager);
}

namespace
{
class AssimpSceneConverter
{
public:
    void convert_from_assimp_scene(const aiScene* ai_scene, Scene& target_scene, ResourceManager& resource_manager);

private:
    void load_meshes(const aiScene* ai_scene, ResourceManager& resource_manager);
    void load_materials(const aiScene* ai_scene, ResourceManager& resource_manager);
    void load_entities(const aiScene* ai_scene, Scene& scene);

    std::unordered_map<unsigned int, uint64_t> _mesh_index_to_resource_id;
};
}  // namespace

void Scene::load_from_assimp_scene(const aiScene* ai_scene)
{
    AssimpSceneConverter assimp_scene_converter;
    assimp_scene_converter.convert_from_assimp_scene(ai_scene, *this, this->_resource_manager);
}

void AssimpSceneConverter::convert_from_assimp_scene(
    const aiScene* ai_scene, Scene& target_scene, ResourceManager& resource_manager)
{
    load_meshes(ai_scene, resource_manager);
    load_materials(ai_scene, resource_manager);
    load_entities(ai_scene, target_scene);
}
void AssimpSceneConverter::load_meshes(const aiScene* ai_scene, ResourceManager& resource_manager)
{
    for (int i = 0; i < ai_scene->mNumMeshes; i++)
    {
        aiMesh* mesh = ai_scene->mMeshes[i];
        auto vertices = vsg::vec3Array::create(mesh->mNumVertices);
        auto normals = vsg::vec3Array::create(mesh->mNumVertices);
        auto texcoords = vsg::vec2Array::create(mesh->mNumVertices);
        std::vector<unsigned int> indices_vector;

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
                indices_vector.push_back(face.mIndices[k]);
            }
        }

        auto myindices = vsg::uintArray::create(static_cast<uint32_t>(indices_vector.size()));
        std::copy(indices_vector.begin(), indices_vector.end(), myindices->data());

        auto* mesh_resource = new MeshResource(vertices, normals, texcoords, myindices);
        uint64_t resource_id = resource_manager.add_resource(mesh_resource);
        _mesh_index_to_resource_id[i] = resource_id;
    }
}
void AssimpSceneConverter::load_materials(const aiScene* ai_scene, ResourceManager& resource_manager)
{
    // TODO
}
void AssimpSceneConverter::load_entities(const aiScene* ai_scene, Scene& scene)
{
    // create entities
    std::stack<aiNode*> ai_node_stack;
    ai_node_stack.push(ai_scene->mRootNode);
    while (!ai_node_stack.empty())
    {
        aiNode* ai_node = ai_node_stack.top();

        if (ai_node != nullptr)
        {
            aiMatrix4x4 m = ai_node->mTransformation;
            aiVector3t<float> scale;
            aiQuaterniont<float> rotation;
            aiVector3t<float> position;
            m.Decompose(scale, rotation, position);

            TransformComponent transform_component;
            transform_component.position = SimpleMath::Vector3(position.x, position.y, position.z);
            transform_component.orientation = SimpleMath::Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);
            transform_component.scale = SimpleMath::Vector3(scale.x, scale.y, scale.z);

            for (int i = 0; i < ai_node->mNumMeshes; i++)
            {
                EntityHandle entity_handle = scene.create_entity();
                entity_handle.add_component(transform_component);

                MeshComponent mesh_component;
                mesh_component.mesh_resource_id = _mesh_index_to_resource_id[i];
                entity_handle.add_component(mesh_component);
            }

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
