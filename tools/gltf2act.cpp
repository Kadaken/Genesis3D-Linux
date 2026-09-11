/* Genesis3D Public License v1.01; see ../g3dlicense.txt.
 * Contributor: Kadaken, 2026. Generic glTF 2.0 to weighted ACT converter.
 * cgltf is MIT licensed; see ../third_party/cgltf/LICENSE.
 */
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "actor.h"
#include "bitmap.h"
#include "body.h"
#include "motion.h"
#include "path.h"
#include "quatern.h"
#include "vfile.h"

#include <png.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

bool finite(float value) { return std::isfinite(static_cast<double>(value)); }

bool xform_valid(const geXForm3d &value)
{
    const float *fields=reinterpret_cast<const float *>(&value);
    for (int i=0; i<12; ++i) if (!finite(fields[i])) return false;
    return geXForm3d_IsOrthonormal(&value)==GE_TRUE;
}

bool to_xform(const cgltf_float m[16], geXForm3d *out)
{
    out->AX=m[0]; out->AY=m[4]; out->AZ=m[8];
    out->BX=m[1]; out->BY=m[5]; out->BZ=m[9];
    out->CX=m[2]; out->CY=m[6]; out->CZ=m[10];
    out->Translation.X=m[12]; out->Translation.Y=m[13]; out->Translation.Z=m[14];
    return xform_valid(*out);
}

bool node_local(const cgltf_node *node, geXForm3d *out)
{
    cgltf_float matrix[16];
    cgltf_node_transform_local(node, matrix);
    return to_xform(matrix, out);
}

const cgltf_accessor *attribute(const cgltf_primitive &primitive,
    cgltf_attribute_type type, int index=0)
{
    for (cgltf_size i=0; i<primitive.attributes_count; ++i)
        if (primitive.attributes[i].type == type && primitive.attributes[i].index == index)
            return primitive.attributes[i].data;
    return nullptr;
}

struct RawInfluence { unsigned bone; float weight; };

bool read_influences(const cgltf_primitive &primitive, cgltf_size vertex,
    const std::vector<int> &joint_to_bone, geBody_WeightedCorner *corner)
{
    std::vector<RawInfluence> raw;
    for (int set=0; set<2; ++set) {
        const cgltf_accessor *joints = attribute(primitive, cgltf_attribute_type_joints, set);
        const cgltf_accessor *weights = attribute(primitive, cgltf_attribute_type_weights, set);
        if (!joints && !weights) continue;
        if (!joints || !weights || joints->count != weights->count || vertex >= joints->count)
            return false;
        cgltf_uint joint_values[4] = {};
        cgltf_float weight_values[4] = {};
        if (!cgltf_accessor_read_uint(joints, vertex, joint_values, 4) ||
            !cgltf_accessor_read_float(weights, vertex, weight_values, 4)) return false;
        for (int i=0; i<4; ++i) {
            if (!finite(weight_values[i]) || weight_values[i] < 0.0f ||
                joint_values[i] >= joint_to_bone.size()) return false;
            if (weight_values[i] == 0.0f) continue;
            const unsigned bone = static_cast<unsigned>(joint_to_bone[joint_values[i]]);
            auto found = std::find_if(raw.begin(), raw.end(),
                [bone](const RawInfluence &item){ return item.bone == bone; });
            if (found == raw.end()) raw.push_back({bone, weight_values[i]});
            else found->weight += weight_values[i];
        }
    }
    if (raw.empty()) return false;
    std::stable_sort(raw.begin(), raw.end(), [](const RawInfluence &a, const RawInfluence &b) {
        return a.weight > b.weight || (a.weight == b.weight && a.bone < b.bone);
    });
    if (raw.size() > GE_BODY_MAX_SKINNING_INFLUENCES)
        raw.resize(GE_BODY_MAX_SKINNING_INFLUENCES);
    double sum = 0.0;
    for (const RawInfluence &item : raw) sum += item.weight;
    if (!(sum > 0.0) || !std::isfinite(sum)) return false;
    struct Quantized { unsigned bone, base; double remainder; };
    std::vector<Quantized> quantized;
    unsigned base_sum = 0;
    for (const RawInfluence &item : raw) {
        const double exact = item.weight / sum * 65535.0;
        const unsigned base = static_cast<unsigned>(std::floor(exact));
        quantized.push_back({item.bone, base, exact - base});
        base_sum += base;
    }
    std::vector<size_t> remainder_order(quantized.size());
    for (size_t i=0; i<remainder_order.size(); ++i) remainder_order[i]=i;
    std::stable_sort(remainder_order.begin(), remainder_order.end(),
        [&quantized](size_t a, size_t b) {
            return quantized[a].remainder > quantized[b].remainder ||
                (quantized[a].remainder == quantized[b].remainder &&
                 quantized[a].bone < quantized[b].bone);
        });
    for (unsigned left=65535u-base_sum, i=0; left>0; --left, ++i)
        ++quantized[remainder_order[i % remainder_order.size()]].base;
    std::sort(quantized.begin(), quantized.end(), [](const Quantized &a, const Quantized &b) {
        return a.bone < b.bone;
    });
    corner->InfluenceCount = static_cast<uint8>(quantized.size());
    for (size_t i=0; i<quantized.size(); ++i) {
        if (quantized[i].base == 0 || quantized[i].base > 65535u) return false;
        corner->Influences[i].BoneIndex = static_cast<uint16>(quantized[i].bone);
        corner->Influences[i].Weight = static_cast<uint16>(quantized[i].base);
    }
    return true;
}

