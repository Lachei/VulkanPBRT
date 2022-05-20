#include "Texture.h"
void vkpbrt::TextureResource::load_internal()
{
    // std::array<aiTextureMapMode, 3> wrapMode{ {aiTextureMapMode_Wrap, aiTextureMapMode_Wrap, aiTextureMapMode_Wrap}
    // };
    //{

    //    if (texPath.data[0] == '*')
    //    {
    //        const auto texIndex = std::atoi(texPath.C_Str() + 1);
    //        const auto texture = scene->mTextures[texIndex];

    //        //qCDebug(lc) << "Handle embedded texture" << texPath.C_Str() << texIndex << texture->achFormatHint <<
    //        texture->mWidth << texture->mHeight;

    //        if (texture->mWidth > 0 && texture->mHeight == 0)
    //        {
    //            auto imageOptions = vsg::Options::create(*options);
    //            imageOptions->extensionHint = texture->achFormatHint;
    //            if (samplerImage.data = vsg::read_cast<vsg::Data>(reinterpret_cast<const uint8_t*>(texture->pcData),
    //            texture->mWidth, imageOptions); !samplerImage.data.valid())
    //                return {};
    //        }
    //    }
    //    else
    //    {
    //        std::vector<vsg::Path> search_paths;
    //        const std::string filename = vsg::findFile(vsg::Path(_resource_file_path.string()), search_paths);

    //        if (_texture_data = vsg::read_cast<vsg::Data>(filename); !_texture_data.valid())
    //        {
    //            std::cerr << "Failed to load texture: " << filename << " texPath = " << _resource_file_path.c_str() <<
    //            std::endl; return;
    //        }
    //    }

    //    samplerImage.sampler = vsg::Sampler::create();

    //    samplerImage.sampler->addressModeU = getWrapMode(wrapMode[0]);
    //    samplerImage.sampler->addressModeV = getWrapMode(wrapMode[1]);
    //    samplerImage.sampler->addressModeW = getWrapMode(wrapMode[2]);

    //    samplerImage.sampler->anisotropyEnable = VK_TRUE;
    //    samplerImage.sampler->maxAnisotropy = 16.0f;

    //    samplerImage.sampler->maxLod = samplerImage.data->getLayout().maxNumMipmaps;

    //    if (samplerImage.sampler->maxLod <= 1.0)
    //    {
    //        //                if (texPath.length > 0)
    //        //                    std::cout << "Auto generating mipmaps for texture: " <<
    //        scene.GetShortFilename(texPath.C_Str()) << std::endl;;

    //        // Calculate maximum lod level
    //        auto maxDim = std::max(samplerImage.data->width(), samplerImage.data->height());
    //        samplerImage.sampler->maxLod = std::floor(std::log2f(static_cast<float>(maxDim)));
    //    }

    //    return samplerImage;
    //}

    // return {};
}
