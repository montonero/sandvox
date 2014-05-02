#include "renderer.hpp"

#include "gfx/geometry.hpp"
#include "gfx/program.hpp"
#include "gfx/texture.hpp"

#include "ui/font.hpp"

namespace ui
{
	Renderer::Renderer(FontLibrary& fonts, Program* program)
    : fonts(fonts)
    , program(program)
	{
	}

	Renderer::~Renderer()
	{
	}

    void Renderer::begin(unsigned int width, unsigned int height, float density)
    {
    }
    
    void Renderer::rect(const vec2& x0y0, const vec2& x1y1, float r, const vec4& color)
    {
    }
    
    void Renderer::text(const vec2& pos, const string& font, const string& text, float size, const vec4& color)
    {
    }
    
    void Renderer::end()
    {
        fonts.flush();
    
        if (vertices.empty())
            return;
        
        if (!vb || vb->getElementCount() < vertices.size())
        {
            size_t size = 256;
            while (size < vertices.size()) size += size * 3 / 2;
            
            vector<Geometry::Element> layout =
            {
                { 0, offsetof(Vertex, pos), Geometry::Format_Float2 },
                { 0, offsetof(Vertex, uv), Geometry::Format_Short2 },
                { 0, offsetof(Vertex, color), Geometry::Format_Color }
            };
            
            vb = make_shared<Buffer>(Buffer::Type_Vertex, sizeof(Vertex), size, Buffer::Usage_Dynamic);
            geometry = make_unique<Geometry>(layout, vb);
        }
 
        vb->upload(0, vertices.data(), vertices.size() * sizeof(Vertex));
        
        if (program)
        {
            program->bind();
            fonts.getTexture()->bind(0);
            geometry->draw(Geometry::Primitive_Triangles, 0, vertices.size());
        }
        
        vertices.clear();
	}
}