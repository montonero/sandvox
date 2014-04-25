#include "program.hpp"

#include "fs/path.hpp"

#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

#include <regex>
#include <fstream>

static int getShaderParameter(unsigned int shader, GLenum pname)
{
    int result = 0;
    glGetShaderiv(shader, pname, &result);
    return result;
}

static int getProgramParameter(unsigned int program, GLenum pname)
{
    int result = 0;
    glGetProgramiv(program, pname, &result);
    return result;
}

template <typename GetParameter, typename GetLog>
static string getInfoLog(unsigned int id, GetParameter getParameter, GetLog getLog)
{
    int infoLength = getParameter(id, GL_INFO_LOG_LENGTH);
    
    if (infoLength > 0)
	{
        vector<char> buffer(infoLength);
        
        getLog(id, infoLength, NULL, &buffer[0]);
        
        return string(&buffer[0]);
	}
	else
	{
        return "";
	}
}

template <typename GetParameter, typename GetLog>
static void dumpInfoLog(unsigned int id, GetParameter getParameter, GetLog getLog)
{
    string infoLog = getInfoLog(id, getParameter, getLog);
    
    if (!infoLog.empty())
    {
        printf("%s", infoLog.c_str());
    }
}

static const regex includeRe("\\s*#\\s*include\\s+(\"(.*)\"|<(.*)>)\\s*");

static void appendPreprocessedSource(string& result, const string& path, const string& file, unordered_set<string>& visited)
{
    if (visited.count(file))
        return;
    
    visited.insert(file);
    
    ifstream in(path + "/" + file);
    if (!in) throw runtime_error("Error opening file " + file);
    
    smatch match;
    string line;
    unsigned int lineIndex = 0;
    
    while (getline(in, line))
    {
        ++lineIndex;
        
        if (regex_match(line, match, includeRe))
        {
            string include = match[2];
            
            string::size_type slash = file.find_last_of('/');
            string includeFile = (slash == std::string::npos) ? include : file.substr(0, slash + 1) + include;
            
            if (!visited.count(include))
            {
                result += "#line 1 1\n";
            
                appendPreprocessedSource(result, path, includeFile, visited);
                
                result += "#line " + to_string(lineIndex + 1) + " 0\n";
            }
        }
        else
        {
            result += line;
            result += "\n";
        }
    }
}

static GLenum getShaderType(const string& name)
{
    if (name.rfind("-vs") == name.length() - 3)
        return GL_VERTEX_SHADER;
    if (name.rfind("-fs") == name.length() - 3)
        return GL_FRAGMENT_SHADER;
    
    throw runtime_error("Unrecognized shader type: " + name);
}

static unsigned int compileShader(const string& source, GLenum type)
{
    unsigned int id = glCreateShader(type);
    assert(id > 0);
    
    const GLchar* sourcePtr = source.c_str();
    GLint sourceLength = source.length();
    
    glShaderSource(id, 1, &sourcePtr, &sourceLength);
    glCompileShader(id);
    
    if (getShaderParameter(id, GL_COMPILE_STATUS) != 1)
    {
        string infoLog = getInfoLog(id, getShaderParameter, glGetShaderInfoLog);
        
        glDeleteShader(id);
        
        throw runtime_error(infoLog);
    }
    
    dumpInfoLog(id, getShaderParameter, glGetShaderInfoLog);
    
    return id;
}

static unsigned int linkProgram(const vector<unsigned int>& shaders)
{
    unsigned int id = glCreateProgram();
    assert(id > 0);
    
    for (auto shader: shaders)
        glAttachShader(id, shader);
    
    glLinkProgram(id);
 
    if (getProgramParameter(id, GL_LINK_STATUS) != 1)
    {
        string infoLog = getInfoLog(id, getProgramParameter, glGetProgramInfoLog);
        
        glDeleteProgram(id);
        
        throw runtime_error(infoLog);
    }
    
    dumpInfoLog(id, getProgramParameter, glGetProgramInfoLog);
    
    return id;
}

Program::Program(unsigned int id, const vector<string>& shaders)
: id(id)
, shaders(shaders)
{
    assert(id > 0);
}

Program::~Program()
{
    glDeleteProgram(id);
}

void Program::bind()
{
    glUseProgram(id);
}

int Program::getHandle(const char* name) const
{
    return glGetUniformLocation(id, name);
}

void Program::reload(unsigned int newId)
{
    assert(newId > 0);
    
    glDeleteProgram(id);
    id = newId;
}

ProgramManager::ProgramManager(const string& basePath, FolderWatcher* watcher)
: basePath(Path::full(basePath))
, watcher(watcher)
{
    if (watcher)
        watcher->addListener(this);
}

ProgramManager::~ProgramManager()
{
    for (auto p: shaders)
        glDeleteShader(p.second);
    
    if (watcher)
        watcher->removeListener(this);
}

Program* ProgramManager::get(const string& vsName, const string& fsName)
{
    return getProgram({vsName, fsName});
}

void ProgramManager::onFileChanged(const string& path)
{
    auto sit = shaderSources.lower_bound(make_pair(path, ""));
    
    for (; sit != shaderSources.end() && sit->first == path; ++sit)
    {
        const string& shader = sit->second;
        
        printf("Reloading shader %s\n", shader.c_str());
        
        getShader(shader, /* cache= */ false);
        
        auto pit = programsByShader.lower_bound(make_pair(shader, ""));
        
        for (; pit != programsByShader.end() && pit->first == shader; ++pit)
        {
            const string& program = pit->second;
            
            getProgram(programs[program]->getShaders(), /* cache= */ false);
        }
    }
}

unsigned int ProgramManager::getShader(const string& name, bool cache)
{
    const string& key = name;
    
    if (cache)
    {
        auto it = shaders.find(key);
        if (it != shaders.end())
            return it->second;
    }
    
    try
    {
        unordered_set<string> visited;
        
        string source;
        appendPreprocessedSource(source, basePath, name + ".glsl", visited);
        
        unsigned int id = compileShader(source, getShaderType(name));
        
        for (auto& p: visited)
            shaderSources.insert(make_pair(Path::full(basePath + "/" + p), name));
        
        return shaders[key] = id;
    }
    catch (exception& e)
    {
        printf("Error compiling shader %s:\n", name.c_str());
        printf("%s\n", e.what());
        
        return shaders[key];
    }
}

Program* ProgramManager::getProgram(const vector<string>& shaders, bool cache)
{
    string key;
    
    for (auto& s: shaders)
    {
        key += s;
        key += ',';
    }
    
    if (cache)
    {
        auto it = programs.find(key);
        if (it != programs.end())
            return it->second.get();
    }
    
    unique_ptr<Program>& result = programs[key];
    
    try
    {
        vector<unsigned int> shaderIds;
        
        for (auto& s: shaders)
        {
            programsByShader.insert(make_pair(s, key));
            
            unsigned int id = getShader(s);
            
            if (!id)
                throw runtime_error("Shader " + s + "failed to compile");
            
            shaderIds.push_back(id);
        }
        
        unsigned int id = linkProgram(shaderIds);
        
        if (result)
            result->reload(id);
        else
            result.reset(new Program(id, shaders));
        
        return result.get();
    }
    catch (exception& e)
    {
        printf("Error compiling program %s:\n", key.c_str());
        printf("%s\n", e.what());
        
        return result.get();
    }
}

