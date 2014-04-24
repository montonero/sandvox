#pragma once

#include "gfx/texture.hpp"

class Image
{
public:
    static unique_ptr<Image> load(istream& in);
    
	Image(Texture::Type type, Texture::Format format, unsigned int width, unsigned int height, unsigned int depth, unsigned int mipLevels);
    
    unsigned char* getData(unsigned int index, unsigned int face, unsigned int mip);
 
    Texture::Type getType() const { return type; }
    Texture::Format getFormat() const { return format; }
    unsigned int getWidth() const { return width; }
    unsigned int getHeight() const { return height; }
    unsigned int getDepth() const { return depth; }
    unsigned int getMipLevels() const { return mipLevels; }
    
    unsigned int getFaces() const;
    unsigned int getLayers() const;

private:
    Texture::Type type;
    Texture::Format format;
    unsigned int width;
    unsigned int height;
    unsigned int depth;
    unsigned int mipLevels;
    
    vector<unique_ptr<unsigned char[]>> data;
};