#include "hair_loaders.h"

void hair_loaders::load_neural_hair(Mesh *const mesh, const char *fileName, Mesh *const skullMesh, bool preload, bool verbose, bool calculateTangents)
{

    std::unique_ptr<std::istream> file_stream;
    std::vector<uint8_t> byte_buffer;
    std::string filePath = fileName;
    try
    {
        if (preload)
        {
            byte_buffer = utils::read_file_binary(filePath);
            file_stream.reset(new utils::memory_stream((char *)byte_buffer.data(), byte_buffer.size()));
        }
        else
        {
            file_stream.reset(new std::ifstream(filePath, std::ios::binary));
        }

        if (!file_stream || file_stream->fail())
            throw std::runtime_error("file_stream failed to open " + filePath);

        file_stream->seekg(0, std::ios::end);
        const float size_mb = file_stream->tellg() * float(1e-6);
        file_stream->seekg(0, std::ios::beg);

        tinyply::PlyFile file;
        file.parse_header(*file_stream);

        if (verbose)
        {
            std::cout << "\t[ply_header] Type: " << (file.is_binary_file() ? "binary" : "ascii") << std::endl;
            for (const auto &c : file.get_comments())
                std::cout << "\t[ply_header] Comment: " << c << std::endl;
            for (const auto &c : file.get_info())
                std::cout << "\t[ply_header] Info: " << c << std::endl;

            for (const auto &e : file.get_elements())
            {
                std::cout << "\t[ply_header] element: " << e.name << " (" << e.size << ")" << std::endl;
                for (const auto &p : e.properties)
                {
                    std::cout << "\t[ply_header] \tproperty: " << p.name << " (type=" << tinyply::PropertyTable[p.propertyType].str << ")";
                    if (p.isList)
                        std::cout << " (list_type=" << tinyply::PropertyTable[p.listType].str << ")";
                    std::cout << std::endl;
                }
            }
        }
        std::shared_ptr<tinyply::PlyData> positions, colors;
        try
        {
            positions = file.request_properties_from_element("vertex", {"x", "y", "z"});
        }
        catch (const std::exception &e)
        {
            std::cerr << "tinyply exception: " << e.what() << std::endl;
        }

        try
        {
            colors = file.request_properties_from_element("vertex", {"red", "green", "blue", "alpha"});
        }
        catch (const std::exception &e)
        {
            if (verbose)
                std::cerr << "tinyply exception: " << e.what() << std::endl;
        }
        utils::ManualTimer readTimer;
        readTimer.start();
        file.read(*file_stream);
        readTimer.stop();

        if (verbose)
        {
            const float parsingTime = static_cast<float>(readTimer.get()) / 1000.f;
            std::cout << "\tparsing " << size_mb << "mb in " << parsingTime << " seconds [" << (size_mb / parsingTime) << " MBps]" << std::endl;
            if (positions)
                std::cout << "\tRead " << positions->count << " total vertices " << std::endl;
            if (colors)
                std::cout << "\tRead " << colors->count << " total vertex colors " << std::endl;
        }

        std::vector<Vertex> vertices;
        vertices.reserve(positions->count);
        std::vector<unsigned int> indices;
        std::vector<unsigned int> rootsIndices;

        if (positions)
        {
            rootsIndices.push_back(0); // First index is certainly a root
            const float *posData = reinterpret_cast<const float *>(positions->buffer.get());
            const unsigned char *colorData = reinterpret_cast<const unsigned char *>(colors->buffer.get());
            for (size_t i = 0; i < positions->count - 1; i++)
            {
                float x = posData[i * 3];
                float y = posData[i * 3 + 1];
                float z = posData[i * 3 + 2];

                // Generate hair tangents
                glm::vec3 pos = {x, y, z};
                float nextX = posData[(i + 1) * 3];
                float nextY = posData[(i + 1) * 3 + 1];
                float nextZ = posData[(i + 1) * 3 + 2];
                glm::vec3 nextPos = {nextX, nextY, nextZ};
                glm::vec3 tangent = glm::normalize(nextPos - pos);

                float r = (float)colorData[i * 4];
                float g = (float)colorData[i * 4 + 1];
                float b = (float)colorData[i * 4 + 2];

                float nextR = (float)colorData[(i + 1) * 4];
                float nextG = (float)colorData[(i + 1) * 4 + 1];
                float nextB = (float)colorData[(i + 1) * 4 + 2];

                vertices.push_back({pos, {0.0f, 0.0f, 0.0f}, tangent, {0.0f, 0.0f}, {r / 255, g / 255, b / 255}});
                if (i == positions->count - 2)
                    vertices.push_back({nextPos, {0.0f, 0.0f, 0.0f}, tangent, {0.0f, 0.0f}, {r / 255, g / 255, b / 255}});

                if (r == nextR && g == nextG && b == nextB)
                {
                    indices.push_back(i);
                    indices.push_back(i + 1);
                }
                else
                {
                    rootsIndices.push_back(i + 1);
                }
            }
        }

        auto samplePoint = [=](glm::vec2 sample, glm::vec3 a, glm::vec3 b, glm::vec3 c)
        {
            // Warp to triangle
            float t = std::sqrt(1.0f - sample.x);
            glm::vec2 uv = glm::vec2(1.f - t, sample.y * t);

            // Get bary
            glm::vec3 bary{1 - (uv.x + uv.y), uv.x, uv.y};

            // Compute global positon accurately
            // using barycentric coordinates
            glm::vec3 point = bary.x * a + bary.y * b + bary.z * c;

            return point;
        };

        auto augmentDensity = [&](Geometry &geom, unsigned int totalStrands)
        {

#define CONCURRENT
            // Neural haircut asures it
            const unsigned int STRAND_LENGTH = rootsIndices[1] - rootsIndices[0] - 1;
            // Neighburs (should be user defined)
            const unsigned int NEIGHBORS = 3;
            // Color compare threshold for scalp vertices
            const float COLOR_THRESHOLD = 0.1f;

            // Create a random number generator engine
            std::random_device rd;
            std::mt19937 gen(rd());                               // Mersenne Twister PRNG
            std::uniform_real_distribution<double> dis(0.0, 1.0); // CHANGE IT TO PSEUDORANDOM (BLUE NOISE OR SOBOL)

            std::vector<Vertex> vertices = skullMesh->get_geometry().vertices;
            std::vector<unsigned int> rawIndices = skullMesh->get_geometry().indices;
            std::vector<unsigned int> indices;

            // Check triangles susceptible of being scalp in skull
            for (size_t i = 0; i < rawIndices.size(); i += 3)
            {
                if (vertices[rawIndices[i]].color.b < COLOR_THRESHOLD || vertices[rawIndices[i + 1]].color.b < COLOR_THRESHOLD || vertices[rawIndices[i + 2]].color.b < COLOR_THRESHOLD)
                {
                    // Save tri indices
                    indices.push_back(rawIndices[i]);
                    indices.push_back(rawIndices[i + 1]);
                    indices.push_back(rawIndices[i + 2]);
                }
            }
            // std::cout<< "SIZE " << indices.size()<< std::endl;

            std::vector<float> areas;
            float totalArea{0.0f};
            areas.reserve(indices.size() / 3);

            // Compute total area
            for (size_t i = 0; i < indices.size(); i += 3)
            {
                float area = 0.5f * glm::length(glm::cross(glm::vec3(vertices[indices[i + 1]].position - vertices[indices[i]].position), glm::vec3(vertices[indices[i + 2]].position - vertices[indices[i]].position)));
                areas.push_back(area);
                totalArea += area;
            }

            struct Neighbor
            {
                unsigned int id;
                float dist;
                float weight;
            };

#ifdef CONCURRENT

            // NUMBER OF OPERATIONS PER TASK
            const unsigned int NUM_TRIS = indices.size() / 3;
            const unsigned int OPERATIONS = 2000;
            const size_t NUM_TASKS = (NUM_TRIS / OPERATIONS) + 1;

            std::vector<std::thread> tasks;
            tasks.reserve(NUM_TASKS);

            struct ScalpFace
            {
                unsigned int a;
                unsigned int b;
                unsigned int c;
                unsigned int strands;
                unsigned int cumulative;
            };

            std::vector<ScalpFace> triangles;
            triangles.reserve(NUM_TRIS);

            size_t t = 0;
            unsigned int accumStrands = 0;
            // Compute triangle areas and strands to grow
            for (size_t i = 0; i < indices.size(); i += 3, t++)
            {
                // Uniformize number
                float pdf = areas[t] / totalArea;
                const unsigned int strands = totalStrands * pdf;
                triangles.push_back({indices[i], indices[i + 1], indices[i + 2], strands, accumStrands});
                accumStrands += strands;
            }

            // Setup key data strcutures
            std::vector<std::vector<Neighbor>> nearestNeighbors;
            nearestNeighbors.resize(accumStrands);
            std::vector<glm::vec3> roots;
            roots.resize(accumStrands);

            auto computeNearestNeighbors = [&](size_t taskID)
            {
                for (size_t t = OPERATIONS * taskID; t < OPERATIONS * (taskID + 1); t++)
                {
                    if (t >= NUM_TRIS)
                        break;

                    // FOR STRAND
                    const size_t START_STRAND = triangles[t].cumulative;
                    const size_t END_STRAND = triangles[t].cumulative + triangles[t].strands;
                    for (size_t s = START_STRAND; s < END_STRAND; s++)
                    {
                        // Get random value
                        glm::vec2 sample2D = glm::vec2(dis(gen), dis(gen));
                        roots[s] = samplePoint(sample2D, vertices[triangles[t].a].position, vertices[triangles[t].b].position, vertices[triangles[t].c].position);

                        // Neighbor adjacency list;
                        std::vector<Neighbor> potentialNeighbors;
                        potentialNeighbors.reserve(rootsIndices.size());

                        for (size_t r = 0; r < rootsIndices.size(); r++)
                        {
                            float dist = glm::distance(geom.vertices[rootsIndices[r]].position, roots[s]);
                            Neighbor potentialN{rootsIndices[r], dist};
                            potentialNeighbors.push_back(potentialN);
                        }

                        // Sort from closest to farthest
                        std::sort(potentialNeighbors.begin(), potentialNeighbors.end(), [](const Neighbor &a, const Neighbor &b)
                                  { return a.dist < b.dist; });

                        int count = 0;
                        for (auto it = potentialNeighbors.begin(); it != potentialNeighbors.end() && count < NEIGHBORS; ++it, ++count)
                        {
                            nearestNeighbors[s].push_back(*it);
                        }

                        // Compute Neighbor weights
                        float totalWeight = 0;
                        for (size_t nn = 0; nn < NEIGHBORS; nn++)
                        {
                            nearestNeighbors[s][nn].weight = 1 / (nearestNeighbors[s][nn].dist * nearestNeighbors[s][nn].dist);
                            totalWeight += nearestNeighbors[s][nn].weight;
                        }
                        // // NORMALIZE WEIGHTS
                        for (size_t nn = 0; nn < NEIGHBORS; nn++)
                        {
                            nearestNeighbors[s][nn].weight = nearestNeighbors[s][nn].weight / totalWeight;
                        }
                    }
                }
            };

            for (size_t tk = 0; tk < NUM_TASKS; tk++)
            {
                std::thread task(computeNearestNeighbors, tk);
                // task.detach();
                tasks.push_back(move(task));
            }

            // WAIT FOR THREADS TO FINISH
            for (std::thread &t : tasks)
            {
                t.join();
            }

            // NEW STRAND

            for (size_t s = 0; s < accumStrands; s++)
            {
                // CHOOSE RANDOM COLOR FOR DEBUG
                glm::vec3 color = {((float)rand()) / RAND_MAX, ((float)rand()) / RAND_MAX, ((float)rand()) / RAND_MAX};

                unsigned int currentIndex = geom.indices.back() + 1;

                // Add root
                geom.vertices.push_back({roots[s], {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, color});
                geom.indices.push_back(currentIndex);
                geom.indices.push_back(currentIndex + 1);
                currentIndex++;

                // Grow
                for (size_t p = 1; p < STRAND_LENGTH; p++)
                {
                    // Compute differentials
                    glm::vec3 avrDiff = glm::vec3(0.0f);
                    std::vector<glm::vec3> diffs;
                    diffs.reserve(3);

                    for (size_t n = 0; n < NEIGHBORS; n++)
                    {
                        if (nearestNeighbors[s][n].weight == 0)
                            continue;
                        glm::vec3 diff = geom.vertices[nearestNeighbors[s][n].id + p].position - geom.vertices[nearestNeighbors[s][n].id + (p - 1)].position;
                        diffs.push_back(diff);
                        avrDiff += diff * nearestNeighbors[s][n].weight;
                    }

                    // Update predecessor FRAME
                    geom.vertices[geom.vertices.size() - 1].tangent = glm::normalize(avrDiff);
                    // Setup new FRAME
                    geom.vertices.push_back({geom.vertices.back().position + avrDiff, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, color});

                    // Control index generation
                    if (p < STRAND_LENGTH - 1)
                    {
                        geom.indices.push_back(currentIndex);
                        geom.indices.push_back(currentIndex + 1);
                        currentIndex++;
                    }

                    // Compare diffs
                    unsigned int checkID = (rand() % NEIGHBORS);
                    glm::vec3 cDiff = glm::normalize(diffs[checkID]);
                    for (size_t n = 0; n < NEIGHBORS; n++)
                    {
                        if (n == checkID)
                            continue;
                        if (nearestNeighbors[s][n].weight == 0.0f)
                            continue;
                        glm::vec3 diff = glm::normalize(diffs[n]);
                        if (glm::dot(cDiff, diff) <= 0.0f)
                            nearestNeighbors[s][n].weight = 0.0f;
                    }
                }
            }
#else
            // Populate
            size_t t = 0;
            // std::cout << indices.size() << std::endl;

            for (size_t i = 0; i < indices.size(); i += 3, t++)
            {
                // Uniformize number
                float pdf = areas[t] / totalArea;
                const unsigned int STRANDS_TO_GROW = totalStrands * pdf;

                for (size_t s = 0; s < STRANDS_TO_GROW; s++)
                {

                    // Get random value
                    glm::vec2 sample2D = glm::vec2(dis(gen), dis(gen));
                    glm::vec3 root = samplePoint(sample2D, vertices[indices[i]].position, vertices[indices[i + 1]].position, vertices[indices[i + 2]].position);

                    // Check nearest Neighbors
                    std::vector<Neighbor> nearestNeighbors;

                    // Neighbor adjacency list;
                    std::vector<Neighbor> roots;
                    roots.reserve(rootsIndices.size());

                    for (size_t r = 0; r < rootsIndices.size(); r++)
                    {
                        float dist = glm::distance(geom.vertices[rootsIndices[r]].position, root);
                        Neighbor potentialN{rootsIndices[r], dist};
                        roots.push_back(potentialN);
                    }

                    // Sort from closest to farthest
                    std::sort(roots.begin(), roots.end(), [](const Neighbor &a, const Neighbor &b)
                              { return a.dist < b.dist; });

                    int count = 0;
                    for (auto it = roots.begin(); it != roots.end() && count < NeighborS; ++it, ++count)
                    {
                        nearestNeighbors.push_back(*it);
                    }

                    // Compute Neighbor weights
                    float totalWeight = 0;

                    // RANDON WEIGHT PER Neighbor
                    for (size_t nn = 0; nn < NeighborS; nn++)
                    {
                        nearestNeighbors[nn].weight = 1 / (nearestNeighbors[nn].dist * nearestNeighbors[nn].dist);
                        totalWeight += nearestNeighbors[nn].weight;
                    }
                    // // NORMALIZE WEIGHTS
                    for (size_t nn = 0; nn < NeighborS; nn++)
                    {
                        nearestNeighbors[nn].weight = nearestNeighbors[nn].weight / totalWeight;
                    }

                    // CHOOSE RANDOM COLOR FOR DEBUG
                    // glm::vec3 color = {((float)rand()) / RAND_MAX, ((float)rand()) / RAND_MAX, ((float)rand()) / RAND_MAX};
                    glm::vec3 color = {1.0f, 0.0f, 0.0f};

                    // NEW STRAND
                    for (size_t s = 0; s < accumStrands; s++)
                    {
                        // CHOOSE RANDOM COLOR FOR DEBUG
                        glm::vec3 color = {((float)rand()) / RAND_MAX, ((float)rand()) / RAND_MAX, ((float)rand()) / RAND_MAX};

                        unsigned int currentIndex = geom.indices.back() + 1;

                        // Add root
                        geom.vertices.push_back({roots[s], {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, color});
                        geom.indices.push_back(currentIndex);
                        geom.indices.push_back(currentIndex + 1);
                        currentIndex++;

                        // Grow
                        for (size_t p = 1; p < STRAND_LENGTH; p++)
                        {
                            // Compute differentials
                            glm::vec3 avrDiff = glm::vec3(0.0f);

                            for (size_t n = 0; n < NEIGHBORS; n++)
                            {
                                glm::vec3 diff = geom.vertices[nearestNeighbors[s][n].id + p].position - geom.vertices[nearestNeighbors[s][n].id + (p - 1)].position;

                                // Check is this diff is way to big

                                avrDiff += diff * nearestNeighbors[s][n].weight;
                            }

                            geom.vertices.push_back({geom.vertices.back().position + avrDiff, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, color});

                            // Control index generation
                            if (p < STRAND_LENGTH - 1)
                            {
                                geom.indices.push_back(currentIndex);
                                geom.indices.push_back(currentIndex + 1);
                                currentIndex++;
                            }
                        }
                    }
                }
            }
#endif
        };

        Geometry g;
        g.vertices = vertices;
        g.indices = indices;
        augmentDensity(g, 40000);
        mesh->set_geometry(g);

        return;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Caught tinyply exception: " << e.what() << std::endl;
    }
}

void hair_loaders::load_cy_hair(Mesh *const mesh, const char *fileName)
{

#define HAIR_FILE_SEGMENTS_BIT 1
#define HAIR_FILE_POINTS_BIT 2
#define HAIR_FILE_THICKNESS_BIT 4
#define HAIR_FILE_TRANSPARENCY_BIT 8
#define HAIR_FILE_COLORS_BIT 16
#define HAIR_FILE_INFO_SIZE 88

    unsigned short *segments{nullptr};
    float *points;
    float *dirs;
    float *thickness;
    float *transparency;
    float *colors;

    struct Header
    {
        char signature[4];        //!< This should be "HAIR"
        unsigned int hair_count;  //!< number of hair strands
        unsigned int point_count; //!< total number of points of all strands
        unsigned int arrays;      //!< bit array of data in the file

        unsigned int d_segments; //!< default number of segments of each strand
        float d_thickness;       //!< default thickness of hair strands
        float d_transparency;    //!< default transparency of hair strands
        float d_color[3];        //!< default color of hair strands

        char info[HAIR_FILE_INFO_SIZE]; //!< information about the file
    };

    Header header;

    header.signature[0] = 'H';
    header.signature[1] = 'A';
    header.signature[2] = 'I';
    header.signature[3] = 'R';
    header.hair_count = 0;
    header.point_count = 0;
    header.arrays = 0; // no arrays
    header.d_segments = 0;
    header.d_thickness = 1.0f;
    header.d_transparency = 0.0f;
    header.d_color[0] = 1.0f;
    header.d_color[1] = 1.0f;
    header.d_color[2] = 1.0f;
    memset(header.info, '\0', HAIR_FILE_INFO_SIZE);

    FILE *fp;
    fp = fopen(fileName, "rb");
    if (fp == nullptr)
        return;

    // read the header
    size_t headread = fread(&header, sizeof(Header), 1, fp);

    // Check if header is correctly read
    if (headread < 1)
        return;

    // Check if this is a hair file
    if (strncmp(header.signature, "HAIR", 4) != 0)
        return;

    // Read segments array
    if (header.arrays & HAIR_FILE_SEGMENTS_BIT)
    {
        segments = new unsigned short[header.hair_count];
        size_t readcount = fread(segments, sizeof(unsigned short), header.hair_count, fp);
        if (readcount < header.hair_count)
        {
            std::cerr << "Error reading segments" << std::endl;
            return;
        }
    }

    // Read points array
    if (header.arrays & HAIR_FILE_POINTS_BIT)
    {
        points = new float[header.point_count * 3];
        size_t readcount = fread(points, sizeof(float), header.point_count * 3, fp);
        if (readcount < header.point_count * 3)
        {
            std::cerr << "Error reading points" << std::endl;
            return;
        }
    }

    // Read thickness array
    if (header.arrays & HAIR_FILE_THICKNESS_BIT)
    {
        thickness = new float[header.point_count];
        size_t readcount = fread(thickness, sizeof(float), header.point_count, fp);
        if (readcount < header.point_count)
        {
            std::cerr << "Error reading thickness" << std::endl;
            return;
        }
    }

    // Read thickness array
    if (header.arrays & HAIR_FILE_TRANSPARENCY_BIT)
    {
        transparency = new float[header.point_count];
        size_t readcount = fread(transparency, sizeof(float), header.point_count, fp);
        if (readcount < header.point_count)
        {
            std::cerr << "Error reading alpha" << std::endl;
            return;
        }
    }

    // Read colors array
    if (header.arrays & HAIR_FILE_COLORS_BIT)
    {
        colors = new float[header.point_count * 3];
        size_t readcount = fread(colors, sizeof(float), header.point_count * 3, fp);
        if (readcount < header.point_count * 3)
        {
            std::cerr << "Error reading colors" << std::endl;
            return;
        }
    }

    fclose(fp);

    auto computeDirection = [](float *d, float &d0len, float &d1len, float const *p0, float const *p1, float const *p2)
    {
        // line from p0 to p1
        float d0[3];
        d0[0] = p1[0] - p0[0];
        d0[1] = p1[1] - p0[1];
        d0[2] = p1[2] - p0[2];
        float d0lensq = d0[0] * d0[0] + d0[1] * d0[1] + d0[2] * d0[2];
        d0len = (d0lensq > 0) ? (float)sqrt(d0lensq) : 1.0f;

        // line from p1 to p2
        float d1[3];
        d1[0] = p2[0] - p1[0];
        d1[1] = p2[1] - p1[1];
        d1[2] = p2[2] - p1[2];
        float d1lensq = d1[0] * d1[0] + d1[1] * d1[1] + d1[2] * d1[2];
        d1len = (d1lensq > 0) ? (float)sqrt(d1lensq) : 1.0f;

        // make sure that d0 and d1 has the same length
        d0[0] *= d1len / d0len;
        d0[1] *= d1len / d0len;
        d0[2] *= d1len / d0len;

        // direction at p1
        d[0] = d0[0] + d1[0];
        d[1] = d0[1] + d1[1];
        d[2] = d0[2] + d1[2];
        float dlensq = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
        float dlen = (dlensq > 0) ? (float)sqrt(dlensq) : 1.0f;
        d[0] /= dlen;
        d[1] /= dlen;
        d[2] /= dlen;

        // return d0len;
    };

    auto fillDirectionArray = [=](float *dir)
    {
        if (dir == nullptr || header.point_count <= 0 || points == nullptr)
            return;

        int p = 0; // point index
        for (unsigned int i = 0; i < header.hair_count; i++)
        {
            int s = (segments) ? segments[i] : header.d_segments;
            if (s > 1)
            {
                // direction at point1
                float len0, len1;
                computeDirection(&dir[(p + 1) * 3], len0, len1, &points[p * 3], &points[(p + 1) * 3], &points[(p + 2) * 3]);

                // direction at point0
                float d0[3];
                d0[0] = points[(p + 1) * 3] - dir[(p + 1) * 3] * len0 * 0.3333f - points[p * 3];
                d0[1] = points[(p + 1) * 3 + 1] - dir[(p + 1) * 3 + 1] * len0 * 0.3333f - points[p * 3 + 1];
                d0[2] = points[(p + 1) * 3 + 2] - dir[(p + 1) * 3 + 2] * len0 * 0.3333f - points[p * 3 + 2];
                float d0lensq = d0[0] * d0[0] + d0[1] * d0[1] + d0[2] * d0[2];
                float d0len = (d0lensq > 0) ? (float)sqrt(d0lensq) : 1.0f;
                dir[p * 3] = d0[0] / d0len;
                dir[p * 3 + 1] = d0[1] / d0len;
                dir[p * 3 + 2] = d0[2] / d0len;

                // We computed the first 2 points
                p += 2;

                // Compute the direction for the rest
                for (int t = 2; t < s; t++, p++)
                {
                    computeDirection(&dir[p * 3], len0, len1, &points[(p - 1) * 3], &points[p * 3], &points[(p + 1) * 3]);
                }

                // direction at the last point
                d0[0] = -points[(p - 1) * 3] + dir[(p - 1) * 3] * len1 * 0.3333f + points[p * 3];
                d0[1] = -points[(p - 1) * 3 + 1] + dir[(p - 1) * 3 + 1] * len1 * 0.3333f + points[p * 3 + 1];
                d0[2] = -points[(p - 1) * 3 + 2] + dir[(p - 1) * 3 + 2] * len1 * 0.3333f + points[p * 3 + 2];
                d0lensq = d0[0] * d0[0] + d0[1] * d0[1] + d0[2] * d0[2];
                d0len = (d0lensq > 0) ? (float)sqrt(d0lensq) : 1.0f;
                dir[p * 3] = d0[0] / d0len;
                dir[p * 3 + 1] = d0[1] / d0len;
                dir[p * 3 + 2] = d0[2] / d0len;
                p++;
            }
            else if (s > 0)
            {
                // if it has a single segment
                float d0[3];
                d0[0] = points[(p + 1) * 3] - points[p * 3];
                d0[1] = points[(p + 1) * 3 + 1] - points[p * 3 + 1];
                d0[2] = points[(p + 1) * 3 + 2] - points[p * 3 + 2];
                float d0lensq = d0[0] * d0[0] + d0[1] * d0[1] + d0[2] * d0[2];
                float d0len = (d0lensq > 0) ? (float)sqrt(d0lensq) : 1.0f;
                dir[p * 3] = d0[0] / d0len;
                dir[p * 3 + 1] = d0[1] / d0len;
                dir[p * 3 + 2] = d0[2] / d0len;
                dir[(p + 1) * 3] = dir[p * 3];
                dir[(p + 1) * 3 + 1] = dir[p * 3 + 1];
                dir[(p + 1) * 3 + 2] = dir[p * 3 + 2];
                p += 2;
            }
            //*/
        }
    };

    dirs = new float[header.point_count * 3];
    fillDirectionArray(dirs);

    std::vector<Vertex> vertices;
    vertices.reserve(header.point_count * 3);
    std::vector<unsigned int> indices;

    size_t index = 0;
    size_t pointId = 0;
    for (size_t hair = 0; hair < header.hair_count; hair++)
    {
        glm::vec3 color = {((float)rand()) / RAND_MAX, ((float)rand()) / RAND_MAX, ((float)rand()) / RAND_MAX};
        size_t max_segments = segments ? segments[hair] : header.d_segments;
        for (size_t i = 0; i < max_segments; i++)
        {
            vertices.push_back({{points[pointId], points[pointId + 1], points[pointId + 2]}, {0.0f, 0.0f, 0.0f}, {dirs[pointId], dirs[pointId + 1], dirs[pointId + 2]}, {0.0f, 0.0f}, color});
            indices.push_back(index);
            indices.push_back(index + 1);
            index++;
            pointId += 3;
        }
        vertices.push_back({{points[pointId], points[pointId + 1], points[pointId + 2]}, {0.0f, 0.0f, 0.0f}, {dirs[pointId], dirs[pointId + 1], dirs[pointId + 2]}, {0.0f, 0.0f}, color});
        pointId += 3;
        index++;
    }

    Geometry g;
    g.vertices = vertices;
    g.indices = indices;
    mesh->set_geometry(g);
}
