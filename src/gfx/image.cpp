#include "common.hpp"
#include "gfx/image.hpp"

#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <istream>

static void scaleLinear(unsigned char* target, unsigned int targetWidth, unsigned int targetHeight, const unsigned char* source, unsigned int sourceWidth, unsigned int sourceHeight)
{
    typedef unsigned long long uint64;
    typedef unsigned int T;

    // srcdata stays at beginning of slice, pdst is a moving pointer
    const unsigned char* srcdata = source;
    unsigned char* pdst = target;

    // sx_48,sy_48 represent current position in source
    // using 16/48-bit fixed precision, incremented by steps
    uint64 stepx = ((uint64)sourceWidth << 48) / targetWidth;
    uint64 stepy = ((uint64)sourceHeight << 48) / targetHeight;
    
    // bottom 28 bits of temp are 16/12 bit fixed precision, used to
    // adjust a source coordinate backwards by half a pixel so that the
    // integer bits represent the first sample (eg, sx1) and the
    // fractional bits are the blend weight of the second sample
    unsigned int temp;
    
    uint64 sy_48 = (stepy >> 1) - 1;
    for (unsigned int y = 0; y < targetHeight; y++, sy_48+=stepy)
    {
        temp = static_cast<unsigned int>(sy_48 >> 36);
        temp = (temp > 0x800) ? temp - 0x800 : 0;
        unsigned int syf = temp & 0xFFF;
        unsigned int sy1 = temp >> 12;
        unsigned int sy2 = std::min(sy1+1, sourceHeight-1);
        unsigned int syoff1 = sy1 * sourceWidth;
        unsigned int syoff2 = sy2 * sourceWidth;

        uint64 sx_48 = (stepx >> 1) - 1;
        for (unsigned int x = 0; x < targetWidth; x++, sx_48+=stepx)
        {
            temp = static_cast<unsigned int>(sx_48 >> 36);
            temp = (temp > 0x800) ? temp - 0x800 : 0;
            unsigned int sxf = temp & 0xFFF;
            unsigned int sx1 = temp >> 12;
            unsigned int sx2 = std::min(sx1+1, sourceWidth-1);

            unsigned int sxfsyf = sxf*syf;
            for (unsigned int k = 0; k < sizeof(T); k++)
            {
                unsigned int accum =
                    srcdata[(sx1 + syoff1)*sizeof(T)+k]*(0x1000000-(sxf<<12)-(syf<<12)+sxfsyf) +
                    srcdata[(sx2 + syoff1)*sizeof(T)+k]*((sxf<<12)-sxfsyf) +
                    srcdata[(sx1 + syoff2)*sizeof(T)+k]*((syf<<12)-sxfsyf) +
                    srcdata[(sx2 + syoff2)*sizeof(T)+k]*sxfsyf;

                // accum is computed using 8/24-bit fixed-point math
                // (maximum is 0xFF000000; rounding will not cause overflow)
                *pdst++ = static_cast<unsigned char>((accum + 0x800000) >> 24);
            }
        }
    }
}

static int stbioRead(void* user, char* data, int size)
{
    istream& in = *static_cast<istream*>(user);
    
    in.read(data, size);
    return in.gcount();
}

static void stbioSkip(void* user, unsigned int n)
{
    istream& in = *static_cast<istream*>(user);
    
    in.seekg(n, ios::cur);
}

static int stbioEof(void* user)
{
    istream& in = *static_cast<istream*>(user);
    
    return in.eof();
}

unique_ptr<Image> Image::load(istream& in)
{
    stbi_io_callbacks callbacks = { stbioRead, stbioSkip, stbioEof };
    
    int width, height;
    unique_ptr<unsigned char, void (*)(void*)> data(stbi_load_from_callbacks(&callbacks, &in, &width, &height, nullptr, 4), stbi_image_free);
    
    if (!data)
    {
        const char* reason = stbi_failure_reason();
        
        throw runtime_error(reason ? reason : "unknown error");
    }
    
    Texture::Format format = Texture::Format_RGBA8;
    unsigned int mipLevels = Texture::getMaxMipCount(width, height, 1);
    
    unique_ptr<Image> result = make_unique<Image>(Texture::Type_2D, format, width, height, 1, mipLevels);
    
    for (unsigned int mip = 0; mip < mipLevels; ++mip)
    {
        unsigned int mipWidth = Texture::getMipSide(width, mip);
        unsigned int mipHeight = Texture::getMipSide(height, mip);
        unsigned int mipSize = Texture::getImageSize(format, mipWidth, mipHeight);
        
        unsigned char* mipData = result->getData(0, 0, mip);
        
        if (mip == 0)
        {
            memcpy(mipData, data.get(), mipSize);
        }
        else
        {
            unsigned int mipWidthPrev = Texture::getMipSide(width, mip - 1);
            unsigned int mipHeightPrev = Texture::getMipSide(height, mip - 1);
            
            unsigned char* mipDataPrev = result->getData(0, 0, mip - 1);
            
            scaleLinear(mipData, mipWidth, mipHeight, mipDataPrev, mipWidthPrev, mipHeightPrev);
        }
    }
    
    return result;
}

Image::Image(Texture::Type type, Texture::Format format, unsigned int width, unsigned int height, unsigned int depth, unsigned int mipLevels)
: type(type)
, format(format)
, width(width)
, height(height)
, depth(depth)
, mipLevels(mipLevels)
{
    assert(width > 0 && height > 0 && depth > 0);
    assert((type != Texture::Type_2D && type != Texture::Type_Cube) || depth == 1);
    assert(mipLevels > 0 && mipLevels <= Texture::getMaxMipCount(width, height, type == Texture::Type_3D ? depth : 1));
    
    for (unsigned int index = 0; index < getLayers(); ++index)
    {
        for (unsigned int face = 0; face < getFaces(); ++face)
        {
            for (unsigned int mip = 0; mip < mipLevels; ++mip)
            {
                unsigned int mipWidth = Texture::getMipSide(width, mip);
                unsigned int mipHeight = Texture::getMipSide(height, mip);
                unsigned int mipDepth = type == Texture::Type_3D ? Texture::getMipSide(depth, mip) : 1;
                
                unsigned int mipSize = Texture::getImageSize(format, mipWidth, mipHeight) * mipDepth;
                
                data.push_back(unique_ptr<unsigned char[]>(new unsigned char[mipSize]));
            }
        }
    }
}

unsigned char* Image::getData(unsigned int index, unsigned int face, unsigned int mip)
{
    unsigned int slot = mip + mipLevels * (face + getFaces() * index);
    
    assert(slot < data.size());
    return data[slot].get();
}

void Image::saveToPNG(const string& path)
{
    assert(type == Texture::Type_2D);
    assert(format == Texture::Format_R8 || format == Texture::Format_RGBA8);
    
    int comp = (format == Texture::Format_R8) ? 1 : 4;
    
    stbi_write_png(path.c_str(), width, height, comp, getData(0, 0, 0), width * comp);
}

unsigned int Image::getFaces() const
{
    return (type == Texture::Type_Cube || type == Texture::Type_ArrayCube) ? 6 : 1;
}

unsigned int Image::getLayers() const
{
    return type == Texture::Type_3D ? 1 : depth;
}
