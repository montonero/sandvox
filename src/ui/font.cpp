#include "font.hpp"

#include "gfx/texture.hpp"

#include <fstream>

#define FONT_USE_FREETYPE 1

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
    
    float getScale(float size) override
    {
        return size / face->units_per_EM;
    }
    
    FontMetrics getMetrics() override
    {
        FontMetrics result;
        
        result.ascender = face->ascender;
        result.descender = face->descender;
        result.height = face->height;
    
        return result;
    }
    
    optional<GlyphMetrics> getGlyphMetrics(unsigned int cp) override
    {
        unsigned int index = FT_Get_Char_Index(face, cp);
        
        if (index)
        {
            FT_Load_Glyph(face, index, FT_LOAD_NO_SCALE);
            
            const FT_Glyph_Metrics& gm = face->glyph->metrics;
            
            return make_optional(GlyphMetrics {float(gm.horiBearingX), float(gm.horiBearingY), float(gm.horiAdvance)});
        }
        
        return {};
    }
    
    optional<GlyphBitmap> getGlyphBitmap(float scale, unsigned int cp) override
    {
        if (optional<GlyphBitmap> cached = atlas->getBitmap(this, scale, cp))
            return cached;
        
        unsigned int index = FT_Get_Char_Index(face, cp);
        
        if (index)
        {
            FT_Size_RequestRec req =
            {
                FT_SIZE_REQUEST_TYPE_REAL_DIM,
                0,
                int(18 * 2 * (1 << 6)),
                0, 0
            };
            
            FT_Request_Size(face, &req);
            FT_Load_Glyph(face, index, FT_LOAD_RENDER);
            
            FT_GlyphSlot glyph = face->glyph;
            
            unsigned int gw = glyph->bitmap.width;
            unsigned int gh = glyph->bitmap.rows;
            
            vector<unsigned char> pixels((gw + 1) * (gh + 1));
            
            for (unsigned int y = 0; y < gh; ++y)
                for (unsigned int x = 0; x < gw; ++x)
                    pixels[x + y * (gw + 1)] = glyph->bitmap.buffer[x + y * glyph->bitmap.pitch];
            
            return atlas->addBitmap(this, scale, cp, gw + 1, gh + 1, pixels.data());
        }
        
        return {};
    }
    
    float getKerning(unsigned int cp1, unsigned int cp2) override
    {
        unsigned int index1 = FT_Get_Char_Index(face, cp1);
        unsigned int index2 = FT_Get_Char_Index(face, cp2);
        
        if (index1 && index2)
        {
            FT_Vector result;
            if (FT_Get_Kerning(face, index1, index2, FT_KERNING_UNSCALED, &result) == 0)
                return result.x;
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
    
    FontAtlas* atlas;
    FT_Face face;
};

Font::~Font()
{
}

bool FontAtlas::GlyphKey::operator==(const GlyphKey& other) const
{
    return font == other.font && scale == other.scale && cp == other.cp;
}

size_t FontAtlas::GlyphKeyHash::operator()(const GlyphKey& key) const
{
    return hash_combine(hash_value(key.font), hash_combine(hash_value(key.scale), hash_value(key.cp)));
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
    
optional<Font::GlyphBitmap> FontAtlas::getBitmap(Font* font, float scale, unsigned int cp)
{
    auto it = glyphs.find({ font, scale, cp });
    
    return (it == glyphs.end()) ? optional<Font::GlyphBitmap>() : make_optional(it->second);
}

optional<Font::GlyphBitmap> FontAtlas::addBitmap(Font* font, float scale, unsigned int cp, unsigned int width, unsigned int height, const unsigned char* pixels)
{
    assert(glyphs.count({ font, scale, cp }) == 0);
    
    auto result = addBitmapData(width, height, pixels);
    
    if (result)
    {
        Font::GlyphBitmap bitmap = { static_cast<unsigned short>(result->first), static_cast<unsigned short>(result->second), static_cast<unsigned short>(width), static_cast<unsigned short>(height) };
        
        glyphs[{ font, scale, cp }] = bitmap;
        
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
        auto result = make_pair(0u, layoutY);
        
        layoutX = 0;
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

void FontLibrary::addFont(const string& name, const string& path)
{
    assert(fonts.count(name) == 0);
    
#if FONT_USE_FREETYPE
    fonts[name] = make_unique<FontFT>(atlas.get(), path);
#else
    fonts[name] = make_unique<FontSTBTT>(atlas.get(), path);
#endif
}

Font* FontLibrary::getFont(const string& name)
{
    auto it = fonts.find(name);
    
    return (it == fonts.end()) ? nullptr : it->second.get();
}