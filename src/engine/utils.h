#ifndef __UTILS__
#define __UTILS__

#include <functional>
#include <deque>
#include <fstream>
#include <chrono>
#include <vector>

#include "core.h"

GLIB_NAMESPACE_BEGIN

namespace utils
{

    struct EventDispatcher
    {
        std::deque<std::function<void()>> functions;

        void push_function(std::function<void()> &&function)
        {
            functions.push_back(function);
        }

        void flush()
        {
            // reverse iterate the deletion queue to execute all the functions
            for (auto it = functions.rbegin(); it != functions.rend(); it++)
            {
                (*it)(); // call functors
            }

            functions.clear();
        }
    };
    class ManualTimer
    {
        std::chrono::high_resolution_clock::time_point t0;
        double timestamp{0.0};

    public:
        void start() { t0 = std::chrono::high_resolution_clock::now(); }
        void stop() { timestamp = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count() * 1000.0; }
        const double &get() { return timestamp; }
    };

    struct memory_buffer : public std::streambuf
    {
        char *p_start{nullptr};
        char *p_end{nullptr};
        size_t size;

        memory_buffer(char const *first_elem, size_t size)
            : p_start(const_cast<char *>(first_elem)), p_end(p_start + size), size(size)
        {
            setg(p_start, p_start, p_end);
        }

        pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override
        {
            if (dir == std::ios_base::cur)
                gbump(static_cast<int>(off));
            else
                setg(p_start, (dir == std::ios_base::beg ? p_start : p_end) + off, p_end);
            return gptr() - p_start;
        }

        pos_type seekpos(pos_type pos, std::ios_base::openmode which) override
        {
            return seekoff(pos, std::ios_base::beg, which);
        }
    };

    struct memory_stream : virtual memory_buffer, public std::istream
    {
        memory_stream(char const *first_elem, size_t size)
            : memory_buffer(first_elem, size), std::istream(static_cast<std::streambuf *>(this)) {}
    };
    inline std::vector<uint8_t> read_file_binary(const std::string &pathToFile)
    {
        std::ifstream file(pathToFile, std::ios::binary);
        std::vector<uint8_t> fileBufferBytes;

        if (file.is_open())
        {
            file.seekg(0, std::ios::end);
            size_t sizeBytes = file.tellg();
            file.seekg(0, std::ios::beg);
            fileBufferBytes.resize(sizeBytes);
            if (file.read((char *)fileBufferBytes.data(), sizeBytes))
                return fileBufferBytes;
        }
        else
            throw std::runtime_error("could not open binary ifstream to path " + pathToFile);
        return fileBufferBytes;
    }
    glm::vec3 get_tangent_gram_smidt(glm::vec3 &p1, glm::vec3 &p2, glm::vec3 &p3, glm::vec2 &uv1, glm::vec2 &uv2, glm::vec2 &uv3, glm::vec3 normal);

    template <typename T, typename... Rest>
    void hash_combine(std::size_t &seed, const T &v, const Rest &...rest)
    {
        seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        (hash_combine(seed, rest), ...);
    }

    const std::string HDRIConverterVertexSource = R"(
    #version 460 core

	out vec2 texCoord;

	void main() 
	{
	   float x = float((gl_VertexID & 1) << 2);
	   float y = float((gl_VertexID & 2) << 1);
	   texCoord.x = x * 0.5;
	   texCoord.y = y * 0.5;
	   gl_Position = vec4(x - 1.0, y - 1.0, 0, 1);
	}
)";

    const std::string HDRIConverterFragmentSource = R"(
    #version 460 core
    #define MATH_PI 3.1415926535897932384626433832795
	#define MATH_INV_PI (1.0 / MATH_PI)

	precision highp float;


	in vec2 texCoord;
	out vec4 fragmentColor;
	
	
	uniform int u_currentFace;
	
	uniform sampler2D u_panorama;
	
	vec3 uvToXYZ(int face, vec2 uv)
	{
		if(face == 0)
			return vec3(     1.f,   uv.y,    -uv.x);
	
		else if(face == 1)
			return vec3(    -1.f,   uv.y,     uv.x);
	
		else if(face == 2)
			return vec3(   +uv.x,   -1.f,    +uv.y);
	
		else if(face == 3)
			return vec3(   +uv.x,    1.f,    -uv.y);
	
		else if(face == 4)
			return vec3(   +uv.x,   uv.y,      1.f);
	
		else //if(face == 5)
		{	return vec3(    -uv.x,  +uv.y,     -1.f);}
	}
	
	vec2 dirToUV(vec3 dir)
	{
		return vec2(
			0.5f + 0.5f * atan(dir.z, dir.x) / MATH_PI,
			1.f - acos(dir.y) / MATH_PI);
	}
	
	vec3 panoramaToCubeMap(int face, vec2 texCoord)
	{
		vec2 texCoordNew = texCoord*2.0-1.0;
		vec3 scan = uvToXYZ(face, texCoordNew);
		vec3 direction = normalize(scan);
		vec2 src = dirToUV(direction);
	
		return  texture(u_panorama, src).rgb;
	}
	
	
	void main(void)
	{
	    fragmentColor = vec4(0.0, 0.0, 0.0, 1.0);

        vec3 color =panoramaToCubeMap(u_currentFace, texCoord);
        color = color / (color + vec3(1.0));

        const float GAMMA = 2.2;
        color = pow(color, vec3(1.0 / GAMMA));
	
		fragmentColor.rgb = color;
	}
)";

}
GLIB_NAMESPACE_END
#endif