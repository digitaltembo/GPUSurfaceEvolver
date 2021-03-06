//Compile with:
// g++ -g update.cpp math2.cpp test.cpp


/*
 * Using arguments:
 *      ./a.out [-v] [-m (tetrahedron|icosahedron|jeep|face|file.toload)]
 *              [-cpu] [-n=#] [-i=#] [-o (Comma seperated list of output values)]
 * 
 *      -v   : activate visualization
 *      -m   : model type, one of tetrahedron, icosahedron, jeep, face, or a file name
 *      -n=# : quadricection count, only makes sense with tetrahedron?
 *      -i=# : iteration count, how many times update should be called
 *      -cpu : perform calculation on CPU
 *      -o   : output a JSON file that is a list of representations of a single iteration
 *              The single iteration will consist of all of the data specified
 *              Options: SurfaceArea, Volume, Force, Curvature, Points
 *              Example: ./a.out -o SurfaceArea, Volume
 *                      will output a list of 2 item lists, the first of which
 *                      will be the surface area at that iteration, the second
 *                      of which will be the volume.
 * 
 * I didn't actually implement any of the functionality, but the code in main will store the 
 * information in relevant variables.
 * 
 */

#include "update.h"
#include "data.h"
#include <cstdlib>

using namespace std;


#define UPDATES 10

/* FOR ARGUMENT PARSING */

// Returns true if arg begins with match
bool argMatch(const char* arg, const char* match){
    //Wheee clean code! 
    for(;(*match)!='\0';match++)
        if((*match) != (*(arg++)))
            return false;
    return true;
}

//returns the last character of an argument
char lastChar(const char* s){
    int i = 0;
    while(s[i++] != '\0');
    return s[i-2];
}

uint3 rearrangeTri(uint3 tri, int pointIndex, int triIndex){
//     cout << pointIndex << ": " << tri << " -> ";
    if(tri.x == pointIndex){
        tri.x = tri.y;
        tri.y = tri.z;
    }else if(tri.y == pointIndex){
        tri.y = tri.x;
        tri.x = tri.z;
    }
//     cout << tri << endl;
    return tri;
}

// takes in an array of points (3d vectors comprised of floats)
// and an array of triangles (lists of the three vertices comprising each triangle,
// represented as indices within the points array),
// and populates ourvarious data storage units for CPU computing
void generateMeshData(MeshData* m, 
                      float3* points, int pointCount,
                      uint3* triangles, int triangleCount){
    m->triangles = triangles;
    m->points1 = points;
    m->points2 = new float3[pointCount];
    m->vertexCount = pointCount;
    m->triangleCount = triangleCount;
    m->triangleCountPerVertex = new int[pointCount];
    m->trianglesByVertex = new uint3[triangleCount * 3];
    m->triangleOffset = new int[pointCount];
    m->areaForce = new float3[pointCount];
    m->volumeForce = new float3[pointCount];
    m->areas = new float[triangleCount];
    int offset = 0;
    int triCount = 0;
    for(int i=0; i < pointCount; i++){
        int triCount = 0;
        m->triangleOffset[i] = offset;
        for(int j=0; j<triangleCount; j++){
            if(triangles[j].x == i ||
               triangles[j].y == i ||
               triangles[j].z == i){
                m->trianglesByVertex[offset + triCount] = rearrangeTri(triangles[j], i, j);
                triCount++;
            }
        }
        m->triangleCountPerVertex[i] = triCount;
        offset += triCount;
    }
}
void deleteMeshData(MeshData* m){
    delete[] m->triangleCountPerVertex;
    delete[] m->triangleOffset;
    //delete[] m->points1;
    delete[] m->points2;
    delete[] m->areaForce;
    delete[] m->volumeForce;
    //delete[] m->triangles;
    delete[] m->areas;
}

//generates a random vector int the cube from (-1, -1, -1) to (1, 1, 1)
float3 randomVector(){
    return vector(-1+(rand()%10)/5.0,-1+(rand()%10)/5.0,-1+(rand()%10)/5.0);
}
int main(int argc, char** argv){
    
    //Default parameters
    
    MeshType meshType = TETRAHEDRON;
    // If meshType is MESH_FILE, load file from mesh stored in meshFile
    char* meshFile = NULL;
    
    // If true, display results. Otherwise, you get a pretty boring output.
    bool visualization = false;
    
    // Number of times the shape is bisected (any number for tetrahedra, preset numbers for
    // icosahedra, and it means nothing for other meshes)
    int bisections = 10;
    
    // Times Update is called
    int iterations = 10;
    
    // If true, use the GPU for calculations. Else, use the CPU
    bool gpu = true;
    
    //Type of stuff to output
    OutputType output[5] = { NONE };
    int outputLength = 0;
    
    // Parse Command Line Arguments:
    for(int i=1;i<argc;i++){
        if(argMatch(argv[i], "-v")){
            visualization = true;
        }else if(argMatch(argv[i], "-n=")){
            bisections = atoi(argv[i]+3);
        }else if(argMatch(argv[i], "-i=")){
            iterations = atoi(argv[i]+3);
        }else if(argMatch(argv[i], "-m")){
            i++;
            if(argMatch(argv[i], "tetrahedron")){
                meshType = TETRAHEDRON;
            }else if(argMatch(argv[i], "icosahedron")){
                meshType = ICOSAHEDRON;
            }else if(argMatch(argv[i], "jeep")){
                meshType = JEEP;
            }else if(argMatch(argv[i], "face")){
                meshType = FACE;
            }else{
                meshType = MESH_FILE;
                meshFile = argv[i];
            }
        }else if(argMatch(argv[i], "-cpu")){
            gpu = false;
        }else if(argMatch(argv[i], "-o")){
            do{
                i++;
                if(argMatch(argv[i], "SurfaceArea")){
                    output[outputLength++] = TOTAL_SURFACE_AREA;
                }else if(argMatch(argv[i], "Volume")){
                    output[outputLength++] = TOTAL_VOLUME;
                }else if(argMatch(argv[i], "Force")){
                    output[outputLength++] = MEAN_NET_FORCE;
                }else if(argMatch(argv[i], "Curvature")){
                    output[outputLength++] = MEAN_CURVATURE;
                }else{
                    output[outputLength++] = POINTS;
                }
            } while(lastChar(argv[i]) == ',');
        }
    }
    
    MeshData m;
    
    switch(bisections){
        case 1:
            generateMeshData(&m, pointsN1, pointCountN1, trianglesN1,  triangleCountN1);
            break;
        case 2:
            generateMeshData(&m, pointsN2, pointCountN2, trianglesN2,  triangleCountN2);
            break;
        default:
            generateMeshData(&m, pointsN10, pointCountN10, trianglesN10,  triangleCountN10);
            break;
    }
    
    for(int i=0; i < iterations; i++){
        update(0, &m, output, outputLength);
    }
   
    deleteMeshData(&m);
}