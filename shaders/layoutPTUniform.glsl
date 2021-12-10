#ifndef LAYOUTPTUNIFORM_H
#define LAYOUTPTUNIFORM_H

layout(binding = 26) uniform Infos{
  uint lightCount;
  float lightStrengthSum;
  uint minRecursionDepth;
  uint maxRecursionDepth;
}infos;

#endif // LAYOUTPTUNIFORM_H