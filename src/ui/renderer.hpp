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
        
        void rect(const vec2& pos, const vec2& size, float r, const vec4& color);
        void text(const vec2& pos, const string& font, const string& text, float size, const vec4& color);
        
        void end();

	private:
		struct Vertex
		{
			vec2 pos;
            glm::i16vec2 uv;
			glm::u8vec4 color;
		};

        void poly(const vec2* vertices, size_t count, float r, const vec4& color);
        void quad(const vec2& x0y0, const vec2& uv0, const vec2& x1y1, const vec2& uv1, const vec4& color);
        
        FontLibrary& fonts;
		Program* program;
        
		shared_ptr<Buffer> vb;
		unique_ptr<Geometry> geometry;
        
		vector<Vertex> vertices;

        vec2 canvasOffset;
        vec2 canvasScale;
        float canvasDensity;
	};
}