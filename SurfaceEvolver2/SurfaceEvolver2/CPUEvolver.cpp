#include "CPUEvolver.h"

#define TWO_PI 6.28318530718

using namespace std;


// Initialize Data for the CPUEvolver, see description of datatypes
// in CPUEvolver.h for a more detailed description. Otherwise, this
// code is a necessary evil because of the complicatedness of things
CPUEvolver::CPUEvolver(Mesh* m, int initItersUntilLambdaUpdate)
    : Evolver(m, initItersUntilLambdaUpdate)
{
    
    triangles = m->getTriangles();
    vector<float3>& points = m->getVertices();
	points1 = new float3[points.size()];
	for (int i = 0; i < points.size(); i++)
		points1[i] = points[i];

    vertexCount = points.size();
    triangleCount = triangles.size();
    
    points2 = new float3[vertexCount];
    triangleCountPerVertex = new int[vertexCount];
    trianglesByVertex = new uint2[triangleCount * 3];
    triangleOffset = new int[vertexCount];
    areaForce = new float3[vertexCount];
    volumeForce = new float3[vertexCount];
    int offset = 0;
    for(int i=0; i < vertexCount; i++){
        int triCount = 0;
        triangleOffset[i] = offset;
        for(int j=0; j<triangleCount; j++){
            if(triangles[j].x == i ||
               triangles[j].y == i ||
               triangles[j].z == i){
                trianglesByVertex[offset + triCount] = rearrangeTri(triangles[j], i);
                triCount++;
            }
        }
        triangleCountPerVertex[i] = triCount;
        offset += triCount;
    }
}

// deletes dynamically allocated stuff
CPUEvolver::~CPUEvolver(){
    delete[] triangleCountPerVertex;
    delete[] triangleOffset;
    delete[] points2;
    delete[] areaForce;
    delete[] volumeForce;
}

// given a triangle in the form of uint3 tri, return a uint2 made of
// the vertices not equal to pointIndex that preserves the winding order
// This is used for populating the trianglesByVertex array
uint2 CPUEvolver::rearrangeTri(uint3 tri, int pointIndex){
    uint2 simpleTri;
    if(tri.x == pointIndex){
        simpleTri.x = tri.y;
        simpleTri.y = tri.z;
    }else if(tri.y == pointIndex){
        simpleTri.y = tri.x;
        simpleTri.x = tri.z;
    }else{
        simpleTri.x = tri.x;
        simpleTri.y = tri.y;
    }
    return simpleTri;
}

// Step simulation runs a single step of surface evolution

void CPUEvolver::stepSimulation(){
    for(int i=0; i < vertexCount; i++){
        areaForce[i] = make_float3(0,0,0);
		volumeForce[i] = make_float3(0, 0, 0);
        for(int j=0, k = triangleCountPerVertex[i]; j < k; j++){
            calculateForces(i,j);
        }

    }
    float alpha = calculateAlpha();
    for(int i=0; i < vertexCount; i++){
        displaceVertex(i);
    }
    getArea();
}

// calculates the area and volume force of a particular triangle on
// a particular vertex
void CPUEvolver::calculateForces(int vertexIndex, int triangleIndex){
    int triangleIndexOffset = triangleOffset[vertexIndex];
    uint2 tri = trianglesByVertex[triangleIndexOffset + triangleIndex];
    
    float3 x1 = points1[vertexIndex];
    float3 x2 = points1[tri.x];
    float3 x3 = points1[tri.y];
    float3 s1 = x2 - x1;
    float3 s2 = x3 - x2;
    
    areaForce[vertexIndex] += SIGMA/2.0f * cross(s2, cross(s1, s2))/length(cross(s1,s2));
    volumeForce[vertexIndex] += cross(x3, x2)/6.0f;
}
    
// calculates the alpha coefficient
float CPUEvolver::calculateAlpha(){
    float sum1 = 0, sum2 = 0;
    for(int i = 0; i < vertexCount; i++){
        sum1+=dot(volumeForce[i], areaForce[i]);
        sum2+=dot(volumeForce[i], volumeForce[i]);
    }
    return sum1 / sum2;
}