bool add_bones(const cgltf_skin &skin, geBody *body, std::vector<int> *joint_to_bone,
    std::vector<std::string> *bone_names)
{
    if (skin.joints_count == 0 || skin.joints_count > 256 || !skin.inverse_bind_matrices ||
        skin.inverse_bind_matrices->count != skin.joints_count) return false;
    std::unordered_map<const cgltf_node *, size_t> joint_slot;
    for (cgltf_size i=0; i<skin.joints_count; ++i) joint_slot[skin.joints[i]]=i;
    joint_to_bone->assign(skin.joints_count, -1);
    bone_names->assign(skin.joints_count, std::string());
    size_t added=0;
    while (added < skin.joints_count) {
        bool progress=false;
        for (cgltf_size slot=0; slot<skin.joints_count; ++slot) {
            if ((*joint_to_bone)[slot] >= 0) continue;
            const cgltf_node *node=skin.joints[slot];
            int parent=-1;
            auto parent_slot=joint_slot.find(node->parent);
            if (parent_slot != joint_slot.end()) {
                parent=(*joint_to_bone)[parent_slot->second];
                if (parent < 0) continue;
            }
            geXForm3d local;
            int bone=-1;
            std::string name=node->name ? node->name : "joint_"+std::to_string(slot);
            if (name.empty() || !node_local(node, &local) ||
                !geBody_AddBone(body, parent, name.c_str(), &local, &bone)) return false;
            (*joint_to_bone)[slot]=bone;
            (*bone_names)[slot]=name;
            ++added; progress=true;
        }
        if (!progress) return false;
    }
    for (cgltf_size slot=0; slot<skin.joints_count; ++slot) {
        cgltf_float matrix[16];
        geXForm3d inverse;
        if (!cgltf_accessor_read_float(skin.inverse_bind_matrices, slot, matrix, 16) ||
            !to_xform(matrix, &inverse) ||
            !geBody_SetBoneInverseBind(body, (*joint_to_bone)[slot], &inverse)) return false;
    }
    return true;
}

