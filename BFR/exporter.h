#pragma once
#include <vector>
#include <string>
#include <fstream>

class Exporter {
public:
    static void export_csv_diff(std::vector<float>& a, std::vector<float>& b, const std::string& filepath, int image_width);
private:
};