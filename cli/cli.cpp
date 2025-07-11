// (c) 2025, UltiMaker -- see LICENCE for details

#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/SceneCombiner.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cstdio>
#include <cxxopts.hpp>
#include <iostream>

#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

// #include "unwrap.hpp"
#include "lscm_unwrap.h"

int main(int argc, char** argv)
{
    cxxopts::Options options("Uvula", "Test interface for the libuvula library");
    options.add_options()("filepath", "Path of the 3D mesh file to be loaded (OBJ, STL, ...)", cxxopts::value<std::string>())(
        "o,outputfile",
        "Path of the output 3D mesh with UV coordinates (OBJ)",
        cxxopts::value<std::string>())("d,definition", "The desired image definition, result should be close to it", cxxopts::value<uint32_t>()->default_value("512"))(
        "h,help",
        "Print this help and exit");
    options.parse_positional({ "filepath" });
    options.positional_help("<filepath>");
    options.show_positional_help();

    cxxopts::ParseResult result = options.parse(argc, argv);
    if (result.count("help") || ! result.count("filepath"))
    {
        std::cout << options.help() << std::endl;
        return 0;
    }

    const std::string file_path = result["filepath"].as<std::string>();
    spdlog::info("Loading mesh from {}", file_path);

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(file_path, /*aiProcess_Triangulate | aiProcess_JoinIdenticalVertices*/ 0);

    if (! scene)
    {
        spdlog::error("Failed to load mesh: {}", importer.GetErrorString());
        return 1;
    }

    if (scene->HasMeshes())
    {
        for (size_t i = 0; i < scene->mNumMeshes; i++)
        {
            const aiMesh* mesh = scene->mMeshes[i];
            spdlog::info("Loaded mesh with {} vertices and {} faces", mesh->mNumVertices, mesh->mNumFaces);
        }
    }
    else
    {
        spdlog::warn("The file doesn't contain any mesh");
    }

    aiScene* export_scene = nullptr;
    if (result.count("outputfile"))
    {
        aiCopyScene(scene, &export_scene);
    }

    for (size_t i = 0; i < scene->mNumMeshes; i++)
    {
        const aiMesh* mesh = scene->mMeshes[i];
        if (mesh->mName.length)
        {
            spdlog::info("Processing mesh {}", mesh->mName.data);
        }
        else
        {
            spdlog::info("Processing (unnamed) mesh", mesh->mName.data);
        }

        std::vector<std::tuple<float, float, float>> vertices;
        vertices.reserve(mesh->mNumVertices);
        for (size_t j = 0; j < mesh->mNumVertices; j++)
        {
            const aiVector3D vertex = mesh->mVertices[j];
            vertices.emplace_back(vertex.x, vertex.y, vertex.z);
        }

        std::vector<std::tuple<int32_t, int32_t, int32_t>> indices;
        indices.reserve(mesh->mNumFaces);
        for (size_t j = 0; j < mesh->mNumFaces; j++)
        {
            const aiFace& face = mesh->mFaces[j];
            indices.emplace_back(face.mIndices[0], face.mIndices[1], face.mIndices[2]);
        }

        std::vector<std::tuple<float, float>> uv_coords(mesh->mNumVertices, { 0.0, 0.0 });
        uint32_t texture_width, texture_height;

        spdlog::stopwatch timer;

        spdlog::info("Start UV unwrapping");
        if (unwrap_lscm(vertices, indices, result["definition"].as<uint32_t>(), uv_coords, texture_width, texture_height))
        {
            spdlog::info("Suggested texture size is {}x{}", texture_width, texture_height);
            spdlog::info("UV unwrapping took {}ms", timer.elapsed_ms().count());

            if (export_scene)
            {
                aiMesh* export_mesh = export_scene->mMeshes[i];

                if (! export_mesh->mTextureCoordsNames)
                {
                    export_mesh->mTextureCoordsNames = new aiString* [AI_MAX_NUMBER_OF_TEXTURECOORDS] {};
                }

                for (size_t j = 0; j < AI_MAX_NUMBER_OF_TEXTURECOORDS; j++)
                {
                    delete export_mesh->mTextureCoordsNames[j];
                    delete export_mesh->mTextureCoords[j];

                    if (j == 0)
                    {
                        export_mesh->mNumUVComponents[j] = 2;
                        export_mesh->mTextureCoordsNames[j] = new aiString("unwrapped");
                        export_mesh->mTextureCoords[j] = new aiVector3D[export_mesh->mNumVertices];
                        for (size_t k = 0; k < export_mesh->mNumVertices; k++)
                        {
                            const std::tuple<float, float>& uv = uv_coords[k];
                            aiVector3D& export_uv = export_mesh->mTextureCoords[j][k];
                            export_uv.x = std::get<0>(uv);
                            export_uv.y = std::get<1>(uv);
                        }
                    }
                    else
                    {
                        export_mesh->mNumUVComponents[j] = 0;
                        export_mesh->mTextureCoordsNames[j] = nullptr;
                        export_mesh->mTextureCoords[j] = nullptr;
                    }
                }
            }
        }
        else
        {
            spdlog::error("Couldn't unwrap UVs!");
        }
    }

    if (export_scene)
    {
        const std::string output_file = result["outputfile"].as<std::string>();
        spdlog::info("Exporting result to {}", output_file);

        Assimp::Exporter exporter;
        exporter.Export(export_scene, "obj", output_file);

        aiFreeScene(export_scene);
    }

    return 0;
}