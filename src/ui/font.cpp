#include "font.hpp"

#include "gfx/texture.hpp"

#define STBTT_malloc(x,u)  malloc(x)
#define STBTT_free(x,u)    free(x)

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <fstream>

Font::Font(const string& path, float size)
{
    ifstream in(path, ios::in | ios::binary);
    if (!in) throw runtime_error("Error opening file " + path);
    
    in.seekg(0, ios::end);
    istream::pos_type length = in.tellg();
    in.seekg(0, ios::beg);
    
    unique_ptr<unsigned char[]> data(new unsigned char[length]);
    in.read(reinterpret_cast<char*>(data.get()), length);
    if (in.gcount() != length) throw runtime_error("Error reading file " + path);
    
    unsigned int width = 1024;
    unsigned int height = 1024;
    unique_ptr<unsigned char[]> pixels(new unsigned char[width * height]);
    unique_ptr<stbtt_bakedchar[]> chars(new stbtt_bakedchar[96]);
    
    unsigned int lines = stbtt_BakeFontBitmap(data.get(), 0, size, pixels.get(), width, height, 32, 96, chars.get());
    
    printf("Packed into %d lines\n", lines);
    
    for (int ch = 0; ch < 96; ++ch)
    {
        const stbtt_bakedchar& c = chars[ch];
        
        glyphs[ch + 32] = {float(c.x0) / width, float(c.y0) / lines, float(c.x1) / width, float(c.y1) / lines, float(c.x1 - c.x0), float(c.y1 - c.y0), c.xoff, c.yoff, c.xadvance };
    }
    
    texture.reset(new Texture(Texture::Type_2D, Texture::Format_R8, width, lines, 1, 1));
    
    texture->upload(0, 0, 0, { 0, 0, 0, width, lines, 1 }, pixels.get(), width * lines);
}

Font::~Font()
{
}

const Font::Glyph* Font::getGlyph(unsigned int id) const
{
    auto it = glyphs.find(id);
    
    return (it != glyphs.end()) ? &it->second : nullptr;
}

int Font::getKerning(unsigned int id1, unsigned int id2) const
{
    auto it = kerning.find(make_pair(id1, id2));
    
    return (it != kerning.end()) ? it->second : 0;
}