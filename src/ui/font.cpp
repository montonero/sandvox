#include "font.hpp"

#include "gfx/texture.hpp"

#include <fstream>

#define STBTT_malloc(x,u)  malloc(x)
#define STBTT_free(x,u)    free(x)

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <ft2build.h>
#include FT_FREETYPE_H

class FontFT: public Font
{
public:
    FontFT(FontAtlas* atlas, const string& path)
    : atlas(atlas)
    , face(nullptr)
    {
        static FT_Library library = initializeLibrary();
        
        if (FT_New_Face(library, path.c_str(), 0, &face))
            throw runtime_error("Error loading font");
    }
    
    ~FontFT()
    {
        FT_Done_Face(face);
    }
    
    FontMetrics getMetrics(float size) override
    {
        float scale = getScale(size);
        
        return { static_cast<short>(face->ascender * scale), static_cast<short>(face->descender * scale), static_cast<short>(face->height * scale) };
    }
    
    optional<GlyphBitmap> getGlyphBitmap(float size, unsigned int cp) override
    {
        if (optional<GlyphBitmap> cached = atlas->getBitmap(this, size, cp))
            return cached;
        
        unsigned int index = FT_Get_Char_Index(face, cp);
        
        if (index)
        {
            FT_Size_RequestRec req =
            {
                FT_SIZE_REQUEST_TYPE_REAL_DIM,
                0,
                int(size * (1 << 6)),
                0, 0
            };
            
            FT_Request_Size(face, &req);
            FT_Load_Glyph(face, index, FT_LOAD_RENDER);
            
            FT_GlyphSlot glyph = face->glyph;
            
            GlyphMetrics gm = { static_cast<short>(glyph->bitmap_left), static_cast<short>(glyph->bitmap_top), static_cast<short>(glyph->metrics.horiAdvance >> 6) };
            
            unsigned int gw = glyph->bitmap.width;
            unsigned int gh = glyph->bitmap.rows;
            
            vector<unsigned char> pixels((gw + 1) * (gh + 1));
            
            for (unsigned int y = 0; y < gh; ++y)
                memcpy(&pixels[y * (gw + 1)], &glyph->bitmap.buffer[y * glyph->bitmap.pitch], gw);
            
            return atlas->addBitmap(this, size, cp, gm, gw + 1, gh + 1, pixels.data());
        }
        
        return {};
    }
    
    short getKerning(float size, unsigned int cp1, unsigned int cp2) override
    {
        unsigned int index1 = FT_Get_Char_Index(face, cp1);
        unsigned int index2 = FT_Get_Char_Index(face, cp2);
        
        if (index1 && index2)
        {
            FT_Vector result;
            if (FT_Get_Kerning(face, index1, index2, FT_KERNING_UNSCALED, &result) == 0)
            {
                return result.x * getScale(size);
            }
        }
        
        return 0;
    }
    
private:
    static FT_Library initializeLibrary()
    {
        FT_Library result;
        if (FT_Init_FreeType(&result))
            throw runtime_error("Error initializing FreeType");
 
        return result;
    }
    
    float getScale(float size) const
    {
        return size / (face->ascender - face->descender);
    }
    
    FontAtlas* atlas;
    
    FT_Face face;
};

class FontSTB: public Font
{
public:
    FontSTB(FontAtlas* atlas, const string& path)
    : atlas(atlas)
    {
        ifstream in(path, ios::in | ios::binary);
        if (!in)
            throw runtime_error("Error loading font");
        
        in.seekg(0, ios::end);
        istream::pos_type length = in.tellg();
        in.seekg(0, ios::beg);
        
        if (length <= 0)
            throw runtime_error("Error loading font");
        
        data.reset(new char[length]);
        
        in.read(data.get(), length);
        
        if (in.gcount() != length)
            throw runtime_error("Error loading font");
        
        if (!stbtt_InitFont(&font, reinterpret_cast<unsigned char*>(data.get()), 0))
            throw runtime_error("Error loading font");
    }
    
    ~FontSTB()
    {
    }
    
    FontMetrics getMetrics(float size) override
    {
        float scale = getScale(size);
        
        int ascent, descent, linegap;
        stbtt_GetFontVMetrics(&font, &ascent, &descent, &linegap);
        
        int height = ascent - descent + linegap;
        
        return { static_cast<short>(ascent * scale), static_cast<short>(descent * scale), static_cast<short>(height * scale) };
    }
    
