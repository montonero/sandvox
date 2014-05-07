#include "common.hpp"
#include "voxel/mesher.hpp"

#include "voxel/grid.hpp"

namespace voxel
{
    namespace surfacenets
    {
        static const unsigned short kEdgeTable[] =
        {
            0x000, 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
            0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
            0x190, 0x099, 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
            0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
            0x230, 0x339, 0x033, 0x13a, 0x636, 0x73f, 0x435, 0x53c,
            0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
            0x3a0, 0x2a9, 0x1a3, 0x0aa, 0x7a6, 0x6af, 0x5a5, 0x4ac,
            0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
            0x460, 0x569, 0x663, 0x76a, 0x066, 0x16f, 0x265, 0x36c,
            0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
            0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0x0ff, 0x3f5, 0x2fc,
            0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
            0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x055, 0x15c,
            0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
            0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0x0cc,
            0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
            0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
            0x0cc, 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
            0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
            0x15c, 0x055, 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
            0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
            0x2fc, 0x3f5, 0x0ff, 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
            0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
            0x36c, 0x265, 0x16f, 0x066, 0x76a, 0x663, 0x569, 0x460,
            0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
            0x4ac, 0x5a5, 0x6af, 0x7a6, 0x0aa, 0x1a3, 0x2a9, 0x3a0,
            0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
            0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x033, 0x339, 0x230,
            0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
            0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x099, 0x190,
            0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
            0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x000
        };
 
        static const unsigned char kVertexIndexTable[][3] =
        {
            {0, 0, 0},
            {1, 0, 0},
            {1, 1, 0},
            {0, 1, 0},
            {0, 0, 1},
            {1, 0, 1},
            {1, 1, 1},
            {0, 1, 1},
        };

        static const unsigned char kEdgeIndexTable[][2] =
        {
            {0, 1},
            {1, 2},
            {2, 3},
            {3, 0},
            {4, 5},
            {5, 6},
            {6, 7},
            {7, 4},
            {0, 4},
            {1, 5},
            {2, 6},
            {3, 7}
        };
 
        struct GridVertex
        {
            float iso;
            float nx, ny, nz;
        };

        template <typename LerpK> struct AdjustableNaiveTraits
        {
            static pair<vec3, vec3> intersect(const GridVertex& g0, const GridVertex& g1, float isolevel, const vec3& corner, const vec3& v0, const vec3& v1)
            {
                float t =
                    (fabsf(g0.iso - g1.iso) > 0.0001)
                    ? (isolevel - g0.iso) / (g1.iso - g0.iso)
                    : 0;
                
                return make_pair(glm::mix(v0, v1, t), glm::normalize(glm::mix(vec3(g0.nx, g0.ny, g0.nz), vec3(g1.nx, g1.ny, g1.nz), t)));
            }
            
            static pair<vec3, vec3> average(const pair<vec3, vec3>* points, size_t count, const vec3& v0, const vec3& v1, const vec3& corner)
            {
                vec3 position;
                vec3 normal;
                
                for (size_t i = 0; i < count; ++i)
                {
                    position += points[i].first;
                    normal += points[i].second;
                }
                
                float n = 1.f / count;
                
                return make_pair(LerpK()(corner, position * n, (v0 + v1) / 2.f), normal * n);
            }
        };
        
        MeshVertex normalLerp(const MeshVertex& v, const vec3& avgp, const vec3& qn)
        {
            float k = glm::dot(v.position - avgp, v.position - avgp);
            
            k = 1 - (1 - k) * (1 - k);
            
            return MeshVertex {v.position, glm::normalize(glm::mix(v.normal, qn, glm::clamp(k, 0.f, 1.f)))};
        }
        
        float norm(const vec3& v)
        {
            return v.x * v.x + v.y * v.y + v.z * v.z;
        }
        
