#pragma once

#include <vector>
#include <array>
#include <string>
#include <fstream>
#include <iostream>

class MatrixParser {
public:
    MatrixParser(const std::string& file);
    ~MatrixParser();

    std::vector<std::array<float, 16>> matrices;

    bool invert_matrix(const float m[16], float invOut[16]);
private:
};