#include "application.h"
#include "tiny_obj_loader.h"
#include "vk_utils.h"
#include "bmfr.h"
#include "gpu_images.h"
#include "matrix_parser.h"
#include "comp.h"
#include "exporter.h"
#include <OpenImageIO/imageio.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define INPUT_DATA_PATH E:/Master Seminar/san-miguel/inputs
#define INPUT_REFERENCE "E:/Master Seminar/living-room/Reference/Living room-Reference-"
#define BMFR_FILE_NAME "E:/Master Seminar/sponza/BMFR/Living room-BMFR-"
#define INPUT_DATA_PATH_STR STR(INPUT_DATA_PATH)
#define NOISY_FILE_NAME INPUT_DATA_PATH_STR"/color"
#define NORMAL_FILE_NAME INPUT_DATA_PATH_STR"/shading_normal"
#define POSITION_FILE_NAME INPUT_DATA_PATH_STR"/world_position"
#define ALBEDO_FILE_NAME INPUT_DATA_PATH_STR"/albedo"
#define DEPTH_FILE_NAME INPUT_DATA_PATH_STR"/depth"
#define MATRIX_FILE_NAME INPUT_DATA_PATH_STR"/matrices.txt"
#define REFERENCE_FILE_NAME INPUT_DATA_PATH_STR"/reference"
#define OUTPUT_FILE_NAME INPUT_DATA_PATH_STR"/L1/mix_40steps_"

#define IMAGE_WIDTH 1280
#define IMAGE_HEIGHT 720

#define FRAME_COUNT 60

#define BLOCK_EDGE_LENGTH 16
#define WORKSET_WIDTH (BLOCK_EDGE_LENGTH * \
   ((IMAGE_WIDTH + BLOCK_EDGE_LENGTH - 1) / BLOCK_EDGE_LENGTH))
#define WORKSET_HEIGHT (BLOCK_EDGE_LENGTH * \
   ((IMAGE_HEIGHT + BLOCK_EDGE_LENGTH - 1) / BLOCK_EDGE_LENGTH))
#define OUTPUT_SIZE (WORKSET_WIDTH * WORKSET_HEIGHT)

bool read_image_file(const std::string& file_name, const int frame, float* buffer)
{
    OpenImageIO_v2_1::ImageInput::unique_ptr in = OpenImageIO_v2_1::ImageInput::open(file_name + std::to_string(frame) + ".exr");
    if (!in || in->spec().width != IMAGE_WIDTH ||
        in->spec().height != IMAGE_HEIGHT || in->spec().nchannels != 3)
    {

        return false;
    }

    // NOTE: this converts .exr files that might be in halfs to single precision floats
    // In the dataset distributed with the BMFR paper all exr files are in single precision
    in->read_image(OpenImageIO_v2_1::TypeDesc::FLOAT, buffer);
    in->close();

    return true;
}

bool read_png_file(const std::string& file_name, const int frame, float* buffer)
{
    OpenImageIO_v2_1::ImageInput::unique_ptr in = OpenImageIO_v2_1::ImageInput::open(file_name + std::to_string(frame) + ".png");
    if (!in || in->spec().width != IMAGE_WIDTH ||
        in->spec().height != IMAGE_HEIGHT || in->spec().nchannels != 3)
    {

        return false;
    }

    // NOTE: this converts .exr files that might be in halfs to single precision floats
    // In the dataset distributed with the BMFR paper all exr files are in single precision
    in->read_image(OpenImageIO_v2_1::TypeDesc::FLOAT, buffer);
    in->close();

    return true;
}


// converts a float3 vector to a float4 vector (3 component vector to 4 component vector)
void f3_to_f4(std::vector<float>& v) {
    std::vector<float> tmp(v);
    v.resize(v.size() / 3 * 4, 0);
    for (int i = 0; i < tmp.size() / 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            v[i * 4 + j] = tmp[i * 3 + j];
        }
    }
}

