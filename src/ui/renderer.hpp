#pragma once

class Buffer;
class Geometry;
class Program;
class Texture;

namespace ui
{
    class FontLibrary;
    
	class Renderer
	{
	public:
		Renderer(FontLibrary& fonts, Program* program);
		~Renderer();
        
        void begin(unsigned int width, unsigned int height, float density);
        
        void rect(const vec2& x0y0, const vec2& x1y1, float r, const vec4& color);
        void text(const vec2& pos, const string& font, const string& text, float size, const vec4& color);
        
        void push(const vec2& pos, const vec2& uv, unsigned int color)
        {
            vertices.push_back(Vertex { pos, glm::i16vec2(uv * 8192.f), color });
        }
        
        void end();

	private:
		struct Vertex
		{
			vec2 pos;
            glm::i16vec2 uv;
			unsigned int color;
		};

		vector<Vertex> vertices;

		shared_ptr<Buffer> vb;
		unique_ptr<Geometry> geometry;

        FontLibrary& fonts;
		Program* program;
	};
}