geBitmap *load_png_bitmap(const cgltf_image &source)
{
    if (!source.buffer_view || !source.buffer_view->buffer ||
        !source.buffer_view->buffer->data ||
        (source.mime_type && std::strcmp(source.mime_type, "image/png") != 0)) {
        std::fprintf(stderr, "gltf2act: only GLB-embedded PNG base-color images are supported\n");
        return nullptr;
    }
    const cgltf_buffer_view &view=*source.buffer_view;
    if (view.offset > view.buffer->size || view.size > view.buffer->size-view.offset)
        return nullptr;
    png_image image={};
    image.version=PNG_IMAGE_VERSION;
    const png_byte *encoded=static_cast<const png_byte *>(view.buffer->data)+view.offset;
    if (!png_image_begin_read_from_memory(&image,encoded,view.size)) return nullptr;
    image.format=PNG_FORMAT_RGBA;
    std::vector<png_byte> pixels(PNG_IMAGE_SIZE(image));
    if (!png_image_finish_read(&image,nullptr,pixels.data(),0,nullptr)) {
        png_image_free(&image);
        return nullptr;
    }
    geBitmap *bitmap=geBitmap_Create(static_cast<int>(image.width),
        static_cast<int>(image.height),1,GE_PIXELFORMAT_32BIT_ABGR);
    geBitmap *lock=nullptr;
    geBitmap_Info info;
    if (!bitmap || !geBitmap_LockForWrite(bitmap,&lock,0,0) ||
        !geBitmap_GetInfo(lock,&info,nullptr)) {
        if (lock) geBitmap_UnLock(lock);
        if (bitmap) geBitmap_Destroy(&bitmap);
        png_image_free(&image);
        return nullptr;
    }
    uint8 *destination=static_cast<uint8 *>(geBitmap_GetBits(lock));
    for (png_uint_32 row=0; row<image.height; ++row)
        std::memcpy(destination+static_cast<size_t>(row)*info.Stride*4,
            pixels.data()+static_cast<size_t>(row)*image.width*4,
            static_cast<size_t>(image.width)*4);
    if (!geBitmap_UnLock(lock)) {
        geBitmap_Destroy(&bitmap);
        bitmap=nullptr;
    }
    png_image_free(&image);
    return bitmap;
}

bool add_materials(const cgltf_data &data, geBody *body, std::vector<int> *indices)
{
    if (data.materials_count == 0) {
        int index;
        if (!geBody_AddMaterial(body, "neutral", nullptr, 255, 255, 255, &index)) return false;
        indices->push_back(index);
        return true;
    }
    for (cgltf_size i=0; i<data.materials_count; ++i) {
        const cgltf_material &source=data.materials[i];
        float color[3]={1,1,1};
        if (source.has_pbr_metallic_roughness)
            std::copy(source.pbr_metallic_roughness.base_color_factor,
                source.pbr_metallic_roughness.base_color_factor+3, color);
        for (float value : color) if (!finite(value)) return false;
        std::string name=source.name ? source.name : "material_"+std::to_string(i);
        name += "_"+std::to_string(i);
        geBitmap *bitmap=nullptr;
        if (source.has_pbr_metallic_roughness &&
            source.pbr_metallic_roughness.base_color_texture.texture) {
            const cgltf_texture *texture=source.pbr_metallic_roughness.base_color_texture.texture;
            if (!texture->image || !(bitmap=load_png_bitmap(*texture->image))) return false;
        }
        int index;
        const bool added=geBody_AddMaterial(body, name.c_str(), bitmap,
            color[0]*255.0f, color[1]*255.0f, color[2]*255.0f, &index)==GE_TRUE;
        if (bitmap) geBitmap_Destroy(&bitmap);
        if (!added) return false;
        indices->push_back(index);
        if (source.has_pbr_metallic_roughness &&
            (source.pbr_metallic_roughness.metallic_factor != 1.0f ||
             source.pbr_metallic_roughness.roughness_factor != 1.0f))
            std::fprintf(stderr, "gltf2act: material %zu metallic/roughness channels are not represented\n",
                static_cast<size_t>(i));
    }
    return true;
}

