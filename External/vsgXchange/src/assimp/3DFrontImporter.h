#pragma once
#include <assimp/BaseImporter.h>

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>
#include <filesystem>

#define AI_MATKEY_CATEGORY_ID "$mat.categoryid",0,0

/*
 * Importer for scenes from the 3D-FRONT data set.
 * https://tianchi.aliyun.com/specials/promotion/alibaba-3d-scene-dataset
 *
 * The directory containing the scene description json-file must also contain the
 * 3D-FRONT-texture and 3D-FRONT-model directories.
 */
class AI3DFrontImporter : public Assimp::BaseImporter
{
public:
    static void ReadConfig(const nlohmann::json config_json);
    
    bool CanRead(const std::string& pFile, Assimp::IOSystem* pIOHandler, bool checkSig) const override;
    const aiImporterDesc* GetInfo() const override;
protected:
    void InternReadFile(const std::string& pFile, aiScene* pScene, Assimp::IOSystem* pIOHandler) override;
private:
    void FindDataDirectories(const std::string& file_path, std::vector<std::filesystem::path>& texture_directories,
                             std::vector<std::filesystem::path>& furniture_directories);
    std::unordered_map<std::string, std::string> LoadJidToCategoryMap(const std::vector<std::filesystem::path>& furniture_directories);
    void LoadMaterials(const std::vector<std::filesystem::path>& texture_directories, const nlohmann::json& scene_json, aiScene* pScene,
                       std::unordered_map<std::string, uint32_t>& material_id_to_index_map, std::vector<float>& material_uv_rotations);
    void LoadMeshes(const nlohmann::json& scene_json, const std::unordered_map<std::string, uint32_t>& material_id_to_index_map,
                    const std::vector<float>& material_uv_rotations, aiScene* pScene,
                    std::unordered_map<std::string, std::vector<uint32_t>>& model_uid_to_mesh_indices_map);

    static float ceiling_light_strength;
    static float lamp_light_strength;
    static std::unordered_map<std::string, uint32_t> category_to_id_map;
};