        vec3 getQuadNormal(const vec3& v0, const vec3& v1, const vec3& v2, const vec3& v3)
        {
            float v01 = norm(v0 - v1);
            float v12 = norm(v1 - v2);
            float v23 = norm(v2 - v3);
            float v30 = norm(v3 - v0);
            
            float v012 = v01 * v12;
            float v123 = v12 * v23;
            float v230 = v23 * v30;
            float v301 = v30 * v01;
            
            if (v012 > v123 && v012 > v230 && v012 > v301)
            {
                return glm::normalize(glm::cross(v1 - v0, v2 - v0));
            }
            else if (v123 > v230 && v123 > v301)
            {
                return glm::normalize(glm::cross(v2 - v1, v3 - v1));
            }
            else if (v230 > v301)
            {
                return glm::normalize(glm::cross(v3 - v2, v0 - v2));
            }
            else
            {
                return glm::normalize(glm::cross(v0 - v3, v1 - v3));
            }
        }
        
        void pushQuad(vector<MeshVertex>& vb, vector<unsigned int>& ib,
            const pair<vec3, MeshVertex>& v0, const pair<vec3, MeshVertex>& v1, const pair<vec3, MeshVertex>& v2, const pair<vec3, MeshVertex>& v3,
            bool flip)
        {
            size_t offset = vb.size();
            
            vec3 qn = (flip ? -1.f : 1.f) * getQuadNormal(v0.second.position, v1.second.position, v2.second.position, v3.second.position);
            
            vb.push_back(normalLerp(v0.second, v0.first, qn));
            vb.push_back(normalLerp(v1.second, v1.first, qn));
            vb.push_back(normalLerp(v2.second, v2.first, qn));
            vb.push_back(normalLerp(v3.second, v3.first, qn));
            
            if (!flip)
            {
                ib.push_back(offset + 0);
                ib.push_back(offset + 2);
                ib.push_back(offset + 1);
                ib.push_back(offset + 0);
                ib.push_back(offset + 3);
                ib.push_back(offset + 2);
            }
            else
            {
                ib.push_back(offset + 0);
                ib.push_back(offset + 1);
                ib.push_back(offset + 2);
                ib.push_back(offset + 0);
                ib.push_back(offset + 2);
                ib.push_back(offset + 3);
            }
        }
        
        struct AdjustableLerpKSmooth
        {
            vec3 operator()(const vec3& corner, const vec3& smoothpt, const vec3& centerpt) const
            {
                return smoothpt;
            }
        };

        struct AdjustableLerpKConstant
        {
            vec3 operator()(const vec3& corner, const vec3& smoothpt, const vec3& centerpt) const
            {
                return glm::mix(smoothpt, centerpt, 0.5f);
            }
        };

        struct AdjustableLerpKRandom
        {
            vec3 operator()(const vec3& corner, const vec3& smoothpt, const vec3& centerpt) const
            {
                return glm::clamp(glm::mix(smoothpt, centerpt + (vec3(rand() / float(RAND_MAX), rand() / float(RAND_MAX), rand() / float(RAND_MAX)) * 2.f - vec3(1.f)) * 0.3f, 0.5f), vec3(0.f), vec3(1.f));
            }
        };

        struct AdjustableLerpKQuantize
        {
            static float quantize(float value, float bound)
            {
                return floor(value / bound) * bound;
            }
            
            vec3 operator()(const vec3& corner, const vec3& smoothpt, const vec3& centerpt) const
            {
                float bound = 1.f / 3.f;
                
                return vec3(quantize(smoothpt.x, bound), quantize(smoothpt.y, bound), quantize(smoothpt.z, bound));
            }
        };