bool add_geometry(const cgltf_data &data, const cgltf_node &mesh_node,
    const std::vector<int> &joint_to_bone, const std::vector<int> &materials,
    geBody *body)
{
    geVec3d bounds_min={1e30f,1e30f,1e30f}, bounds_max={-1e30f,-1e30f,-1e30f};
    geXForm3d mesh_transform;
    if (!node_local(&mesh_node, &mesh_transform)) return false;
    geXForm3d identity; geXForm3d_SetIdentity(&identity);
    const float *actual=reinterpret_cast<const float *>(&mesh_transform);
    const float *expected=reinterpret_cast<const float *>(&identity);
    for (int i=0; i<12; ++i) if (std::fabs(actual[i]-expected[i]) > 1e-5f) {
        std::fprintf(stderr, "gltf2act: skinned mesh node transform must be identity\n");
        return false;
    }
    for (cgltf_size p=0; p<mesh_node.mesh->primitives_count; ++p) {
        const cgltf_primitive &primitive=mesh_node.mesh->primitives[p];
        const cgltf_accessor *positions=attribute(primitive,cgltf_attribute_type_position);
        const cgltf_accessor *normals=attribute(primitive,cgltf_attribute_type_normal);
        const cgltf_accessor *uvs=attribute(primitive,cgltf_attribute_type_texcoord);
        if (primitive.type != cgltf_primitive_type_triangles || !positions || !normals ||
            positions->count != normals->count || (uvs && uvs->count != positions->count))
            return false;
        cgltf_size index_count=primitive.indices ? primitive.indices->count : positions->count;
        if (index_count % 3 != 0) return false;
        int material=materials[0];
        if (primitive.material) {
            ptrdiff_t slot=primitive.material-data.materials;
            if (slot < 0 || static_cast<size_t>(slot) >= materials.size()) return false;
            material=materials[slot];
        }
        for (cgltf_size first=0; first<index_count; first+=3) {
            geBody_WeightedCorner corners[3] = {};
            for (int corner=0; corner<3; ++corner) {
                cgltf_size vertex=primitive.indices ?
                    cgltf_accessor_read_index(primitive.indices, first+corner) : first+corner;
                cgltf_float point[3], normal[3], uv[2]={0,0};
                if (vertex >= positions->count ||
                    !cgltf_accessor_read_float(positions,vertex,point,3) ||
                    !cgltf_accessor_read_float(normals,vertex,normal,3) ||
                    (uvs && !cgltf_accessor_read_float(uvs,vertex,uv,2))) return false;
                for (float value : point) if (!finite(value)) return false;
                for (float value : normal) if (!finite(value)) return false;
                corners[corner].Vertex={point[0],point[1],point[2]};
                corners[corner].Normal={normal[0],normal[1],normal[2]};
                corners[corner].U=uv[0]; corners[corner].V=uv[1];
                if (!read_influences(primitive,vertex,joint_to_bone,&corners[corner])) return false;
                bounds_min.X=std::min(bounds_min.X,point[0]);
                bounds_min.Y=std::min(bounds_min.Y,point[1]);
                bounds_min.Z=std::min(bounds_min.Z,point[2]);
                bounds_max.X=std::max(bounds_max.X,point[0]);
                bounds_max.Y=std::max(bounds_max.Y,point[1]);
                bounds_max.Z=std::max(bounds_max.Z,point[2]);
            }
            if (!geBody_AddWeightedFace(body,corners,material)) return false;
        }
    }
    geBody_SetBoundingBox(body,GE_BODY_ROOT,&bounds_min,&bounds_max);
    return true;
}

const cgltf_animation_channel *channel_for(const cgltf_animation &animation,
    const cgltf_node *node, cgltf_animation_path_type path)
{
    for (cgltf_size i=0; i<animation.channels_count; ++i)
        if (animation.channels[i].target_node==node && animation.channels[i].target_path==path)
            return &animation.channels[i];
    return nullptr;
}

bool add_channel_keys(gePath *path, const cgltf_animation_channel *channel, int mask)
{
    if (!channel) return true;
    const cgltf_animation_sampler *sampler=channel->sampler;
    if (!sampler || !sampler->input || !sampler->output ||
        sampler->input->count != sampler->output->count ||
        sampler->interpolation == cgltf_interpolation_type_cubic_spline ||
        sampler->interpolation == cgltf_interpolation_type_step) return false;
    for (cgltf_size i=0; i<sampler->input->count; ++i) {
        cgltf_float time, value[4]={0,0,0,1};
        geXForm3d transform;
        if (!cgltf_accessor_read_float(sampler->input,i,&time,1) || !finite(time)) return false;
        geXForm3d_SetIdentity(&transform);
        if (mask == GE_PATH_TRANSLATION_CHANNEL) {
            if (!cgltf_accessor_read_float(sampler->output,i,value,3)) return false;
            geXForm3d_SetTranslation(&transform,value[0],value[1],value[2]);
        } else {
            geQuaternion quaternion;
            if (!cgltf_accessor_read_float(sampler->output,i,value,4)) return false;
            geQuaternion_Set(&quaternion,value[3],value[0],value[1],value[2]);
            if (!geQuaternion_IsUnit(&quaternion)) geQuaternion_Normalize(&quaternion);
            geQuaternion_ToMatrix(&quaternion,&transform);
        }
        if (!gePath_InsertKeyframe(path,mask,time,&transform)) return false;
    }
    return true;
}

