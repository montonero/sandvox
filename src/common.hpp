#include <cassert>

#include <memory>
#include <functional>

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <map>
#include <set>

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

typedef glm::vec2 vec2;
typedef glm::vec3 vec3;
typedef glm::vec4 vec4;

typedef glm::mat3 mat3;
typedef glm::mat4 mat4;

typedef glm::quat quat;

using namespace std;
using namespace std::placeholders;

struct noncopyable
{
    noncopyable() = default;
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
};