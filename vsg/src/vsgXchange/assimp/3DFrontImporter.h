#pragma once
#include <assimp/BaseImporter.h>
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
    bool CanRead(const std::string& pFile, Assimp::IOSystem* pIOHandler, bool checkSig) const override;
    const aiImporterDesc* GetInfo() const override;
protected:
    void InternReadFile(const std::string& pFile, aiScene* pScene, Assimp::IOSystem* pIOHandler) override;
};