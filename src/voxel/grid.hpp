#pragma once

namespace voxel
{
    struct Cell
    {
        unsigned char occupancy;
        unsigned char material;
    };
    
    class Region
    {
    public:
        Region(const glm::i32vec3& begin, const glm::i32vec3& end)
        : begin_(begin)
        , end_(end)
        {
            assert(begin.x <= end.x && begin.y <= end.y && begin.z <= end.z);
        }
        
        Region(const glm::i32vec3& begin, unsigned int size)
        : begin_(begin)
        , end_(begin + int(size))
        {
        }
        
        const glm::i32vec3& begin() const { return begin_; }
        const glm::i32vec3& end() const { return end_; }
        
        glm::i32vec3 size() const { return end_ - begin_; }
 
        bool empty() const { return begin_.x == end_.x || begin_.y == end_.y || begin_.z == end_.z; }
        
        Region intersect(const Region& other) const;
    
    private:
        glm::i32vec3 begin_;
        glm::i32vec3 end_;
    };
    
    class Box
    {
    public:
        Box(unsigned int width, unsigned int height, unsigned int depth);
        
        Cell& operator()(unsigned int x, unsigned int y, unsigned int z)
        {
            assert(x < width && y < height && z < depth);
            return data[x + width * (y + height * z)];
        }
        
        const Cell& operator()(unsigned int x, unsigned int y, unsigned int z) const
        {
            assert(x < width && y < height && z < depth);
            return data[x + width * (y + height * z)];
        }
        
        unsigned int getWidth() const { return width; }
        unsigned int getHeight() const { return height; }
        unsigned int getDepth() const { return depth; }
        
    private:
        unsigned int width;
        unsigned int height;
        unsigned int depth;
        
        unique_ptr<Cell[]> data;
    };
    
    class Grid
    {
    public:
        Box read(const Region& region) const;
        void write(const Region& region, const Box& box);
    
    private:
        struct Chunk
        {
            Box box;
            
            Chunk();
        };
        
        unordered_map<glm::i32vec3, Chunk> chunks;
    };
}