// displaces an individual vertex, either overwriting its position
// data or putting it in the other points buffer depending
// on the truthiness of mutateMesh
void CPUEvolver::displaceVertex(int vertexIndex){
    if(mutateMesh)
        points1[vertexIndex] = points1[vertexIndex] + lambda*(areaForce[vertexIndex] - alpha*volumeForce[vertexIndex]);
    else
        points2[vertexIndex] = points1[vertexIndex] + lambda*(areaForce[vertexIndex] - alpha*volumeForce[vertexIndex]);
}    

// returns the total surface area of the mesh
float CPUEvolver::getArea(){
    float3* outPoints = (mutateMesh) ? points1 : points2;

    float surfaceArea = 0;
    for(int i=0; i < triangleCount; i++){
        uint3 t = triangles[i];
        float3 s1 = outPoints[t.y] - outPoints[t.x];
        float3 s2 = outPoints[t.z] - outPoints[t.y];
        surfaceArea += length(cross(s1, s2))/2;
    }
    return surfaceArea;
}

// returns the mean net force of the mesh
float CPUEvolver::getMeanNetForce(){
    float total = 0;
    for(int i=0; i < vertexCount; i++){
        total += length(areaForce[i] + volumeForce[i]);
    }
    return total/vertexCount;
}

// approximates mean curvature by using angular difference
float CPUEvolver::getMeanCurvature(){
    float3* outPoints = (mutateMesh) ? points1 : points2;
	float totalCurvature = 0;
    for(int i=0; i < vertexCount; i++){
        int triangleIndex = triangleOffset[i];
        float totalAngle = 0,
              totalArea  = 0;
        for(int j=0; j < triangleCountPerVertex[i]; j++){
            uint2 tri = trianglesByVertex[triangleIndex + j];
            float3 u = outPoints[tri.x] - outPoints[i],
                   v = outPoints[tri.y] - outPoints[i];
            totalAngle += acos(dot(u, v) / sqrt(dot(u, u) * dot(v, v)));
            totalArea  += length(cross(u, v))/2;
        }
        totalCurvature += (TWO_PI - totalAngle) / totalArea;
    }
    return totalCurvature / vertexCount;
}

// returns the volume of the mesh
float CPUEvolver::getVolume(){
    float3* outPoints = (mutateMesh) ? points1 : points2;

    float v = 0;
    for(int i=0; i < triangleCount; i++){
        uint3 t = triangles[i];
        v += dot(points2[t.x], cross(outPoints[t.y],outPoints[t.z]))/6.0f;
    }
    return v;
};

// Some quite straightforward output functions
void CPUEvolver::outputPoints(){
    float3* outPoints = (mutateMesh) ? points1 : points2;
    
    for(int i=0; i<vertexCount; i++){
        if(i>0)
            cout << ", ";
        cout << "[ " << outPoints[i].x 
             << ", " << outPoints[i].y 
             << ", " << outPoints[i].z << "]";
    }
}
void CPUEvolver::outputVolumeForces(){
    for(int i=0; i<vertexCount; i++){
        if(i>0)
            cout << ", ";
        cout << "[ " << volumeForce[i].x 
             << ", " << volumeForce[i].y 
             << ", " << volumeForce[i].z << "]";
    }
}
    
void CPUEvolver::outputAreaForces(){
    for(int i=0; i<vertexCount; i++){
        if(i>0)
            cout << ", ";
        cout << "[ " << areaForce[i].x 
             << ", " << areaForce[i].y 
             << ", " << areaForce[i].z << "]";
    }
}
    
void CPUEvolver::outputNetForces(){
    for(int i=0; i<vertexCount; i++){
        if(i>0)
            cout << ", ";
        cout << "[ " << volumeForce[i].x + areaForce[i].x
             << ", " << volumeForce[i].y + areaForce[i].y
             << ", " << volumeForce[i].z + areaForce[i].z << "]";
    }
}
    


    