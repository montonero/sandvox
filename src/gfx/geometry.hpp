#pragma once

class Buffer
{
public:
    enum Type
    {
        Type_Vertex,
        Type_Index,
        Type_Uniform,
        
        Type_Count
    };
    
    enum Usage
    {
        Usage_Static,
        Usage_Dynamic,
        
        Usage_Count
    };
    
    enum LockMode
    {
        Lock_Normal,
        Lock_Discard,
        
        Lock_Count
    };
    
    Buffer(Type type, unsigned int elementSize, unsigned int elementCount, Usage usage);
    ~Buffer();
    
    template <typename T> T* lock(LockMode mode = Lock_Normal)
    {
        assert(sizeof(T) == elementSize);
        
        return static_cast<T*>(lock(mode));
    }
    
    void* lock(LockMode mode = Lock_Normal);
    void unlock();
    
    void upload(unsigned int offset, const void* data, unsigned int size);
 
    unsigned int getId() const { return id; }
    
    Type getType() const { return type; }
    
    unsigned int getElementSize() const { return elementSize; }
    unsigned int getElementCount() const { return elementCount; }
    
private:
    unsigned int id;
    
    Type type;
    unsigned int elementSize;
    unsigned int elementCount;
    Usage usage;
};

class Geometry
{
public:
    enum Format
    {
        Format_Float1,
        Format_Float2,
        Format_Float3,
        Format_Float4,
        Format_Short2,
        Format_Short4,
        Format_UByte4,
        Format_Color,
        
        Format_Count
    };
    
    struct Element
    {
        unsigned int stream;
        unsigned int offset;
        
        Format format;
        
        Element(unsigned int offset, Format format);
        Element(unsigned int stream, unsigned int offset, Format format);
    };
    
    enum Primitive
	{
        Primitive_Triangles,
        Primitive_Lines,
        Primitive_Points,
        Primitive_TriangleStrip,
        
        Primitive_Count
	};
 
    Geometry(const vector<Element>& elements, const vector<shared_ptr<Buffer>>& vertexBuffers, const shared_ptr<Buffer>& indexBuffer = shared_ptr<Buffer>());
    Geometry(const vector<Element>& elements, const shared_ptr<Buffer>& vertexBuffer, const shared_ptr<Buffer>& indexBuffer = shared_ptr<Buffer>());
    ~Geometry();
    
    void draw(Primitive primitive, unsigned int offset, unsigned int count, unsigned int baseVertexIndex = 0);
    
private:
    unsigned int id;
    vector<shared_ptr<Buffer>> vertexBuffers;
    shared_ptr<Buffer> indexBuffer;
};