// converts a float3 vector to a float4 vector (3 component vector to 1 component vector)
void f3_to_f1(std::vector<float>& v) {
    std::vector<float> tmp(v);
    assert(v.size() % 3 == 0);  //safety check if it is indeed a 3 component vector (or at least can be interpreted as one)
    v.resize(v.size() / 3);
    for (int i = 0; i < v.size(); ++i) {
        v[i] = tmp[3 * i];
    }
}

//converts a float4 normal vector to a float2 normal vector (normal conversion from cartesian to spherical coordinate system)
//Note: NOT ANYMORE, instead just drops the third component
void cart_to_spher(std::vector<float>& v) {
    std::vector<float> tmp(v);
    assert(v.size() % 4 == 0);  //safety check if it is indeed a 3 component vector (or at least can be interpreted as one)
    v.resize(v.size() / 2);
    for (int i = 0; i < v.size() / 2; ++i) {
        v[2 * i] = tmp[i * 4];
        v[2 * i + 1] = tmp[i * 4 + 1];
    }
    //float rho, theta;
    //for (int i = 0; i < tmp.size() / 4; ++i) {
    //    rho = std::atan2(tmp[i * 4 + 1], tmp[i * 4]);
    //    theta = std::acos(tmp[i * 4 + 2]);
    //    if (rho != rho)
    //        rho = 0;
    //    v[i * 2] = rho;
    //    v[i * 2 + 1] = theta;
    //}
}

void f4_to_f3(std::vector<float>& v) {
    std::vector<float> tmp = v;
    v.resize(v.size() / 4 * 3, 0);
    for (int i = 0; i < tmp.size() / 4; ++i) {
        for (int j = 0; j < 3; ++j) {
            v[i * 3 + j] = tmp[i * 4 + j];
        }
    }
}

void pos_to_depth(std::vector<float>& pos, std::vector<float>& depth, std::array<float,16>& mat) {
    float* p;
    float dep[4];
    float minmax[4]{10,10,-10,-10};
    std::vector<float> t(pos);
    pos.resize(IMAGE_HEIGHT * IMAGE_WIDTH);
    for (int y = 0; y < IMAGE_HEIGHT; ++y) {
        for (int x = 0; x < IMAGE_WIDTH; ++x) {
            p = t.data() + (y * IMAGE_WIDTH + x) * 4;
            for (int i = 0; i < 4; ++i) {
                dep[i] = p[0] * mat[i] + p[1] * mat[i + 4] + p[2] * mat[i + 8] + mat[i + 12];
            }
            for (int i = 0; i < 3; ++i) {
                dep[i] /= dep[3];
            }
            depth[y * IMAGE_WIDTH + x] = dep[2] * .5f + .5f;
            //minmax[0] = std::min(minmax[0], dep[0]);
            //minmax[1] = std::min(minmax[1], dep[1]);
            //minmax[2] = std::max(minmax[2], dep[0]);
            //minmax[3] = std::max(minmax[3], dep[1]);
        }
    }

}

void check_depth(std::vector<float>& pos, std::vector<float>& depth, MatrixParser& mat, int index) {
    float inv[16];
    mat.invert_matrix(mat.matrices[index].data(), inv);
    float xyz[4];
    float res[4];
    float diff;
    for (int y = 0; y < IMAGE_HEIGHT; ++y) {
        for (int x = 0; x < IMAGE_WIDTH; ++x) {
            xyz[0] = float(x) / IMAGE_WIDTH * 2 - 1;
            xyz[1] = float(y) / IMAGE_HEIGHT * 2 - 1;
            xyz[2] = depth[y * IMAGE_WIDTH + x] * 2 - 1;
            for (int i = 0; i < 4; ++i) {
                res[i] = xyz[0] * inv[i] + xyz[1] * inv[i + 4] + xyz[2] * inv[i + 8] + inv[i + 12];
            }
            for (int i = 0; i < 3; ++i) {
                res[i] /= res[3];
            }
            xyz[0] = pos[(y * IMAGE_WIDTH + x) * 4 + 0] - res[0];
            xyz[1] = pos[(y * IMAGE_WIDTH + x) * 4 + 1] - res[1];
            xyz[2] = pos[(y * IMAGE_WIDTH + x) * 4 + 2] - res[2];
            diff = sqrt(xyz[0] * xyz[0] + xyz[1] * xyz[1] + xyz[2] * xyz[2]);
        }
    }
}

