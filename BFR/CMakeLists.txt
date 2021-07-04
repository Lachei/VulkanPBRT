file(GLOB BFR_SRC
    "*.h"
    "*.cpp"
	"*.hpp"
	"*.cc"
    PARENT_SCOPE
)

file(GLOB BFR_SHADER
    "*.comp"
    PARENT_SCOPE
)

message("${BFR_SHADER}")