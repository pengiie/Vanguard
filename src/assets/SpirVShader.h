#pragma once

#include <shaderc/shaderc.hpp>
#include "File.h"
#include "Asset.h"
#include "../Logger.h"
#include "../util/Timer.h"

namespace vanguard {
    typedef std::vector<uint32_t> SpirVShaderCode;

    static Asset loadSpirVShader(const File& file) {
        FTIMER();

        shaderc_shader_kind stage;
        auto delim = file.name().find_last_of('.');
        std::string shaderName = file.name().substr(0, delim);
        std::string stageExtension = file.name().substr(delim + 1);
        if(stageExtension == "vert") {
            stage = shaderc_shader_kind ::shaderc_glsl_vertex_shader;
        } else if(stageExtension == "frag") {
            stage = shaderc_shader_kind ::shaderc_glsl_fragment_shader;
        } else if(stageExtension == "comp") {
            stage = shaderc_shader_kind ::shaderc_glsl_compute_shader;
        } else {
            ERROR("Unknown or unsupported shader stage: {}", stageExtension);
            return Asset(SpirVShaderCode{});
        }

        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        options.SetSourceLanguage(shaderc_source_language_glsl);

        auto source = file.load(false);

        shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source.data(), source.size(), stage, shaderName.c_str(), options);
        if(module.GetCompilationStatus() != shaderc_compilation_status_success) {
            ERROR("Failed to compile shader: \n{}", module.GetErrorMessage());
            ERROR("Shader source: \n{}", std::string(source.begin(), source.end()));
            return Asset(SpirVShaderCode{});
        }

        auto code = SpirVShaderCode(module.cbegin(), module.cend());

        return Asset(code);
    }
}