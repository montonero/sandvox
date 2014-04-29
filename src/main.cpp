#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

#include <stdlib.h>
#include <stdio.h>

#include "gfx/program.hpp"
#include "gfx/geometry.hpp"
#include "gfx/texture.hpp"
#include "gfx/image.hpp"

#include "scene/camera.hpp"

#include "ui/imgui.hpp"
#include "ui/font.hpp"
#include "ui/renderer.hpp"

#include "fs/folderwatcher.hpp"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"

#define DUAL 1

struct dual
{
    float v, dx, dy, dz;
    
    dual(float v, float dx, float dy, float dz): v(v), dx(dx), dy(dy), dz(dz) {}
    
    static dual c(float v) { return dual(v, 0, 0, 0); }
    static dual vx(float v) { return dual(v, 1, 0, 0); }
    static dual vy(float v) { return dual(v, 0, 1, 0); }
    static dual vz(float v) { return dual(v, 0, 0, 1); }
};

dual operator-(const dual& v) { return dual(-v.v, -v.dx, -v.dy, -v.dz); }

dual operator+(const dual& l, const dual& r) { return dual(l.v + r.v, l.dx + r.dx, l.dy + r.dy, l.dz + r.dz); }
dual operator-(const dual& l, const dual& r) { return dual(l.v - r.v, l.dx - r.dx, l.dy - r.dy, l.dz - r.dz); }

dual operator*(const dual& l, const dual& r)
{
    return dual(l.v * r.v, l.dx * r.v + l.v * r.dx, l.dy * r.v + l.v * r.dy, l.dz * r.v + l.v * r.dz);
}

dual operator*(const dual& l, float r) { return dual(l.v * r, l.dx * r, l.dy * r, l.dz * r); }
dual operator*(float l, const dual& r) { return dual(l * r.v, l * r.dx, l * r.dy, l * r.dz); }
dual operator/(const dual& l, float r) { return dual(l.v / r, l.dx / r, l.dy / r, l.dz / r); }

dual abs(const dual& v) { return v.v >= 0 ? v : -v; }

dual max(const dual& l, const dual& r) { return l.v >= r.v ? l : r; }
dual min(const dual& l, const dual& r) { return l.v <= r.v ? l : r; }

dual sin(const dual& v) { return dual(sinf(v.v), cosf(v.v)*v.dx, cosf(v.v)*v.dy, cosf(v.v)*v.dz); }
dual cos(const dual& v) { return dual(cosf(v.v), -sinf(v.v)*v.dx, -sinf(v.v)*v.dy, -sinf(v.v)*v.dz); }

dual sqrt(const dual& v) { return v.v > 0 ? dual(sqrtf(v.v), 0.5f/sqrtf(v.v)*v.dx, 0.5f/sqrtf(v.v)*v.dy, 0.5f/sqrtf(v.v)*v.dz) : dual::c(0); }

dual length(const dual& x, const dual& y) { return sqrt(x * x + y * y); }
dual length(const dual& x, const dual& y, const dual& z) { return sqrt(x * x + y * y + z * z); }

static const unsigned short MC_EDGETABLE[] =
{
    0x000, 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
    0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
    0x190, 0x099, 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
    0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
    0x230, 0x339, 0x033, 0x13a, 0x636, 0x73f, 0x435, 0x53c,
    0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
    0x3a0, 0x2a9, 0x1a3, 0x0aa, 0x7a6, 0x6af, 0x5a5, 0x4ac,
    0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
    0x460, 0x569, 0x663, 0x76a, 0x066, 0x16f, 0x265, 0x36c,
    0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
    0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0x0ff, 0x3f5, 0x2fc,
    0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
    0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x055, 0x15c,
    0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
    0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0x0cc,
    0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
    0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
    0x0cc, 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
    0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
    0x15c, 0x055, 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
    0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
    0x2fc, 0x3f5, 0x0ff, 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
    0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
    0x36c, 0x265, 0x16f, 0x066, 0x76a, 0x663, 0x569, 0x460,
    0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
    0x4ac, 0x5a5, 0x6af, 0x7a6, 0x0aa, 0x1a3, 0x2a9, 0x3a0,
    0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
    0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x033, 0x339, 0x230,
    0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
    0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x099, 0x190,
    0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
    0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x000
};

