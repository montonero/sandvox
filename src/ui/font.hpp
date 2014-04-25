#pragma once

class Texture;

class Font
{
public:
	struct Glyph
	{
        float u0, v0, u1, v1;
        float width, height;
        float xoffset, yoffset, xadvance;
	};
    
	Font(const string& path, float size);
	~Font();
    
    const Glyph* getGlyph(unsigned int id) const;
    int getKerning(unsigned int id1, unsigned int id2) const;
    
    Texture* getTexture() const { return texture.get(); }

private:
	unique_ptr<Texture> texture;
    
    unordered_map<unsigned int, Glyph> glyphs;
    map<pair<unsigned int, unsigned int>, int> kerning;
};