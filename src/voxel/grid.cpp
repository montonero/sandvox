#include "common.hpp"
#include "voxel/grid.hpp"

namespace voxel
{
    const unsigned int kChunkSizeLog2 = 5;
    const unsigned int kChunkSize = 1 << kChunkSizeLog2;
    
    Region Region::intersect(const Region& other) const
    {
        glm::i32vec3 ibegin = glm::max(begin(), other.begin());
        glm::i32vec3 iend = glm::min(end(), other.end());
        
        return Region(ibegin, glm::max(ibegin, iend));
    }
 
    Box::Box(unsigned int width, unsigned int height, unsigned int depth)
    : width(width)
    , height(height)
    , depth(depth)
    {
        data.reset(new Cell[width * height * depth]);
        
        fill(data.get(), data.get() + width * height * depth, Cell { 0, 0 });
    }
    
    static vector<glm::i32vec3> getChunkIds(const Region& region)
    {
        if (region.empty())
            return {};
        
        vector<glm::i32vec3> result;
        
        glm::i32vec3 min = region.begin() >> int(kChunkSizeLog2);
        glm::i32vec3 max = (region.end() - 1) >> int(kChunkSizeLog2);
        
        for (int z = min.z; z <= max.z; ++z)
            for (int y = min.y; y <= max.y; ++y)
                for (int x = min.x; x <= max.x; ++x)
                    result.push_back(glm::i32vec3(x, y, z));
        
        return result;
    }
    
    static void copyCells(Box& targetBox, const Region& targetRegion, const Box& sourceBox, const Region& sourceRegion)
    {
        Region region = sourceRegion.intersect(targetRegion);
        
        if (region.empty())
            return;
        
        glm::ivec3 sourceOffset = region.begin() - sourceRegion.begin();
        glm::ivec3 targetOffset = region.begin() - targetRegion.begin();
        
        glm::ivec3 size = region.size();
        
        for (int z = 0; z < size.z; ++z)
            for (int y = 0; y < size.y; ++y)
            {
                const Cell& sourceRow = sourceBox(sourceOffset.x, sourceOffset.y + y, sourceOffset.z + z);
                Cell& targetRow = targetBox(targetOffset.x, targetOffset.y + y, targetOffset.z + z);
                
                memcpy(&targetRow, &sourceRow, size.x * sizeof(Cell));
            }
    }
    
    Grid::Chunk::Chunk()
    : box(kChunkSize, kChunkSize, kChunkSize)
    {
    }
    
    Box Grid::read(const Region& region) const
    {
        Box result(region.size().x, region.size().y, region.size().z);
        
        vector<glm::i32vec3> chunkIds = getChunkIds(region);
        
        for (auto cid: chunkIds)
        {
            auto cit = chunks.find(cid);
            
            if (cit != chunks.end())
            {
                const Chunk& chunk = cit->second;
                Region chunkRegion(cid << int(kChunkSizeLog2), kChunkSize);
                
                copyCells(result, region, chunk.box, chunkRegion);
            }
        }
        
        return result;
    }
    
    void Grid::write(const Region& region, const Box& box)
    {
        assert(region.size() == glm::i32vec3(box.getWidth(), box.getHeight(), box.getDepth()));
        
        vector<glm::i32vec3> chunkIds = getChunkIds(region);
        
        for (auto cid: chunkIds)
        {
            Chunk& chunk = chunks[cid];
            Region chunkRegion(cid << int(kChunkSizeLog2), kChunkSize);
            
            copyCells(chunk.box, chunkRegion, box, region);
        }
    }
}