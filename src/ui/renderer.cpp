#include "common.hpp"
#include "ui/renderer.hpp"

#include "gfx/geometry.hpp"
#include "gfx/program.hpp"
#include "gfx/texture.hpp"

#include "ui/font.hpp"

// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

static const uint8_t utf8d[] =
{
    // The first part of the table maps bytes to character classes that
    // to reduce the size of the transition table and create bitmasks.
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,
    
    // The second part is a transition table that maps a combination
    // of a state of the automaton and a character class to a state.
    0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
    12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
    12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
    12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
    12,36,12,12,12,12,12,12,12,12,12,12, 
};

inline uint32_t utf8decode(uint32_t* state, uint32_t* codep, uint32_t byte)
{
    uint32_t type = utf8d[byte];
    *codep = (*state != UTF8_ACCEPT) ? (byte & 0x3fu) | (*codep << 6) : (0xff >> type) & (byte);
    *state = utf8d[256 + *state + type];
    return *state;
}

namespace ui
{
	Renderer::Renderer(FontLibrary& fonts, Program* program)
    : fonts(fonts)
    , program(program)
    , canvasDensity(1)
	{
        vb = make_shared<Buffer>(Buffer::Type_Vertex, sizeof(Vertex), 256, Buffer::Usage_Dynamic);
        ib = make_shared<Buffer>(Buffer::Type_Index, sizeof(unsigned int), 1024, Buffer::Usage_Dynamic);
	}

	Renderer::~Renderer()
	{
	}

    void Renderer::begin(unsigned int width, unsigned int height, float density)
    {
        assert(vertices.empty() && indices.empty());
        
        canvasScale = vec2(2.f / width, -2.f / height);
        canvasOffset = vec2(-1, 1);
        canvasDensity = density;
    }
    
    void Renderer::rect(const vec2& pos, const vec2& size, float r, const vec4& color)
    {
        if (r == 0)
        {
            vec2 uv = vec2(3, 3);
        
            quad(pos, uv, pos + size, uv, color);
        }
        else
        {
            static const int kP = 8;
            
            vec2 v[4*kP];
            
            for (int i = 0; i < kP; ++i)
            {
                float a = float(i) / (kP - 1) * glm::pi<float>() / 2;
                float x = glm::cos(a) * r;
                float y = glm::sin(a) * r;
                
                v[0*kP+i] = pos + vec2(r-x, r-y);
                v[1*kP+i] = pos + vec2(size.x-r+y, r-x);
                v[2*kP+i] = pos + vec2(size.x-r+x, size.y-r+y);
                v[3*kP+i] = pos + vec2(r-y, size.y-r+x);
            }
            
            poly(v, 4*kP, 0.5f, color);
        }
    }
    
    void Renderer::text(const vec2& pos, const string& font, const string& text, float size, const vec4& color)
    {
        ui::Font* f = fonts.getFont(font);
        if (!f) return;
        
        float su = 1.f / fonts.getTexture()->getWidth();
        float sv = 1.f / fonts.getTexture()->getHeight();
        
        Font::FontMetrics metrics = f->getMetrics(size * canvasDensity);
        
        vec2 pen = pos * canvasDensity - vec2(0, metrics.ascender);
        
        unsigned int lastch = 0;
        
        uint32_t utfstate = 0;
        uint32_t utfcode = 0;
        
        for (char ch: text)
        {
            if (utf8decode(&utfstate, &utfcode, static_cast<unsigned char>(ch)) != UTF8_ACCEPT)
                continue;
            
            if (auto bitmap = f->getGlyphBitmap(size * canvasDensity, utfcode))
            {
                pen += f->getKerning(size * canvasDensity, lastch, utfcode);
                
                float x0 = (pen.x + bitmap->metrics.bearingX) / canvasDensity;
                float y0 = (pen.y - bitmap->metrics.bearingY) / canvasDensity;
                float x1 = x0 + bitmap->w / canvasDensity;
                float y1 = y0 + bitmap->h / canvasDensity;
                
                float u0 = su * bitmap->x;
                float u1 = su * (bitmap->x + bitmap->w);
                float v0 = sv * bitmap->y;
                float v1 = sv * (bitmap->y + bitmap->h);
                
                quad(vec2(x0, y0), vec2(u0, v0), vec2(x1, y1), vec2(u1, v1), color);
                
                pen.x += bitmap->metrics.advance;
                
                lastch = utfcode;
            }
            else
            {
                lastch = 0;
            }
        }
    }
    
