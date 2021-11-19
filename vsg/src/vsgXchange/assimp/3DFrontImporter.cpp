#include "3DFrontImporter.h"

#include <assimp/scene.h>
#include <assimp/DefaultIOSystem.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

static const aiImporterDesc desc = {
    "3D-FRONT scene importer",
    "",
    "",
    "",
    aiImporterFlags_SupportTextFlavour,
    0,
    0,
    0,
    0,
    "json"
};

bool AI3DFrontImporter::CanRead(const std::string& pFile, Assimp::IOSystem* pIOHandler, bool checkSig) const
{
    return SimpleExtensionCheck(pFile, "json");
}
const aiImporterDesc* AI3DFrontImporter::GetInfo() const
{
    return &desc;
}
void AI3DFrontImporter::InternReadFile(const std::string& pFile, aiScene* pScene, Assimp::IOSystem* pIOHandler)
{
    std::ifstream scene_file(pFile);
    if (!scene_file)
    {
        throw DeadlyImportError("Failed to open file " + pFile + ".");
    }

    nlohmann::json scene_json;
    scene_file >> scene_json;

    // parse room meshes
    std::unordered_map<std::string, std::vector<uint32_t>> model_uid_to_mesh_indices_map;
    const auto& room_meshes = scene_json["mesh"];
    pScene->mNumMeshes = room_meshes.size();
    if (pScene->mNumMeshes > 0)
    {
        pScene->mMeshes = new aiMesh*[pScene->mNumMeshes];
        uint32_t mesh_index = 0;
        for (const auto& raw_mesh : room_meshes)
        {
            pScene->mMeshes[mesh_index] = new aiMesh;
            auto& ai_mesh = pScene->mMeshes[mesh_index];
            ai_mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
            ai_mesh->mNumUVComponents[0] = 2;

            // parse vertices, normals and tex coords
            const auto& raw_vertices = raw_mesh["xyz"];
            ai_mesh->mNumVertices = raw_vertices.size() / 3;
            if (ai_mesh->mNumVertices > 0)
            {
                const auto& raw_normals = raw_mesh["normal"];
                const auto& raw_tex_coords = raw_mesh["uv"];

                ai_mesh->mVertices = new aiVector3D[ai_mesh->mNumVertices];
                ai_mesh->mNormals = new aiVector3D[ai_mesh->mNumVertices];
                ai_mesh->mTextureCoords[0] = new aiVector3D[ai_mesh->mNumVertices];
                for (int i = 0; i < ai_mesh->mNumVertices; i++)
                {
                    int x_index = i * 3;
                    int y_index = x_index + 1;
                    int z_index = x_index + 2;
                    int u_index = i * 2;
                    int v_index = u_index + 1;

                    ai_mesh->mVertices[i] = aiVector3D(raw_vertices[x_index], raw_vertices[y_index], raw_vertices[z_index]);
                    ai_mesh->mNormals[i] = aiVector3D(raw_normals[x_index], raw_normals[y_index], raw_normals[z_index]);
                    ai_mesh->mTextureCoords[0][i] = aiVector3D(raw_tex_coords[u_index], raw_tex_coords[v_index], 0);
                }
            }

            // parse indices
            const auto& raw_indices = raw_mesh["faces"];
            ai_mesh->mNumFaces = raw_indices.size() / 3;
            if (ai_mesh->mNumFaces > 0)
            {
                ai_mesh->mFaces = new aiFace[ai_mesh->mNumFaces];
                for (int i = 0; i < ai_mesh->mNumFaces; i++)
                {
                    int index_0 = i * 3;
                    int index_1 = index_0 + 1;
                    int index_2 = index_0 + 2;

                    ai_mesh->mFaces[i] = aiFace();
                    auto& face = ai_mesh->mFaces[i];
                    face.mNumIndices = 3;
                    face.mIndices = new unsigned int[3];
                    face.mIndices[0] = raw_indices[index_0];
                    face.mIndices[1] = raw_indices[index_1];
                    face.mIndices[2] = raw_indices[index_2];
                }
            }
            // TODO: load material
            // TODO: set AABB

            ai_mesh->mName = raw_mesh["instanceid"];
            model_uid_to_mesh_indices_map[raw_mesh["uid"]].push_back(mesh_index++);
        }
    }

    // get furniture diretories
    auto root_path_3d_front = fs::absolute(fs::path(pFile)).parent_path().parent_path();
    std::vector<fs::path> furniture_directories;
    for (const auto& directory_entry : fs::directory_iterator(root_path_3d_front))
    {
        if (!directory_entry.is_directory())
        {
            continue;
        }
        const fs::path& directory_path = directory_entry.path();
        std::string directory_name = directory_path.filename().string();
        if (directory_name.find("3D-FUTURE-model") != std::string::npos)
        {
            furniture_directories.push_back(directory_path);
        }
    }

    // load furniture
    const auto& furniture = scene_json["furniture"];
    // TODO: load objs from model directories
    // TODO: move meshes to main aiScene and add indices to model_uid_to_mesh_indices_map


    // parse scene
    pScene->mRootNode = new aiNode;
    const auto& raw_rooms = scene_json["scene"]["room"];
    pScene->mRootNode->mNumChildren = raw_rooms.size();
    pScene->mRootNode->mChildren = new aiNode*[raw_rooms.size()];
    for (int room_index = 0; room_index < raw_rooms.size(); room_index++)
    {
        const auto& raw_room = raw_rooms[room_index];
        const auto& raw_children = raw_room["children"];

        pScene->mRootNode->mChildren[room_index] = new aiNode;
        auto& room_node = pScene->mRootNode->mChildren[room_index];
        room_node->mName = raw_room["instanceid"];
        room_node->mParent = pScene->mRootNode;
        room_node->mNumChildren = raw_children.size();
        room_node->mChildren = new aiNode*[raw_children.size()];

        for (int child_index = 0; child_index < raw_children.size(); child_index++)
        {
            const auto& raw_child = raw_children[child_index];
            room_node->mChildren[child_index] = new aiNode;

            std::string instance_id = raw_child["instanceid"];
            auto& child_node = room_node->mChildren[child_index];
            child_node->mName = instance_id;

            // create transformation
            const auto& scale = raw_child["scale"];
            const auto& rotation = raw_child["rot"];
            const auto& position = raw_child["pos"];
            child_node->mTransformation = aiMatrix4x4(
                aiVector3D(scale[0], scale[1], scale[2]),
                aiQuaternion(rotation[0], rotation[1], rotation[2], rotation[3]),
                aiVector3D(position[0], position[1], position[2])
            );

            child_node->mParent = room_node;

            if (instance_id.find("furniture") != std::string::npos) {
                // TODO: remove this to include furniture
                continue;
            }
            auto& mesh_indices = model_uid_to_mesh_indices_map[raw_child["ref"]];
            child_node->mNumMeshes = mesh_indices.size();
            child_node->mMeshes = new unsigned int[child_node->mNumMeshes];
            std::copy(mesh_indices.begin(), mesh_indices.end(), child_node->mMeshes);
        }
    }

    // TODO: load furniture vertices
    // TODO: load room materials
    // TODO: load furniture materials
}
