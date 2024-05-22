#ifndef __CORE__
#define __CORE__

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#ifdef _WIN32
#define ASSERT(x) \
	if (!(x))     \
		__debugbreak();
#else
#define ASSERT(x) \
	if (!(x))     \
		__builtin_trap();
#endif

#define GL_CHECK(x) \
	GLclearError(); \
	x;              \
	ASSERT(GLlogCall(#x, __FILE__, __LINE__))
#define DEBUG_LOG(msg)                 \
	{                                  \
		std::cout << msg << std::endl; \
	}
#define ERR_LOG(msg)                   \
	{                                  \
		std::cerr << msg << std::endl; \
	}
#define GLFW_CHECK() GLFW_check_error();

#define GLIB_NAMESPACE_BEGIN \
	namespace glib           \
	{

#define GLIB_NAMESPACE_END }
#define USING_NAMESPACE_GLIB using namespace glib;

void GLFW_check_error();
void GLclearError();
bool GLlogCall(const char *function, const char *file, int line);

struct Extent2D
{
	int width{0};
	int height{0};

	inline bool operator==(Extent2D o)
	{
		return width == o.width && height == o.height;
	}
	inline bool operator!=(Extent2D o)
	{
		return width != o.width && height != o.height;
	}
};
struct Position2D
{
	int x{0};
	int y{0};

	inline bool operator==(Position2D o)
	{
		return x == o.x && y == o.y;
	}
	inline bool operator!=(Position2D o)
	{
		return x != o.x && y != o.y;
	}
};
struct Extent3D
{
	int width{0};
	int height{0};
	int depth{0};

	inline bool operator==(Extent3D o)
	{
		return width == o.width && height == o.height && depth == o.depth;
	}
	inline bool operator!=(Extent3D o)
	{
		return width != o.width || height != o.height || depth != o.depth;
	}
};

typedef enum DepthFuncType
{
	NEVER = GL_NEVER,
	LESS = GL_LESS,
	GREATER = GL_GREATER,
	EQUAL = GL_EQUAL,
	ALWAYS = GL_ALWAYS,
	LEQUAL = GL_LEQUAL,
	GEQUAL = GL_GEQUAL,
	NOTEQUAL = GL_NOTEQUAL
} DepthFuncType;

typedef enum BlendFuncType
{
	ZERO = GL_ZERO,
	ONE = GL_ONE,
	SRC_CLR = GL_SRC_COLOR,
	ONE_MINUS_SRC_CLR = GL_ONE_MINUS_SRC_COLOR,
	DST_CLR = GL_DST_COLOR,
	ONE_MINUS_DST_CLR = GL_ONE_MINUS_DST_COLOR,
	SRC_ALPHA = GL_SRC_ALPHA,
	ONE_MINUS_SRC_A = GL_ONE_MINUS_SRC_ALPHA
} BlendFuncType;

typedef enum BlendOperationType
{
	ADD = GL_FUNC_ADD,
	SUBSTRACT = GL_FUNC_SUBTRACT,
	REV_SUBSTRACT = GL_FUNC_REVERSE_SUBTRACT,
	MIN = GL_MIN,
	MAX = GL_MAX,
} BlendOperationType;

typedef enum TextureType
{
	TEXTURE_2D = GL_TEXTURE_2D,
	TEXTURE_2D_MULTISAMPLE = GL_TEXTURE_2D_MULTISAMPLE,
	TEXTURE_2D_ARRAY = GL_TEXTURE_2D_ARRAY,
	TEXTURE_2D_MULRISAMPLE_ARRAY = GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
	TEXTURE_3D = GL_TEXTURE_3D,
	TEXTURE_CUBEMAP = GL_TEXTURE_CUBE_MAP
} TextureType;

#endif