#include "texture.hpp"

#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

#include "stb_image.h"

struct TextureFormatGL
{
    unsigned int bits;
    GLenum internalFormat;
    GLenum dataFormat;
    GLenum dataType;
};

static const GLenum kTextureTarget[Texture::Type_Count] =
{
	GL_TEXTURE_2D,
    GL_TEXTURE_2D_ARRAY,
	GL_TEXTURE_CUBE_MAP,
	GL_TEXTURE_CUBE_MAP_ARRAY,
	GL_TEXTURE_3D,
};

static const TextureFormatGL kTextureFormat[Texture::Format_Count] =
{
	{ 32, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE },
	{ 64, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT },
	{ 64, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0, 0 },
	{ 128, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 0, 0 },
	{ 128, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 0, 0 },
	{ 128, GL_COMPRESSED_RED_RGTC1, 0, 0 },
	{ 128, GL_COMPRESSED_RG_RGTC2, 0, 0 },
	{ GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8 },
};

Texture::Texture(Type type, Format format, unsigned int width, unsigned int height, unsigned int depth, unsigned int mipLevels)
: id(0)
, type(type)
, format(format)
, width(width)
, height(height)
, depth(depth)
, mipLevels(mipLevels)
{
    assert(width > 0 && height > 0 && depth > 0);
    assert(mipLevels > 0 && mipLevels <= getMaxMipCount(width, height, type == Type_3D ? depth : 1));
    
    GLenum target = kTextureTarget[type];
    
	const TextureFormatGL& desc = kTextureFormat[format];

    glGenTextures(1, &id);
    
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(target, id);

    if (type == Type_2D || type == Type_Cube)
    {
        glTexStorage2D(target, mipLevels, desc.internalFormat, width, height);
    }
    else
    {
        glTexStorage3D(target, mipLevels, desc.internalFormat, width, height, depth);
    }

	glBindTexture(target, 0);
}

Texture::~Texture()
{
    glDeleteTextures(1, &id);
}

void Texture::upload(unsigned int index, unsigned int face, unsigned int mip, const TextureRegion& region, const void* data, unsigned int size)
{
    assert(index < ((type == Type_Array2D || type == Type_ArrayCube) ? depth : 1));
    assert(face < ((type == Type_Cube || type == Type_ArrayCube) ? 6 : 1));
    assert(mip < mipLevels);
    
#ifndef NDEBUG
    unsigned int mipWidth = getMipSide(width, mip);
    unsigned int mipHeght = getMipSide(height, mip);
    unsigned int mipDepth = (type == Type_3D) ? getMipSide(depth, mip) : 1;
#endif
    
    assert(region.x + region.width <= mipWidth && region.y + region.height <= mipHeight && region.z + region.depth <= mipDepth);
    assert(size == getImageSize(format, mipWidth, mipHeight) * mipDepth);
    
    GLenum target = kTextureTarget[type];
    GLenum faceTarget = (type == Type_Cube || type == Type_ArrayCube) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face : target;
    
	const TextureFormatGL& desc = kTextureFormat[format];
    
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(target, id);
    
    if (isFormatCompressed(format))
    {
        assert((region.x | region.y | region.width | region.height) % 4 == 0);
        
        if (type == Type_2D || type == Type_Cube)
        {
            glCompressedTexSubImage2D(faceTarget, mip, region.x, region.y, region.width, region.height, desc.internalFormat, size, data);
        }
        else
        {
            glCompressedTexSubImage3D(faceTarget, mip, region.x, region.y, region.z, region.width, region.height, region.depth, desc.internalFormat, size, data);
        }
    }
    else
    {
        if (type == Type_2D || type == Type_Cube)
        {
            glTexSubImage2D(faceTarget, mip, region.x, region.y, region.width, region.height, desc.dataFormat, desc.dataType, data);
        }
        else
        {
            glTexSubImage3D(faceTarget, mip, region.x, region.y, region.z, region.width, region.height, region.depth, desc.dataFormat, desc.dataType, data);
        }
    }
    
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
	glBindTexture(target, 0);
}

