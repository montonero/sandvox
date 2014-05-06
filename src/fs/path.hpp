#pragma once

class Path
{
public:
    static string full(const string& path);
    
    static bool exists(const string& path);
};