bool add_motions(const cgltf_data &data, const cgltf_skin &skin,
    const std::vector<std::string> &bone_names, geActor_Def *actor)
{
    for (cgltf_size a=0; a<data.animations_count; ++a) {
        const cgltf_animation &source=data.animations[a];
        geMotion *motion=geMotion_Create(GE_TRUE);
        int motion_index;
        if (!motion) return false;
        std::string motion_name=source.name ? source.name : "motion_"+std::to_string(a);
        if (!geMotion_SetName(motion,motion_name.c_str())) { geMotion_Destroy(&motion); return false; }
        for (cgltf_size joint=0; joint<skin.joints_count; ++joint) {
            const cgltf_node *node=skin.joints[joint];
            const cgltf_animation_channel *translation=channel_for(source,node,cgltf_animation_path_type_translation);
            const cgltf_animation_channel *rotation=channel_for(source,node,cgltf_animation_path_type_rotation);
            if (channel_for(source,node,cgltf_animation_path_type_scale) ||
                channel_for(source,node,cgltf_animation_path_type_weights)) {
                std::fprintf(stderr,"gltf2act: animation scale/weight channels are unsupported\n");
                geMotion_Destroy(&motion); return false;
            }
            gePath *path=gePath_Create(GE_PATH_INTERPOLATE_LINEAR,GE_PATH_INTERPOLATE_SLERP,GE_FALSE);
            geXForm3d base;
            int path_index;
            if (!path || !node_local(node,&base)) { if(path)gePath_Destroy(&path); geMotion_Destroy(&motion); return false; }
            if (!translation && !gePath_InsertKeyframe(path,GE_PATH_TRANSLATION_CHANNEL,0,&base))
                { gePath_Destroy(&path); geMotion_Destroy(&motion); return false; }
            if (!rotation && !gePath_InsertKeyframe(path,GE_PATH_ROTATION_CHANNEL,0,&base))
                { gePath_Destroy(&path); geMotion_Destroy(&motion); return false; }
            if (!add_channel_keys(path,translation,GE_PATH_TRANSLATION_CHANNEL) ||
                !add_channel_keys(path,rotation,GE_PATH_ROTATION_CHANNEL) ||
                !geMotion_AddPath(motion,path,bone_names[joint].c_str(),&path_index))
                { gePath_Destroy(&path); geMotion_Destroy(&motion); return false; }
            gePath_Destroy(&path);
        }
        if (!geActor_AddMotion(actor,motion,&motion_index)) { geMotion_Destroy(&motion); return false; }
    }
    return true;
}

bool write_actor(const char *path, geActor_Def *actor)
{
    geVFile *file=geVFile_OpenNewSystem(nullptr,GE_VFILE_TYPE_DOS,path,nullptr,GE_VFILE_OPEN_CREATE);
    if (!file) return false;
    const bool result=geActor_DefWriteToFile(actor,file)==GE_TRUE;
    return geVFile_Close(file)==GE_TRUE && result;
}

