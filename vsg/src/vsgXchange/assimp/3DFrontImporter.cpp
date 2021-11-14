#include "3DFrontImporter.h"

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
    // TODO
}
