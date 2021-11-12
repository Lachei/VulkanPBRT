#ifndef LAYOUTRTUNIFORM_H
#define LAYOUTRTUNIFORM_H

layout(binding = 26) uniform Infos{
  uint lightCount;
  uint minRecursionDepth;
  uint maxRecursionDepth;
}infos;

#endif // LAYOUTRTUNIFORM_H