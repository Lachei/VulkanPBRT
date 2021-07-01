#pragma once
#include <vector>
#include <cassert>

class ImageComparison {
public:
    // calculates the root mean squared error between an image and its reference. 
    // Both image and reference are supposed to be in a 4 component vector where the error is only calculated betweeen the first 3 components.
    static float RMSE(std::vector<float> image, std::vector<float> reference);

    // calculates the structural similarity between an image and its reference
    // both image and reference are supposed to be in a 4 component vector where the error is only calculated betweeen the first 3 components.
    static float SSIM(std::vector<float> image, std::vector<float> reference);
private:
    static inline float square(float x);
    static inline float rgb_to_grey(float rgb[3]);
};
