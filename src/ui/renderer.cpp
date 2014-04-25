#include "renderer.hpp"

#include "gfx/geometry.hpp"
#include "gfx/program.hpp"
#include "gfx/texture.hpp"

namespace ui
{
	Renderer::Renderer(unsigned int vertexCount, Program* program)
    : program(program)
	{
        vector<Geometry::Element> layout =
        {
            { 0, offsetof(Vertex, posuv), Geometry::Format_Float4 },
            { 0, offsetof(Vertex, color), Geometry::Format_Color }
        };
        
        vb = make_shared<Buffer>(Buffer::Type_Vertex, sizeof(Vertex), vertexCount, Buffer::Usage_Dynamic);
        geometry = make_unique<Geometry>(layout, vb);
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::push(const vec2& pos, const vec2& uv, unsigned int color)
	{
        vertices.push_back({ vec4(pos, uv), color });
	}

	void Renderer::flush(Texture* texture)
	{
        if (vertices.empty())
            return;
        
        vb->upload(0, vertices.data(), vertices.size() * sizeof(Vertex));
        
        if (program)
        {
            program->bind();
            texture->bind(0);
            geometry->draw(Geometry::Primitive_Triangles, 0, vertices.size());
        }
        
        vertices.clear();
	}
}