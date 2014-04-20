#include "path.hpp"

string Path::full(const string& path)
{
    char result[PATH_MAX];
    
    return realpath(path.c_str(), result) ? string(result) : path;
}