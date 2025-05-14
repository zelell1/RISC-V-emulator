#include <iostream>
#include <string>
#include <cstring>


namespace ERRORS {
    const std::string kErrorOrder = "Неправильное количество аргументов\n";
    const uint8_t kCountArgs_1 = 3;
    const uint8_t kCountArgs_2 = 7;
}

struct Data {
    std::string filename1 = "";
    std::string filename2 = "";
    uint32_t begin_addres = 0;
    uint32_t size = 0;
    bool error = false;
    std::string error_name = "";
};

class Parser {
public:
    Data Parse(int argc, char* argv[]) {
        Data data;
        if (argc != ERRORS::kCountArgs_1 && argc != ERRORS::kCountArgs_2) {
            data.error = 1;
            data.error_name = ERRORS::kErrorOrder;
        }
        for (int i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "-i") == 0) {
                data.filename1 = argv[++i];
            } else if (strcmp(argv[i], "-o") == 0) {
                data.filename2 = argv[++i];
                data.begin_addres = std::stoul(argv[++i], 0, 16);
                data.size = std::stoul(argv[++i]);
            }
        }
        return data;
    }
};