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

static bool isRangeValid(unsigned long long start, unsigned int size, unsigned int wrap)
{
    return start / wrap == (start + size - 1) / wrap;
}

FontAtlas::LayoutShelf::LayoutShelf(unsigned int atlasWidth, unsigned int atlasHeight)
: atlasWidth(atlasWidth)
, atlasHeight(atlasHeight)
, areaBegin(0)
, areaEnd(atlasHeight)
, lineBegin(0)
, lineEnd(0)
, position(0)
{
}

optional<pair<unsigned int, unsigned long long>> FontAtlas::LayoutShelf::addRect(unsigned int width, unsigned int height)
{
    // Try to fit in the same line
    if (position + width <= atlasWidth && lineBegin + height <= areaEnd && isRangeValid(lineBegin, height, atlasHeight))
    {
        auto result = make_pair(position, lineBegin);
        
        position += width;
        lineEnd = max(lineEnd, lineBegin + height);
        
        return make_optional(result);
    }
    
    // Try to fit in the next line
    if (width <= atlasWidth && lineEnd + height <= areaEnd)
    {
        if (isRangeValid(lineEnd, height, atlasHeight))
        {
            auto result = make_pair(0u, lineEnd);
            
            position = width;
            lineBegin = lineEnd;
            lineEnd = lineEnd + height;
            
            return make_optional(result);
        }
        else
        {
            // Try to fit with a wraparound
            unsigned long long lineWrap = (lineEnd + height) / atlasHeight * atlasHeight;
            
            if (lineWrap + height <= areaEnd)
            {
                auto result = make_pair(0u, lineWrap);
                
                position = width;
                lineBegin = lineWrap;
                lineEnd = lineWrap + height;
                
                return make_optional(result);
            }
        }
    }
    
    // Fail
    return {};
}

optional<pair<unsigned long long, unsigned long long>> FontAtlas::LayoutShelf::growFreeArea(unsigned int desiredHeight)
{
    assert(areaBegin < areaEnd);
    assert(lineBegin <= lineEnd);
    assert(lineBegin >= areaBegin && lineEnd <= areaEnd);
    assert(areaEnd - areaBegin <= atlasHeight);
    
    unsigned int freeHeight = areaEnd - lineEnd;
    
    if (freeHeight < desiredHeight)
    {
        unsigned int difference = desiredHeight - freeHeight;
        
        areaBegin += difference;
        areaEnd += difference;
        
        // Move the layout state to make sure rectangles fit within the allowed area
        lineBegin = max(lineBegin, areaBegin);
        
        return make_optional(make_pair(areaBegin - difference, areaBegin));
    }
    
    return {};
}

FontAtlas::LayoutSkyline::LayoutSkyline(unsigned int atlasWidth, unsigned int atlasHeight)
: atlasWidth(atlasWidth)
, atlasHeight(atlasHeight)
, areaBegin(0)
, areaEnd(atlasHeight)
{
    skyline.push_back(make_pair(0, 0));
}

optional<pair<unsigned int, unsigned long long>> FontAtlas::LayoutSkyline::addRect(unsigned int width, unsigned int height)
{
    int bestindex = -1;
    unsigned long long besty = 0;
    unsigned int bestspan = 0;
    
    // Try to find a skyline fit without wrapping
    for (unsigned int index = 0; index < skyline.size(); ++index)
    {
        auto fr = tryFit(index, width, height);
        
        if (fr && (bestindex < 0 || fr->first < besty))
        {
            bestindex = index;
            besty = fr->first;
            bestspan = fr->second;
        }
    }
    
    if (bestindex >= 0)
    {
        unsigned int x = skyline[bestindex].first;
        
        if (bestindex + bestspan < skyline.size())
            skyline[bestindex + bestspan].first = x + width;
        
        skyline.erase(skyline.begin() + bestindex, skyline.begin() + bestindex + bestspan);
        skyline.insert(skyline.begin() + bestindex, make_pair(x, besty + height));
        
        return make_optional(make_pair(x, besty));
    }
    else
    {
        // Try to fit with a wraparound
        unsigned long long lineEnd = getLineEnd();
        unsigned long long lineWrap = (lineEnd + height) / atlasHeight * atlasHeight;
        
        if (lineWrap + height <= areaEnd)
        {
            skyline.clear();
            skyline.push_back(make_pair(0, lineWrap + height));
            skyline.push_back(make_pair(width, lineWrap));
        
            return make_optional(make_pair(0u, lineWrap));
        }
    }
    
    // Fail
    return {};
}