void Texture::bind(unsigned int stage)
{
    GLenum target = kTextureTarget[type];
    
	glActiveTexture(GL_TEXTURE0 + stage);
    glBindTexture(target, id);
}

bool Texture::isFormatCompressed(Format format)
{
    return
        format == Format_BC1 ||
        format == Format_BC2 ||
        format == Format_BC3 ||
        format == Format_BC4 ||
        format == Format_BC5;
}

bool Texture::isFormatDepth(Format format)
{
    return
        format == Format_D24S8;
}

unsigned int Texture::getImageSize(Format format, unsigned int width, unsigned int height)
{
    const TextureFormatGL& desc = kTextureFormat[format];
    
    if (isFormatCompressed(format))
        return ((width + 3) / 4) * ((height + 3) / 4) * (desc.bits / 8);
    else
        return width * height * (desc.bits / 8);
}

unsigned int Texture::getMipSide(unsigned int value, unsigned int mip)
{
    return max(value >> mip, 1u);
}

unsigned int Texture::getMaxMipCount(unsigned int width, unsigned int height, unsigned int depth)
{
    unsigned int side = max(width, max(height, depth));
    
    return sizeof(unsigned int) * 8 - __builtin_clz(side);
}

shared_ptr<Texture> TextureRef::emptyTexture = shared_ptr<Texture>();

TextureRef::TextureRef(const shared_ptr<Texture>& texture)
    : data(make_shared<shared_ptr<Texture>>(texture))
{
}

void TextureRef::updateAllRefs(const shared_ptr<Texture>& texture)
{
    assert(data);
    
    *data = texture;
}

TextureManager::TextureManager(const string& basePath, FolderWatcher* watcher)
: basePath(basePath)
, watcher(watcher)
{
    if (watcher)
        watcher->addListener(this);
}

TextureManager::~TextureManager()
{
    if (watcher)
        watcher->removeListener(this);
}
    
TextureRef TextureManager::get(const string& name)
{
    auto it = textures.find(name);
    if (it != textures.end())
        return it->second;
    
    try
    {
        shared_ptr<Texture> texture = loadTexture(basePath + "/" + name);
        
        return textures[name] = texture;
    }
    catch (exception& e)
    {
        printf("Error loading texture %s: %s\n", name.c_str(), e.what());
        
        return textures[name] = TextureRef();
    }
}

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

shared_ptr<Texture> TextureManager::loadTexture(const string& path)
{
    int width, height;
    unique_ptr<unsigned char, void (*)(void*)> data(stbi_load(path.c_str(), &width, &height, nullptr, 4), stbi_image_free);
    
    if (!data)
    {
        const char* reason = stbi_failure_reason();
        
        throw runtime_error(reason ? reason : "unknown error");
    }
    
    Texture::Format format = Texture::Format_RGBA8;
    unsigned int mipLevels = Texture::getMaxMipCount(width, height, 1);
    
    unsigned int mipBufferSize = Texture::getImageSize(format, Texture::getMipSide(width, 1), Texture::getMipSide(height, 1));
    unique_ptr<unsigned char[]> mipBuffer(new unsigned char[mipBufferSize]);
    
    shared_ptr<Texture> result = make_shared<Texture>(Texture::Type_2D, format, width, height, 1, mipLevels);
    
    for (unsigned int mip = 0; mip < mipLevels; ++mip)
    {
        unsigned int mipWidth = Texture::getMipSide(width, mip);
        unsigned int mipHeight = Texture::getMipSide(height, mip);
        unsigned int mipSize = Texture::getImageSize(format, mipWidth, mipHeight);
        
        if (mip != 0)
        {
            unsigned int mipWidthPrev = Texture::getMipSide(width, mip - 1);
            unsigned int mipHeightPrev = Texture::getMipSide(height, mip - 1);
            
            scaleLinear(mipBuffer.get(), mipWidth, mipHeight, data.get(), mipWidthPrev, mipHeightPrev);
            memcpy(data.get(), mipBuffer.get(), mipSize);
        }
        
        result->upload(0, 0, mip, TextureRegion {0, 0, 0, mipWidth, mipHeight, 1}, data.get(), mipSize);
    }
    
    return result;
}

void TextureManager::onFileChanged(const string& path)
{
}