        class Mesher: public voxel::Mesher
        {
            pair<vector<MeshVertex>, vector<unsigned int>> generate(const Box& box, const vec3& offset, float cellSize, const MeshOptions& options) override
            {
                typedef AdjustableNaiveTraits<AdjustableLerpKSmooth> Traits;
                
                const float isolevel = 0.5f / 255.f;
                
                unsigned int sizeX = box.getWidth(), sizeY = box.getHeight(), sizeZ = box.getDepth();
                assert(sizeX > 2 && sizeY > 2 && sizeZ > 2);
                
                unique_ptr<GridVertex[]> grid(new GridVertex[sizeX * sizeY * sizeZ]);
                
                for (int z = 0; z < sizeZ; ++z)
                    for (int y = 0; y < sizeY; ++y)
                        for (int x = 0; x < sizeX; ++x)
                        {
                            GridVertex& gv = grid[x + sizeX * (y + sizeY * z)];
                            
                            gv.iso = box(x, y, z).occupancy / 255.f;
                            gv.nx = 0;
                            gv.ny = 0;
                            gv.nz = 1;
                        }
                
                unique_ptr<pair<vec3, MeshVertex>[]> gv(new pair<vec3, MeshVertex>[sizeX * sizeY * sizeZ]);
                
                for (int z = 0; z + 1 < sizeZ; ++z)
                    for (int y = 0; y + 1 < sizeY; ++y)
                        for (int x = 0; x + 1 < sizeX; ++x)
                        {
                            const GridVertex& v000 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))];
                            const GridVertex& v100 = grid[(x + 1) + sizeX * ((y + 0) + sizeY * (z + 0))];
                            const GridVertex& v110 = grid[(x + 1) + sizeX * ((y + 1) + sizeY * (z + 0))];
                            const GridVertex& v010 = grid[(x + 0) + sizeX * ((y + 1) + sizeY * (z + 0))];
                            const GridVertex& v001 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 1))];
                            const GridVertex& v101 = grid[(x + 1) + sizeX * ((y + 0) + sizeY * (z + 1))];
                            const GridVertex& v111 = grid[(x + 1) + sizeX * ((y + 1) + sizeY * (z + 1))];
                            const GridVertex& v011 = grid[(x + 0) + sizeX * ((y + 1) + sizeY * (z + 1))];
                            
                            // get cube index from grid values
                            int cubeindex = 0;
                            
                            if (v000.iso < isolevel) cubeindex |= 1 << 0;
                            if (v100.iso < isolevel) cubeindex |= 1 << 1;
                            if (v110.iso < isolevel) cubeindex |= 1 << 2;
                            if (v010.iso < isolevel) cubeindex |= 1 << 3;
                            if (v001.iso < isolevel) cubeindex |= 1 << 4;
                            if (v101.iso < isolevel) cubeindex |= 1 << 5;
                            if (v111.iso < isolevel) cubeindex |= 1 << 6;
                            if (v011.iso < isolevel) cubeindex |= 1 << 7;
                            
                            int edgemask = kEdgeTable[cubeindex];
                            
                            if (edgemask != 0)
                            {
                                vec3 corner = offset + vec3(x, y, z) * cellSize;
                                
                                pair<vec3, vec3> ev[12];
                                size_t ecount = 0;
                                vec3 evavg;
                                
                                // add vertices
                                for (int i = 0; i < 12; ++i)
                                {
                                    if (edgemask & (1 << i))
                                    {
                                        int e0 = kEdgeIndexTable[i][0];
                                        int e1 = kEdgeIndexTable[i][1];
                                        int p0x = kVertexIndexTable[e0][0];
                                        int p0y = kVertexIndexTable[e0][1];
                                        int p0z = kVertexIndexTable[e0][2];
                                        int p1x = kVertexIndexTable[e1][0];
                                        int p1y = kVertexIndexTable[e1][1];
                                        int p1z = kVertexIndexTable[e1][2];
                                        
                                        const GridVertex& g0 = grid[(x + p0x) + sizeX * ((y + p0y) + sizeY * (z + p0z))];
                                        const GridVertex& g1 = grid[(x + p1x) + sizeX * ((y + p1y) + sizeY * (z + p1z))];
                                        
                                        pair<vec3, vec3> gt = Traits::intersect(g0, g1, isolevel, corner, vec3(p0x, p0y, p0z) * cellSize, vec3(p1x, p1y, p1z) * cellSize);
                                        
                                        ev[ecount++] = gt;
                                        evavg += gt.first;
                                    }
                                }
                                
                                pair<vec3, vec3> ga = Traits::average(ev, ecount, vec3(), vec3(cellSize), corner);
                                
                                gv[x + sizeX * (y + sizeY * z)] = make_pair(corner + evavg / float(ecount), MeshVertex { corner + ga.first, glm::normalize(ga.second) });
                            }
                        }
                
                vector<MeshVertex> vb;
                vector<unsigned int> ib;
                
                for (int z = 1; z + 1 < sizeZ; ++z)
                    for (int y = 1; y + 1 < sizeY; ++y)
                        for (int x = 1; x + 1 < sizeX; ++x)
                        {
                            const GridVertex& v000 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))];
                            const GridVertex& v100 = grid[(x + 1) + sizeX * ((y + 0) + sizeY * (z + 0))];
                            const GridVertex& v010 = grid[(x + 0) + sizeX * ((y + 1) + sizeY * (z + 0))];
                            const GridVertex& v001 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 1))];
                            
                            // add quads
                            if ((v000.iso < isolevel) != (v100.iso < isolevel))
                            {
                                pushQuad(vb, ib,
                                         gv[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))],
                                         gv[(x + 0) + sizeX * ((y - 1) + sizeY * (z + 0))],
                                         gv[(x + 0) + sizeX * ((y - 1) + sizeY * (z - 1))],
                                         gv[(x + 0) + sizeX * ((y + 0) + sizeY * (z - 1))],
                                         !(v000.iso < isolevel));
                            }
                            
                            if ((v000.iso < isolevel) != (v010.iso < isolevel))
                            {
                                pushQuad(vb, ib,
                                         gv[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))],
                                         gv[(x - 1) + sizeX * ((y + 0) + sizeY * (z + 0))],
                                         gv[(x - 1) + sizeX * ((y + 0) + sizeY * (z - 1))],
                                         gv[(x + 0) + sizeX * ((y + 0) + sizeY * (z - 1))],
                                         v000.iso < isolevel);
                            }
                            
                            if ((v000.iso < isolevel) != (v001.iso < isolevel))
                            {
                                pushQuad(vb, ib,
                                         gv[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))],
                                         gv[(x - 1) + sizeX * ((y + 0) + sizeY * (z + 0))],
                                         gv[(x - 1) + sizeX * ((y - 1) + sizeY * (z + 0))],
                                         gv[(x + 0) + sizeX * ((y - 1) + sizeY * (z + 0))],
                                         !(v000.iso < isolevel));
                            }
                        }
                
                if (true)
                {
                    // rebuild normals from scratch
                    unordered_map<glm::i32vec3, vec3> normals;
                    
                    #define Q(v) glm::i32vec3((v - offset) * 16.f + 0.5f)
                    
                    for (size_t i = 0; i < ib.size(); i += 3)
                    {
                        vec3 vn = glm::cross(vb[ib[i+1]].position - vb[ib[i+0]].position, vb[ib[i+2]].position - vb[ib[i+0]].position);
                        normals[Q(vb[ib[i+0]].position)] += vn;
                        normals[Q(vb[ib[i+1]].position)] += vn;
                        normals[Q(vb[ib[i+2]].position)] += vn;
                    }
                    
                    for (size_t i = 0; i < vb.size(); ++i)
                        vb[i].normal = glm::normalize(normals[Q(vb[i].position)]);
                    
                    #undef Q
                }
 
                return make_pair(move(vb), move(ib));
            }
        };
    }
    
    unique_ptr<Mesher> createMesherSurfaceNets()
    {
        return make_unique<surfacenets::Mesher>();
    }
}