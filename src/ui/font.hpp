#pragma once

class Texture;

class Font
{
public:
    struct FontMetrics
    {
        short ascender;
        short descender;
        short height;
    };
    
    struct GlyphMetrics
    {
        short bearingX, bearingY, advance;
    };
    
    struct GlyphBitmap
    {
        GlyphMetrics metrics;
        short x, y, w, h;
    };
    
    virtual ~Font();
    
    virtual FontMetrics getMetrics(float size) = 0;
    
    virtual optional<GlyphBitmap> getGlyphBitmap(float size, unsigned int cp) = 0;
    
    virtual short getKerning(float size, unsigned int cp1, unsigned int cp2) = 0;
};

class FontAtlas
{
public:
    FontAtlas(unsigned int atlasWidth, unsigned int atlasHeight);
    ~FontAtlas();
    
    optional<Font::GlyphBitmap> getBitmap(Font* font, float size, unsigned int cp);
    optional<Font::GlyphBitmap> addBitmap(Font* font, float size, unsigned int cp, const Font::GlyphMetrics& metrics, unsigned int width, unsigned int height, const unsigned char* pixels);
    
    Texture* getTexture() const { return texture.get(); }

private:
    struct GlyphKey
    {
        Font* font;
        float size;
        unsigned int cp;
        
        bool operator==(const GlyphKey& other) const;
    };
    
    struct GlyphKeyHash
    {
        size_t operator()(const GlyphKey& key) const;
    };
    
    optional<pair<unsigned int, unsigned int>> addBitmapData(unsigned int width, unsigned int height, const unsigned char* pixels);
    optional<pair<unsigned int, unsigned int>> layoutBitmap(unsigned int width, unsigned int height);
    
    unique_ptr<Texture> texture;
    
    unordered_map<GlyphKey, Font::GlyphBitmap, GlyphKeyHash> glyphs;
    
    unsigned int layoutX;
    unsigned int layoutY;
    unsigned int layoutNextY;
};

class FontLibrary
{
public:
    FontLibrary(unsigned int atlasWidth, unsigned int atlasHeight);
    ~FontLibrary();
    
    void addFont(const string& name, const string& path, bool freetype);
    Font* getFont(const string& name);
    
    Texture* getTexture() const { return atlas->getTexture(); }
    
private:
    unique_ptr<FontAtlas> atlas;
    
    unordered_map<string, unique_ptr<Font>> fonts;
};