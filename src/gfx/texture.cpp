#include "texture.hpp"

#include "fs/path.hpp"
#include "gfx/image.hpp"

#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

#include <fstream>

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
    assert((type != Type_2D && type != Type_Cube) || depth == 1);
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
: basePath(Path::full(basePath))
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

shared_ptr<Texture> TextureManager::loadTexture(const string& path)
{
    ifstream in(path, ios::in | ios::binary);
    unique_ptr<Image> image = Image::load(in);
    
    shared_ptr<Texture> result = make_shared<Texture>(image->getType(), image->getFormat(), image->getWidth(), image->getHeight(), image->getDepth(), image->getMipLevels());
    
   for (unsigned int index = 0; index < image->getLayers(); ++index)
    {
        for (unsigned int face = 0; face < image->getFaces(); ++face)
        {
            for (unsigned int mip = 0; mip < image->getMipLevels(); ++mip)
            {
                unsigned int mipWidth = Texture::getMipSide(image->getWidth(), mip);
                unsigned int mipHeight = Texture::getMipSide(image->getHeight(), mip);
                unsigned int mipDepth = image->getType() == Texture::Type_3D ? Texture::getMipSide(image->getDepth(), mip) : 1;
                
                unsigned int mipSize = Texture::getImageSize(image->getFormat(), mipWidth, mipHeight) * mipDepth;
                
                unsigned char* mipData = image->getData(index, face, mip);
                
                result->upload(0, 0, mip, TextureRegion {0, 0, 0, mipWidth, mipHeight, mipDepth}, mipData, mipSize);
            }
        }
    }
 
    return result;
}

void TextureManager::onFileChanged(const string& path)
{
    string prefix = basePath + "/";
    
    if (path.compare(0, prefix.length(), prefix) == 0)
    {
        string name = path.substr(prefix.length());
        
        auto it = textures.find(name);
        if (it != textures.end())
        {
            printf("Reloading textures %s\n", name.c_str());
            
            try
            {
                shared_ptr<Texture> texture = loadTexture(basePath + "/" + name);
                
                textures[name] = texture;
            }
            catch (exception& e)
            {
                printf("Error loading texture %s: %s\n", name.c_str(), e.what());
            }
        }
    }
}
