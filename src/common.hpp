#include <cassert>

#include <memory>
#include <functional>
#include <optional>

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <string>

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

template <typename T> inline size_t hash_value(const T& v)
{
    return hash<T>()(v);
}

inline size_t hash_combine(size_t a, size_t b)
{
    return b + 0x9e3779b9 + (a << 6) + (a >> 2);
}

namespace std
{
    template <typename T, typename U> struct hash<pair<T, U>>
    {
        size_t operator()(const pair<T, U>& p) const noexcept
        {
            return hash_combine(hash_value(p.first), hash_value(p.second));
        }
    };
}