bool open_images(std::vector<float> out_data[], std::vector<float> albedos[], std::vector<float> normals[], std::vector<float> depths[], std::vector<float> noisy_input[]) {
    bool error = false;
#pragma omp parallel for
    for (int frame = 0; frame < FRAME_COUNT; ++frame) {
        std::cout << "Reading frame " << frame << std::endl << std::flush;
        if (error)
            continue;

        out_data[frame].resize(3 * IMAGE_WIDTH * IMAGE_HEIGHT);

        albedos[frame].resize(3 * IMAGE_WIDTH * IMAGE_HEIGHT);
        bool result = read_image_file(ALBEDO_FILE_NAME, frame, albedos[frame].data());
        f3_to_f4(albedos[frame]);

        if (!result)
        {
            error = true;
            printf("Albedo buffer loading failed\n");
            continue;
        }

        normals[frame].resize(3 * IMAGE_WIDTH * IMAGE_HEIGHT);
        result = read_image_file(NORMAL_FILE_NAME, frame, normals[frame].data());
        f3_to_f4(normals[frame]);
        if (!result)
        {
            error = true;
            printf("Normal buffer loading failed\n");
            continue;
        }

        depths[frame].resize(3 * IMAGE_WIDTH * IMAGE_HEIGHT);
        result = read_image_file(POSITION_FILE_NAME, frame, depths[frame].data());
        f3_to_f4(depths[frame]);
        if (!result)
        {
            error = true;
            printf("Position buffer loading failed\n");
            continue;
        }

        noisy_input[frame].resize(3 * IMAGE_WIDTH * IMAGE_HEIGHT);
        result = read_image_file(NOISY_FILE_NAME, frame, noisy_input[frame].data());
        f3_to_f4(noisy_input[frame]);
        if (!result)
        {
            error = true;
            printf("Noisy buffer loading failed\n");
            continue;
        }
    }
    return error;
}

bool open_reference(std::vector<float> reference[]) {
    bool error = false;
#pragma omp parallel for
    for (int frame = 0; frame < FRAME_COUNT; ++frame) {
        std::cout << "Reading reference frame "<< REFERENCE_FILE_NAME << frame << std::endl << std::flush;
        if (error)
            continue;

        reference[frame].resize(3 * IMAGE_WIDTH * IMAGE_HEIGHT);

        reference[frame].resize(3 * IMAGE_WIDTH * IMAGE_HEIGHT);
        std::vector<float>  comp(reference[frame]);
        //bool result = read_image_file(REFERENCE_FILE_NAME, frame, reference[frame].data());
        bool result = read_png_file(INPUT_REFERENCE + ((frame < 9) ? std::string("00"):std::string("0")), frame + 1, reference[frame].data());
        //result = read_png_file(BMFR_FILE_NAME + ((frame < 9) ? std::string("00"):std::string("0")), frame + 1, comp.data());
        f3_to_f4(reference[frame]);
        //f3_to_f4(comp);
        //std::cout << ImageComparison::SSIM(reference[frame], comp) << std::endl;

        if (!result)
        {
            error = true;
            printf("Reference buffer loading failed\n");
            continue;
        }
    }
    return error;
}

