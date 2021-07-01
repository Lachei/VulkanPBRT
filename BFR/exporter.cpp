#include "exporter.h"

void Exporter::export_csv_diff(std::vector<float>& a, std::vector<float>& b, const std::string& filepath, int image_width)
{
    std::ofstream file(filepath);
    file << "x,y,diffr,diffg,diffb,diff\n";
    for (int i = 0; i < a.size() / 4; ++i) {
        float diff = 0;
        int x = i % image_width;
        int y = i / image_width;
        file << x << "," << y << ",";
        for (int j = 0; j < 3; ++j) {
            float d = a[4 * i + j] - b[4 * i + j];
            diff += d;
            file << d << ",";
        }
        file << diff << "\n";
    }
}
