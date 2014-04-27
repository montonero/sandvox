#include "font.hpp"

#include "gfx/texture.hpp"

#define STBTT_malloc(x,u)  malloc(x)
#define STBTT_free(x,u)    free(x)

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <fstream>

int ft_BakeFontBitmap(const unsigned char *data, int data_size, float pixel_height, unsigned char *pixels, int pw, int ph, int first_char, int num_chars, stbtt_bakedchar *chardata)
{
    FT_Library library;
    FT_Init_FreeType(&library);
    
    FT_Face face;
    FT_New_Memory_Face(library, data, data_size, 0, &face);
    
    FT_Size_RequestRec sizereq =
    {
        FT_SIZE_REQUEST_TYPE_REAL_DIM,
        0,
        int(pixel_height * (1 << 6)),
        0, 0
    };
    
    FT_Request_Size(face, &sizereq);

    memset(pixels, 0, pw*ph); // background of 0 around pixels
    
    int x = 1;
    int y = 1;
    int bottom_y = 1;
    
    for (int i = 0; i < num_chars; ++i)
    {
        int g = FT_Get_Char_Index(face, first_char + i);
        
        FT_Load_Glyph(face, g, 0);
        FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
        
        FT_GlyphSlot slot = face->glyph;
        
        int gw = slot->bitmap.width;
        int gh = slot->bitmap.rows;
        
        if (x + gw + 1 >= pw)
            y = bottom_y, x = 1; // advance to next row
        if (y + gh + 1 >= ph) // check if it fits vertically AFTER potentially moving to next row
            return -i;
        
        for (int iy = 0; iy < gh; ++iy)
        for (int ix = 0; ix < gw; ++ix)
        pixels[(x+ix)+pw*(y+iy)] = slot->bitmap.buffer[ix+iy*slot->bitmap.pitch];
        
        chardata[i].x0 = (stbtt_int16) x;
        chardata[i].y0 = (stbtt_int16) y;
        chardata[i].x1 = (stbtt_int16) (x + gw);
        chardata[i].y1 = (stbtt_int16) (y + gh);
        chardata[i].xadvance = slot->advance.x >> 6;
        chardata[i].xoff     = slot->bitmap_left;
        chardata[i].yoff     = -slot->bitmap_top;
        
        x = x + gw + 2;
        if (y+gh+2 > bottom_y)
            bottom_y = y+gh+2;
    }
    
    return bottom_y;
}

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
    
    clock_t start = clock();
    
#if 1
    unsigned int lines = ft_BakeFontBitmap(data.get(), length, size, pixels.get(), width, height, 32, 96, chars.get());
#else
    unsigned int lines = stbtt_BakeFontBitmap(data.get(), 0, size, pixels.get(), width, height, 32, 96, chars.get());
#endif

    clock_t end = clock();
    
    printf("Packed into %d lines in %.1f ms\n", lines, (end - start) * 1000.0 / CLOCKS_PER_SEC);
    
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