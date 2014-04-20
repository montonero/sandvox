#pragma once

#include "fs/folderwatcher.hpp"

class Program: noncopyable
{
public:
    explicit Program(unsigned int id, const vector<string>& shaders);
    ~Program();
    
    void bind();
    
    void reload(unsigned int newId);
    
    unsigned int getId() const { return id; }
    const vector<string>& getShaders() const { return shaders; }
    
private:
    unsigned int id;
    vector<string> shaders;
};

class ProgramManager: public FolderWatcher::Listener
{
public:
    ProgramManager(const string& sourcePath, FolderWatcher* watcher);
    ~ProgramManager();
    
    Program* get(const string& vsName, const string& fsName);
    
    void onFileChanged(const string& path) override;
    
private:
    unsigned int getShader(const string& name, bool cache = true);
    Program* getProgram(const vector<string>& shaders, bool cache = true);
    
    string sourcePath;
    
    FolderWatcher* watcher;
    
    unordered_map<string, unsigned int> shaders;
    unordered_map<string, unique_ptr<Program>> programs;
    
    set<pair<string, string>> shaderSources;
    set<pair<string, string>> programsByShader;
};