#pragma once

class Program
{
public:
    explicit Program(unsigned int id);
    ~Program();
    
    void bind();
    
private:
    unsigned int id;
};

class ProgramManager
{
public:
    ProgramManager(const string& sourcePath);
    ~ProgramManager();
    
    Program* get(const string& vsName, const string& fsName);
    
private:
    unsigned int getShader(const string& name);
    
    string sourcePath;
    
    unordered_map<string, unsigned int> shaders;
    unordered_map<string, unique_ptr<Program>> programs;
};