bool verify_actor(const char *path, int expected_bones, int expected_motions,
    int expected_materials, int expected_textured_materials)
{
    geVFile *file=geVFile_OpenNewSystem(nullptr,GE_VFILE_TYPE_DOS,path,nullptr,GE_VFILE_OPEN_READONLY);
    if (!file) return false;
    geActor_Def *actor=geActor_DefCreateFromFile(file);
    geVFile_Close(file);
    if (!actor) return false;
    geBody *body=geActor_GetBody(actor);
    bool result=geBody_GetBoneCount(body)==expected_bones &&
        geActor_GetMotionCount(actor)==expected_motions &&
        geBody_GetMaterialCount(body)==expected_materials;
    int textured=0;
    for (int material=0; result && material<expected_materials; ++material) {
        const char *name=nullptr;
        geBitmap *bitmap=nullptr;
        geFloat red,green,blue;
        result=geBody_GetMaterial(body,material,&name,&bitmap,&red,&green,&blue)==GE_TRUE &&
            name && name[0] && finite(red) && finite(green) && finite(blue);
        textured += bitmap != nullptr;
    }
    result=result && textured==expected_textured_materials;
    geActor *instance=result ? geActor_Create(actor) : nullptr;
    geXForm3d transform;
    for (int motion_index=0; instance && result && motion_index<expected_motions; ++motion_index) {
        geMotion *motion=geActor_GetMotionByIndex(actor,motion_index);
        geFloat begin,end;
        result=motion && geMotion_GetName(motion) && geMotion_GetName(motion)[0] &&
            geMotion_GetTimeExtents(motion,&begin,&end)==GE_TRUE && finite(begin) && finite(end);
        for (int endpoint=0; result && endpoint<2; ++endpoint) {
            geActor_SetPose(instance,motion,endpoint ? end : begin,nullptr);
            for (int bone=0; result && bone<expected_bones; ++bone)
                result=geActor_GetBoneTransformByIndex(instance,bone,&transform)==GE_TRUE &&
                    xform_valid(transform);
        }
    }
    if (instance) geActor_Destroy(&instance);
    geActor_DefDestroy(&actor);
    return result;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 3) {
        std::fprintf(stderr,"usage: gltf2act input.glb output.act\n");
        return 2;
    }
    cgltf_options options={};
    cgltf_data *data=nullptr;
    geActor_Def *actor=nullptr;
    geBody *body=nullptr;
    const cgltf_node *mesh_node=nullptr;
    int result=1;
    if (cgltf_parse_file(&options,argv[1],&data)!=cgltf_result_success ||
        cgltf_load_buffers(&options,data,argv[1])!=cgltf_result_success ||
        cgltf_validate(data)!=cgltf_result_success) {
        std::fprintf(stderr,"gltf2act: input is not a valid glTF 2.0 file\n"); goto done;
    }
    for (cgltf_size i=0;i<data->nodes_count;++i) if(data->nodes[i].mesh && data->nodes[i].skin) {
        if(mesh_node) { std::fprintf(stderr,"gltf2act: exactly one skinned mesh node is supported\n"); goto done; }
        mesh_node=&data->nodes[i];
    }
    if(!mesh_node) { std::fprintf(stderr,"gltf2act: no skinned mesh node found\n"); goto done; }
    actor=geActor_DefCreate(); body=geBody_Create();
    if(!actor || !body) goto done;
    {
        std::vector<int> joint_to_bone, materials;
        std::vector<std::string> bone_names;
        if(!add_bones(*mesh_node->skin,body,&joint_to_bone,&bone_names) ||
            !add_materials(*data,body,&materials) ||
            !add_geometry(*data,*mesh_node,joint_to_bone,materials,body) ||
            !geActor_SetBody(actor,body)) goto done;
        body=nullptr;
        const std::string temporary=std::string(argv[2])+".tmp";
        int textured_materials=0;
        for (cgltf_size material=0; material<data->materials_count; ++material)
            textured_materials += data->materials[material].has_pbr_metallic_roughness &&
                data->materials[material].pbr_metallic_roughness.base_color_texture.texture;
        std::remove(temporary.c_str());
        if(!add_motions(*data,*mesh_node->skin,bone_names,actor) ||
            !write_actor(temporary.c_str(),actor) ||
            !verify_actor(temporary.c_str(),static_cast<int>(mesh_node->skin->joints_count),
                static_cast<int>(data->animations_count),
                static_cast<int>(data->materials_count ? data->materials_count : 1),
                textured_materials)) {
            std::remove(temporary.c_str());
            std::fprintf(stderr,"gltf2act: output verification failed\n"); goto done;
        }
        if(std::rename(temporary.c_str(),argv[2]) != 0) {
            std::remove(temporary.c_str());
            std::fprintf(stderr,"gltf2act: could not install verified output\n"); goto done;
        }
    }
    std::fprintf(stderr,"gltf2act: wrote verified weighted actor: bones=%zu motions=%zu\n",
        static_cast<size_t>(mesh_node->skin->joints_count),
        static_cast<size_t>(data->animations_count));
    result=0;
done:
    if(body) geBody_Destroy(&body);
    if(actor) geActor_DefDestroy(&actor);
    if(data) cgltf_free(data);
    geVFile_CloseAPI();
    return result;
}
