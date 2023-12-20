#define TINYOBJLOADER_IMPLEMENTATION
#include "loaders.h"

bool OBJ_loader::load_mesh(Mesh *const mesh, bool overrideGeometry, const char *fileName, bool importMaterials, bool calculateTangents)
{
    // Preparing output
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;

    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, fileName, importMaterials ? nullptr : nullptr);

    // Check for errors
    if (!warn.empty())
    {
        DEBUG_LOG("WARN: " + warn);
    }
    if (!err.empty())
    {
        ERR_LOG(err);
        DEBUG_LOG("ERROR: Couldn't load mesh");
        return false;
    }

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::unordered_map<Vertex, unsigned int> uniqueVertices;

    size_t shape_id = 0;
    // attrib.
    for (const tinyobj::shape_t &shape : shapes)
    {
        if (!shape.mesh.indices.empty())
            // IS INDEXED
            for (const tinyobj::index_t &index : shape.mesh.indices)
            {
                Vertex vertex = {};

                // positionition and color
                if (index.vertex_index >= 0)
                {
                    vertex.position.x = attrib.vertices[3 * index.vertex_index + 0];
                    vertex.position.y = attrib.vertices[3 * index.vertex_index + 1];
                    vertex.position.z = attrib.vertices[3 * index.vertex_index + 2];

                    vertex.color.r = attrib.colors[3 + index.vertex_index + 0];
                    vertex.color.g = attrib.colors[3 + index.vertex_index + 1];
                    vertex.color.b = attrib.colors[3 + index.vertex_index + 2];
                }
                // Normal
                if (index.normal_index >= 0)
                {
                    vertex.normal.x = attrib.normals[3 * index.normal_index + 0];
                    vertex.normal.y = attrib.normals[3 * index.normal_index + 1];
                    vertex.normal.z = attrib.normals[3 * index.normal_index + 2];
                }

                if (calculateTangents)
                {

                    // vertex.tangent = utils::get_tangent_gram_smidt(vertex.position,
                    //                                                vertices[indices[indices.size() - 2]].position,
                    //                                                vertices[indices[indices.size() - 1]].position,
                    //                                                vertex.uv,
                    //                                                vertices[indices[indices.size() - 2]].uv,
                    //                                                vertices[indices[indices.size() - 1]].uv,
                    //                                                vertex.tangent);
                }
                else
                {
                    vertex.tangent = {0.0, 0.0, 0.0};
                }

                // UV
                if (index.texcoord_index >= 0)
                {
                    vertex.uv.x = attrib.texcoords[2 * index.texcoord_index + 0];
                    vertex.uv.y = attrib.texcoords[2 * index.texcoord_index + 1];
                }

                // Check if the vertex is already in the map
                if (uniqueVertices.count(vertex) == 0)
                {
                    uniqueVertices[vertex] = static_cast<unsigned int>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
        else
            // NOT INDEXED
            for (size_t i = 0; i < shape.mesh.num_face_vertices.size(); i++)
            {
                for (size_t j = 0; j < shape.mesh.num_face_vertices[i]; j++)
                {
                    Vertex vertex{};
                    size_t vertex_index = shape.mesh.indices[i * shape.mesh.num_face_vertices[i] + j].vertex_index;
                    // position
                    if (!attrib.vertices.empty())
                    {
                        vertex.position.x = attrib.vertices[3 * vertex_index + 0];
                        vertex.position.y = attrib.vertices[3 * vertex_index + 1];
                        vertex.position.z = attrib.vertices[3 * vertex_index + 2];
                    }
                    // Normals
                    if (!attrib.normals.empty())
                    {
                        vertex.normal.x = attrib.normals[3 * vertex_index + 0];
                        vertex.normal.y = attrib.normals[3 * vertex_index + 1];
                        vertex.normal.z = attrib.normals[3 * vertex_index + 2];
                    }
                    // Tangents
                    if (calculateTangents)
                    {
                        // vertex.tangent = utils::get_tangent_gram_smidt(vertex.position,
                        //                                                vertices[indices[indices.size() - 2]].position,
                        //                                                vertices[indices[indices.size() - 1]].position,
                        //                                                vertex.uv,
                        //                                                vertices[indices[indices.size() - 2]].uv,
                        //                                                vertices[indices[indices.size() - 1]].uv,
                        //                                                vertex.tangent);
                    }
                    else
                    {
                        vertex.tangent = {0.0, 0.0, 0.0};
                    }
                    // UV
                    if (!attrib.texcoords.empty())
                    {
                        vertex.uv.x = attrib.texcoords[2 * vertex_index + 0];
                        vertex.uv.y = attrib.texcoords[2 * vertex_index + 1];
                    }
                    // COLORS
                    if (!attrib.colors.empty())
                    {
                        vertex.color.r = attrib.colors[3 * vertex_index + 0];
                        vertex.color.g = attrib.colors[3 * vertex_index + 1];
                        vertex.color.b = attrib.colors[3 * vertex_index + 2];
                    }

                    vertices.push_back(vertex);
                }
            }

        Geometry g;
        g.vertices = vertices;
        g.indices = indices;
        g.triangles = 30;
        mesh->set_geometry(g);

        shape_id++;
    }
    return true;
}