    void Renderer::end()
    {
        fonts.flush();
    
        if (vertices.empty() || indices.empty())
            return;
        
        growBuffer(vb, vertices.size());
        growBuffer(ib, indices.size());
        
        if (!geometry)
        {
            vector<Geometry::Element> layout =
            {
                { 0, offsetof(Vertex, pos), Geometry::Format_Float2 },
                { 0, offsetof(Vertex, uv), Geometry::Format_Short2 },
                { 0, offsetof(Vertex, color), Geometry::Format_Color }
            };
            
            geometry = make_unique<Geometry>(layout, vb, ib);
        }
 
        vb->upload(0, vertices.data(), vertices.size() * sizeof(Vertex));
        ib->upload(0, indices.data(), indices.size() * sizeof(unsigned int));
        
        if (program)
        {
            program->bind();
            fonts.getTexture()->bind(0);
            geometry->draw(Geometry::Primitive_Triangles, 0, indices.size());
        }
        
        vertices.clear();
        indices.clear();
	}
    
    void Renderer::poly(const vec2* data, size_t count, float r, const vec4& color)
    {
        glm::u8vec4 c = glm::u8vec4(color * 255.f + 0.5f);
        glm::i16vec2 t = glm::i16vec2(3 * 8192);
        
        size_t offset = vertices.size();
        
        // draw the solid portion
        for (size_t i = 0; i < count; ++i)
        {
            vec2 v = data[i] * canvasScale + canvasOffset;
            
            vertices.push_back({ v, t, c });
        }
        
        for (size_t i = 2; i < count; ++i)
        {
            indices.push_back(offset + 0);
            indices.push_back(offset + (i - 1));
            indices.push_back(offset + i);
        }
        
        // draw the antialiased border
        if (r > 0)
        {
            for (size_t i = 0; i < count; ++i)
            {
                // vec2 v = data[i] * canvasScale + canvasOffset;
                //
                // vertices.push_back({ v, t, c });
            }
        }
    }
   
    void Renderer::quad(const vec2& x0y0, const vec2& uv0, const vec2& x1y1, const vec2& uv1, const vec4& color)
    {
        glm::u8vec4 c = glm::u8vec4(color * 255.f + 0.5f);
        
        vec2 v0 = x0y0 * canvasScale + canvasOffset;
        vec2 v1 = x1y1 * canvasScale + canvasOffset;
        
        glm::i16vec2 t0 = glm::i16vec2(uv0 * 8192.f);
        glm::i16vec2 t1 = glm::i16vec2(uv1 * 8192.f);
        
        size_t offset = vertices.size();
        
        vertices.push_back({ vec2(v0.x, v0.y), glm::i16vec2(t0.x, t0.y), c });
        vertices.push_back({ vec2(v1.x, v0.y), glm::i16vec2(t1.x, t0.y), c });
        vertices.push_back({ vec2(v1.x, v1.y), glm::i16vec2(t1.x, t1.y), c });
        vertices.push_back({ vec2(v0.x, v1.y), glm::i16vec2(t0.x, t1.y), c });
        
        indices.push_back(offset + 0);
        indices.push_back(offset + 1);
        indices.push_back(offset + 2);
        
        indices.push_back(offset + 0);
        indices.push_back(offset + 2);
        indices.push_back(offset + 3);
    }
    
    void Renderer::growBuffer(shared_ptr<Buffer>& buffer, size_t count)
    {
        if (buffer->getElementCount() < count)
        {
            size_t newCount = buffer->getElementCount();
            while (newCount < count)
                newCount = newCount * 3 / 2;
            
            buffer = make_shared<Buffer>(buffer->getType(), buffer->getElementSize(), newCount, buffer->getUsage());
            
            geometry.reset();
        }
    }
}