bool store_images(std::vector<float> noise[], std::vector<float> data[], std::vector<float> taa[]) {
    bool error = false;
#pragma omp parallel for
    for (int frame = 0; frame < FRAME_COUNT; ++frame)
    {
        std::cout << "Storing frame " << frame << std::endl << std::flush;

        if (error)
            continue;

        // Output image
        std::string output_file_name = OUTPUT_FILE_NAME + std::to_string(frame) + ".png";
        f4_to_f3(taa[frame]);
        // Crops back from WORKSET_SIZE to IMAGE_SIZE
        OpenImageIO_v2_1::ImageSpec  spec = OpenImageIO_v2_1::ImageSpec(IMAGE_WIDTH, IMAGE_HEIGHT, 3,
            OpenImageIO_v2_1::TypeDesc::FLOAT);
        std::unique_ptr<OpenImageIO_v2_1::ImageOutput> out = std::unique_ptr<OpenImageIO_v2_1::ImageOutput>(OpenImageIO_v2_1::ImageOutput::create(output_file_name));
        if (out && out->open(output_file_name, spec))
        {
            out->write_image(OpenImageIO_v2_1::TypeDesc::FLOAT, taa[frame].data(),
                3 * sizeof(float), WORKSET_WIDTH * 3 * sizeof(float), 0);
            out->close();
        }
        else
        {
            printf("Can't create image file on disk to location %s\n",
                output_file_name.c_str());
            error = true;
            continue;
        }
    }

    return error;
}

void store_depth(std::vector<float>& depth, std::string path) {
    // Crops back from WORKSET_SIZE to IMAGE_SIZE
    OpenImageIO_v2_1::ImageSpec spec(IMAGE_WIDTH, IMAGE_HEIGHT, 1,
        OpenImageIO_v2_1::TypeDesc::FLOAT);
    std::unique_ptr<OpenImageIO_v2_1::ImageOutput>
        out(OpenImageIO_v2_1::ImageOutput::create(path));
    if (out && out->open(path, spec))
    {
        out->write_image(OpenImageIO_v2_1::TypeDesc::FLOAT, depth.data(),
            sizeof(float), IMAGE_WIDTH * sizeof(float), 0);
        out->close();
    }
    else
    {
        printf("Can't create image file on disk to location %s\n",
            path.c_str());
    }
}

