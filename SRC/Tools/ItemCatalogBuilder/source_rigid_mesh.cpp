// Offline, exact Granny SDK extraction for otherwise missing rigid item models.
// Refuses animation/skinning rather than silently baking an approximate pose.
#include <granny.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using Json=nlohmann::json;
static GRANNY_CALLBACK(void) Log(granny_log_message_type,granny_log_message_origin,char const*,granny_int32x,char const* message,void*){std::cerr<<"Granny: "<<message<<'\n';}
int main(int argc,char** argv){
    try{
        if(argc!=3)throw std::runtime_error("usage: source_rigid_mesh source.gr2 output.json");
        const granny_log_callback log{Log,nullptr};GrannySetLogCallback(&log);std::cerr<<"Granny SDK "<<GrannyGetVersionString()<<'\n';
        auto* file=GrannyReadEntireFile(argv[1]);if(!file)throw std::runtime_error("Cannot read source GR2");
        auto* info=GrannyGetFileInfo(file);if(!info || info->ModelCount!=1 || info->AnimationCount)throw std::runtime_error("Expected one static source model");
        const auto* model=info->Models[0];float placement[16];GrannyGetModelInitialPlacement4x4(model,placement);
        Json out={{"source",argv[1]},{"placement",std::vector<float>(placement,placement+16)},{"meshes",Json::array()}};
        for(int n=0;n<model->MeshBindingCount;++n){
            const auto* mesh=model->MeshBindings[n].Mesh;
            if(!GrannyMeshIsRigid(mesh))throw std::runtime_error("Skinned model needs a dedicated skeleton conversion");
            const auto count=GrannyGetMeshVertexCount(mesh),indexCount=GrannyGetMeshIndexCount(mesh);
            if(count<=0 || indexCount<=0 || indexCount%3)throw std::runtime_error("Empty or non-triangular mesh");
            std::vector<granny_pnt332_vertex> vertices(count);std::vector<int> indices(indexCount);
            GrannyCopyMeshVertices(mesh,GrannyPNT332VertexType,vertices.data());GrannyCopyMeshIndices(mesh,4,indices.data());
            Json part={{"name",mesh->Name},{"positions",Json::array()},{"normals",Json::array()},{"uv",Json::array()},{"indices",indices},{"groups",Json::array()},{"materials",Json::array()}};
            for(const auto& v:vertices){part["positions"].push_back({v.Position[0],v.Position[1],v.Position[2]});part["normals"].push_back({v.Normal[0],v.Normal[1],v.Normal[2]});part["uv"].push_back({v.UV[0],v.UV[1]});}
            const auto* groups=GrannyGetMeshTriangleGroups(mesh);
            for(int j=0;j<GrannyGetMeshTriangleGroupCount(mesh);++j)part["groups"].push_back({{"material",groups[j].MaterialIndex},{"first",groups[j].TriFirst*3},{"count",groups[j].TriCount*3}});
            for(int j=0;j<mesh->MaterialBindingCount;++j){const auto* material=mesh->MaterialBindings[j].Material;const auto* texture=GrannyGetMaterialTextureByType(material,GrannyDiffuseColorTexture);part["materials"].push_back({{"name",material->Name},{"texture",texture && texture->FromFileName ? texture->FromFileName : ""}});}
            out["meshes"].push_back(std::move(part));
        }
        std::ofstream output(argv[2]);output<<out.dump(2)<<'\n';if(!output.good())throw std::runtime_error("Cannot write extracted geometry");
        GrannyFreeFile(file);std::cout<<"SOURCE_RIGID_MESH=SUCCESS meshes="<<out["meshes"].size()<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