static const signed char MC_TRITABLE[][16] =
{
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
    {3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
    {3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
    {3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
    {9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
    {9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
    {2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
    {8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
    {9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
    {4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
    {3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
    {1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
    {4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
    {4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
    {5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
    {2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
    {9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
    {0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
    {2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
    {10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
    {5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
    {5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
    {9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
    {0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
    {1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
    {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
    {8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
    {2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
    {7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
    {2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
    {11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
    {5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
    {11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
    {11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
    {1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
    {9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
    {5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
    {2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
    {5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
    {6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
    {3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
    {6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
    {5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
    {1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
    {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
    {6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
    {8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
    {7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
    {3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
    {5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
    {0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
    {9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
    {8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
    {5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
    {0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
    {6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
    {10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
    {10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
    {8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
    {1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
    {0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
    {10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
    {3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
    {6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
    {9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
    {8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
    {3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
    {6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
    {0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
    {10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
    {10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
    {2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
    {7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
    {7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
    {2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
    {1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
    {11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
    {8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
    {0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
    {7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
    {10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
    {2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
    {6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
    {7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
    {2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
    {1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
    {10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
    {10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
    {0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
    {7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
    {6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
    {8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
    {9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
    {6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
    {4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
    {10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
    {8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
    {0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
    {1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
    {8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
    {10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
    {4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
    {10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
    {5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
    {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
    {9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
    {6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
    {7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
    {3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
    {7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
    {3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
    {6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
    {9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
    {1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
    {4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
    {7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
    {6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
    {3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
    {0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
    {6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
    {0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
    {11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
    {6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
    {5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
    {9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
    {1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
    {1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
    {10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
    {0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
    {5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
    {10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
    {11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
    {9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
    {7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
    {2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
    {8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
    {9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
    {9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
    {1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
    {9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
    {9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
    {5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
    {0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
    {10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
    {2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
    {0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
    {0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
    {9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
    {5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
    {3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
    {5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
    {8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
    {0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
    {9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
    {1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
    {3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
    {4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
    {9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
    {11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
    {11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
    {2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
    {9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
    {3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
    {1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
    {4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
    {3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
    {0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
    {9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
    {1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
};

static const unsigned char MC_VERTINDEX[][3] =
{
    {0, 0, 0},
    {1, 0, 0},
    {1, 1, 0},
    {0, 1, 0},
    {0, 0, 1},
    {1, 0, 1},
    {1, 1, 1},
    {0, 1, 1},
};

static const unsigned char MC_EDGEINDEX[][2] =
{
    {0, 1},
    {1, 2},
    {2, 3},
    {3, 0},
    {4, 5},
    {5, 6},
    {6, 7},
    {7, 4},
    {0, 4},
    {1, 5},
    {2, 6},
    {3, 7}
};

struct GridVertex
{
    float iso;
    float nx, ny, nz;
};

struct TerrainVertex
{
    vec3 position;
    vec3 normal;
};

#if DUAL
template <typename F>
GridVertex evaluateSDF(const F& f, const vec3& p)
{
    dual d = f(dual::vx(p.x), dual::vy(p.y), dual::vz(p.z));
    
    return { d.v, d.dx, d.dy, d.dz };
}
#else
template <typename F>
GridVertex evaluateSDF(const F& f, const vec3& p)
{
    float grad = 0.01f;
    
    float iso = f(p);
    float nx = f(p + vec3(grad, 0, 0)) - iso;
    float ny = f(p + vec3(0, grad, 0)) - iso;
    float nz = f(p + vec3(0, 0, grad)) - iso;
    
    return { iso, nx, ny, nz };
}
#endif

struct MeshPhysicsGeometry: btTriangleIndexVertexArray, noncopyable
{
    vector<vec3> vertices;
    vector<unsigned int> indices;
    
    MeshPhysicsGeometry(const vector<TerrainVertex>& vb, const vector<unsigned int>& ib)
    {
        vertices.reserve(vb.size());
        for (auto& v: vb) vertices.push_back(v.position);
        
        indices = ib;
        
        btIndexedMesh mesh;
        mesh.m_numTriangles = ib.size() / 3;
        mesh.m_triangleIndexBase = reinterpret_cast<const unsigned char*>(indices.data());
        mesh.m_triangleIndexStride = 3 * sizeof(unsigned int);
        mesh.m_numVertices = vb.size();
        mesh.m_vertexBase = reinterpret_cast<const unsigned char*>(vertices.data());
        mesh.m_vertexStride = sizeof(vec3);

        addIndexedMesh(mesh);
    }
};

struct Mesh
{
    unique_ptr<Geometry> geometry;
    unsigned int geometryIndices;
    
    unique_ptr<MeshPhysicsGeometry> physicsGeometry;
    unique_ptr<btCollisionShape> physicsShape;
    
    static Mesh create(const vector<TerrainVertex>& vb, const vector<unsigned int>& ib)
    {
        if (ib.empty())
            return Mesh();
        
        shared_ptr<Buffer> gvb = make_shared<Buffer>(Buffer::Type_Vertex, sizeof(TerrainVertex), vb.size(), Buffer::Usage_Static);
        shared_ptr<Buffer> gib = make_shared<Buffer>(Buffer::Type_Index, sizeof(unsigned int), ib.size(), Buffer::Usage_Static);
    
        gvb->upload(0, vb.data(), vb.size() * sizeof(TerrainVertex));
        gib->upload(0, ib.data(), ib.size() * sizeof(unsigned int));
        
        vector<Geometry::Element> layout =
        {
            Geometry::Element(offsetof(TerrainVertex, position), Geometry::Format_Float3),
            Geometry::Element(offsetof(TerrainVertex, normal), Geometry::Format_Float3),
        };
        
        auto physicsGeometry = make_unique<MeshPhysicsGeometry>(vb, ib);
        
        unique_ptr<btCollisionShape> physicsShape(new btBvhTriangleMeshShape(physicsGeometry.get(), true));
        
        return Mesh { make_unique<Geometry>(layout, gvb, gib), static_cast<unsigned int>(ib.size()), move(physicsGeometry), move(physicsShape) };
    }
};

namespace MarchingCubes
{
    template <int Lod>
    struct CubeGenerator
    {
        static void generate(
            vector<TerrainVertex>& vb, vector<unsigned int>& ib,
            const GridVertex& v000, const GridVertex& v100, const GridVertex& v110, const GridVertex& v010,
            const GridVertex& v001, const GridVertex& v101, const GridVertex& v111, const GridVertex& v011,
            float isolevel, const vec3& offset, float scale)
        {
            // get cube index from grid values
            int cubeindex = 0;
            
            if (v000.iso < isolevel) cubeindex |= 1 << 0;
            if (v100.iso < isolevel) cubeindex |= 1 << 1;
            if (v110.iso < isolevel) cubeindex |= 1 << 2;
            if (v010.iso < isolevel) cubeindex |= 1 << 3;
            if (v001.iso < isolevel) cubeindex |= 1 << 4;
            if (v101.iso < isolevel) cubeindex |= 1 << 5;
            if (v111.iso < isolevel) cubeindex |= 1 << 6;
            if (v011.iso < isolevel) cubeindex |= 1 << 7;
            
            int edgemask = MC_EDGETABLE[cubeindex];
            
            if (edgemask != 0)
            {
                const GridVertex* grid[2][2][2] =
                {
                    {
                        {&v000, &v100},
                        {&v010, &v110},
                    },
                    {
                        {&v001, &v101},
                        {&v011, &v111},
                    }
                };
                
                // tesselate 2x2x2 grid into 3x3x3 grid & recurse
                GridVertex tgrid[3][3][3];
                
                for (int z = 0; z < 3; ++z)
                    for (int y = 0; y < 3; ++y)
                        for (int x = 0; x < 3; ++x)
                        {
                            int x0 = x >> 1;
                            int x1 = x0 + (x & 1);
                            int y0 = y >> 1;
                            int y1 = y0 + (y & 1);
                            int z0 = z >> 1;
                            int z1 = z0 + (z & 1);
                            
                            tgrid[z][y][x].iso = (
                                                  (*grid[z0][y0][x0]).iso + (*grid[z0][y0][x1]).iso + (*grid[z0][y1][x0]).iso + (*grid[z0][y1][x1]).iso +
                                                  (*grid[z1][y0][x0]).iso + (*grid[z1][y0][x1]).iso + (*grid[z1][y1][x0]).iso + (*grid[z1][y1][x1]).iso) / 8;
                            
                            tgrid[z][y][x].nx = (*grid[z0][y0][x0]).nx;
                            tgrid[z][y][x].ny = (*grid[z0][y0][x0]).ny;
                            tgrid[z][y][x].nz = (*grid[z0][y0][x0]).nz;
                        }
                
                for (int z = 0; z < 2; ++z)
                    for (int y = 0; y < 2; ++y)
                        for (int x = 0; x < 2; ++x)
                        {
                            CubeGenerator<Lod-1>::generate(vb, ib,
                                                           tgrid[z+0][y+0][x+0],
                                                           tgrid[z+0][y+0][x+1],
                                                           tgrid[z+0][y+1][x+1],
                                                           tgrid[z+0][y+1][x+0],
                                                           tgrid[z+1][y+0][x+0],
                                                           tgrid[z+1][y+0][x+1],
                                                           tgrid[z+1][y+1][x+1],
                                                           tgrid[z+1][y+1][x+0],
                                                           isolevel,
                                                           offset + vec3(x, y, z) * (scale / 2),
                                                           scale / 2);
                        }
            }
        }
    };
    
    template <>
    struct CubeGenerator<0>
    {
        static void generate(
            vector<TerrainVertex>& vb, vector<unsigned int>& ib,
            const GridVertex& v000, const GridVertex& v100, const GridVertex& v110, const GridVertex& v010,
            const GridVertex& v001, const GridVertex& v101, const GridVertex& v111, const GridVertex& v011,
            float isolevel, const vec3& offset, float scale)
        {
            // get cube index from grid values
            int cubeindex = 0;
            
            if (v000.iso < isolevel) cubeindex |= 1 << 0;
            if (v100.iso < isolevel) cubeindex |= 1 << 1;
            if (v110.iso < isolevel) cubeindex |= 1 << 2;
            if (v010.iso < isolevel) cubeindex |= 1 << 3;
            if (v001.iso < isolevel) cubeindex |= 1 << 4;
            if (v101.iso < isolevel) cubeindex |= 1 << 5;
            if (v111.iso < isolevel) cubeindex |= 1 << 6;
            if (v011.iso < isolevel) cubeindex |= 1 << 7;
            
            int edgemask = MC_EDGETABLE[cubeindex];
            
            if (edgemask != 0)
            {
                const GridVertex* grid[2][2][2] =
                {
                    {
                        {&v000, &v100},
                        {&v010, &v110},
                    },
                    {
                        {&v001, &v101},
                        {&v011, &v111},
                    }
                };
                
                int edges[12];
                
                // add vertices
                for (int i = 0; i < 12; ++i)
                {
                    if (edgemask & (1 << i))
                    {
                        edges[i] = vb.size();
                        
                        int e0 = MC_EDGEINDEX[i][0];
                        int e1 = MC_EDGEINDEX[i][1];
                        int p0x = MC_VERTINDEX[e0][0];
                        int p0y = MC_VERTINDEX[e0][1];
                        int p0z = MC_VERTINDEX[e0][2];
                        int p1x = MC_VERTINDEX[e1][0];
                        int p1y = MC_VERTINDEX[e1][1];
                        int p1z = MC_VERTINDEX[e1][2];
                        
                        const GridVertex& g0 = *grid[p0z][p0y][p0x];
                        const GridVertex& g1 = *grid[p1z][p1y][p1x];
                        
                        float t =
                        (fabsf(g0.iso - g1.iso) > 0.0001)
                        ? (isolevel - g0.iso) / (g1.iso - g0.iso)
                        : 0;
                        
                        float px = p0x + (p1x - p0x) * t;
                        float py = p0y + (p1y - p0y) * t;
                        float pz = p0z + (p1z - p0z) * t;
                        
                        float nx = g0.nx + (g1.nx - g0.nx) * t;
                        float ny = g0.ny + (g1.ny - g0.ny) * t;
                        float nz = g0.nz + (g1.nz - g0.nz) * t;
                        
                        vb.push_back({ vec3(px, py, pz) * scale + offset, glm::normalize(vec3(nx, ny, nz)) });
                    }
                }
                
                // add indices
                for (int i = 0; i < 15; i += 3)
                {
                    if (MC_TRITABLE[cubeindex][i] < 0)
                        break;
                    
                    ib.push_back(edges[MC_TRITABLE[cubeindex][i+0]]);
                    ib.push_back(edges[MC_TRITABLE[cubeindex][i+2]]);
                    ib.push_back(edges[MC_TRITABLE[cubeindex][i+1]]);
                }
            }
        }
    };
    
    template <int Lod, typename F>
    Mesh generateSDF(const F& f, float isolevel, const vec3& min, const vec3& max, float cubesize)
    {
        vector<TerrainVertex> vb;
        vector<unsigned int> ib;
        
        int sizeX = ceil((max.x - min.x) / cubesize);
        int sizeY = ceil((max.y - min.y) / cubesize);
        int sizeZ = ceil((max.z - min.z) / cubesize);
        
        assert(sizeX > 0 && sizeY > 0 && sizeZ > 0);
        
        unique_ptr<GridVertex[]> grid(new GridVertex[sizeX * sizeY * sizeZ]);
        
        for (int z = 0; z < sizeZ; ++z)
            for (int y = 0; y < sizeY; ++y)
                for (int x = 0; x < sizeX; ++x)
                    grid[x + sizeX * (y + sizeY * z)] = evaluateSDF(f, min + vec3(x, y, z) * cubesize);
        
        for (int z = 0; z + 1 < sizeZ; ++z)
            for (int y = 0; y + 1 < sizeY; ++y)
                for (int x = 0; x + 1 < sizeX; ++x)
                {
                    const GridVertex& v000 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))];
                    const GridVertex& v100 = grid[(x + 1) + sizeX * ((y + 0) + sizeY * (z + 0))];
                    const GridVertex& v110 = grid[(x + 1) + sizeX * ((y + 1) + sizeY * (z + 0))];
                    const GridVertex& v010 = grid[(x + 0) + sizeX * ((y + 1) + sizeY * (z + 0))];
                    const GridVertex& v001 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 1))];
                    const GridVertex& v101 = grid[(x + 1) + sizeX * ((y + 0) + sizeY * (z + 1))];
                    const GridVertex& v111 = grid[(x + 1) + sizeX * ((y + 1) + sizeY * (z + 1))];
                    const GridVertex& v011 = grid[(x + 0) + sizeX * ((y + 1) + sizeY * (z + 1))];
                    
                    CubeGenerator<Lod>::generate(vb, ib, v000, v100, v110, v010, v001, v101, v111, v011, isolevel, min + vec3(x, y, z) * cubesize, cubesize);
                }
        
        return Mesh::create(vb, ib);
    }
}

namespace SurfaceNets
{
    template <typename F> struct NaiveTraits
    {
        static pair<vec3, vec3> intersect(const GridVertex& g0, const GridVertex& g1, const F& f, float isolevel, const vec3& corner, const vec3& v0, const vec3& v1)
        {
            float t =
                (fabsf(g0.iso - g1.iso) > 0.0001)
                ? (isolevel - g0.iso) / (g1.iso - g0.iso)
                : 0;
            
            return make_pair(glm::mix(v0, v1, t), glm::mix(vec3(g0.nx, g0.ny, g0.nz), vec3(g1.nx, g1.ny, g1.nz), t));
        }
        
        static pair<vec3, vec3> average(const pair<vec3, vec3>* points, size_t count, const vec3& v0, const vec3& v1, const vec3& corner)
        {
            vec3 position;
            vec3 normal;
            
            for (size_t i = 0; i < count; ++i)
            {
                position += points[i].first;
                normal += points[i].second;
            }
            
            float n = 1.f / count;
            
            return make_pair(position * n, normal * n);
        }
    };
    
    template <typename LerpK> struct AdjustableNaiveTraits
    {
        template <typename F> struct Value
        {
            static pair<vec3, vec3> intersect(const GridVertex& g0, const GridVertex& g1, const F& f, float isolevel, const vec3& corner, const vec3& v0, const vec3& v1)
            {
                float t =
                    (fabsf(g0.iso - g1.iso) > 0.0001)
                    ? (isolevel - g0.iso) / (g1.iso - g0.iso)
                    : 0;
                
                return make_pair(glm::mix(v0, v1, t), glm::normalize(glm::mix(vec3(g0.nx, g0.ny, g0.nz), vec3(g1.nx, g1.ny, g1.nz), t)));
            }
            
            static pair<vec3, vec3> average(const pair<vec3, vec3>* points, size_t count, const vec3& v0, const vec3& v1, const vec3& corner)
            {
                vec3 position;
                vec3 normal;
                
                for (size_t i = 0; i < count; ++i)
                {
                    position += points[i].first;
                    normal += points[i].second;
                }
                
                float n = 1.f / count;
                
                return make_pair(LerpK()(corner, position * n, (v0 + v1) / 2.f), normal * n);
            }
        };
    };
    
    template <typename F> struct DualContouringTraits
    {
        static pair<vec3, vec3> intersect(const GridVertex& g0, const GridVertex& g1, const F& f, float isolevel, const vec3& corner, const vec3& v0, const vec3& v1)
        {
            if (g0.iso > g1.iso)
                return intersect(g1, g0, f, isolevel, corner, v1, v0);
            
            float mint = 0;
            float miniso = g0.iso;
            float maxt = 1;
            float maxiso = g1.iso;
            
            for (int i = 0; i < 10; ++i)
            {
                float t = (isolevel - miniso) / (maxiso - miniso) * (maxt - mint) + mint;
                float iso = evaluateSDF(f, glm::mix(v0, v1, t) + corner).iso;
                
                if (iso < isolevel)
                    mint = t, miniso = iso;
                else
                    maxt = t, maxiso = iso;
            }
            
            float t = (mint + maxt) / 2;
            
            GridVertex g = evaluateSDF(f, glm::mix(v0, v1, t) + corner);
            
            return make_pair(glm::mix(v0, v1, t), glm::normalize(vec3(g.nx, g.ny, g.nz)));
        }
        
        static vec3 cg(const mat3& A, const vec3& B, const vec3& x0)
        {
            vec3 r = B - A * x0;
            vec3 p = r;
            vec3 x = x0;
            
            for (int i = 0; i < 3; ++i)
            {
                vec3 Ap = A * p;
                
                float pAp = glm::dot(p, Ap);
                if (fabsf(pAp) < 1e-3f) break;
                
                float rr = glm::dot(r, r);
                if (fabsf(rr) < 1e-3f) break;
                
                float alpha = rr / pAp;
                
                vec3 xn = x + alpha * p;
                vec3 rn = r - alpha * Ap;
                
                float beta = glm::dot(rn, rn) / rr;
                
                vec3 pn = rn + beta * p;
                
                r = rn;
                p = pn;
                x = xn;
            }
            
            return x;
        }
        
        static pair<vec3, vec3> average(const pair<vec3, vec3>* points, size_t count, const vec3& v0, const vec3& v1, const vec3& corner)
        {
            pair<vec3, vec3> avg = NaiveTraits<F>::average(points, count, v0, v1, corner);
            
            float ata[6] = {}, atb[3] = {};

            for (size_t i = 0; i < count; ++i)
            {
                const vec3& p = points[i].first;
                const vec3& n = points[i].second;

                ata[0] += n.x * n.x;
                ata[1] += n.x * n.y;
                ata[2] += n.x * n.z;
                ata[3] += n.y * n.y;
                ata[4] += n.y * n.z;
                ata[5] += n.z * n.z;
                
                float pn = glm::dot(p, n);
                
                atb[0] += n.x * pn;
                atb[1] += n.y * pn;
                atb[2] += n.z * pn;
            }
            
            vec3 mp = cg(mat3(ata[0], ata[1], ata[2], ata[1], ata[3], ata[4], ata[2], ata[4], ata[5]), vec3(atb[0], atb[1], atb[2]), avg.first);
            
            return make_pair(mp, avg.second);
        }
    };
    
    TerrainVertex normalLerp(const TerrainVertex& v, const vec3& avgp, const vec3& qn)
    {
        float k = glm::dot(v.position - avgp, v.position - avgp);
        
        k = 1 - (1 - k) * (1 - k);
        
        return TerrainVertex {v.position, glm::normalize(glm::mix(v.normal, qn, glm::clamp(k, 0.f, 1.f)))};
    }
    
    float norm(const vec3& v)
    {
        return v.x * v.x + v.y * v.y + v.z * v.z;
    }
    
    vec3 getQuadNormal(const vec3& v0, const vec3& v1, const vec3& v2, const vec3& v3)
    {
        float v01 = norm(v0 - v1);
        float v12 = norm(v1 - v2);
        float v23 = norm(v2 - v3);
        float v30 = norm(v3 - v0);
        
        float v012 = v01 * v12;
        float v123 = v12 * v23;
        float v230 = v23 * v30;
        float v301 = v30 * v01;
        
        if (v012 > v123 && v012 > v230 && v012 > v301)
        {
            return glm::normalize(glm::cross(v1 - v0, v2 - v0));
        }
        else if (v123 > v230 && v123 > v301)
        {
            return glm::normalize(glm::cross(v2 - v1, v3 - v1));
        }
        else if (v230 > v301)
        {
            return glm::normalize(glm::cross(v3 - v2, v0 - v2));
        }
        else
        {
            return glm::normalize(glm::cross(v0 - v3, v1 - v3));
        }
    }
    
    void pushQuad(vector<TerrainVertex>& vb, vector<unsigned int>& ib,
        const pair<vec3, TerrainVertex>& v0, const pair<vec3, TerrainVertex>& v1, const pair<vec3, TerrainVertex>& v2, const pair<vec3, TerrainVertex>& v3,
        bool flip)
    {
        size_t offset = vb.size();
        
        vec3 qn = (flip ? -1.f : 1.f) * getQuadNormal(v0.second.position, v1.second.position, v2.second.position, v3.second.position);
        
        vb.push_back(normalLerp(v0.second, v0.first, qn));
        vb.push_back(normalLerp(v1.second, v1.first, qn));
        vb.push_back(normalLerp(v2.second, v2.first, qn));
        vb.push_back(normalLerp(v3.second, v3.first, qn));
        
        if (flip)
        {
            ib.push_back(offset + 0);
            ib.push_back(offset + 2);
            ib.push_back(offset + 1);
            ib.push_back(offset + 0);
            ib.push_back(offset + 3);
            ib.push_back(offset + 2);
        }
        else
        {
            ib.push_back(offset + 0);
            ib.push_back(offset + 1);
            ib.push_back(offset + 2);
            ib.push_back(offset + 0);
            ib.push_back(offset + 2);
            ib.push_back(offset + 3);
        }
    }
    
    template <template <typename> class Traits, typename F>
    Mesh generateSDF(const F& f, float isolevel, const vec3& min, const vec3& max, float cubesize)
    {
        vector<TerrainVertex> vb;
        vector<unsigned int> ib;
        
        int sizeX = ceil((max.x - min.x) / cubesize);
        int sizeY = ceil((max.y - min.y) / cubesize);
        int sizeZ = ceil((max.z - min.z) / cubesize);
        
        assert(sizeX > 0 && sizeY > 0 && sizeZ > 0);
        
        unique_ptr<GridVertex[]> grid(new GridVertex[sizeX * sizeY * sizeZ]);
        
        for (int z = 0; z < sizeZ; ++z)
            for (int y = 0; y < sizeY; ++y)
                for (int x = 0; x < sizeX; ++x)
                    grid[x + sizeX * (y + sizeY * z)] = evaluateSDF(f, min + vec3(x, y, z) * cubesize);
        
        unique_ptr<pair<vec3, TerrainVertex>[]> gv(new pair<vec3, TerrainVertex>[sizeX * sizeY * sizeZ]);
        
        for (int z = 0; z + 1 < sizeZ; ++z)
            for (int y = 0; y + 1 < sizeY; ++y)
                for (int x = 0; x + 1 < sizeX; ++x)
                {
                    const GridVertex& v000 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))];
                    const GridVertex& v100 = grid[(x + 1) + sizeX * ((y + 0) + sizeY * (z + 0))];
                    const GridVertex& v110 = grid[(x + 1) + sizeX * ((y + 1) + sizeY * (z + 0))];
                    const GridVertex& v010 = grid[(x + 0) + sizeX * ((y + 1) + sizeY * (z + 0))];
                    const GridVertex& v001 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 1))];
                    const GridVertex& v101 = grid[(x + 1) + sizeX * ((y + 0) + sizeY * (z + 1))];
                    const GridVertex& v111 = grid[(x + 1) + sizeX * ((y + 1) + sizeY * (z + 1))];
                    const GridVertex& v011 = grid[(x + 0) + sizeX * ((y + 1) + sizeY * (z + 1))];
                    
                    // get cube index from grid values
                    int cubeindex = 0;
                    
                    if (v000.iso < isolevel) cubeindex |= 1 << 0;
                    if (v100.iso < isolevel) cubeindex |= 1 << 1;
                    if (v110.iso < isolevel) cubeindex |= 1 << 2;
                    if (v010.iso < isolevel) cubeindex |= 1 << 3;
                    if (v001.iso < isolevel) cubeindex |= 1 << 4;
                    if (v101.iso < isolevel) cubeindex |= 1 << 5;
                    if (v111.iso < isolevel) cubeindex |= 1 << 6;
                    if (v011.iso < isolevel) cubeindex |= 1 << 7;
                    
                    int edgemask = MC_EDGETABLE[cubeindex];
                    
                    if (edgemask != 0)
                    {
                        vec3 corner = vec3(x, y, z) * cubesize + min;
                        
                        pair<vec3, vec3> ev[12];
                        size_t ecount = 0;
                        vec3 evavg;
                        
                        // add vertices
                        for (int i = 0; i < 12; ++i)
                        {
                            if (edgemask & (1 << i))
                            {
                                int e0 = MC_EDGEINDEX[i][0];
                                int e1 = MC_EDGEINDEX[i][1];
                                int p0x = MC_VERTINDEX[e0][0];
                                int p0y = MC_VERTINDEX[e0][1];
                                int p0z = MC_VERTINDEX[e0][2];
                                int p1x = MC_VERTINDEX[e1][0];
                                int p1y = MC_VERTINDEX[e1][1];
                                int p1z = MC_VERTINDEX[e1][2];
                                
                                const GridVertex& g0 = grid[(x + p0x) + sizeX * ((y + p0y) + sizeY * (z + p0z))];
                                const GridVertex& g1 = grid[(x + p1x) + sizeX * ((y + p1y) + sizeY * (z + p1z))];
                                
                                pair<vec3, vec3> gt = Traits<F>::intersect(g0, g1, f, isolevel, corner, vec3(p0x, p0y, p0z) * cubesize, vec3(p1x, p1y, p1z) * cubesize);
                                
                                ev[ecount++] = gt;
                                evavg += gt.first;
                            }
                        }
                        
                        pair<vec3, vec3> ga = Traits<F>::average(ev, ecount, vec3(), vec3(cubesize), corner);
                        
                        gv[x + sizeX * (y + sizeY * z)] = make_pair(corner + evavg / float(ecount), TerrainVertex { corner + ga.first, glm::normalize(ga.second) });
                    }
                }
        
        for (int z = 1; z + 1 < sizeZ; ++z)
            for (int y = 1; y + 1 < sizeY; ++y)
                for (int x = 1; x + 1 < sizeX; ++x)
                {
                    const GridVertex& v000 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))];
                    const GridVertex& v100 = grid[(x + 1) + sizeX * ((y + 0) + sizeY * (z + 0))];
                    const GridVertex& v010 = grid[(x + 0) + sizeX * ((y + 1) + sizeY * (z + 0))];
                    const GridVertex& v001 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 1))];
                    
                    // add quads
                    if ((v000.iso < isolevel) != (v100.iso < isolevel))
                    {
                        pushQuad(vb, ib,
                            gv[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))],
                            gv[(x + 0) + sizeX * ((y - 1) + sizeY * (z + 0))],
                            gv[(x + 0) + sizeX * ((y - 1) + sizeY * (z - 1))],
                            gv[(x + 0) + sizeX * ((y + 0) + sizeY * (z - 1))],
                            !(v000.iso < isolevel));
                    }
                    
                    if ((v000.iso < isolevel) != (v010.iso < isolevel))
                    {
                        pushQuad(vb, ib,
                            gv[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))],
                            gv[(x - 1) + sizeX * ((y + 0) + sizeY * (z + 0))],
                            gv[(x - 1) + sizeX * ((y + 0) + sizeY * (z - 1))],
                            gv[(x + 0) + sizeX * ((y + 0) + sizeY * (z - 1))],
                            v000.iso < isolevel);
                    }
                    
                    if ((v000.iso < isolevel) != (v001.iso < isolevel))
                    {
                        pushQuad(vb, ib,
                            gv[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))],
                            gv[(x - 1) + sizeX * ((y + 0) + sizeY * (z + 0))],
                            gv[(x - 1) + sizeX * ((y - 1) + sizeY * (z + 0))],
                            gv[(x + 0) + sizeX * ((y - 1) + sizeY * (z + 0))],
                            !(v000.iso < isolevel));
                    }
                }
        
        return Mesh::create(vb, ib);
    }
}

namespace World
{
#if DUAL
    template <typename T>
    auto mktransform(T t, const mat4& xf)
    {
        mat4 xfi = glm::inverse(xf);
        
        return [=](const dual& px, const dual& py, const dual& pz)
        {
            dual ptx = px * xfi[0][0] + py * xfi[1][0] + pz * xfi[2][0] + dual::c(xfi[3][0]);
            dual pty = px * xfi[0][1] + py * xfi[1][1] + pz * xfi[2][1] + dual::c(xfi[3][1]);
            dual ptz = px * xfi[0][2] + py * xfi[1][2] + pz * xfi[2][2] + dual::c(xfi[3][2]);
            
            return t(ptx, pty, ptz);
        };
    }
    
    template <typename T>
    auto mktranslate(T t, const vec3& v)
    {
        return mktransform(t, glm::translate(mat4(), v));
    }
    
    template <typename T>
    auto mkrotate(T t, float angle, const vec3& axis)
    {
        return mktransform(t, glm::rotate(mat4(), angle, axis));
    }
    
    auto mksphere(float radius)
    {
        return [=](const dual& px, const dual& py, const dual& pz)
        {
            return length(px, py, pz) - dual::c(radius);
        };
    }
    
    auto mkbox(float ex, float ey, float ez)
    {
        return [=](const dual& px, const dual& py, const dual& pz)
        {
            dual dx = abs(px) - dual::c(ex), dy = abs(py) - dual::c(ey), dz = abs(pz) - dual::c(ez);
            
            dual face = min(max(dx, max(dy, dz)), dual::c(0));
            dual edge = length(max(dx, dual::c(0)), max(dy, dual::c(0)), max(dz, dual::c(0)));
            
            return face + edge;
        };
    }
    
    auto mkcone(float radius, float height)
    {
        return [=](const dual& px, const dual& py, const dual& pz)
        {
            dual q = length(px, py);
            return pz.v <= 0
                ? length(pz, max(q - dual::c(radius), dual::c(0)))
                : pz.v > height
                    ? length(px, py, pz - dual::c(height))
                    : q - (dual::c(1) - pz / height) * radius;
        };
    }
    
    template <typename T, typename U>
    auto mkunion(T t, U u)
    {
        return [=](const dual& px, const dual& py, const dual& pz) { return min(t(px, py, pz), u(px, py, pz)); };
    }
    
    template <typename T, typename U>
    auto mksubtract(T t, U u)
    {
        return [=](const dual& px, const dual& py, const dual& pz) { return max(t(px, py, pz), -u(px, py, pz)); };
    }
    
    template <typename T, typename U>
    auto mkintersect(T t, U u)
    {
        return [=](const dual& px, const dual& py, const dual& pz) { return max(t(px, py, pz), u(px, py, pz)); };
    }
    
    template <typename T>
    auto mktwist(T t, float scale)
    {
        return [=](const dual& px, const dual& py, const dual& pz)
        {
            dual angle = -pz * scale;
            dual sina = sin(angle), cosa = cos(angle);
            
            dual prx = px * cosa + py * sina;
            dual pry = px * -sina + py * cosa;
            
            return t(prx, pry, pz);
        };
    }
#else
    template <typename T>
    auto mktransform(T t, const mat4& xf)
    {
        mat4 xfi = glm::inverse(xf);
        
        return [=](const vec3& p) { return t(vec3(xfi * vec4(p, 1))); };
    }
    
    template <typename T>
    auto mktranslate(T t, const vec3& v)
    {
        return mktransform(t, glm::translate(mat4(), v));
    }
    
    template <typename T>
    auto mkrotate(T t, float angle, const vec3& axis)
    {
        return mktransform(t, glm::rotate(mat4(), angle, axis));
    }
    
    auto mksphere(float radius)
    {
        return [=](const vec3& p) { return glm::length(p) - radius; };
    }
    
    auto mkbox(float ex, float ey, float ez)
    {
        return [=](const vec3& p) { vec3 d = glm::abs(p) - vec3(ex, ey, ez); return min(max(d.x, max(d.y, d.z)), 0.f) + glm::length(glm::max(d, 0.f)); };
    }
    
    auto mkcone(float radius, float height)
    {
        return [=](const vec3& p) { float q = glm::length(vec2(p.x, p.y)); return p.z <= 0 ? glm::length(vec2(p.z, max(q - radius, 0.f))) : p.z > height ? glm::distance(p, vec3(0, 0, height)) : q - (1 - p.z / height) * radius; };
    }
    
    template <typename T, typename U>
    auto mkunion(T t, U u)
    {
        return [=](const vec3& p) { return min(t(p), u(p)); };
    }
    
    template <typename T, typename U>
    auto mksubtract(T t, U u)
    {
        return [=](const vec3& p) { return max(t(p), -u(p)); };
    }
    
    template <typename T, typename U>
    auto mkintersect(T t, U u)
    {
        return [=](const vec3& p) { return max(t(p), u(p)); };
    }
    
    template <typename T>
    auto mktwist(T t, float scale)
    {
        return [=](const vec3& p) { return t(vec3(glm::rotate(mat4(), p.z * scale, vec3(0.f, 0.f, 1.f)) * vec4(p, 0))); };
    }
#endif
    
    auto mkworld()
    {
    #if 0
        return mkrotate(mkcone(2, 6), glm::radians(90.f), vec3(0, 1, 0));
    #else
        return
            mkunion(
                mkunion(
                    mkunion(
                        mkunion(
                            mksubtract(
                                mksubtract(
                                    mkunion(
                                        mkunion(
                                            mktranslate(mksphere(5), vec3(7, 0, 0)),
                                            mktranslate(mksphere(7), vec3(-7, 0, 0))),
                                        mkbox(10, 8, 1)),
                                    mktranslate(mkbox(2, 2, 2), vec3(0, 7.5f, 0))),
                                mktranslate(mkbox(2, 2, 2), vec3(0, -7, 0))),
                            mktranslate(mkrotate(mkcone(2, 6), glm::radians(90.f), vec3(0, 1, 0)), vec3(11, 0, 0))),
                        mktranslate(mkrotate(mkbox(3, 3, 1), glm::radians(45.f), vec3(0, 0, 1)), vec3(5, 8, -2))),
                    mktranslate(
                        mkintersect(
                            mkrotate(mkbox(6, 6, 6), glm::radians(45.f), glm::normalize(vec3(1, 1, 0))),
                            mkbox(6, 6, 6)), vec3(-30, 0, 0))),
            mktranslate(mktwist(mkbox(4, 4, 10), 1.f / 10.f), vec3(30, 0, 0)));
    #endif
    }
}

struct AdjustableLerpKConstant
{
    vec3 operator()(const vec3& corner, const vec3& smoothpt, const vec3& centerpt) const
    {
        return glm::mix(smoothpt, centerpt, 0.5f);
    }
};

struct AdjustableLerpKHeight
{
    vec3 operator()(const vec3& corner, const vec3& smoothpt, const vec3& centerpt) const
    {
        return glm::mix(smoothpt, centerpt, glm::smoothstep(2.f, 2.5f, corner.z));
    }
};

struct AdjustableLerpKRandom
{
    vec3 operator()(const vec3& corner, const vec3& smoothpt, const vec3& centerpt) const
    {
        return glm::clamp(glm::mix(smoothpt, centerpt + (vec3(rand() / float(RAND_MAX), rand() / float(RAND_MAX), rand() / float(RAND_MAX)) * 2.f - vec3(1.f)) * 0.3f, 0.5f), vec3(0.f), vec3(1.f));
    }
};

struct AdjustableLerpKRandomRight
{
    vec3 operator()(const vec3& corner, const vec3& smoothpt, const vec3& centerpt) const
    {
        return glm::mix(smoothpt, AdjustableLerpKRandom()(corner, smoothpt, centerpt), glm::smoothstep(3.f, 4.f, corner.x));
    }
};

struct AdjustableLerpKQuantize
{
    static float quantize(float value, float bound)
    {
        return floor(value / bound) * bound;
    }
    
    vec3 operator()(const vec3& corner, const vec3& smoothpt, const vec3& centerpt) const
    {
        float bound = 1.f / 3.f;
        
        return vec3(quantize(smoothpt.x, bound), quantize(smoothpt.y, bound), quantize(smoothpt.z, bound));
    }
};

Mesh generateWorld(int index)
{
    srand(42);
    
    float isolevel = 0.01f;
    vec3 min = vec3(-40, -16, -16);
    vec3 max = vec3(40, 16, 16);

    switch (index)
    {
    case 1:
        return MarchingCubes::generateSDF<0>(World::mkworld(), isolevel, min, max, 1);
        break;
        
    case 2:
        return SurfaceNets::generateSDF<SurfaceNets::NaiveTraits>(World::mkworld(), isolevel, min, max, 1);
        break;
        
    case 3:
        return SurfaceNets::generateSDF<SurfaceNets::AdjustableNaiveTraits<AdjustableLerpKConstant>::Value>(World::mkworld(), isolevel, min, max, 1);
        break;
        
    case 4:
        return SurfaceNets::generateSDF<SurfaceNets::AdjustableNaiveTraits<AdjustableLerpKHeight>::Value>(World::mkworld(), isolevel, min, max, 1);
        break;
        
    case 5:
        return SurfaceNets::generateSDF<SurfaceNets::AdjustableNaiveTraits<AdjustableLerpKRandom>::Value>(World::mkworld(), isolevel, min, max, 1);
        break;
        
    case 6:
        return SurfaceNets::generateSDF<SurfaceNets::AdjustableNaiveTraits<AdjustableLerpKRandomRight>::Value>(World::mkworld(), isolevel, min, max, 1);
        break;
        
    case 7:
        return SurfaceNets::generateSDF<SurfaceNets::AdjustableNaiveTraits<AdjustableLerpKQuantize>::Value>(World::mkworld(), isolevel, min, max, 1);
        break;
        
    case 8:
        return SurfaceNets::generateSDF<SurfaceNets::DualContouringTraits>(World::mkworld(), isolevel, min, max, 1);
        break;
        
    case 9:
        return SurfaceNets::generateSDF<SurfaceNets::DualContouringTraits>(World::mkworld(), isolevel, min, max, 1.f / 2.f);
        break;
        
    default:
        return Mesh();
    }
}

pair<unique_ptr<Geometry>, unsigned int> generateSphere(float radius)
{
    vector<TerrainVertex> vb;
    vector<unsigned int> ib;
    
    int U = 10, V = 20;
    
    for (int ui = 0; ui < U; ++ui)
        for (int vi = 0; vi < V; ++vi)
        {
            float u = float(ui) / (U - 1) * glm::pi<float>();
            float v = float(vi) / (V - 1) * 2 * glm::pi<float>();
        
            vec3 p = vec3(glm::cos(v) * glm::sin(u), glm::sin(v) * glm::sin(u), glm::cos(u));
            
            vb.push_back({p * radius, p});
        }
    
    for (int ui = 0; ui < U; ++ui)
    {
        int un = (ui + 1) % U;
        
        for (int vi = 0; vi < V; ++vi)
        {
            int vn = (vi + 1) % V;
            
            ib.push_back(ui * V + vi);
            ib.push_back(un * V + vi);
            ib.push_back(un * V + vn);
            
            ib.push_back(ui * V + vi);
            ib.push_back(un * V + vn);
            ib.push_back(ui * V + vn);
        }
    }
    
    shared_ptr<Buffer> gvb = make_shared<Buffer>(Buffer::Type_Vertex, sizeof(TerrainVertex), vb.size(), Buffer::Usage_Static);
    shared_ptr<Buffer> gib = make_shared<Buffer>(Buffer::Type_Index, sizeof(unsigned int), ib.size(), Buffer::Usage_Static);

    gvb->upload(0, vb.data(), vb.size() * sizeof(TerrainVertex));
    gib->upload(0, ib.data(), ib.size() * sizeof(unsigned int));
    
    vector<Geometry::Element> layout =
    {
        Geometry::Element(offsetof(TerrainVertex, position), Geometry::Format_Float3),
        Geometry::Element(offsetof(TerrainVertex, normal), Geometry::Format_Float3),
    };
    
    return make_pair(make_unique<Geometry>(layout, gvb, gib), ib.size());
}

bool wireframe = false;
Camera camera;
vec3 cameraAngles;
vec3 brushPosition;

bool keyDown[GLFW_KEY_LAST];
bool mouseDown[GLFW_MOUSE_BUTTON_LAST];
double mouseDeltaX, mouseDeltaY;
double mouseLastX, mouseLastY;

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
        keyDown[key] = true;
    
    if (action == GLFW_RELEASE)
        keyDown[key] = false;
    
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
        wireframe = !wireframe;
    
    if (action == GLFW_PRESS && (key >= GLFW_KEY_1 && key <= GLFW_KEY_9))
    {
        auto p = static_cast<Mesh*>(glfwGetWindowUserPointer(window));
        
        clock_t start = clock();
        *p = generateWorld(key - GLFW_KEY_1 + 1);
        clock_t end = clock();
        
        printf("Generated world %d (%d tri) in %.1f msec\n", key - GLFW_KEY_1, p->geometryIndices / 3, (end - start) * 1000.0 / CLOCKS_PER_SEC);
    }
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (action == GLFW_PRESS)
        mouseDown[button] = true;
    
    if (action == GLFW_RELEASE)
        mouseDown[button] = false;
}

static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    mouseDeltaX += (xpos - mouseLastX);
    mouseDeltaY += (ypos - mouseLastY);
    
    mouseLastX = xpos;
    mouseLastY = ypos;
}

static void errorCallback(int error, const char* description)
{
    fputs(description, stderr);
}

void drawtext(ui::Renderer& r, FontLibrary& fl, const char* font, int width, int height, int x, int y, float size, const char* text, unsigned int color)
{
    Font* f = fl.getFont(font);
    
    float sx = 1.f / width, sy = 1.f / height;
    float su = 1.f / fl.getTexture()->getWidth();
    float sv = 1.f / fl.getTexture()->getHeight();
    
    float xpos = roundf(x);
    float ypos = roundf(y);
    
    float scale = f->getScale(size);
    
    unsigned int lastch = 0;
    
    for (const char* s = text; *s; ++s)
    {
        auto metrics = f->getGlyphMetrics(*s);
        auto bitmap = f->getGlyphBitmap(scale, *s);
        
        if (metrics && bitmap)
        {
            xpos += roundf(f->getKerning(lastch, *s) * scale);
            
            float x0 = roundf(xpos + metrics->bearingX * scale);
            float y0 = roundf(ypos - metrics->bearingY * scale);
            float x1 = x0 + bitmap->w;
            float y1 = y0 + bitmap->h;
            
            float u0 = su * bitmap->x;
            float u1 = su * (bitmap->x + bitmap->w);
            float v0 = sv * bitmap->y;
            float v1 = sv * (bitmap->y + bitmap->h);
            
            r.push(vec2(x0 * sx * 2 - 1, 1 - y0 * sy * 2), vec2(u0, v0), color);
            r.push(vec2(x1 * sx * 2 - 1, 1 - y0 * sy * 2), vec2(u1, v0), color);
            r.push(vec2(x1 * sx * 2 - 1, 1 - y1 * sy * 2), vec2(u1, v1), color);
            
            r.push(vec2(x0 * sx * 2 - 1, 1 - y0 * sy * 2), vec2(u0, v0), color);
            r.push(vec2(x1 * sx * 2 - 1, 1 - y1 * sy * 2), vec2(u1, v1), color);
            r.push(vec2(x0 * sx * 2 - 1, 1 - y1 * sy * 2), vec2(u0, v1), color);
            
            xpos += roundf(metrics->advance * scale);
            
            lastch = *s;
        }
        else
        {
            lastch = 0;
        }
    }
    
    r.flush(fl.getTexture());
}

float getWindowDensity(GLFWwindow* window)
{
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    return height == 0 ? 1 : float(fbHeight) / height;
}

void dumpTexture(Texture* tex, const string& path)
{
    Image image(Texture::Type_2D, tex->getFormat(), tex->getWidth(), tex->getHeight(), 1, 1);
    
    tex->download(0, 0, 0, image.getData(0, 0, 0), Texture::getImageSize(tex->getFormat(), tex->getWidth(), tex->getHeight()));
    
    image.saveToPNG(path);
}

int main()
{
    glfwSetErrorCallback(errorCallback);
    
    if (!glfwInit())
        exit(EXIT_FAILURE);
    
    glfwWindowHint(GLFW_VISIBLE, false);
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
 
    GLFWwindow* window = glfwCreateWindow(1280, 720, "sandvox", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    
    glfwMakeContextCurrent(window);
    
    glfwShowWindow(window);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    
    printf("Version: %s\n", glGetString(GL_VERSION));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));
    
    FolderWatcher fw("../..");
    ProgramManager pm("../../src/shaders", &fw);
    TextureManager tm("../../data", &fw);
    
    FontLibrary fonts(512, 512);
    fonts.addFont("sans", "../../data/Roboto-Regular.ttf");
    
    auto chunk = generateWorld(2);
    
    auto brushGeom = generateSphere(0.1);
    
    camera.setView(vec3(0.5f, 35.f, 42.f), quat(cameraAngles = vec3(0.f, 0.83f, 4.683f)));
    
    glfwSetWindowUserPointer(window, &chunk);
    
    glfwGetCursorPos(window, &mouseLastX, &mouseLastY);
    
    clock_t frameStamp = clock();
    
	btDefaultCollisionConfiguration collisionConfiguration;
	btCollisionDispatcher collisionDispatcher(&collisionConfiguration);
	btDbvtBroadphase broadphase;
	btSequentialImpulseConstraintSolver solver;
	btDiscreteDynamicsWorld dynamicsWorld(&collisionDispatcher, &broadphase, &solver, &collisionConfiguration);
	
	dynamicsWorld.setGravity(btVector3(0,-10,0));

    if (false)
    {
        btTransform groundTransform;
        groundTransform.setIdentity();

        btDefaultMotionState* motionState = new btDefaultMotionState(groundTransform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(0, motionState, chunk.physicsShape.get(), btVector3(0, 0, 0));
        
        btRigidBody* chunkBody = new btRigidBody(rbInfo);

        dynamicsWorld.addRigidBody(chunkBody);
    }
    
    ui::Renderer uir(1024, pm.get("ui-vs", "ui-fs"));
    
    while (!glfwWindowShouldClose(window))
    {
        clock_t frameStampNew = clock();
        double frameTime = (frameStampNew - frameStamp) / double(CLOCKS_PER_SEC);
        frameStamp = frameStampNew;
        
        dynamicsWorld.stepSimulation(frameTime);
        
        glfwPollEvents();
        
        {
            vec3 ccposition = camera.getPosition();
            quat ccorientation = camera.getOrientation();
            
            vec3 offset;
            
            if (keyDown[GLFW_KEY_W] || keyDown[GLFW_KEY_UP])
                offset.x += 1;
            
            if (keyDown[GLFW_KEY_S] || keyDown[GLFW_KEY_DOWN])
                offset.x -= 1;
            
            if (keyDown[GLFW_KEY_A] || keyDown[GLFW_KEY_LEFT])
                offset.y += 1;
            
            if (keyDown[GLFW_KEY_D] || keyDown[GLFW_KEY_RIGHT])
                offset.y -= 1;
            
            float moveAmount = 2000 * frameTime;
            
            ccposition += (ccorientation * offset) * moveAmount;
            
            if (mouseDown[GLFW_MOUSE_BUTTON_RIGHT])
            {
                float rotateAmount = glm::radians(400.f) * frameTime;
            
                cameraAngles += vec3(0.f, rotateAmount * mouseDeltaY, -rotateAmount * mouseDeltaX);
            }
                
            ccorientation = quat(cameraAngles);
            
            camera.setView(ccposition, ccorientation);
        }
        
        mouseDeltaX = 0;
        mouseDeltaY = 0;
    
        fw.processChanges();
        
        float density = getWindowDensity(window);
    
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        
        camera.setProjection(float(width) / height, glm::radians(60.f), 0.1f, 1000.f);
        
        mat4 viewproj = camera.getViewProjectionMatrix();
        
        if (chunk.physicsShape)
        {
            int width, height;
            glfwGetWindowSize(window, &width, &height);
        
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            
            vec4 clip0(xpos / width * 2 - 1, 1 - ypos / height * 2, 0, 1);
            vec4 clip1(xpos / width * 2 - 1, 1 - ypos / height * 2, 1, 1);
            
            vec4 pw0 = glm::inverse(viewproj) * clip0;
            vec4 pw1 = glm::inverse(viewproj) * clip1;
            
            vec3 w0(pw0 / pw0.w);
            vec3 w1(pw1 / pw1.w);
            
            btDynamicsWorld::ClosestRayResultCallback callback(btVector3(w0.x, w0.y, w0.z), btVector3(w1.x, w1.y, w1.z));
            
            dynamicsWorld.rayTest(callback.m_rayFromWorld, callback.m_rayToWorld, callback);
            
            if (callback.hasHit())
                brushPosition = vec3(callback.m_hitPointWorld.getX(), callback.m_hitPointWorld.getY(), callback.m_hitPointWorld.getZ());
        }
        
        glViewport(0, 0, width, height);
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClearDepth(1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        
        if (Program* prog = pm.get("terrain-vs", "terrain-fs"))
        {
            prog->bind();
            
            tm.get("grass.png").get()->bind(0);
            tm.get("ground.png").get()->bind(1);
            
            glUniform1i(prog->getHandle("AlbedoTop"), 0);
            glUniform1i(prog->getHandle("AlbedoSide"), 1);
            glUniformMatrix4fv(prog->getHandle("ViewProjection"), 1, false, glm::value_ptr(viewproj));
            
            if (chunk.geometry)
                chunk.geometry->draw(Geometry::Primitive_Triangles, 0, chunk.geometryIndices);
        }
        
        if (Program* prog = pm.get("brush-vs", "brush-fs"))
        {
            prog->bind();
            
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            mat4 world = glm::translate(glm::mat4(), brushPosition);
            
            glUniformMatrix4fv(prog->getHandle("World"), 1, false, glm::value_ptr(world));
            glUniformMatrix4fv(prog->getHandle("ViewProjection"), 1, false, glm::value_ptr(viewproj));
            
            if (brushGeom.first)
                brushGeom.first->draw(Geometry::Primitive_Triangles, 0, brushGeom.second);
        }
        
        glDisable(GL_CULL_FACE);
        
        drawtext(uir, fonts, "sans", width, height, 10, 50, 18 * density, "Hello World", ~0u);
        
        glfwSwapBuffers(window);
    }
    
    dumpTexture(fonts.getTexture(), "../../fontatlas.png");
    
    glfwDestroyWindow(window);
    
    glfwTerminate();
}