optional<pair<unsigned long long, unsigned long long>> FontAtlas::LayoutSkyline::growFreeArea(unsigned int desiredHeight)
{
    unsigned long long lineEnd = getLineEnd();

    assert(areaBegin < areaEnd);
    assert(lineEnd <= areaEnd);
    assert(areaEnd - areaBegin <= atlasHeight);
 
    unsigned int freeHeight = areaEnd - lineEnd;
    
    if (freeHeight < desiredHeight)
    {
        unsigned int difference = desiredHeight - freeHeight;
        
        areaBegin += difference;
        areaEnd += difference;
        
        // Move the layout state to make sure rectangles fit within the allowed area
        for (size_t i = 0; i < skyline.size(); ++i)
            skyline[i].second = max(skyline[i].second, areaBegin);
        
        return make_optional(make_pair(areaBegin - difference, areaBegin));
    }
    
    return {};
}

unsigned long long FontAtlas::LayoutSkyline::getLineEnd() const
{
    unsigned long long result = 0;
    
    for (unsigned int i = 0; i < skyline.size(); ++i)
        result = max(result, skyline[i].second);
    
    return result;
}

unsigned int FontAtlas::LayoutSkyline::getSegmentWidth(unsigned int index) const
{
    unsigned int edge = (index + 1 < skyline.size()) ? skyline[index + 1].first : atlasWidth;
    
    return edge - skyline[index].first;
}

optional<pair<unsigned long long, unsigned int>> FontAtlas::LayoutSkyline::tryFit(unsigned int index, unsigned int width, unsigned int height) const
{
    if (skyline[index].first + width <= atlasWidth)
    {
        unsigned long long y = 0;
        
        for (unsigned int i = index; i < skyline.size(); ++i)
        {
            y = max(y, skyline[i].second);
            
            unsigned int sw = getSegmentWidth(i);
            
            if (width <= sw)
            {
                if (y + height <= areaEnd && isRangeValid(y, height, atlasHeight))
                    return make_optional(make_pair(y, (i - index) + (width == sw)));
                else
                    return {};
            }
            
            width -= sw;
        }
    }
    
    return {};
}

FontAtlas::FontAtlas(unsigned int atlasWidth, unsigned int atlasHeight)
: layout(atlasWidth, atlasHeight)
{
    texture = make_unique<Texture>(Texture::Type_2D, Texture::Format_R8, atlasWidth, atlasHeight, 1, 1);
}

FontAtlas::~FontAtlas()
{
}
    
optional<Font::GlyphBitmap> FontAtlas::getBitmap(Font* font, float size, unsigned int cp)
{
    GlyphKey key = { font, size, cp };
    auto it = glyphs.find(key);
    
    return (it == glyphs.end()) ? optional<Font::GlyphBitmap>() : make_optional(it->second);
}

optional<Font::GlyphBitmap> FontAtlas::addBitmap(Font* font, float size, unsigned int cp, const Font::GlyphMetrics& metrics, unsigned int width, unsigned int height, const unsigned char* pixels)
{
    GlyphKey key = { font, size, cp };
        
    assert(glyphs.count(key) == 0);
    
    auto result = layout.addRect(width, height);
    
    if (result)
    {
        unsigned int x = result->first;
        unsigned int y = result->second % texture->getHeight();
        
        texture->upload(0, 0, 0, TextureRegion { x, y, 0, width, height, 1 }, pixels, width * height);
        
        Font::GlyphBitmap bitmap = { metrics, static_cast<short>(x), static_cast<short>(y), static_cast<short>(width), static_cast<short>(height) };
        
        glyphs[key] = bitmap;
        glyphsY.insert(make_pair(result->second, key));
        
        return make_optional(bitmap);
    }
    
    return {};
}

void FontAtlas::flush()
{
    // Make sure we have at least 1/3 of the texture free for filling
    if (auto range = layout.growFreeArea(texture->getHeight() / 3))
    {
        auto begin = glyphsY.lower_bound(range->first);
        auto end = glyphsY.upper_bound(range->second);
    
        for (auto it = begin; it != end; ++it)
            glyphs.erase(it->second);
        
        glyphsY.erase(begin, end);
    }
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

void FontLibrary::flush()
{
    atlas->flush();
}