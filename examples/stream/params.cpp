#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <sstream>
#include <iomanip>

class Params {
private:
    struct Param {
        std::string description;
        std::string default_value;
        std::function<void(const std::string&)> parser;
        std::function<std::string()> getter;
    };

    std::map<std::string, Param> params;
    std::map<std::string, Param*> param_lookup;

public:
    float context_duration_s = 100.0f;
    float archive_interval_s = 20.0f;
    float recognition_interval_s = 0.4;

    bool save_audio = false;
    bool save_sync = false;
    bool use_gpu = false;
    bool flash_attn = false;

    std::string model = "models/ggml-base.en.bin";
    // std::string model = "models/ggml-medium.bin";
    // std::string model = "models/models/ggml-base.en-q4_0.bin";
    // std::string model = "models/models/ggml-base.en-q2_k.bin";
    std::string translate = "";

    Params() {
        addParam("-ri", "--recognition-interval", "Interval between recognitions in seconds",
                 [this](const std::string& val) { recognition_interval_s = std::stof(val); },
                 [this]() { return std::to_string(recognition_interval_s); });

        addParam("-ai", "--archive-interval", "Duration of the audio archive in seconds",
                 [this](const std::string& val) { archive_interval_s = std::stof(val); },
                 [this]() { return std::to_string(archive_interval_s); });

        addParam("--save-audio", "", "Save audio to file",
                 [this](const std::string&) { save_audio = true; },
                 [this]() { return save_audio ? "true" : "false"; });

        addParam("--save-sync", "", "Save audio and subtitles in sync",
                 [this](const std::string&) { save_sync = true; },
                 [this]() { return save_sync ? "true" : "false"; });

        addParam("--use-gpu", "", "Use GPU for computation",
                 [this](const std::string&) { use_gpu = true; },
                 [this]() { return use_gpu ? "true" : "false"; });

        addParam("--flash-attn", "", "Use flash attention",
                 [this](const std::string&) { flash_attn = true; },
                 [this]() { return flash_attn ? "true" : "false"; });

        addParam("-m", "--model", "Path to the model file",
                 [this](const std::string& val) { model = val; },
                 [this]() { return model; });

        addParam("-tr", "--translate", "Translate to the target language",
                 [this](const std::string& val) { translate = val; },
                 [this]() { return translate; });
    }

    void addParam(const std::string& short_name, const std::string& long_name,
                  const std::string& description,
                  std::function<void(const std::string&)> parser,
                  std::function<std::string()> getter) {
        std::ostringstream key_list;
        if (!short_name.empty()) key_list << short_name;
        if (!short_name.empty() && !long_name.empty()) key_list << " | ";
        if (!long_name.empty()) key_list << long_name;

        params[key_list.str()] = {description, getter(), parser, getter};

        if (!short_name.empty()) {
            param_lookup[short_name] = &params[key_list.str()];
        }
        if (!long_name.empty()) {
            param_lookup[long_name] = &params[key_list.str()];
        }
    }

    bool parse(int argc, char** argv) {
        try {
            for (int i = 1; i < argc; ++i) {
                std::string arg = argv[i];
                if (arg == "-h" || arg == "--help") {
                    printUsage(argv[0]);
                    exit(0);
                }
                if (param_lookup.count(arg)) {
                    const auto* param = param_lookup[arg];
                    if (param->parser) {
                        if (i + 1 < argc && argv[i + 1][0] != '-') {
                            param->parser(argv[++i]);
                        } else {
                            param->parser("");
                        }
                    }
                } else {
                    std::cerr << "Unknown argument: " << arg << "\n";
                    printUsage(argv[0]);
                    return false;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing argument: " << e.what() << "\n";
            return false;
        }
        return true;
    }

    void printUsage(const char* program_name) const {
        std::cerr << "\nUsage: " << program_name << " [options]\n\nOptions:\n";

        const int col1_width = 30;
        const int col2_width = 30;

        std::vector<std::pair<std::string, Param>> int_params;
        std::vector<std::pair<std::string, Param>> bool_params;
        std::vector<std::pair<std::string, Param>> string_params;

        for (const auto& [key, param] : params) {
            if (key.find("-ad") != std::string::npos || key.find("-cd") != std::string::npos || key.find("-ri") != std::string::npos) {
                int_params.push_back({key, param});
            } else if (key.find("--save") != std::string::npos || key.find("--use-gpu") != std::string::npos || key.find("--flash-attn") != std::string::npos) {
                bool_params.push_back({key, param});
            } else {
                string_params.push_back({key, param});
            }
        }

        std::cerr << "Integer Parameters:\n";
        for (const auto& [key, param] : int_params) {
            std::ostringstream line;
            line << "  " << std::left << std::setw(col1_width) << key
                << std::left << std::setw(col2_width) << ("[" + param.default_value + "]")
                << param.description;
            std::cerr << line.str() << "\n";
        }

        std::cerr << "Boolean Parameters:\n";
        for (const auto& [key, param] : bool_params) {
            std::ostringstream line;
            line << "  " << std::left << std::setw(col1_width) << key
                << std::left << std::setw(col2_width) << ("[" + param.default_value + "]")
                << param.description;
            std::cerr << line.str() << "\n";
        }

        std::cerr << "String Parameters:\n";
        for (const auto& [key, param] : string_params) {
            std::ostringstream line;
            line << "  " << std::left << std::setw(col1_width) << key
                << std::left << std::setw(col2_width) << ("[" + param.default_value + "]")
                << param.description;
            std::cerr << line.str() << "\n";
        }

        std::cerr << "\n";
    }
};
