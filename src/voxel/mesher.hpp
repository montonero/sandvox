#pragma once

namespace voxel
{
    class Box;
    
    struct MeshVertex
    {
        vec3 position;
        vec3 normal;
    };
    
    struct MeshOptions
    {
    };
    
    class Mesher
    {
    public:
        virtual ~Mesher() {}
        
        virtual pair<vector<MeshVertex>, vector<unsigned int>> generate(const Box& box, const vec3& offset, float cellSize, const MeshOptions& options) = 0;
    };
    
    unique_ptr<Mesher> createMesherSurfaceNets();
    unique_ptr<Mesher> createMesherMarchingCubes();
}