#pragma once
#include <vector>
#include "Vertex.h"
#include <tiny_obj_loader.h>

namespace Engine
{
    class ObjLoaderHelper
    {
    public:
        static void ProcessAndInterleave(
            const tinyobj::attrib_t& attrib,
            const std::vector<tinyobj::shape_t>& shapes,
            std::vector<Vertex>& outVertices,
            std::vector<unsigned int>& outIndices)
        {
            unsigned int index = 0;
            for (const auto& shape : shapes)
            {
                for (const auto& index_t : shape.mesh.indices)
                {
                    Vertex v{};
                    v.px = attrib.vertices[3 * index_t.vertex_index + 0];
                    v.py = attrib.vertices[3 * index_t.vertex_index + 1];
                    v.pz = attrib.vertices[3 * index_t.vertex_index + 2];

                    if (index_t.normal_index >= 0)
                    {
                        v.nx = attrib.normals[3 * index_t.normal_index + 0];
                        v.ny = attrib.normals[3 * index_t.normal_index + 1];
                        v.nz = attrib.normals[3 * index_t.normal_index + 2];
                    }

                    if (index_t.texcoord_index >= 0)
                    {
                        v.u = attrib.texcoords[2 * index_t.texcoord_index + 0];
                        v.v = attrib.texcoords[2 * index_t.texcoord_index + 1];
                    }

                    outVertices.push_back(v);
                    outIndices.push_back(index++);
                }
            }
        }
    };
}