int main(int argc, char** argv)
{
    /* opening all images which should be denoised*/
    std::vector<float> out_data[FRAME_COUNT];
    std::vector<float> albedos[FRAME_COUNT];
    std::vector<float> normals[FRAME_COUNT];
    //std::vector<float> positions[FRAME_COUNT];
    std::vector<float> depths[FRAME_COUNT];
    std::vector<float> noisy_input[FRAME_COUNT];
    //std::vector<float> noise_acc[FRAME_COUNT];
    //std::vector<float> data_acc[FRAME_COUNT];
    std::vector<float> taa_acc[FRAME_COUNT];
    //std::vector<float> references[FRAME_COUNT];
    for (int i = 0; i < FRAME_COUNT; ++i) {
        //noise_acc[i].resize(IMAGE_WIDTH * IMAGE_HEIGHT * 4);
        //data_acc[i].resize(IMAGE_WIDTH * IMAGE_HEIGHT * 4);
        taa_acc[i].resize(IMAGE_WIDTH * IMAGE_HEIGHT * 4);
        depths[i].resize(IMAGE_WIDTH * IMAGE_HEIGHT);
    }
    MatrixParser mat(MATRIX_FILE_NAME);
    bool error = open_images(out_data, albedos, normals, depths, noisy_input);
    //error |= open_reference(references);
    for (int i = 0; i < FRAME_COUNT; ++i) {
        cart_to_spher(normals[i]);
        pos_to_depth(depths[i], depths[i], mat.matrices[i]);
    }
    //float max = 0;
    //float secondmax = 0;
    //for (int i = 0; i < albedos[0].size(); ++i) {
    //    if (albedos[0][i] > max) { secondmax = max; max = albedos[0][i]; }
    //}
    
    if (error)
    {
        printf("One or more errors occurred during buffer loading\n");
        return 1;
    }

    VkUtil::Vk_Context compute_ctx{VK_QUEUE_COMPUTE_BIT};
    std::vector<const char*> extensions{};
    VkUtil::create_vk_context(extensions, compute_ctx);
    {
        GpuDoubleImage normal(compute_ctx, IMAGE_WIDTH, IMAGE_HEIGHT, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0);
        GpuDoubleImage depth(compute_ctx, IMAGE_WIDTH, IMAGE_HEIGHT, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0);
        GpuDoubleImage position(compute_ctx, IMAGE_WIDTH, IMAGE_HEIGHT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0);
        GpuDoubleImage noisy(compute_ctx, IMAGE_WIDTH, IMAGE_HEIGHT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0);
        GpuDoubleImage albedo(compute_ctx, IMAGE_WIDTH, IMAGE_HEIGHT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0);
        //albedos[0].resize(IMAGE_WIDTH * IMAGE_HEIGHT * 4, 0);
        //testImage.set_cur_image(albedos[0].data(), albedos[0].size() * sizeof(float));
        //albedos[0].resize(0);
        //albedos[0].resize(IMAGE_WIDTH * IMAGE_HEIGHT * 4, 10);
        //testImage.get_cur_image(albedos[0].data(), albedos[0].size() * sizeof(float));
        Denoiser denoiser(compute_ctx, IMAGE_WIDTH, IMAGE_HEIGHT);
        float zeros[16]{};
        VkImage image;
        VkImageView imageView;
        for (int frame = 0; frame < FRAME_COUNT; ++frame) {
            denoiser.set_frame_number(frame);
            (frame) ? denoiser.set_prev_camera_matrix(mat.matrices[frame - 1].data()) : denoiser.set_prev_camera_matrix(zeros);
            mat.invert_matrix(mat.matrices[frame].data(), zeros);
            denoiser.set_camera_matrix(zeros);
            normal.set_cur_image(normals[frame].data(), normals[frame].size() * sizeof(float));
            depth.set_cur_image(depths[frame].data(), depths[frame].size() * sizeof(float));
            noisy.set_cur_image(noisy_input[frame].data(), noisy_input[frame].size() * sizeof(float));
            albedo.set_cur_image(albedos[frame].data(), albedos[frame].size() * sizeof(float));

            normal.get_cur_image(image, imageView);
            denoiser.set_normals_image(image, imageView);
            normal.get_prev_image(image, imageView);
            denoiser.set_previous_normals_image(image, imageView);

            depth.get_cur_image(image, imageView);
            denoiser.set_depth_image(image, imageView);
            depth.get_prev_image(image, imageView);
            denoiser.set_previous_depth_image(image, imageView);

            albedo.get_cur_image(image, imageView);
            denoiser.set_albedo_image(image, imageView);

            noisy.get_cur_image(image, imageView);
            denoiser.set_noisy_image(image, imageView);

            denoiser.denoise();

            //denoiser.get_noise_acc(taa_acc[frame].data(), taa_acc[frame].size() * sizeof(float));
            //denoiser.get_data_acc(taa_acc[frame].data(), taa_acc[frame].size() * sizeof(float));
            denoiser.get_denoised(taa_acc[frame].data(), taa_acc[frame].size() * sizeof(float));

            normal.swap_images();
            position.swap_images();
            depth.swap_images();
            noisy.swap_images();
            albedo.swap_images();
        }

        // calculating numerical errors in the output images
        //for (int i = 0; i < FRAME_COUNT; ++i) {
        //    std::cout << "Image " << i << " RMSE: " << ImageComparison::RMSE(taa_acc[i], references[i]) << ", SSIM: " << ImageComparison::SSIM(taa_acc[i], references[i]) << std::endl;
        //}

        //for (int i = 0; i < FRAME_COUNT; ++i) {
        //    Exporter::export_csv_diff(taa_acc[i], references[i], "error" + std::to_string(i) + ".csv", IMAGE_WIDTH);
        //}
        
        error = store_images(taa_acc, taa_acc, taa_acc);

        if (error)
        {
            printf("One or more errors occurred during image saving\n");
            return 1;
        }
    }
    VkUtil::destroy_vk_context(compute_ctx);

    return 0;

    Application app;
    app.Run();

    return 0;
}