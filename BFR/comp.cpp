#include "comp.h"

float ImageComparison::RMSE(std::vector<float> image, std::vector<float> reference)
{
    float rmse = 0;
    int n = 0;
    for (int i = 0; i < image.size() / 4; ++i) {
        for (int j = 0; j < 3; ++j) {
            rmse += square(image[i * 4 + j] - reference[i * 4 + j]);
            if (isnan(rmse))
                bool wtf = true;
            ++n;
        }
    }
    return sqrt(rmse / n);
}

float ImageComparison::SSIM(std::vector<float> image, std::vector<float> reference)
{
    assert(image.size() == reference.size());
    assert(image.size() % 4 == 0);

    double mean_x = 0, mean_y = 0;
    double variance_x = 0, variance_y = 0;
    double covariance = 0;
    for (int i = 0; i < image.size() / 4; ++i) {
        mean_x += rgb_to_grey(&image[4 * i]);
        mean_y += rgb_to_grey(&reference[4 * i]);
    }
    mean_x /= image.size() / 4; mean_y /= image.size() / 4;
    for (int i = 0; i < image.size() / 4; ++i) {
        variance_x += square(rgb_to_grey(&image[4 * i]) - mean_x);
        variance_y += square(rgb_to_grey(&reference[4 * i]) - mean_y);
        covariance += (rgb_to_grey(&image[4 * i]) - mean_x) * (rgb_to_grey(&reference[4 * i]) - mean_y);
    }
    variance_x /= image.size() / 4; variance_y /= image.size() / 4; covariance /= image.size() / 4;
    double c1 = square(.001), c2 = square(.003);

    return ((2 * mean_x * mean_y + c1) * (2 * covariance + c2))/((square(mean_x) + square(mean_y) + c1) * (variance_x + variance_y + c2));
}

inline float ImageComparison::square(float x)
{
    return x * x;
}

inline float ImageComparison::rgb_to_grey(float rgb[3])
{
    return 0.2989 * rgb[0]+  0.5870 * rgb[1] + 0.1140 * rgb[2];
}