    optional<GlyphBitmap> getGlyphBitmap(float size, unsigned int cp) override
    {
        if (optional<GlyphBitmap> cached = atlas->getBitmap(this, size, cp))
            return cached;
        
        unsigned int index = stbtt_FindGlyphIndex(&font, cp);
        
        if (index)
        {
            float scale = getScale(size);
            
            int x0, y0, x1, y1;
            stbtt_GetGlyphBitmapBox(&font, index, scale, scale, &x0, &y0, &x1, &y1);
            
            int advance, leftSideBearing;
            stbtt_GetGlyphHMetrics(&font, index, &advance, &leftSideBearing);
            
            GlyphMetrics gm = {static_cast<short>(x0), static_cast<short>(-y0), static_cast<short>(advance * scale)};
            unsigned int gw = x1 - x0;
            unsigned int gh = y1 - y0;
            
            vector<unsigned char> pixels((gw + 1) * (gh + 1));
            
            stbtt_MakeGlyphBitmap(&font, pixels.data(), gw, gh, gw + 1, scale, scale, index);
            
            return atlas->addBitmap(this, size, cp, gm, gw + 1, gh + 1, pixels.data());
        }
        
        return {};
    }
    
    short getKerning(float size, unsigned int cp1, unsigned int cp2) override
    {
        if (!font.kern) return 0;
        
        unsigned int index1 = stbtt_FindGlyphIndex(&font, cp1);
        unsigned int index2 = stbtt_FindGlyphIndex(&font, cp2);
        
        if (index1 && index2)
        {
            return stbtt_GetGlyphKernAdvance(&font, index1, index2) * getScale(size);
        }
        
        return 0;
    }
    
private:
    float getScale(float size)
    {
        return stbtt_ScaleForPixelHeight(&font, size);
    }
    
    FontAtlas* atlas;
    
    stbtt_fontinfo font;
    unique_ptr<char[]> data;
};

Font::~Font()
{
}

bool FontAtlas::GlyphKey::operator==(const GlyphKey& other) const
{
    return font == other.font && size == other.size && cp == other.cp;
}

size_t FontAtlas::GlyphKeyHash::operator()(const GlyphKey& key) const
{
    return hash_combine(hash_value(key.font), hash_combine(hash_value(key.size), hash_value(key.cp)));
}

FontAtlas::FontAtlas(unsigned int atlasWidth, unsigned int atlasHeight)
: layoutX(0)
, layoutY(0)
, layoutNextY(0)
{
    texture = make_unique<Texture>(Texture::Type_2D, Texture::Format_R8, atlasWidth, atlasHeight, 1, 1);
}

FontAtlas::~FontAtlas()
{
}
    
optional<Font::GlyphBitmap> FontAtlas::getBitmap(Font* font, float size, unsigned int cp)
{
    auto it = glyphs.find({ font, size, cp });
    
    return (it == glyphs.end()) ? optional<Font::GlyphBitmap>() : make_optional(it->second);
}

optional<Font::GlyphBitmap> FontAtlas::addBitmap(Font* font, float size, unsigned int cp, const Font::GlyphMetrics& metrics, unsigned int width, unsigned int height, const unsigned char* pixels)
{
    assert(glyphs.count({ font, size, cp }) == 0);
    
    auto result = addBitmapData(width, height, pixels);
    
    if (result)
    {
        Font::GlyphBitmap bitmap = { metrics, static_cast<short>(result->first), static_cast<short>(result->second), static_cast<short>(width), static_cast<short>(height) };
        
        glyphs[{ font, size, cp }] = bitmap;
        
        return make_optional(bitmap);
    }
    
    return {};
}

optional<pair<unsigned int, unsigned int>> FontAtlas::addBitmapData(unsigned int width, unsigned int height, const unsigned char* pixels)
{
    auto result = layoutBitmap(width, height);
    
    if (result)
    {
        texture->upload(0, 0, 0, TextureRegion { result->first, result->second, 0, width, height, 1 }, pixels, width * height);
    }
    
    return result;
}

optional<pair<unsigned int, unsigned int>> FontAtlas::layoutBitmap(unsigned int width, unsigned int height)
{
    // Try to fit in the same line
    if (layoutX + width <= texture->getWidth() && layoutY + height <= texture->getHeight())
    {
        auto result = make_pair(layoutX, layoutY);
        
        layoutX += width;
        layoutNextY = max(layoutNextY, layoutY + height);
        
        return make_optional(result);
    }
    
    // Try to fit in the next line
    if (width <= texture->getWidth() && layoutNextY + height <= texture->getHeight())
    {
        auto result = make_pair(0u, layoutNextY);
        
        layoutX = width;
        layoutY = layoutNextY;
        layoutNextY = layoutY + height;
        
        return make_optional(result);
    }
    
    // Fail
    return {};
}

FontLibrary::FontLibrary(unsigned int atlasWidth, unsigned int atlasHeight)
: atlas(make_unique<FontAtlas>(atlasWidth, atlasHeight))
{
}

FontLibrary::~FontLibrary()
{
}

void FontLibrary::addFont(const string& name, const string& path, bool freetype)
{
    assert(fonts.count(name) == 0);
    
    if (freetype)
        fonts[name] = make_unique<FontFT>(atlas.get(), path);
    else
        fonts[name] = make_unique<FontSTB>(atlas.get(), path);
}

Font* FontLibrary::getFont(const string& name)
{
    auto it = fonts.find(name);
    
    return (it == fonts.end()) ? nullptr : it->second.get();
}