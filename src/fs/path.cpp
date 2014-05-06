#include "common.hpp"
#include "fs/path.hpp"

#include <sys/stat.h>

string Path::full(const string& path)
{
    char result[PATH_MAX];
    
    return realpath(path.c_str(), result) ? string(result) : path;
}

bool Path::exists(const string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}