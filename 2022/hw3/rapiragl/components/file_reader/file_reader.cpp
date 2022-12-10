#include <fstream>
#include <sstream>

#include "file_reader.h"

namespace rapiragl::components {
    std::string components::FileReader::read(const FilePath &file_path) {
        std::ifstream in(file_path.GetUnderLying().data(), std::ios::in | std::ios::binary);
        std::ostringstream sstr{};
        sstr << in.rdbuf();
        return sstr.str();
    }
}
