#pragma once

class Buffer;
class Geometry;
class Program;
class Texture;

namespace ui
{
	class Renderer
	{
	public:
		Renderer(unsigned int vertexCount, Program* program);
		~Renderer();

		void push(const vec2& pos, const vec2& uv, unsigned int color);
		void flush(Texture* texture);

	private:
		struct Vertex
		{
			vec4 posuv;
			unsigned int color;
		};

		vector<Vertex> vertices;

		shared_ptr<Buffer> vb;
		unique_ptr<Geometry> geometry;

		Program* program;
	};
}