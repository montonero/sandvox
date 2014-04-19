#include "program.hpp"

#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

#include <regex>

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

static const regex includeRe("#\\s*include\\s+(\"(.*)\"|<(.*)>)\\s*");

static void appendPreprocessedSource(string& result, const string& path, const string& file)
{
    ifstream in(path + "/" + file);
    if (!in) throw std::runtime_error("Error opening file " + file);
    
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
            
            result += "#line 1 1\n";
            
            if (slash == string::npos)
                appendPreprocessedSource(result, path, include);
            else
                appendPreprocessedSource(result, path, file.substr(0, slash + 1) + include);
            
            result += "#line " + to_string(lineIndex + 1) + " 0\n";
        }
        else
        {
            result += line;
            result += "\n";
        }
    }
}

static string getPreprocessedSource(const string& path, const string& file)
{
    string result;
    appendPreprocessedSource(result, path, file);
    
    return result;
}

static GLenum getShaderType(const string& name)
{
    if (name.rfind("-vs") == name.length() - 3)
        return GL_VERTEX_SHADER;
    if (name.rfind("-fs") == name.length() - 3)
        return GL_FRAGMENT_SHADER;
    
    throw std::runtime_error("Unrecognized shader type: " + name);
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
        
        throw std::runtime_error(infoLog);
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
        
        throw std::runtime_error(infoLog);
    }
    
    dumpInfoLog(id, getProgramParameter, glGetProgramInfoLog);
    
    return id;
}

Program::Program(unsigned int id)
    : id(id)
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

ProgramManager::ProgramManager(const string& sourcePath)
    : sourcePath(sourcePath)
{
}

ProgramManager::~ProgramManager()
{
    for (auto p: shaders)
        glDeleteShader(p.second);
}

Program* ProgramManager::get(const string& vsName, const string& fsName)
{
    string key = vsName + "," + fsName;
    
    auto it = programs.find(key);
    if (it != programs.end())
        return it->second.get();
    
    unique_ptr<Program>& result = programs[key];
    assert(!result);
        
    vector<unsigned int> shaders = {getShader(vsName), getShader(fsName)};
    
    try
    {
        unsigned int id = linkProgram(shaders);
        
        result.reset(new Program(id));
        
        return result.get();
    }
    catch (exception& e)
    {
        printf("Error compiling program %s:\n", key.c_str());
        printf("%s\n", e.what());
        
        return result.get();
    }
}

unsigned int ProgramManager::getShader(const string& name)
{
    const string& key = name;
    
    auto it = shaders.find(key);
    if (it != shaders.end())
        return it->second;
    
    try
    {
        string source = getPreprocessedSource(sourcePath, name + ".glsl");
        
        unsigned int id = compileShader(source, getShaderType(name));
        
        return shaders[key] = id;
    }
    catch (exception& e)
    {
        printf("Error compiling shader %s:\n", name.c_str());
        printf("%s\n", e.what());
        
        return shaders[key] = 0;
    }
}
