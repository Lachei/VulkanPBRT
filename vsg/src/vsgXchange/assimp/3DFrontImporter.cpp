#include "3DFrontImporter.h"

#include <fstream>
#include <nlohmann/json.hpp>

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
    nlohmann::json scene_json;
    scene_file >> scene_json;
    // TODO
}
