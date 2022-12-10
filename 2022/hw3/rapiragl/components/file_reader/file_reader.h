#pragma once

#include <string>
#include "../../utils/strong_typedef.h"
#include <rapiragl/common/types.h>

namespace rapiragl::components {
    class FileReader {
    public:
        static std::string read(const FilePath &file_path);
    };
}