#pragma once

#include "fs/folderwatcher.hpp"

struct TextureRegion
{
    unsigned int x;
    unsigned int y;
    unsigned int z;

    unsigned int width;
    unsigned int height;
    unsigned int depth;
};

class Texture: public noncopyable
{
public:
    enum Type
	{
        Type_2D,
        Type_Array2D,
        Type_Cube,
        Type_ArrayCube,
        Type_3D,

        Type_Count
	};

    enum Format
	{
        Format_R8,
        Format_RGBA8,
        Format_RGBA16F,
        Format_BC1,
        Format_BC2,
        Format_BC3,
        Format_BC4,
        Format_BC5,
        Format_D24S8,

        Format_Count
	};

	Texture(Type type, Format format, unsigned int width, unsigned int height, unsigned int depth, unsigned int mipLevels);
    ~Texture();

    void upload(unsigned int index, unsigned int face, unsigned int mip, const TextureRegion& region, const void* data, unsigned int size);
    void download(unsigned int index, unsigned int face, unsigned int mip, void* data, unsigned int size);
    
    void bind(unsigned int stage);

	Type getType() const { return type; }
	Format getFormat() const { return format; }

	unsigned int getWidth() const { return width; }
	unsigned int getHeight() const { return height; }
	unsigned int getDepth() const { return depth; }
    
	unsigned int getMipLevels() const { return mipLevels; }

    static bool isFormatCompressed(Format format);
    static bool isFormatDepth(Format format);
    static unsigned int getImageSize(Format format, unsigned int width, unsigned int height);

    static unsigned int getMipSide(unsigned int value, unsigned int mip);
    static unsigned int getMaxMipCount(unsigned int width, unsigned int height, unsigned int depth);

private:
    unsigned int id;
    
    Type type;
    Format format;
    unsigned int width;
    unsigned int height;
    unsigned int depth;
    unsigned int mipLevels;
};

class TextureRef
{
public:
    TextureRef() {}
	TextureRef(const shared_ptr<Texture>& texture);

    void updateAllRefs(const shared_ptr<Texture>& texture);

    const shared_ptr<Texture>& get() const
	{
        return data ? *data : emptyTexture;
	}

private:
    shared_ptr<shared_ptr<Texture>> data;

    static shared_ptr<Texture> emptyTexture;
};

class TextureManager: public FolderWatcher::Listener
{
public:
    TextureManager(const string& basePath, FolderWatcher* watcher);
    ~TextureManager();
    
    TextureRef get(const string& name);
    
private:
    void onFileChanged(const string& path) override;
    
    shared_ptr<Texture> loadTexture(const string& path);
    
    string basePath;
    
    FolderWatcher* watcher;
    
    unordered_map<string, TextureRef> textures;
};