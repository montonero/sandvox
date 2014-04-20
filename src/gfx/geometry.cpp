#include "geometry.hpp"

#include <OpenGL/gl3.h>

static const GLenum kBufferTarget[Buffer::Type_Count] =
{
    GL_ARRAY_BUFFER,
    GL_ELEMENT_ARRAY_BUFFER,
    GL_UNIFORM_BUFFER
};

static const GLenum kBufferUsage[Buffer::Usage_Count] =
{
    GL_STATIC_DRAW,
	GL_DYNAMIC_DRAW
};

static const GLenum kBufferLock[Buffer::Lock_Count] =
{
	GL_MAP_WRITE_BIT,
	GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT
};

struct VertexFormatGL
{
    int size;
    GLenum type;
    bool normalized;
};

static const VertexFormatGL kVertexFormat[Geometry::Format_Count] =
{
	{ 1, GL_FLOAT, false },
	{ 2, GL_FLOAT, false },
	{ 3, GL_FLOAT, false },
	{ 4, GL_FLOAT, false },
	{ 2, GL_UNSIGNED_SHORT, false },
	{ 4, GL_UNSIGNED_SHORT, false },
	{ 4, GL_UNSIGNED_BYTE, false },
	{ 4, GL_UNSIGNED_BYTE, true },
};

static const GLenum kGeometryPrimitive[Geometry::Primitive_Count] =
{
    GL_TRIANGLES,
    GL_LINES,
	GL_POINTS,
	GL_TRIANGLE_STRIP
};

Buffer::Buffer(Type type, unsigned int elementSize, unsigned int elementCount, Usage usage)
: id(0)
, type(type)
, elementSize(elementSize)
, elementCount(elementCount)
, usage(usage)
{
    glGenBuffers(1, &id);
    assert(id > 0);
    
    glBindBuffer(kBufferTarget[type], id);
    glBufferData(kBufferTarget[type], elementSize * elementCount, nullptr, kBufferUsage[usage]);
}

Buffer::~Buffer()
{
    glDeleteBuffers(1, &id);
}
                 
void* Buffer::lock(LockMode mode)
{
    glBindBuffer(kBufferTarget[type], id);
    void* result = glMapBufferRange(kBufferTarget[type], 0, elementSize * elementCount, kBufferLock[mode]);
    
    return result;
}

void Buffer::unlock()
{
    glBindBuffer(kBufferTarget[type], id);
    glUnmapBuffer(kBufferTarget[type]);
}

void Buffer::upload(unsigned int offset, const void* data, unsigned int size)
{
    assert(data);
    assert(offset % elementSize == 0);
    assert(size % elementSize == 0);
    assert(offset + size <= elementCount * elementSize);
    
    glBindBuffer(kBufferTarget[type], id);
    glBufferSubData(kBufferTarget[type], offset, size, data);
}

Geometry::Element::Element(unsigned int offset, Format format)
: stream(0)
, offset(offset)
, format(format)
{
}

Geometry::Element::Element(unsigned int stream, unsigned int offset, Format format)
: stream(stream)
, offset(offset)
, format(format)
{
}

Geometry::Geometry(const vector<Element>& elements, const vector<shared_ptr<Buffer>>& vertexBuffers, const shared_ptr<Buffer>& indexBuffer)
: id(0)
, vertexBuffers(vertexBuffers)
, indexBuffer(indexBuffer)
{
    glGenVertexArrays(1, &id);
    glBindVertexArray(id);
    
    for (size_t i = 0; i < elements.size(); ++i)
    {
        const auto& e = elements[i];
        
        assert(e.stream < vertexBuffers.size());
        
        const shared_ptr<Buffer>& buffer = vertexBuffers[e.stream];
        assert(buffer->getType() == Buffer::Type_Vertex);
        assert(e.offset < buffer->getElementSize());
        
        const VertexFormatGL& f = kVertexFormat[e.format];
        
        glBindBuffer(GL_ARRAY_BUFFER, buffer->getId());
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, f.size, f.type, f.normalized, buffer->getElementSize(), reinterpret_cast<const void*>(e.offset));
    }
    
    if (indexBuffer)
    {
        assert(indexBuffer->getType() == Buffer::Type_Index);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->getId());
    }
    
    glBindVertexArray(0);
}

Geometry::Geometry(const vector<Element>& elements, const shared_ptr<Buffer>& vertexBuffer, const shared_ptr<Buffer>& indexBuffer)
: Geometry(elements, vector<shared_ptr<Buffer>>(1, vertexBuffer), indexBuffer)
{
}

Geometry::~Geometry()
{
    glDeleteVertexArrays(1, &id);
}

void Geometry::draw(Primitive primitive, unsigned int offset, unsigned int count, unsigned int baseVertexIndex)
{
    glBindVertexArray(id);
    
    if (indexBuffer)
    {
        assert(indexBuffer->getElementSize() == 2 || indexBuffer->getElementSize() == 4);
        GLenum indexType = (indexBuffer->getElementSize() == 2) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
        
        glDrawElementsBaseVertex(kGeometryPrimitive[primitive], count, indexType, reinterpret_cast<const void*>(offset * indexBuffer->getElementSize()), baseVertexIndex);
    }
    else
    {
        assert(baseVertexIndex == 0);
        glDrawArrays(kGeometryPrimitive[primitive], offset, count);
    }
    
    glBindVertexArray(0);
}