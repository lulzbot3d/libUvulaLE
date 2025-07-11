/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "lscm_unwrap.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <ranges>
#include <set>

#include "xatlas.h"

// #include "uvedit_intern.hh"

enum eUVPackIsland_RotationMethod
{
    /** No rotation. */
    ED_UVPACK_ROTATION_NONE = 0,
    /** Rotated to a minimal rectangle, either vertical or horizontal. */
    ED_UVPACK_ROTATION_AXIS_ALIGNED,
    /** Align along X axis (wide islands). */
    ED_UVPACK_ROTATION_AXIS_ALIGNED_X,
    /** Align along Y axis (tall islands). */
    ED_UVPACK_ROTATION_AXIS_ALIGNED_Y,
    /** Only 90 degree rotations are allowed. */
    ED_UVPACK_ROTATION_CARDINAL,
    /** Any angle. */
    ED_UVPACK_ROTATION_ANY,
};

enum eUVPackIsland_PinMethod
{
    /** Pin has no impact on packing. */
    ED_UVPACK_PIN_NONE = 0,
    /**
     * Ignore islands containing any pinned UV's.
     * \note Not exposed in the UI, used only for live-unwrap.
     */
    ED_UVPACK_PIN_IGNORE,
    ED_UVPACK_PIN_LOCK_ROTATION,
    ED_UVPACK_PIN_LOCK_ROTATION_SCALE,
    ED_UVPACK_PIN_LOCK_SCALE,
    /** Lock the island in-place (translation, rotation and scale). */
    ED_UVPACK_PIN_LOCK_ALL,
};

enum eUVPackIsland_MarginMethod
{
    /** Use scale of existing UVs to multiply margin. */
    ED_UVPACK_MARGIN_SCALED = 0,
    /** Just add the margin, ignoring any UV scale. */
    ED_UVPACK_MARGIN_ADD,
    /** Specify a precise fraction of final UV output. */
    ED_UVPACK_MARGIN_FRACTION,
};

enum eUVPackIsland_ShapeMethod
{
    /** Use Axis-Aligned Bounding-Boxes. */
    ED_UVPACK_SHAPE_AABB = 0,
    /** Use convex hull. */
    ED_UVPACK_SHAPE_CONVEX,
    /** Use concave hull. */
    ED_UVPACK_SHAPE_CONCAVE,
};

struct UVPackIsland_Params
{
    /** Restrictions around island rotation. */
    eUVPackIsland_RotationMethod rotate_method{ ED_UVPACK_ROTATION_NONE };
    /** Resize islands to fill the unit square. */
    bool scale_to_fit{ true };
    /** (In UV Editor) only pack islands which have one or more selected UVs. */
    bool only_selected_uvs{ false };
    /** (In 3D Viewport or UV Editor) only pack islands which have selected faces. */
    bool only_selected_faces{ false };
    /** When determining islands, use Seams as boundary edges. */
    bool use_seams{ false };
    /** (In 3D Viewport or UV Editor) use aspect ratio from face. */
    bool correct_aspect{ false };
    /** How will pinned islands be treated. */
    eUVPackIsland_PinMethod pin_method{ ED_UVPACK_PIN_NONE };
    /** Treat unselected UVs as if they were pinned. */
    bool pin_unselected{ false };
    /** Overlapping islands stick together. */
    bool merge_overlap{ false };
    /** Additional space to add around each island. */
    float margin{ 0.001f };
    /** Which formula to use when scaling island margin. */
    eUVPackIsland_MarginMethod margin_method{ ED_UVPACK_MARGIN_SCALED };
    /** Additional translation for bottom left corner. */
    float udim_base_offset[2]{ 0.0f, 0.0f };
    /** Target vertical extent. Should be 1.0f for the unit square. */
    float target_extent{ 1.0f };
    /** Target aspect ratio. */
    float target_aspect_y{ 1.0f };
    /** Which shape to use when packing. */
    eUVPackIsland_ShapeMethod shape_method{ ED_UVPACK_SHAPE_AABB };

    /** Abandon packing early when set by the job system. */
    bool* stop{ nullptr };
    bool* do_update{ nullptr };
    /** How much progress we have made. From wmJob. */
    float* progress{ nullptr };
};

// struct FaceIsland
// {
//     FaceIsland* next;
//     FaceIsland* prev;
//     BMFace** faces;
//     int faces_len;
//     /**
//      * \note While this is duplicate information,
//      * it allows islands from multiple meshes to be stored in the same list.
//      */
//     BMUVOffsets offsets;
//     float aspect_y;
// };

// static bool uvedit_ensure_uvs(Object* obedit)
// {
//     if (ED_uvedit_test(obedit))
//     {
//         return true;
//     }
//
//     BMEditMesh* em = BKE_editmesh_from_object(obedit);
//     BMFace* efa;
//     BMIter iter;
//
//     if (em && em->bm->totface && ! CustomData_has_layer(&em->bm->ldata, CD_PROP_FLOAT2))
//     {
//         ED_mesh_uv_add(static_cast<Mesh*>(obedit->data), nullptr, true, true, nullptr);
//     }
//
//     /* Happens when there are no faces. */
//     if (! ED_uvedit_test(obedit))
//     {
//         return false;
//     }
//
//     const char* active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
//     BM_uv_map_attr_vert_select_ensure(em->bm, active_uv_name);
//     BM_uv_map_attr_edge_select_ensure(em->bm, active_uv_name);
//     const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);
//
//     /* select new UVs (ignore UV_FLAG_SYNC_SELECT in this case) */
//     BM_ITER_MESH(efa, &iter, em->bm, BM_FACES_OF_MESH)
//     {
//         BMIter liter;
//         BMLoop* l;
//
//         BM_ITER_ELEM(l, &liter, efa, BM_LOOPS_OF_FACE)
//         {
//             BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, true);
//             BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, true);
//         }
//     }
//
//     return true;
// }

// static void uv_map_operator_property_correct_aspect(wmOperatorType* ot)
// {
//     RNA_def_boolean(ot->srna, "correct_aspect", true, "Correct Aspect", "Map UVs taking aspect ratio of the image associated with the material into account");
// }

// void blender::geometry::UVPackIsland_Params::setUDIMOffsetFromSpaceImage(const SpaceImage* sima)
// {
//     if (! sima)
//     {
//         return; /* Nothing to do. */
//     }
//
//     /* NOTE: Presently, when UDIM grid and tiled image are present together, only active tile for
//      * the tiled image is considered. */
//     const Image* image = sima->image;
//     if (image && image->source == IMA_SRC_TILED)
//     {
//         ImageTile* active_tile = static_cast<ImageTile*>(BLI_findlink(&image->tiles, image->active_tile_index));
//         if (active_tile)
//         {
//             udim_base_offset[0] = (active_tile->tile_number - 1001) % 10;
//             udim_base_offset[1] = (active_tile->tile_number - 1001) / 10;
//         }
//         return;
//     }
//
//     /* TODO: Support storing an active UDIM when there are no tiles present.
//      * Until then, use 2D cursor to find the active tile index for the UDIM grid. */
//     if (uv_coords_isect_udim(sima->image, sima->tile_grid_shape, sima->cursor))
//     {
//         udim_base_offset[0] = floorf(sima->cursor[0]);
//         udim_base_offset[1] = floorf(sima->cursor[1]);
//     }
// }

// bool blender::geometry::UVPackIsland_Params::isCancelled() const
// {
//     if (stop)
//     {
//         return *stop;
//     }
//     return false;
// }

// struct UnwrapOptions
// {
//     /** Connectivity based on UV coordinates instead of seams. */
//     bool topology_from_uvs;
//     /** Also use seams as well as UV coordinates (only valid when `topology_from_uvs` is enabled). */
//     bool topology_from_uvs_use_seams;
//     /** Only affect selected faces. */
//     bool only_selected_faces;
//     /**
//      * Only affect selected UVs.
//      * \note Disable this for operations that don't run in the image-window.
//      * Unwrapping from the 3D view for example, where only 'only_selected_faces' should be used.
//      */
//     bool only_selected_uvs;
//     /** Fill holes to better preserve shape. */
//     bool fill_holes;
//     /** Correct for mapped image texture aspect ratio. */
//     bool correct_aspect;
//     /** Treat unselected uvs as if they were pinned. */
//     bool pin_unselected;
//
//     int method;
//     bool use_slim;
//     bool use_abf;
//     bool use_subsurf;
//     bool use_weights;
//
//     ParamSlimOptions slim;
//     char weight_group[MAX_VGROUP_NAME];
// };

// void blender::geometry::UVPackIsland_Params::setFromUnwrapOptions(const UnwrapOptions& options)
// {
//     only_selected_uvs = options.only_selected_uvs;
//     only_selected_faces = options.only_selected_faces;
//     use_seams = ! options.topology_from_uvs || options.topology_from_uvs_use_seams;
//     correct_aspect = options.correct_aspect;
//     pin_unselected = options.pin_unselected;
// }

// static void modifier_unwrap_state(Object* obedit, const UnwrapOptions* options, bool* r_use_subsurf)
// {
//     ModifierData* md;
//     bool subsurf = options->use_subsurf;
//
//     md = static_cast<ModifierData*>(obedit->modifiers.first);
//
//     /* Subdivision-surface will take the modifier settings
//      * only if modifier is first or right after mirror. */
//     if (subsurf)
//     {
//         if (md && md->type == eModifierType_Subsurf)
//         {
//             const SubsurfModifierData& smd = *reinterpret_cast<const SubsurfModifierData*>(md);
//             if (smd.levels > 0)
//             {
//                 /* Skip all calculation for zero subdivision levels, similar to the way the modifier is
//                  * disabled in that case. */
//                 subsurf = true;
//             }
//             else
//             {
//                 subsurf = false;
//             }
//         }
//         else
//         {
//             subsurf = false;
//         }
//     }
//
//     *r_use_subsurf = subsurf;
// }

// static bool uvedit_have_selection(const Scene* scene, BMEditMesh* em, const UnwrapOptions* options)
// {
//     BMFace* efa;
//     BMLoop* l;
//     BMIter iter, liter;
//     const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);
//
//     if (offsets.uv == -1)
//     {
//         return (em->bm->totfacesel != 0);
//     }
//
//     /* verify if we have any selected uv's before unwrapping,
//      * so we can cancel the operator early */
//     BM_ITER_MESH(efa, &iter, em->bm, BM_FACES_OF_MESH)
//     {
//         if (scene->toolsettings->uv_flag & UV_FLAG_SYNC_SELECT)
//         {
//             if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN))
//             {
//                 continue;
//             }
//         }
//         else if (! BM_elem_flag_test(efa, BM_ELEM_SELECT))
//         {
//             continue;
//         }
//
//         BM_ITER_ELEM(l, &liter, efa, BM_LOOPS_OF_FACE)
//         {
//             if (uvedit_uv_select_test(scene, l, offsets))
//             {
//                 break;
//             }
//         }
//
//         if (options->only_selected_uvs && ! l)
//         {
//             continue;
//         }
//
//         return true;
//     }
//
//     return false;
// }

// static bool uvedit_have_selection_multi(const Scene* scene, const Span<Object*> objects, const UnwrapOptions* options)
// {
//     bool have_select = false;
//     for (Object* obedit : objects)
//     {
//         BMEditMesh* em = BKE_editmesh_from_object(obedit);
//         if (uvedit_have_selection(scene, em, options))
//         {
//             have_select = true;
//             break;
//         }
//     }
//     return have_select;
// }

// void ED_uvedit_get_aspect_from_material(Object* ob, const int material_index, float* r_aspx, float* r_aspy)
// {
//     if (UNLIKELY(material_index < 0 || material_index >= ob->totcol))
//     {
//         *r_aspx = 1.0f;
//         *r_aspy = 1.0f;
//         return;
//     }
//     Image* ima;
//     ED_object_get_active_image(ob, material_index + 1, &ima, nullptr, nullptr, nullptr);
//     ED_image_get_uv_aspect(ima, nullptr, r_aspx, r_aspy);
// }

// static void construct_param_handle_face_add(
//     ParamHandle* handle,
//     const Scene* scene,
//     BMFace* efa,
//     blender::geometry::ParamKey face_index,
//     const UnwrapOptions* options,
//     const BMUVOffsets& offsets,
//     const int cd_weight_offset,
//     const int cd_weight_index)
// {
//     blender::Array<ParamKey, BM_DEFAULT_NGON_STACK_SIZE> vkeys(efa->len);
//     blender::Array<bool, BM_DEFAULT_NGON_STACK_SIZE> pin(efa->len);
//     blender::Array<bool, BM_DEFAULT_NGON_STACK_SIZE> select(efa->len);
//     blender::Array<const float*, BM_DEFAULT_NGON_STACK_SIZE> co(efa->len);
//     blender::Array<float*, BM_DEFAULT_NGON_STACK_SIZE> uv(efa->len);
//     blender::Array<float, BM_DEFAULT_NGON_STACK_SIZE> weight(efa->len);
//
//     int i;
//
//     BMIter liter;
//     BMLoop* l;
//
//     /* let parametrizer split the ngon, it can make better decisions
//      * about which split is best for unwrapping than poly-fill. */
//     BM_ITER_ELEM_INDEX(l, &liter, efa, BM_LOOPS_OF_FACE, i)
//     {
//         float* luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
//
//         vkeys[i] = blender::geometry::uv_find_pin_index(handle, BM_elem_index_get(l->v), luv);
//         co[i] = l->v->co;
//         uv[i] = luv;
//         pin[i] = BM_ELEM_CD_GET_BOOL(l, offsets.pin);
//         select[i] = uvedit_uv_select_test(scene, l, offsets);
//         if (options->pin_unselected && ! select[i])
//         {
//             pin[i] = true;
//         }
//
//         /* Optional vertex group weighting. */
//         if (cd_weight_offset >= 0 && cd_weight_index >= 0)
//         {
//             MDeformVert* dv = (MDeformVert*)BM_ELEM_CD_GET_VOID_P(l->v, cd_weight_offset);
//             weight[i] = BKE_defvert_find_weight(dv, cd_weight_index);
//         }
//         else
//         {
//             weight[i] = 1.0f;
//         }
//     }
//
//     blender::geometry::uv_parametrizer_face_add(handle, face_index, i, vkeys.data(), co.data(), uv.data(), weight.data(), pin.data(), select.data());
// }

/*
 * Version of #construct_param_handle_multi with a separate BMesh parameter.
 */
// static ParamHandle* construct_param_handle(const Scene* scene, Object* ob, BMesh* bm, const UnwrapOptions* options, int* r_count_failed = nullptr)
// {
//     BMFace* efa;
//     BMIter iter;
//     int i;
//
//     ParamHandle* handle = new blender::geometry::ParamHandle();
//
//     if (options->correct_aspect)
//     {
//         blender::geometry::uv_parametrizer_aspect_ratio(handle, ED_uvedit_get_aspect_y(ob));
//     }
//
//     /* we need the vert indices */
//     BM_mesh_elem_index_ensure(bm, BM_VERT);
//
//     const BMUVOffsets offsets = BM_uv_map_offsets_get(bm);
//     const int cd_weight_offset = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);
//     const int cd_weight_index = BKE_object_defgroup_name_index(ob, options->weight_group);
//
//     BM_ITER_MESH_INDEX(efa, &iter, bm, BM_FACES_OF_MESH, i)
//     {
//         if (uvedit_is_face_affected(scene, efa, options, offsets))
//         {
//             uvedit_prepare_pinned_indices(handle, scene, efa, options, offsets);
//         }
//     }
//
//     BM_ITER_MESH_INDEX(efa, &iter, bm, BM_FACES_OF_MESH, i)
//     {
//         if (uvedit_is_face_affected(scene, efa, options, offsets))
//         {
//             construct_param_handle_face_add(handle, scene, efa, i, options, offsets, cd_weight_offset, cd_weight_index);
//         }
//     }
//
//     construct_param_edge_set_seams(handle, bm, options);
//
//     blender::geometry::uv_parametrizer_construct_end(handle, options->fill_holes, options->topology_from_uvs, r_count_failed);
//
//     return handle;
// }

/**
 * Version of #construct_param_handle that handles multiple objects.
 */
// static ParamHandle* construct_param_handle_multi(const Scene* scene, const Span<Object*> objects, const UnwrapOptions* options)
// {
//     BMFace* efa;
//     BMIter iter;
//     int i;
//
//     ParamHandle* handle = new blender::geometry::ParamHandle();
//
//     if (options->correct_aspect)
//     {
//         Object* ob = objects[0];
//         blender::geometry::uv_parametrizer_aspect_ratio(handle, ED_uvedit_get_aspect_y(ob));
//     }
//
//     /* we need the vert indices */
//     EDBM_mesh_elem_index_ensure_multi(objects, BM_VERT);
//
//     int offset = 0;
//
//     for (Object* obedit : objects)
//     {
//         BMEditMesh* em = BKE_editmesh_from_object(obedit);
//         BMesh* bm = em->bm;
//
//         const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);
//
//         if (offsets.uv == -1)
//         {
//             continue;
//         }
//
//         const int cd_weight_offset = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);
//         const int cd_weight_index = BKE_object_defgroup_name_index(obedit, options->weight_group);
//
//         BM_ITER_MESH_INDEX(efa, &iter, bm, BM_FACES_OF_MESH, i)
//         {
//             if (uvedit_is_face_affected(scene, efa, options, offsets))
//             {
//                 uvedit_prepare_pinned_indices(handle, scene, efa, options, offsets);
//             }
//         }
//
//         BM_ITER_MESH_INDEX(efa, &iter, bm, BM_FACES_OF_MESH, i)
//         {
//             if (uvedit_is_face_affected(scene, efa, options, offsets))
//             {
//                 construct_param_handle_face_add(handle, scene, efa, i + offset, options, offsets, cd_weight_offset, cd_weight_index);
//             }
//         }
//
//         construct_param_edge_set_seams(handle, bm, options);
//
//         offset += bm->totface;
//     }
//
//     blender::geometry::uv_parametrizer_construct_end(handle, options->fill_holes, options->topology_from_uvs, nullptr);
//
//     return handle;
// }

// static void island_uv_transform(
//     FaceIsland* island,
//     const float matrix[2][2], /* Scale and rotation. */
//     const float pre_translate[2] /* (pre) Translation. */
// )
// {
//     /* Use a pre-transform to compute `A * (x+b)`
//      *
//      * \note Ordinarily, we'd use a post_transform like `A * x + b`
//      * In general, post-transforms are easier to work with when using homogenous co-ordinates.
//      *
//      * When UV mapping into the unit square, post-transforms can lose precision on small islands.
//      * Instead we're using a pre-transform to maintain precision.
//      *
//      * To convert post-transform to pre-transform, use `A * x + b == A * (x + c), c = A^-1 * b`
//      */
//
//     const int cd_loop_uv_offset = island->offsets.uv;
//     const int faces_len = island->faces_len;
//     for (int i = 0; i < faces_len; i++)
//     {
//         BMFace* f = island->faces[i];
//         BMLoop* l;
//         BMIter iter;
//         BM_ITER_ELEM(l, &iter, f, BM_LOOPS_OF_FACE)
//         {
//             float* luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
//             blender::geometry::mul_v2_m2_add_v2v2(luv, matrix, luv, pre_translate);
//         }
//     }
// }

/**
 * Calculates distance to nearest UDIM image tile in UV space and its UDIM tile number.
 */
// static float uv_nearest_image_tile_distance(const Image* image, const float coords[2], float nearest_tile_co[2])
// {
//     BKE_image_find_nearest_tile_with_offset(image, coords, nearest_tile_co);
//
//     /* Add 0.5 to get tile center coordinates. */
//     float nearest_tile_center_co[2] = { nearest_tile_co[0], nearest_tile_co[1] };
//     add_v2_fl(nearest_tile_center_co, 0.5f);
//
//     return len_squared_v2v2(coords, nearest_tile_center_co);
// }

/**
 * Calculates distance to nearest UDIM grid tile in UV space and its UDIM tile number.
 */
// static float uv_nearest_grid_tile_distance(const int udim_grid[2], const float coords[2], float nearest_tile_co[2])
// {
//     const float coords_floor[2] = { floorf(coords[0]), floorf(coords[1]) };
//
//     if (coords[0] > udim_grid[0])
//     {
//         nearest_tile_co[0] = udim_grid[0] - 1;
//     }
//     else if (coords[0] < 0)
//     {
//         nearest_tile_co[0] = 0;
//     }
//     else
//     {
//         nearest_tile_co[0] = coords_floor[0];
//     }
//
//     if (coords[1] > udim_grid[1])
//     {
//         nearest_tile_co[1] = udim_grid[1] - 1;
//     }
//     else if (coords[1] < 0)
//     {
//         nearest_tile_co[1] = 0;
//     }
//     else
//     {
//         nearest_tile_co[1] = coords_floor[1];
//     }
//
//     /* Add 0.5 to get tile center coordinates. */
//     float nearest_tile_center_co[2] = { nearest_tile_co[0], nearest_tile_co[1] };
//     add_v2_fl(nearest_tile_center_co, 0.5f);
//
//     return len_squared_v2v2(coords, nearest_tile_center_co);
// }

// static bool island_has_pins(const Scene* scene, FaceIsland* island, const blender::geometry::UVPackIsland_Params* params)
// {
//     const bool pin_unselected = params->pin_unselected;
//     const bool only_selected_faces = params->only_selected_faces;
//     BMLoop* l;
//     BMIter iter;
//     const int pin_offset = island->offsets.pin;
//     for (int i = 0; i < island->faces_len; i++)
//     {
//         BMFace* efa = island->faces[i];
//         if (pin_unselected && only_selected_faces && ! BM_elem_flag_test(efa, BM_ELEM_SELECT))
//         {
//             return true;
//         }
//         BM_ITER_ELEM(l, &iter, island->faces[i], BM_LOOPS_OF_FACE)
//         {
//             if (BM_ELEM_CD_GET_BOOL(l, pin_offset))
//             {
//                 return true;
//             }
//             if (pin_unselected && ! uvedit_uv_select_test(scene, l, island->offsets))
//             {
//                 return true;
//             }
//         }
//     }
//     return false;
// }

#if 0
int bm_mesh_calc_uv_islands(
    const Scene* scene,
    BMesh* bm,
    ListBase* island_list,
    const bool only_selected_faces,
    const bool only_selected_uvs,
    const bool use_seams,
    const float aspect_y,
    const BMUVOffsets& uv_offsets)
{
    BLI_assert(uv_offsets.uv >= 0);
    int island_added = 0;
    BM_mesh_elem_table_ensure(bm, BM_FACE);

    int* groups_array = MEM_malloc_arrayN<int>(bm->totface, __func__);

    int(*group_index)[2];

    /* Set the tag for `BM_mesh_calc_face_groups`. */
    BMFace* f;
    BMIter iter;
    BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH)
    {
        const bool face_affected = uvedit_is_face_affected_for_calc_uv_islands(scene, f, only_selected_faces, only_selected_uvs, uv_offsets);
        BM_elem_flag_set(f, BM_ELEM_TAG, face_affected);
    }

    SharedUVLoopData user_data = { { 0 } };
    user_data.offsets = uv_offsets;
    user_data.use_seams = use_seams;

    const int group_len = BM_mesh_calc_face_groups(bm, groups_array, &group_index, nullptr, bm_loop_uv_shared_edge_check, &user_data, BM_ELEM_TAG, BM_EDGE);

    for (int i = 0; i < group_len; i++)
    {
        const int faces_start = group_index[i][0];
        const int faces_len = group_index[i][1];
        BMFace** faces = MEM_malloc_arrayN<BMFace*>(faces_len, __func__);

        for (int j = 0; j < faces_len; j++)
        {
            faces[j] = BM_face_at_index(bm, groups_array[faces_start + j]);
        }

        FaceIsland* island = MEM_callocN<FaceIsland>(__func__);
        island->faces = faces;
        island->faces_len = faces_len;
        island->offsets = uv_offsets;
        island->aspect_y = aspect_y;
        BLI_addtail(island_list, island);
        island_added += 1;
    }

    MEM_freeN(groups_array);
    MEM_freeN(group_index);
    return island_added;
}

/**
 * Pack UV islands from multiple objects.
 *
 * \param scene: Scene containing the objects to be packed.
 * \param objects: Array of Objects to pack.
 * \param objects_len: Length of `objects` array.
 * \param bmesh_override: BMesh array aligned with `objects`.
 * Optional, when non-null this overrides object's BMesh.
 * This is needed to perform UV packing on objects that aren't in edit-mode.
 * \param udim_source_closest: UDIM source SpaceImage.
 * \param original_selection: Pack to original selection.
 * \param notify_wm: Notify the WM of any changes. (UI thread only.)
 * \param params: Parameters and options to pass to the packing engine.
 */
// Kepp this
static void uvedit_pack_islands_multi(
    const Scene* scene,
    BMesh** bmesh_override,
    const SpaceImage* udim_source_closest,
    const bool original_selection,
    const bool notify_wm,
    const UVPackIsland_Params& params)
{
    std::vector<FaceIsland*> island_vector;
    std::vector<bool> pinned_vector;

    BMesh* bm = nullptr;

    const float aspect_y = params.correct_aspect ? ED_uvedit_get_aspect_y(obedit) : 1.0f;

    bool only_selected_faces = params.only_selected_faces;
    bool only_selected_uvs = params->only_selected_uvs;
    const bool ignore_pinned = params->pin_method == ED_UVPACK_PIN_IGNORE;
    if (ignore_pinned && params->pin_unselected)
    {
        only_selected_faces = false;
        only_selected_uvs = false;
    }
    std::vector<FaceIsland*> island_list;
    // ListBase island_list = { nullptr };
    bm_mesh_calc_uv_islands(scene, bm, &island_list, only_selected_faces, only_selected_uvs, params->use_seams, aspect_y, offsets);

    /* Remove from linked list and append to blender::Vector. */
    for (FaceIsland* island : &island_list)
    {
        BLI_remlink(&island_list, island);
        const bool pinned = island_has_pins(scene, island, params);
        if (ignore_pinned && pinned)
        {
            MEM_freeN(island->faces);
            MEM_freeN(island);
            continue;
        }
        island_vector.append(island);
        pinned_vector.append(pinned);
    }

    if (island_vector.is_empty())
    {
        return;
    }

    /* Coordinates of bounding box containing all selected UVs. */
    float selection_min_co[2], selection_max_co[2];
    INIT_MINMAX2(selection_min_co, selection_max_co);

    for (int index = 0; index < island_vector.size(); index++)
    {
        FaceIsland* island = island_vector[index];

        for (int i = 0; i < island->faces_len; i++)
        {
            BMFace* f = island->faces[i];
            BM_face_uv_minmax(f, selection_min_co, selection_max_co, island->offsets.uv);
        }
    }

    /* Center of bounding box containing all selected UVs. */
    float selection_center[2];
    mid_v2_v2v2(selection_center, selection_min_co, selection_max_co);

    if (original_selection)
    {
        /* Protect against degenerate source AABB. */
        if ((selection_max_co[0] - selection_min_co[0]) * (selection_max_co[1] - selection_min_co[1]) > 1e-40f)
        {
            copy_v2_v2(params->udim_base_offset, selection_min_co);
            params->target_extent = selection_max_co[1] - selection_min_co[1];
            params->target_aspect_y = (selection_max_co[0] - selection_min_co[0]) / (selection_max_co[1] - selection_min_co[1]);
        }
    }

    MemArena* arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
    Heap* heap = BLI_heap_new();

    blender::Vector<blender::geometry::PackIsland*> pack_island_vector;
    for (int i = 0; i < island_vector.size(); i++)
    {
        FaceIsland* face_island = island_vector[i];
        blender::geometry::PackIsland* pack_island = new blender::geometry::PackIsland();
        pack_island->caller_index = i;
        pack_island->aspect_y = face_island->aspect_y;
        pack_island->pinned = pinned_vector[i];
        pack_island_vector.append(pack_island);

        for (int i = 0; i < face_island->faces_len; i++)
        {
            BMFace* f = face_island->faces[i];

            /* Storage. */
            blender::Array<blender::float2> uvs(f->len);

            /* Obtain UVs of face. */
            BMLoop* l;
            BMIter iter;
            int j;
            BM_ITER_ELEM_INDEX(l, &iter, f, BM_LOOPS_OF_FACE, j)
            {
                copy_v2_v2(uvs[j], BM_ELEM_CD_GET_FLOAT_P(l, face_island->offsets.uv));
            }

            pack_island->add_polygon(uvs, arena, heap);

            BLI_memarena_clear(arena);
        }
    }
    BLI_heap_free(heap, nullptr);
    BLI_memarena_free(arena);

    const float scale = pack_islands(pack_island_vector, *params);
    const bool is_cancelled = params->isCancelled();

    float base_offset[2] = { 0.0f, 0.0f };
    copy_v2_v2(base_offset, params->udim_base_offset);

    if (udim_source_closest)
    {
        const Image* image = udim_source_closest->image;
        const int* udim_grid = udim_source_closest->tile_grid_shape;
        /* Check if selection lies on a valid UDIM grid tile. */
        bool is_valid_udim = uv_coords_isect_udim(image, udim_grid, selection_center);
        if (is_valid_udim)
        {
            base_offset[0] = floorf(selection_center[0]);
            base_offset[1] = floorf(selection_center[1]);
        }
        /* If selection doesn't lie on any UDIM then find the closest UDIM grid or image tile. */
        else
        {
            float nearest_image_tile_co[2] = { FLT_MAX, FLT_MAX };
            float nearest_image_tile_dist = FLT_MAX, nearest_grid_tile_dist = FLT_MAX;
            if (image)
            {
                nearest_image_tile_dist = uv_nearest_image_tile_distance(image, selection_center, nearest_image_tile_co);
            }

            float nearest_grid_tile_co[2] = { 0.0f, 0.0f };
            nearest_grid_tile_dist = uv_nearest_grid_tile_distance(udim_grid, selection_center, nearest_grid_tile_co);

            base_offset[0] = (nearest_image_tile_dist < nearest_grid_tile_dist) ? nearest_image_tile_co[0] : nearest_grid_tile_co[0];
            base_offset[1] = (nearest_image_tile_dist < nearest_grid_tile_dist) ? nearest_image_tile_co[1] : nearest_grid_tile_co[1];
        }
    }

    float matrix[2][2];
    float matrix_inverse[2][2];
    float pre_translate[2];
    for (const int64_t i : pack_island_vector.index_range())
    {
        if (is_cancelled)
        {
            continue;
        }
        blender::geometry::PackIsland* pack_island = pack_island_vector[i];
        FaceIsland* island = island_vector[pack_island->caller_index];
        const float island_scale = pack_island->can_scale_(*params) ? scale : 1.0f;
        pack_island->build_transformation(island_scale, pack_island->angle, matrix);
        invert_m2_m2(matrix_inverse, matrix);

        /* Add base_offset, post transform. */
        mul_v2_m2v2(pre_translate, matrix_inverse, base_offset);

        /* Add pre-translation from #pack_islands. */
        pre_translate[0] += pack_island->pre_translate.x;
        pre_translate[1] += pack_island->pre_translate.y;

        /* Perform the transformation. */
        island_uv_transform(island, matrix, pre_translate);
    }

    for (const int64_t i : pack_island_vector.index_range())
    {
        blender::geometry::PackIsland* pack_island = pack_island_vector[i];
        /* Cleanup memory. */
        pack_island_vector[i] = nullptr;
        delete pack_island;
    }

    if (notify_wm && ! is_cancelled)
    {
        for (Object* obedit : objects)
        {
            DEG_id_tag_update(static_cast<ID*>(obedit->data), ID_RECALC_GEOMETRY);
            WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
        }
    }

    for (FaceIsland* island : island_vector)
    {
        MEM_freeN(island->faces);
        MEM_freeN(island);
    }
}
#endif
/* Packing targets. */
// enum
// {
//     PACK_UDIM_SRC_CLOSEST = 0,
//     PACK_UDIM_SRC_ACTIVE,
//     PACK_ORIGINAL_AABB,
// };

// struct UVPackIslandsData
// {
//     wmWindowManager* wm;
//
//     const Scene* scene;
//
//     Vector<Object*> objects;
//     const SpaceImage* sima;
//     int udim_source;
//
//     bContext* undo_context;
//     const char* undo_str;
//     bool use_job;
//
//     blender::geometry::UVPackIsland_Params pack_island_params;
// };

// static void pack_islands_startjob(void* pidv, wmJobWorkerStatus* worker_status)
// {
//     worker_status->progress = 0.02f;
//
//     UVPackIslandsData* pid = static_cast<UVPackIslandsData*>(pidv);
//
//     pid->pack_island_params.stop = &worker_status->stop;
//     pid->pack_island_params.do_update = &worker_status->do_update;
//     pid->pack_island_params.progress = &worker_status->progress;
//
//     uvedit_pack_islands_multi(
//         pid->scene,
//         pid->objects,
//         nullptr,
//         (pid->udim_source == PACK_UDIM_SRC_CLOSEST) ? pid->sima : nullptr,
//         (pid->udim_source == PACK_ORIGINAL_AABB),
//         ! pid->use_job,
//         &pid->pack_island_params);
//
//     worker_status->progress = 0.99f;
//     worker_status->do_update = true;
// }

// static void pack_islands_endjob(void* pidv)
// {
//     UVPackIslandsData* pid = static_cast<UVPackIslandsData*>(pidv);
//     for (Object* obedit : pid->objects)
//     {
//         DEG_id_tag_update(static_cast<ID*>(obedit->data), ID_RECALC_GEOMETRY);
//         WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
//     }
//     WM_main_add_notifier(NC_SPACE | ND_SPACE_IMAGE, nullptr);
//
//     if (pid->undo_str)
//     {
//         ED_undo_push(pid->undo_context, pid->undo_str);
//     }
// }

// static void pack_islands_freejob(void* pidv)
// {
//     WM_cursor_wait(false);
//     UVPackIslandsData* pid = static_cast<UVPackIslandsData*>(pidv);
//     WM_set_locked_interface(pid->wm, false);
//     MEM_delete(pid);
// }

// static wmOperatorStatus pack_islands_exec(bContext* C, wmOperator* op)
// {
//     ViewLayer* view_layer = CTX_data_view_layer(C);
//     const Scene* scene = CTX_data_scene(C);
//     const SpaceImage* sima = CTX_wm_space_image(C);
//
//     UnwrapOptions options = unwrap_options_get(op, nullptr, scene->toolsettings);
//     options.topology_from_uvs = true;
//     options.only_selected_faces = true;
//     options.only_selected_uvs = true;
//     options.fill_holes = false;
//     options.correct_aspect = true;
//
//     Vector<Object*> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(scene, view_layer, CTX_wm_view3d(C));
//
//     /* Early exit in case no UVs are selected. */
//     if (! uvedit_have_selection_multi(scene, objects, &options))
//     {
//         return OPERATOR_CANCELLED;
//     }
//
//     /* RNA props */
//     const int udim_source = RNA_enum_get(op->ptr, "udim_source");
//     if (RNA_struct_property_is_set(op->ptr, "margin"))
//     {
//         scene->toolsettings->uvcalc_margin = RNA_float_get(op->ptr, "margin");
//     }
//     else
//     {
//         RNA_float_set(op->ptr, "margin", scene->toolsettings->uvcalc_margin);
//     }
//
//     UVPackIslandsData* pid = MEM_new<UVPackIslandsData>(__func__);
//     pid->use_job = op->flag & OP_IS_INVOKE;
//     pid->scene = scene;
//     pid->objects = std::move(objects);
//     pid->sima = sima;
//     pid->udim_source = udim_source;
//     pid->wm = CTX_wm_manager(C);
//
//     blender::geometry::UVPackIsland_Params& pack_island_params = pid->pack_island_params;
//     {
//         /* Call default constructor and copy the defaults. */
//         blender::geometry::UVPackIsland_Params default_params;
//         pack_island_params = default_params;
//     }
//
//     pack_island_params.setFromUnwrapOptions(options);
//     if (RNA_boolean_get(op->ptr, "rotate"))
//     {
//         pack_island_params.rotate_method = eUVPackIsland_RotationMethod(RNA_enum_get(op->ptr, "rotate_method"));
//     }
//     else
//     {
//         pack_island_params.rotate_method = ED_UVPACK_ROTATION_NONE;
//     }
//     pack_island_params.scale_to_fit = RNA_boolean_get(op->ptr, "scale");
//     pack_island_params.merge_overlap = RNA_boolean_get(op->ptr, "merge_overlap");
//
//     if (RNA_boolean_get(op->ptr, "pin"))
//     {
//         pack_island_params.pin_method = eUVPackIsland_PinMethod(RNA_enum_get(op->ptr, "pin_method"));
//     }
//     else
//     {
//         pack_island_params.pin_method = ED_UVPACK_PIN_NONE;
//     }
//
//     pack_island_params.margin_method = eUVPackIsland_MarginMethod(RNA_enum_get(op->ptr, "margin_method"));
//     pack_island_params.margin = RNA_float_get(op->ptr, "margin");
//     pack_island_params.shape_method = eUVPackIsland_ShapeMethod(RNA_enum_get(op->ptr, "shape_method"));
//
//     if (udim_source == PACK_UDIM_SRC_ACTIVE)
//     {
//         pack_island_params.setUDIMOffsetFromSpaceImage(sima);
//     }
//
//     if (pid->use_job)
//     {
//         /* Setup job. */
//         if (pid->wm->op_undo_depth == 0)
//         {
//             /* The job must do its own undo push. */
//             pid->undo_context = C;
//             pid->undo_str = op->type->name;
//         }
//
//         wmJob* wm_job = WM_jobs_get(pid->wm, CTX_wm_window(C), scene, "Packing UVs", WM_JOB_PROGRESS, WM_JOB_TYPE_UV_PACK);
//         WM_jobs_customdata_set(wm_job, pid, pack_islands_freejob);
//         WM_jobs_timer(wm_job, 0.1, 0, 0);
//         WM_set_locked_interface(pid->wm, true);
//         WM_jobs_callbacks(wm_job, pack_islands_startjob, nullptr, nullptr, pack_islands_endjob);
//
//         WM_cursor_wait(true);
//         G.is_break = false;
//         WM_jobs_start(CTX_wm_manager(C), wm_job);
//         return OPERATOR_FINISHED;
//     }
//
//     wmJobWorkerStatus worker_status = {};
//     pack_islands_startjob(pid, &worker_status);
//     pack_islands_endjob(pid);
//     pack_islands_freejob(pid);
//
//     return OPERATOR_FINISHED;
// }

// static const EnumPropertyItem pack_margin_method_items[] = {
//     { ED_UVPACK_MARGIN_SCALED, "SCALED", 0, "Scaled", "Use scale of existing UVs to multiply margin" },
//     { ED_UVPACK_MARGIN_ADD, "ADD", 0, "Add", "Just add the margin, ignoring any UV scale" },
//     { ED_UVPACK_MARGIN_FRACTION, "FRACTION", 0, "Fraction", "Specify a precise fraction of final UV output" },
//     { 0, nullptr, 0, nullptr, nullptr },
// };

// static const EnumPropertyItem pack_rotate_method_items[] = {
//     { ED_UVPACK_ROTATION_ANY, "ANY", 0, "Any", "Any angle is allowed for rotation" },
//     { ED_UVPACK_ROTATION_CARDINAL, "CARDINAL", 0, "Cardinal", "Only 90 degree rotations are allowed" },
//     RNA_ENUM_ITEM_SEPR,
//
// #define PACK_ROTATE_METHOD_AXIS_ALIGNED_OFFSET 3
//     { ED_UVPACK_ROTATION_AXIS_ALIGNED, "AXIS_ALIGNED", 0, "Axis-aligned", "Rotated to a minimal rectangle, either vertical or horizontal" },
//     { ED_UVPACK_ROTATION_AXIS_ALIGNED_X, "AXIS_ALIGNED_X", 0, "Axis-aligned (Horizontal)", "Rotate islands to be aligned horizontally" },
//     { ED_UVPACK_ROTATION_AXIS_ALIGNED_Y, "AXIS_ALIGNED_Y", 0, "Axis-aligned (Vertical)", "Rotate islands to be aligned vertically" },
//     { 0, nullptr, 0, nullptr, nullptr },
// };

// static const EnumPropertyItem pack_shape_method_items[] = {
//     { ED_UVPACK_SHAPE_CONCAVE, "CONCAVE", 0, "Exact Shape (Concave)", "Uses exact geometry" },
//     { ED_UVPACK_SHAPE_CONVEX, "CONVEX", 0, "Boundary Shape (Convex)", "Uses convex hull" },
//     RNA_ENUM_ITEM_SEPR,
//     { ED_UVPACK_SHAPE_AABB, "AABB", 0, "Bounding Box", "Uses bounding boxes" },
//     { 0, nullptr, 0, nullptr, nullptr },
// };

/**
 * \note #ED_UVPACK_PIN_NONE is exposed as a boolean "pin".
 * \note #ED_UVPACK_PIN_IGNORE is intentionally not exposed as it is confusing from the UI level
 * (users can simply not select these islands).
 * The option is kept internally because it's used for live unwrap.
 */
// static const EnumPropertyItem pinned_islands_method_items[] = {
//     { ED_UVPACK_PIN_LOCK_SCALE, "SCALE", 0, "Scale", "Pinned islands won't rescale" },
//     { ED_UVPACK_PIN_LOCK_ROTATION, "ROTATION", 0, "Rotation", "Pinned islands won't rotate" },
//     { ED_UVPACK_PIN_LOCK_ROTATION_SCALE, "ROTATION_SCALE", 0, "Rotation and Scale", "Pinned islands will translate only" },
//     { ED_UVPACK_PIN_LOCK_ALL, "LOCKED", 0, "All", "Pinned islands are locked in place" },
//     { 0, nullptr, 0, nullptr, nullptr },
// };

// static void uv_pack_islands_ui(bContext* /*C*/, wmOperator* op)
// {
//     uiLayout* layout = op->layout;
//     layout->use_property_split_set(true);
//     layout->use_property_decorate_set(false);
//     layout->prop(op->ptr, "shape_method", UI_ITEM_NONE, std::nullopt, ICON_NONE);
//     layout->prop(op->ptr, "scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
//     {
//         layout->prop(op->ptr, "rotate", UI_ITEM_NONE, std::nullopt, ICON_NONE);
//         uiLayout* sub = &layout->row(true);
//         sub->active_set(RNA_boolean_get(op->ptr, "rotate"));
//         sub->prop(op->ptr, "rotate_method", UI_ITEM_NONE, std::nullopt, ICON_NONE);
//         layout->separator();
//     }
//     layout->prop(op->ptr, "margin_method", UI_ITEM_NONE, std::nullopt, ICON_NONE);
//     layout->prop(op->ptr, "margin", UI_ITEM_NONE, std::nullopt, ICON_NONE);
//     layout->separator();
//     {
//         layout->prop(op->ptr, "pin", UI_ITEM_NONE, std::nullopt, ICON_NONE);
//         uiLayout* sub = &layout->row(true);
//         sub->active_set(RNA_boolean_get(op->ptr, "pin"));
//         sub->prop(op->ptr, "pin_method", UI_ITEM_NONE, IFACE_("Lock Method"), ICON_NONE);
//         layout->separator();
//     }
//     layout->prop(op->ptr, "merge_overlap", UI_ITEM_NONE, std::nullopt, ICON_NONE);
//     layout->prop(op->ptr, "udim_source", UI_ITEM_NONE, std::nullopt, ICON_NONE);
//     layout->separator();
// }

// static wmOperatorStatus uv_pack_islands_invoke(bContext* C, wmOperator* op, const wmEvent* event)
// {
//     return WM_operator_props_popup_confirm_ex(C, op, event, IFACE_("Pack Islands"), IFACE_("Pack"));
// }

// static struct
// {
//     ParamHandle** handles;
//     uint len, len_alloc;
//     wmTimer* timer;
// } g_live_unwrap = { nullptr };

// bool ED_uvedit_live_unwrap_timer_check(const wmTimer* timer)
// {
//     /* NOTE: don't validate the timer, assume the timer passed in is valid. */
//     return g_live_unwrap.timer == timer;
// }

/**
 * In practice the timer should practically always be valid.
 * Use this to prevent the unlikely case of a stale timer being set.
 *
 * Loading a new file while unwrapping is running could cause this for example.
 */
// static bool uvedit_live_unwrap_timer_validate(const wmWindowManager* wm)
// {
//     if (g_live_unwrap.timer == nullptr)
//     {
//         return false;
//     }
//     if (BLI_findindex(&wm->timers, g_live_unwrap.timer) != -1)
//     {
//         return false;
//     }
//     g_live_unwrap.timer = nullptr;
//     return true;
// }

// void ED_uvedit_live_unwrap_begin(Scene* scene, Object* obedit, wmWindow* win_modal)
// {
//     ParamHandle* handle = nullptr;
//     BMEditMesh* em = BKE_editmesh_from_object(obedit);
//
//     if (! ED_uvedit_test(obedit))
//     {
//         return;
//     }
//
//     UnwrapOptions options = unwrap_options_get(nullptr, obedit, scene->toolsettings);
//     options.topology_from_uvs = false;
//     options.only_selected_faces = false;
//     options.only_selected_uvs = false;
//
//     if (options.use_subsurf)
//     {
//         handle = construct_param_handle_subsurfed(scene, obedit, em, &options, nullptr);
//     }
//     else
//     {
//         handle = construct_param_handle(scene, obedit, em->bm, &options, nullptr);
//     }
//
//     if (options.use_slim)
//     {
//         options.slim.no_flip = false;
//         options.slim.skip_init = true;
//         uv_parametrizer_slim_live_begin(handle, &options.slim);
//
//         if (win_modal)
//         {
//             wmWindowManager* wm = static_cast<wmWindowManager*>(G_MAIN->wm.first);
//             /* Clear in the unlikely event this is still set. */
//             uvedit_live_unwrap_timer_validate(wm);
//             BLI_assert(! g_live_unwrap.timer);
//             g_live_unwrap.timer = WM_event_timer_add(wm, win_modal, TIMER, 0.01f);
//         }
//     }
//     else
//     {
//         blender::geometry::uv_parametrizer_lscm_begin(handle, true, options.use_abf);
//     }
//
//     /* Create or increase size of g_live_unwrap.handles array */
//     if (g_live_unwrap.handles == nullptr)
//     {
//         g_live_unwrap.len_alloc = 32;
//         g_live_unwrap.handles = MEM_malloc_arrayN<ParamHandle*>(g_live_unwrap.len_alloc, "uvedit_live_unwrap_liveHandles");
//         g_live_unwrap.len = 0;
//     }
//     if (g_live_unwrap.len >= g_live_unwrap.len_alloc)
//     {
//         g_live_unwrap.len_alloc *= 2;
//         g_live_unwrap.handles = static_cast<ParamHandle**>(MEM_reallocN(g_live_unwrap.handles, sizeof(ParamHandle*) * g_live_unwrap.len_alloc));
//     }
//     g_live_unwrap.handles[g_live_unwrap.len] = handle;
//     g_live_unwrap.len++;
// }

// void ED_uvedit_live_unwrap_re_solve()
// {
//     if (g_live_unwrap.handles)
//     {
//         for (int i = 0; i < g_live_unwrap.len; i++)
//         {
//             if (uv_parametrizer_is_slim(g_live_unwrap.handles[i]))
//             {
//                 uv_parametrizer_slim_live_solve_iteration(g_live_unwrap.handles[i]);
//             }
//             else
//             {
//                 blender::geometry::uv_parametrizer_lscm_solve(g_live_unwrap.handles[i], nullptr, nullptr);
//             }
//
//             blender::geometry::uv_parametrizer_flush(g_live_unwrap.handles[i]);
//         }
//     }
// }

// void ED_uvedit_live_unwrap_end(const bool cancel)
// {
//     if (g_live_unwrap.timer)
//     {
//         wmWindowManager* wm = static_cast<wmWindowManager*>(G_MAIN->wm.first);
//         uvedit_live_unwrap_timer_validate(wm);
//         if (g_live_unwrap.timer)
//         {
//             wmWindow* win = g_live_unwrap.timer->win;
//             WM_event_timer_remove(wm, win, g_live_unwrap.timer);
//             g_live_unwrap.timer = nullptr;
//         }
//     }
//
//     if (g_live_unwrap.handles)
//     {
//         for (int i = 0; i < g_live_unwrap.len; i++)
//         {
//             if (uv_parametrizer_is_slim(g_live_unwrap.handles[i]))
//             {
//                 uv_parametrizer_slim_live_end(g_live_unwrap.handles[i]);
//             }
//             else
//             {
//                 blender::geometry::uv_parametrizer_lscm_end(g_live_unwrap.handles[i]);
//             }
//
//             if (cancel)
//             {
//                 blender::geometry::uv_parametrizer_flush_restore(g_live_unwrap.handles[i]);
//             }
//             delete (g_live_unwrap.handles[i]);
//         }
//         MEM_freeN(g_live_unwrap.handles);
//         g_live_unwrap.handles = nullptr;
//         g_live_unwrap.len = 0;
//         g_live_unwrap.len_alloc = 0;
//     }
// }

#define VIEW_ON_EQUATOR 0
#define VIEW_ON_POLES 1
#define ALIGN_TO_OBJECT 2

#define POLAR_ZX 0
#define POLAR_ZY 1

// enum
// {
//     PINCH = 0,
//     FAN = 1,
// };

// static void uv_map_transform_calc_bounds(BMEditMesh* em, float r_min[3], float r_max[3])
// {
//     BMFace* efa;
//     BMIter iter;
//     INIT_MINMAX(r_min, r_max);
//     BM_ITER_MESH(efa, &iter, em->bm, BM_FACES_OF_MESH)
//     {
//         if (BM_elem_flag_test(efa, BM_ELEM_SELECT))
//         {
//             BM_face_calc_bounds_expand(efa, r_min, r_max);
//         }
//     }
// }

// static void uv_map_transform_calc_center_median(BMEditMesh* em, float r_center[3])
// {
//     BMFace* efa;
//     BMIter iter;
//     uint center_accum_num = 0;
//     zero_v3(r_center);
//     BM_ITER_MESH(efa, &iter, em->bm, BM_FACES_OF_MESH)
//     {
//         if (BM_elem_flag_test(efa, BM_ELEM_SELECT))
//         {
//             float center[3];
//             BM_face_calc_center_median(efa, center);
//             add_v3_v3(r_center, center);
//             center_accum_num += 1;
//         }
//     }
//     mul_v3_fl(r_center, 1.0f / float(center_accum_num));
// }

// static void uv_map_transform_center(const Scene* scene, View3D* v3d, Object* ob, BMEditMesh* em, float r_center[3], float r_bounds[2][3])
// {
//     /* only operates on the edit object - this is all that's needed now */
//     const int around = (v3d) ? scene->toolsettings->transform_pivot_point : int(V3D_AROUND_CENTER_BOUNDS);
//
//     float bounds[2][3];
//     INIT_MINMAX(bounds[0], bounds[1]);
//     bool is_minmax_set = false;
//
//     switch (around)
//     {
//     case V3D_AROUND_CENTER_BOUNDS: /* bounding box center */
//     {
//         uv_map_transform_calc_bounds(em, bounds[0], bounds[1]);
//         is_minmax_set = true;
//         mid_v3_v3v3(r_center, bounds[0], bounds[1]);
//         break;
//     }
//     case V3D_AROUND_CENTER_MEDIAN:
//     {
//         uv_map_transform_calc_center_median(em, r_center);
//         break;
//     }
//     case V3D_AROUND_CURSOR: /* cursor center */
//     {
//         invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
//         mul_v3_m4v3(r_center, ob->world_to_object().ptr(), scene->cursor.location);
//         break;
//     }
//     case V3D_AROUND_ACTIVE:
//     {
//         BMEditSelection ese;
//         if (BM_select_history_active_get(em->bm, &ese))
//         {
//             BM_editselection_center(&ese, r_center);
//             break;
//         }
//         ATTR_FALLTHROUGH;
//     }
//     case V3D_AROUND_LOCAL_ORIGINS: /* object center */
//     default:
//         zero_v3(r_center);
//         break;
//     }
//
//     /* if this is passed, always set! */
//     if (r_bounds)
//     {
//         if (! is_minmax_set)
//         {
//             uv_map_transform_calc_bounds(em, bounds[0], bounds[1]);
//         }
//         copy_v3_v3(r_bounds[0], bounds[0]);
//         copy_v3_v3(r_bounds[1], bounds[1]);
//     }
// }

// static void uv_map_rotation_matrix_ex(float result[4][4], RegionView3D* rv3d, Object* ob, float upangledeg, float sideangledeg, float radius, const float offset[4])
// {
//     float rotup[4][4], rotside[4][4], viewmatrix[4][4], rotobj[4][4];
//     float sideangle = 0.0f, upangle = 0.0f;
//
//     /* get rotation of the current view matrix */
//     if (rv3d)
//     {
//         copy_m4_m4(viewmatrix, rv3d->viewmat);
//     }
//     else
//     {
//         unit_m4(viewmatrix);
//     }
//
//     /* but shifting */
//     zero_v3(viewmatrix[3]);
//
//     /* get rotation of the current object matrix */
//     copy_m4_m4(rotobj, ob->object_to_world().ptr());
//     zero_v3(rotobj[3]);
//
//     /* but shifting */
//     add_v4_v4(rotobj[3], offset);
//     rotobj[3][3] = 0.0f;
//
//     zero_m4(rotup);
//     zero_m4(rotside);
//
//     /* Compensate front/side.. against opengl x,y,z world definition.
//      * This is "a sledgehammer to crack a nut" (overkill), a few plus minus 1 will do here.
//      * I wanted to keep the reason here, so we're rotating. */
//     sideangle = float(M_PI) * (sideangledeg + 180.0f) / 180.0f;
//     rotside[0][0] = cosf(sideangle);
//     rotside[0][1] = -sinf(sideangle);
//     rotside[1][0] = sinf(sideangle);
//     rotside[1][1] = cosf(sideangle);
//     rotside[2][2] = 1.0f;
//
//     upangle = float(M_PI) * upangledeg / 180.0f;
//     rotup[1][1] = cosf(upangle) / radius;
//     rotup[1][2] = -sinf(upangle) / radius;
//     rotup[2][1] = sinf(upangle) / radius;
//     rotup[2][2] = cosf(upangle) / radius;
//     rotup[0][0] = 1.0f / radius;
//
//     /* Calculate transforms. */
//     mul_m4_series(result, rotup, rotside, viewmatrix, rotobj);
// }

// static void uv_map_transform(bContext* C, wmOperator* op, float rotmat[3][3])
// {
//     Object* obedit = CTX_data_edit_object(C);
//     RegionView3D* rv3d = CTX_wm_region_view3d(C);
//
//     const int align = RNA_enum_get(op->ptr, "align");
//     const int direction = RNA_enum_get(op->ptr, "direction");
//     const float radius = RNA_struct_find_property(op->ptr, "radius") ? RNA_float_get(op->ptr, "radius") : 1.0f;
//
//     /* Be compatible to the "old" sphere/cylinder mode. */
//     if (direction == ALIGN_TO_OBJECT)
//     {
//         unit_m3(rotmat);
//
//         if (align == POLAR_ZY)
//         {
//             rotmat[0][0] = 0.0f;
//             rotmat[0][1] = 1.0f;
//             rotmat[1][0] = -1.0f;
//             rotmat[1][1] = 0.0f;
//         }
//         return;
//     }
//
//     const float up_angle_deg = (direction == VIEW_ON_EQUATOR) ? 90.0f : 0.0f;
//     const float side_angle_deg = (align == POLAR_ZY) == (direction == VIEW_ON_EQUATOR) ? 90.0f : 0.0f;
//     const float offset[4] = { 0 };
//     float rotmat4[4][4];
//     uv_map_rotation_matrix_ex(rotmat4, rv3d, obedit, up_angle_deg, side_angle_deg, radius, offset);
//     copy_m3_m4(rotmat, rotmat4);
// }

// static void uv_transform_properties(wmOperatorType* ot, int radius)
// {
//     static const EnumPropertyItem direction_items[] = {
//         { VIEW_ON_EQUATOR, "VIEW_ON_EQUATOR", 0, "View on Equator", "3D view is on the equator" },
//         { VIEW_ON_POLES, "VIEW_ON_POLES", 0, "View on Poles", "3D view is on the poles" },
//         { ALIGN_TO_OBJECT, "ALIGN_TO_OBJECT", 0, "Align to Object", "Align according to object transform" },
//         { 0, nullptr, 0, nullptr, nullptr },
//     };
//     static const EnumPropertyItem align_items[] = {
//         { POLAR_ZX, "POLAR_ZX", 0, "Polar ZX", "Polar 0 is X" },
//         { POLAR_ZY, "POLAR_ZY", 0, "Polar ZY", "Polar 0 is Y" },
//         { 0, nullptr, 0, nullptr, nullptr },
//     };
//
//     static const EnumPropertyItem pole_items[] = {
//         { PINCH, "PINCH", 0, "Pinch", "UVs are pinched at the poles" },
//         { FAN, "FAN", 0, "Fan", "UVs are fanned at the poles" },
//         { 0, nullptr, 0, nullptr, nullptr },
//     };
//
//     RNA_def_enum(ot->srna, "direction", direction_items, VIEW_ON_EQUATOR, "Direction", "Direction of the sphere or cylinder");
//     RNA_def_enum(ot->srna, "align", align_items, POLAR_ZX, "Align", "How to determine rotation around the pole");
//     RNA_def_enum(ot->srna, "pole", pole_items, PINCH, "Pole", "How to handle faces at the poles");
//     RNA_def_boolean(ot->srna, "seam", false, "Preserve Seams", "Separate projections by islands isolated by seams");
//
//     if (radius)
//     {
//         RNA_def_float(ot->srna, "radius", 1.0f, 0.0f, FLT_MAX, "Radius", "Radius of the sphere or cylinder", 0.0001f, 100.0f);
//     }
// }

// static void shrink_loop_uv_by_aspect_ratio(BMFace* efa, const int cd_loop_uv_offset, const float aspect_y)
// {
//     BLI_assert(aspect_y != 1.0f); /* Nothing to do, should be handled by caller. */
//     BLI_assert(aspect_y > 0.0f); /* Negative aspect ratios are not supported. */
//
//     BMLoop* l;
//     BMIter iter;
//     BM_ITER_ELEM(l, &iter, efa, BM_LOOPS_OF_FACE)
//     {
//         float* luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
//         if (aspect_y > 1.0f)
//         {
//             /* Reduce round-off error, i.e. `u = (u - 0.5) / aspect_y + 0.5`. */
//             luv[0] = luv[0] / aspect_y + (0.5f - 0.5f / aspect_y);
//         }
//         else
//         {
//             /* Reduce round-off error, i.e. `v = (v - 0.5) * aspect_y + 0.5`. */
//             luv[1] = luv[1] * aspect_y + (0.5f - 0.5f * aspect_y);
//         }
//     }
// }

// static void correct_uv_aspect(Object* ob, BMEditMesh* em)
// {
//     const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_PROP_FLOAT2);
//     const float aspect_y = ED_uvedit_get_aspect_y(ob);
//     if (aspect_y == 1.0f)
//     {
//         /* Scaling by 1.0 has no effect. */
//         return;
//     }
//     BMFace* efa;
//     BMIter iter;
//     BM_ITER_MESH(efa, &iter, em->bm, BM_FACES_OF_MESH)
//     {
//         if (BM_elem_flag_test(efa, BM_ELEM_SELECT))
//         {
//             shrink_loop_uv_by_aspect_ratio(efa, cd_loop_uv_offset, aspect_y);
//         }
//     }
// }

// static void correct_uv_aspect_per_face(Object* ob, BMEditMesh* em)
// {
//     const int materials_num = ob->totcol;
//     if (materials_num == 0)
//     {
//         /* Without any materials, there is no aspect_y information and nothing to do. */
//         return;
//     }
//
//     blender::Array<float, 16> material_aspect_y(materials_num, -1);
//     /* Lazily initialize aspect ratio for materials. */
//
//     const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_PROP_FLOAT2);
//
//     BMFace* efa;
//     BMIter iter;
//     BM_ITER_MESH(efa, &iter, em->bm, BM_FACES_OF_MESH)
//     {
//         if (! BM_elem_flag_test(efa, BM_ELEM_SELECT))
//         {
//             continue;
//         }
//
//         const int material_index = efa->mat_nr;
//         if (UNLIKELY(material_index < 0 || material_index >= materials_num))
//         {
//             /* The index might be for a material slot which is not currently setup. */
//             continue;
//         }
//
//         float aspect_y = material_aspect_y[material_index];
//         if (aspect_y == -1.0f)
//         {
//             /* Lazily initialize aspect ratio for materials. */
//             float aspx, aspy;
//             ED_uvedit_get_aspect_from_material(ob, material_index, &aspx, &aspy);
//             aspect_y = aspx / aspy;
//             material_aspect_y[material_index] = aspect_y;
//         }
//
//         if (aspect_y == 1.0f)
//         {
//             /* Scaling by 1.0 has no effect. */
//             continue;
//         }
//         shrink_loop_uv_by_aspect_ratio(efa, cd_loop_uv_offset, aspect_y);
//     }
// }

#undef VIEW_ON_EQUATOR
#undef VIEW_ON_POLES
#undef ALIGN_TO_OBJECT

#undef POLAR_ZX
#undef POLAR_ZY

// static void uv_map_clip_correct_properties_ex(wmOperatorType* ot, bool clip_to_bounds)
// {
//     uv_map_operator_property_correct_aspect(ot);
//     /* Optional, since not all unwrapping types need to be clipped. */
//     if (clip_to_bounds)
//     {
//         RNA_def_boolean(ot->srna, "clip_to_bounds", false, "Clip to Bounds", "Clip UV coordinates to bounds after unwrapping");
//     }
//     RNA_def_boolean(ot->srna, "scale_to_bounds", false, "Scale to Bounds", "Scale UV coordinates to bounds after unwrapping");
// }

// static void uv_map_clip_correct_properties(wmOperatorType* ot)
// {
//     uv_map_clip_correct_properties_ex(ot, true);
// }

/**
 * \param per_face_aspect: Calculate the aspect ratio per-face,
 * otherwise use a single aspect for all UVs based on the material of the active face.
 * TODO: using per-face aspect may split UV islands so more advanced UV projection methods
 * such as "Unwrap" & "Smart UV Projections" will need to handle aspect correction themselves.
 * For now keep using a single aspect for all faces in this case.
 */
// static void uv_map_clip_correct(const Scene* scene, const Span<Object*> objects, wmOperator* op, bool per_face_aspect, bool only_selected_uvs)
// {
//     BMFace* efa;
//     BMLoop* l;
//     BMIter iter, liter;
//     float dx, dy, min[2], max[2];
//     const bool correct_aspect = RNA_boolean_get(op->ptr, "correct_aspect");
//     const bool clip_to_bounds = (RNA_struct_find_property(op->ptr, "clip_to_bounds") && RNA_boolean_get(op->ptr, "clip_to_bounds"));
//     const bool scale_to_bounds = RNA_boolean_get(op->ptr, "scale_to_bounds");
//
//     INIT_MINMAX2(min, max);
//
//     for (Object* ob : objects)
//     {
//         BMEditMesh* em = BKE_editmesh_from_object(ob);
//         const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);
//
//         /* Correct for image aspect ratio. */
//         if (correct_aspect)
//         {
//             if (per_face_aspect)
//             {
//                 correct_uv_aspect_per_face(ob, em);
//             }
//             else
//             {
//                 correct_uv_aspect(ob, em);
//             }
//         }
//
//         if (scale_to_bounds)
//         {
//             /* find uv limits */
//             BM_ITER_MESH(efa, &iter, em->bm, BM_FACES_OF_MESH)
//             {
//                 if (! BM_elem_flag_test(efa, BM_ELEM_SELECT))
//                 {
//                     continue;
//                 }
//
//                 if (only_selected_uvs && ! uvedit_face_select_test(scene, efa, offsets))
//                 {
//                     continue;
//                 }
//
//                 BM_ITER_ELEM(l, &liter, efa, BM_LOOPS_OF_FACE)
//                 {
//                     float* luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
//                     minmax_v2v2_v2(min, max, luv);
//                 }
//             }
//         }
//         else if (clip_to_bounds)
//         {
//             /* clipping and wrapping */
//             BM_ITER_MESH(efa, &iter, em->bm, BM_FACES_OF_MESH)
//             {
//                 if (! BM_elem_flag_test(efa, BM_ELEM_SELECT))
//                 {
//                     continue;
//                 }
//
//                 if (only_selected_uvs && ! uvedit_face_select_test(scene, efa, offsets))
//                 {
//                     continue;
//                 }
//
//                 BM_ITER_ELEM(l, &liter, efa, BM_LOOPS_OF_FACE)
//                 {
//                     float* luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
//                     clamp_v2(luv, 0.0f, 1.0f);
//                 }
//             }
//         }
//     }
//
//     if (scale_to_bounds)
//     {
//         /* rescale UV to be in 1/1 */
//         dx = (max[0] - min[0]);
//         dy = (max[1] - min[1]);
//
//         if (dx > 0.0f)
//         {
//             dx = 1.0f / dx;
//         }
//         if (dy > 0.0f)
//         {
//             dy = 1.0f / dy;
//         }
//
//         if (dx == 1.0f && dy == 1.0f && min[0] == 0.0f && min[1] == 0.0f)
//         {
//             /* Scaling by 1.0, without translating, has no effect. */
//             return;
//         }
//
//         for (Object* ob : objects)
//         {
//             BMEditMesh* em = BKE_editmesh_from_object(ob);
//             const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);
//
//             BM_ITER_MESH(efa, &iter, em->bm, BM_FACES_OF_MESH)
//             {
//                 if (! BM_elem_flag_test(efa, BM_ELEM_SELECT))
//                 {
//                     continue;
//                 }
//
//                 if (only_selected_uvs && ! uvedit_face_select_test(scene, efa, offsets))
//                 {
//                     continue;
//                 }
//
//                 BM_ITER_ELEM(l, &liter, efa, BM_LOOPS_OF_FACE)
//                 {
//                     float* luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
//
//                     luv[0] = (luv[0] - min[0]) * dx;
//                     luv[1] = (luv[1] - min[1]) * dy;
//                 }
//             }
//         }
//     }
// }

/* Assumes UV Map exists, doesn't run update functions. */
// static void uvedit_unwrap(const Scene* scene, Object* obedit, const UnwrapOptions* options, int* r_count_changed, int* r_count_failed)
// {
//     BMEditMesh* em = BKE_editmesh_from_object(obedit);
//     if (! CustomData_has_layer(&em->bm->ldata, CD_PROP_FLOAT2))
//     {
//         return;
//     }
//
//     bool use_subsurf;
//     modifier_unwrap_state(obedit, options, &use_subsurf);
//
//     ParamHandle* handle;
//     if (use_subsurf)
//     {
//         handle = construct_param_handle_subsurfed(scene, obedit, em, options, r_count_failed);
//     }
//     else
//     {
//         handle = construct_param_handle(scene, obedit, em->bm, options, r_count_failed);
//     }
//
//     if (options->use_slim)
//     {
//         uv_parametrizer_slim_solve(handle, &options->slim, r_count_changed, r_count_failed);
//     }
//     else
//     {
//         blender::geometry::uv_parametrizer_lscm_begin(handle, false, options->use_abf);
//         blender::geometry::uv_parametrizer_lscm_solve(handle, r_count_changed, r_count_failed);
//         blender::geometry::uv_parametrizer_lscm_end(handle);
//     }
//
//     blender::geometry::uv_parametrizer_average(handle, true, false, false);
//
//     blender::geometry::uv_parametrizer_flush(handle);
//
//     delete (handle);
// }

// static void uvedit_unwrap_multi(const Scene* scene, const Span<Object*> objects, const UnwrapOptions* options, int* r_count_changed = nullptr, int* r_count_failed = nullptr)
// {
//     for (Object* obedit : objects)
//     {
//         uvedit_unwrap(scene, obedit, options, r_count_changed, r_count_failed);
//         DEG_id_tag_update(static_cast<ID*>(obedit->data), ID_RECALC_GEOMETRY);
//         WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
//     }
// }

// void ED_uvedit_live_unwrap(const Scene* scene, const Span<Object*> objects)
// {
//     if (scene->toolsettings->edge_mode_live_unwrap)
//     {
//         UnwrapOptions options = unwrap_options_get(nullptr, nullptr, scene->toolsettings);
//         options.topology_from_uvs = false;
//         options.only_selected_faces = false;
//         options.only_selected_uvs = false;
//
//         uvedit_unwrap_multi(scene, objects, &options, nullptr);
//
//         blender::geometry::UVPackIsland_Params pack_island_params;
//         pack_island_params.setFromUnwrapOptions(options);
//         pack_island_params.rotate_method = ED_UVPACK_ROTATION_ANY;
//         pack_island_params.pin_method = ED_UVPACK_PIN_IGNORE;
//         pack_island_params.margin_method = ED_UVPACK_MARGIN_SCALED;
//         pack_island_params.margin = scene->toolsettings->uvcalc_margin;
//
//         uvedit_pack_islands_multi(scene, objects, nullptr, nullptr, false, true, &pack_island_params);
//     }
// }

// enum
// {
//     UNWRAP_ERROR_NONUNIFORM = (1 << 0),
//     UNWRAP_ERROR_NEGATIVE = (1 << 1),
// };

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smart UV Project Operator
 * \{ */

/* Ignore all areas below this, as the UVs get zeroed. */
// Keep this
static const float smart_uv_project_area_ignore = 1e-12f;

// Keep this
struct ThickFace
{
    float area;
    const Face* face;
    size_t face_index;
    Vertex normal;
};

// Keep this
static bool smart_uv_project_thickface_area_cmp_fn(const ThickFace& tf_a, const ThickFace& tf_b)
{
    return tf_a.area > tf_b.area;

    // /* Ignore the area of small faces.
    //  * Also, order checks so `!isfinite(...)` values are counted as zero area. */
    // if (! ((tf_a->area > smart_uv_project_area_ignore) || (tf_b->area > smart_uv_project_area_ignore)))
    // {
    //     return 0;
    // }
    //
    // if (tf_a->area < tf_b->area)
    // {
    //     return 1;
    // }
    // if (tf_a->area > tf_b->area)
    // {
    //     return -1;
    // }
    // return 0;
}

// Keep this
static float dot_v3v3(const Vertex& v1, const Vertex& v2)
{
    return std::get<0>(v1) * std::get<0>(v2) + std::get<1>(v1) * std::get<1>(v2) + std::get<2>(v1) * std::get<2>(v2);
}

// Keep this
static void add_v3_v3(Vertex& vertex, const Vertex& add)
{
    std::get<0>(vertex) += std::get<0>(add);
    std::get<1>(vertex) += std::get<1>(add);
    std::get<2>(vertex) += std::get<2>(add);
}

static void madd_v3_v3(Vertex& vertex, const Vertex& add, const float factor)
{
    std::get<0>(vertex) += std::get<0>(add) * factor;
    std::get<1>(vertex) += std::get<1>(add) * factor;
    std::get<2>(vertex) += std::get<2>(add) * factor;
}

static void mul_v3_v3(Vertex& v1, const Vertex& v2, const float factor)
{
    std::get<0>(v1) = std::get<0>(v2) * factor;
    std::get<1>(v1) = std::get<1>(v2) * factor;
    std::get<2>(v1) = std::get<2>(v2) * factor;
}

static float normalize_v3_v3_length(Vertex& v1, const Vertex& v2, const float unit_length)
{
    float d = dot_v3v3(v2, v2);

    if (d > 1.0e-35f)
    {
        d = std::sqrt(d);
        mul_v3_v3(v1, v2, unit_length / d);
    }
    else
    {
        /* Either the vector is small or one of it's values contained `nan`. */
        v1 = { 0.0, 0.0, 0.0 };
        d = 0.0f;
    }

    return d;
}

static float normalize_v3_v3(Vertex& v1, const Vertex& v2)
{
    return normalize_v3_v3_length(v1, v2, 1.0);
}

static float normalize_v3(Vertex& vertex)
{
    return normalize_v3_v3(vertex, vertex);
}

// Keep this
static std::vector<Vertex> smart_uv_project_calculate_project_normals(
    const std::vector<ThickFace>& thick_faces,
    const std::vector<Vertex>& vertices,
    const float project_angle_limit_half_cos,
    const float project_angle_limit_cos,
    const float area_weight)
{
    if (thick_faces.empty()) [[unlikely]]
    {
        return {};
    }

    Vertex project_normal = thick_faces.front().normal;

    std::vector<const ThickFace*> project_thick_faces;
    std::vector<Vertex> project_normal_array;
    std::vector<bool> face_flags(thick_faces.size(), false);

    while (true)
    {
        for (int f_index = thick_faces.size() - 1; f_index >= 0; f_index--)
        {
            if (face_flags[f_index])
            {
                continue;
            }

            if (dot_v3v3(thick_faces[f_index].normal, project_normal) > project_angle_limit_half_cos)
            {
                project_thick_faces.push_back(&thick_faces[f_index]);
                face_flags[f_index] = true;
            }
        }

        Vertex average_normal = { 0.0, 0.0, 0.0 };

        if (area_weight <= 0.0)
        {
            for (int f_proj_index = 0; f_proj_index < project_thick_faces.size(); f_proj_index++)
            {
                const ThickFace* face = project_thick_faces[f_proj_index];
                add_v3_v3(average_normal, face->normal);
            }
        }
        else if (area_weight >= 1.0)
        {
            for (int f_proj_index = 0; f_proj_index < project_thick_faces.size(); f_proj_index++)
            {
                const ThickFace* face = project_thick_faces[f_proj_index];
                madd_v3_v3(average_normal, face->normal, face->area);
            }
        }
        else
        {
            for (int f_proj_index = 0; f_proj_index < project_thick_faces.size(); f_proj_index++)
            {
                const ThickFace* face = project_thick_faces[f_proj_index];
                const float area_blend = (face->area * area_weight) + (1.0f - area_weight);
                madd_v3_v3(average_normal, face->normal, area_blend);
            }
        }

        /* Avoid NAN. */
        if (normalize_v3(average_normal) != 0.0f)
        {
            project_normal_array.push_back(average_normal);
        }

        /* Find the most unique angle that points away from other normals. */
        float angle_best = 1.0;
        size_t angle_best_index = 0;

        for (int f_index = thick_faces.size() - 1; f_index >= 0; f_index--)
        {
            if (face_flags[f_index])
            {
                continue;
            }

            float angle_test = -1.0f;
            for (int p_index = 0; p_index < project_normal_array.size(); p_index++)
            {
                angle_test = std::max(angle_test, dot_v3v3(project_normal_array[p_index], thick_faces[f_index].normal));
            }

            if (angle_test < angle_best)
            {
                angle_best = angle_test;
                angle_best_index = f_index;
            }
        }

        if (angle_best < project_angle_limit_cos)
        {
            project_normal = thick_faces[angle_best_index].normal;
            project_thick_faces.clear();
            project_thick_faces.push_back(&thick_faces[angle_best_index]);
            face_flags[angle_best_index] = true;
        }
        else if (! project_normal_array.empty())
        {
            break;
        }
    }

    return project_normal_array;
}

static void add_newell_cross_v3_v3v3(Vertex& result, const Vertex& v1, const Vertex& v2)
{
    std::get<0>(result) += (std::get<1>(v1) - std::get<1>(v2)) * (std::get<2>(v1) + std::get<2>(v2));
    std::get<1>(result) += (std::get<2>(v1) - std::get<2>(v2)) * (std::get<0>(v1) + std::get<0>(v2));
    std::get<2>(result) += (std::get<0>(v1) - std::get<0>(v2)) * (std::get<1>(v1) + std::get<1>(v2));
}

static float len_v3(const Vertex& vertex)
{
    return std::sqrt(dot_v3v3(vertex, vertex));
}

using Matrix33 = std::tuple<Vertex, Vertex, Vertex>;

static float len_squared_v2(const Vertex& vertex)
{
    return (std::get<0>(vertex) * std::get<0>(vertex)) + (std::get<1>(vertex) * std::get<1>(vertex));
}

static void ortho_basis_v3v3_v3(Vertex& v1, Vertex& v2, const Vertex& normal)
{
    constexpr float eps = std::numeric_limits<float>::epsilon();
    const float f = len_squared_v2(normal);

    if (f > eps)
    {
        const float d = 1.0f / std::sqrt(f);

        // BLI_assert(isfinite(d));

        std::get<0>(v1) = std::get<1>(normal) * d;
        std::get<1>(v1) = -std::get<0>(normal) * d;
        std::get<2>(v1) = 0.0f;
        std::get<0>(v2) = -std::get<2>(normal) * std::get<1>(v1);
        std::get<1>(v2) = std::get<2>(normal) * std::get<0>(v1);
        std::get<2>(v2) = std::get<0>(normal) * std::get<1>(v1) - std::get<1>(normal) * std::get<0>(v1);
    }
    else
    {
        /* degenerate case */
        std::get<0>(v1) = (std::get<2>(normal) < 0.0f) ? -1.0f : 1.0f;
        std::get<1>(v1) = std::get<2>(v1) = std::get<0>(v2) = std::get<2>(v2) = 0.0f;
        std::get<1>(v2) = 1.0f;
    }
}

static void transpose_m3(Matrix33& matrix)
{
    std::swap(std::get<1>(std::get<0>(matrix)), std::get<0>(std::get<1>(matrix)));
    std::swap(std::get<2>(std::get<0>(matrix)), std::get<0>(std::get<2>(matrix)));
    std::swap(std::get<2>(std::get<1>(matrix)), std::get<1>(std::get<2>(matrix)));
}

/**
 * \brief Normal to x,y matrix
 *
 * Creates a 3x3 matrix from a normal.
 * This matrix can be applied to vectors so their `z` axis runs along \a normal.
 * In practice it means you can use x,y as 2d coords. \see
 *
 * \param r_mat: The matrix to return.
 * \param normal: A unit length vector.
 */
// Keep this
static void axis_dominant_v3_to_m3(Matrix33& r_mat, const Vertex& normal)
{
    // BLI_ASSERT_UNIT_V3(normal);

    std::get<2>(r_mat) = normal;
    ortho_basis_v3v3_v3(std::get<0>(r_mat), std::get<1>(r_mat), std::get<2>(r_mat));

    // BLI_ASSERT_UNIT_V3(r_mat[0]);
    // BLI_ASSERT_UNIT_V3(r_mat[1]);

    transpose_m3(r_mat);

    // BLI_assert(!is_negative_m3(r_mat));
    // BLI_assert((fabsf(dot_m3_v3_row_z(r_mat, normal) - 1.0f) < BLI_ASSERT_UNIT_EPSILON) ||
    // is_zero_v3(normal));
}

// Keep this
static void mul_v2_m3v3(UVCoord& res, const Matrix33& M, const Vertex& a)
{
    const Vertex tmp = a;

    std::get<0>(res) = std::get<0>(std::get<0>(M)) * std::get<0>(tmp) + std::get<0>(std::get<1>(M)) * std::get<1>(tmp) + std::get<0>(std::get<2>(M)) * std::get<2>(tmp);
    std::get<1>(res) = std::get<1>(std::get<0>(M)) * std::get<0>(tmp) + std::get<1>(std::get<1>(M)) * std::get<1>(tmp) + std::get<1>(std::get<2>(M)) * std::get<2>(tmp);
}

// Keep this
static void cross_v3_v3v3(Vertex& res, const Vertex& v1, const Vertex& v2)
{
    std::get<0>(res) = std::get<1>(v1) * std::get<2>(v2) - std::get<2>(v1) * std::get<1>(v2);
    std::get<1>(res) = std::get<2>(v1) * std::get<0>(v2) - std::get<0>(v1) * std::get<2>(v2);
    std::get<2>(res) = std::get<0>(v1) * std::get<1>(v2) - std::get<1>(v1) * std::get<0>(v2);
}

// Keep this
static void diff_v3_v3(Vertex& res, const Vertex& v1)
{
    std::get<0>(res) -= std::get<0>(v1);
    std::get<1>(res) -= std::get<1>(v1);
    std::get<2>(res) -= std::get<2>(v1);
}

// Keep this
static float triangle_area_v3(const Vertex& v1, const Vertex& v2, const Vertex& v3)
{
    Vertex n = { 0.0, 0.0, 0.0 };

    add_newell_cross_v3_v3v3(n, v1, v2);
    add_newell_cross_v3_v3v3(n, v2, v3);
    add_newell_cross_v3_v3v3(n, v3, v1);

    return len_v3(n) * 0.5f;
}

static Vertex triangle_normal(const Vertex& v1, const Vertex& v2, const Vertex& v3)
{
    Vertex n1, n2, normal;

    std::get<0>(n1) = std::get<0>(v1) - std::get<0>(v2);
    std::get<0>(n2) = std::get<0>(v2) - std::get<0>(v3);
    std::get<1>(n1) = std::get<1>(v1) - std::get<1>(v2);
    std::get<1>(n2) = std::get<1>(v2) - std::get<1>(v3);
    std::get<2>(n1) = std::get<2>(v1) - std::get<2>(v2);
    std::get<2>(n2) = std::get<2>(v2) - std::get<2>(v3);
    std::get<0>(normal) = std::get<1>(n1) * std::get<2>(n2) - std::get<2>(n1) * std::get<1>(n2);
    std::get<1>(normal) = std::get<2>(n1) * std::get<0>(n2) - std::get<0>(n1) * std::get<2>(n2);
    std::get<2>(normal) = std::get<0>(n1) * std::get<1>(n2) - std::get<1>(n1) * std::get<0>(n2);

    normalize_v3(normal);

    return normal;
}

static std::vector<ThickFace> makeThickFaces(const std::vector<Vertex>& vertices, const std::vector<Face>& faces)
{
    std::vector<ThickFace> thick_faces;
    thick_faces.reserve(faces.size());

    for (size_t i = 0; i < faces.size(); i++)
    {
        const Face& face = faces[i];

        const Vertex& v1 = vertices[std::get<0>(face)];
        const Vertex& v2 = vertices[std::get<1>(face)];
        const Vertex& v3 = vertices[std::get<2>(face)];

        thick_faces.emplace_back(triangle_area_v3(v1, v2, v3), &face, i, triangle_normal(v1, v2, v3));
    }

    return thick_faces;
}

static float deg2rad(float angle)
{
    return angle * std::numbers::pi / 180.0;
}

// Keep this
static std::vector<std::vector<size_t>> make_faces_groups(const std::vector<Vertex>& vertices, const std::vector<Face>& faces, std::vector<UVCoord>& uv_coords)
{
    constexpr float project_angle_limit = 10.0;
    constexpr float island_margin = 0.01;
    constexpr float area_weight = 0.0;

    const float project_angle_limit_cos = std::cos(deg2rad(project_angle_limit));
    const float project_angle_limit_half_cos = std::cos(deg2rad(project_angle_limit / 2));

    /* Memory arena for list links (cleared for each object). */
    // MemArena* arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

    // Vector<Object*> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(scene, view_layer, v3d);

    // Vector<Object*> objects_changed;

    // BMFace* efa;
    // BMIter iter;

    // Object* obedit = objects[ob_index];
    // BMEditMesh* em = BKE_editmesh_from_object(obedit);
    // bool changed = false;

    // if (! uvedit_ensure_uvs(obedit))
    // {
    //     continue;
    // }

    // const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);
    //  BLI_assert(offsets.uv >= 0);
    //   ThickFace* thick_faces = MEM_malloc_arrayN<ThickFace>(em->bm->totface, __func__);
    std::vector<ThickFace> thick_faces = makeThickFaces(vertices, faces);

    std::ranges::sort(thick_faces, smart_uv_project_thickface_area_cmp_fn);

    /* Remove all zero area faces. */
    size_t zero_faces_count = 0;
    while (zero_faces_count < thick_faces.empty() && thick_faces[thick_faces.size() - 1 - zero_faces_count].area <= smart_uv_project_area_ignore)
    {
        // /* Zero UVs so they don't overlap with other faces being unwrapped. */
        // BMIter liter;
        // BMLoop* l;
        // BM_ITER_ELEM(l, &liter, thick_faces[thick_faces_len - 1].efa, BM_LOOPS_OF_FACE)
        // {
        //     float* luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
        //     zero_v2(luv);
        //     changed = true;
        // }

        zero_faces_count++;
    }
    thick_faces.resize(thick_faces.size() - zero_faces_count);

    std::vector<Vertex> project_normal_array
        = smart_uv_project_calculate_project_normals(thick_faces, vertices, project_angle_limit_half_cos, project_angle_limit_cos, area_weight);

    if (project_normal_array.empty())
    {
        return {};
    }

    /* After finding projection vectors, we find the uv positions. */
    // LinkNode** thickface_project_groups = static_cast<LinkNode**>(MEM_callocN(sizeof(*thickface_project_groups) * project_normal_array.size(), __func__));
    std::map<size_t, std::vector<const ThickFace*>> thickface_project_groups;

    // BLI_memarena_clear(arena);

    for (const ThickFace& face : thick_faces | std::views::reverse)
    {
        float angle_best = dot_v3v3(face.normal, project_normal_array.front());
        size_t angle_best_index = 0;

        for (size_t p_index = 1; p_index < project_normal_array.size(); ++p_index)
        {
            const float angle_test = dot_v3v3(face.normal, project_normal_array[p_index]);
            if (angle_test > angle_best)
            {
                angle_best = angle_test;
                angle_best_index = p_index;
            }
        }

        thickface_project_groups[angle_best_index].push_back(&face);
        // BLI_linklist_prepend_arena(&thickface_project_groups[angle_best_index], &thick_faces[f_index], arena);
    }

    for (size_t p_index = 0; p_index < project_normal_array.size(); ++p_index)
    {
        auto iterator = thickface_project_groups.find(p_index);
        if (iterator == thickface_project_groups.end())
        {
            continue;
        }

        Matrix33 axis_mat;
        axis_dominant_v3_to_m3(axis_mat, project_normal_array[p_index]);

        for (const ThickFace* tf : iterator->second)
        {
            for (size_t vertex_index : { std::get<0>(*tf->face), std::get<1>(*tf->face), std::get<2>(*tf->face) })
            {
                UVCoord& uv = uv_coords[vertex_index];
                const Vertex& vertex = vertices[vertex_index];
                mul_v2_m3v3(uv, axis_mat, vertex);
            }
        }
    }

    std::vector<std::vector<size_t>> grouped_faces_indices;

    for (auto iterator = thickface_project_groups.begin(); iterator != thickface_project_groups.end(); ++iterator)
    {
        std::vector<size_t> faces_group;
        faces_group.reserve(iterator->second.size());

        for (const ThickFace* face : iterator->second)
        {
            faces_group.push_back(face->face_index);
        }

        grouped_faces_indices.push_back(std::move(faces_group));
    }

    return grouped_faces_indices;

    /* Pack islands & Stretch to UV bounds */
    // scene->toolsettings->uvcalc_margin = island_margin;
    /* Depsgraph refresh functions are called here. */
#if 0
    constexpr bool correct_aspect = true;
    UVPackIsland_Params params;
    params.rotate_method = ED_UVPACK_ROTATION_AXIS_ALIGNED_Y;
    params.only_selected_faces = true;
    params.correct_aspect = correct_aspect;
    params.use_seams = true;
    params.margin = island_margin;
    uvedit_pack_islands_multi(scene, nullptr, nullptr, false, true, params);
    /* #uvedit_pack_islands_multi only supports `per_face_aspect = false`. */
    constexpr bool per_face_aspect = false;
#endif
    // uv_map_clip_correct(scene, op, per_face_aspect, only_selected_uvs);

    // return true;
}

const std::vector<std::vector<size_t>> splitNonAdjacentFacesGroups(const std::vector<std::vector<size_t>>& grouped_faces, const std::vector<Face>& indices)
{
    std::vector<std::vector<size_t>> result;

    for (const std::vector<size_t>& faces_group : grouped_faces)
    {
        size_t max_group_index = 0; // Incrementing group index

        // Keep a double cache so that we can find very quickly the group of a vertex, and all the vertices from a group
        std::map<size_t, size_t> new_indices_groups; // vertex_index: group_index
        std::map<size_t, std::set<size_t>> new_groups_vertices; // group_index: [vertex_index]

        for (const size_t face_index : faces_group)
        {
            const Face& face = indices[face_index];
            const size_t i0 = std::get<0>(face);
            const size_t i1 = std::get<1>(face);
            const size_t i2 = std::get<2>(face);
            const auto it0 = new_indices_groups.find(i0);
            const auto it1 = new_indices_groups.find(i1);
            const auto it2 = new_indices_groups.find(i2);
            const bool assigned0 = it0 != new_indices_groups.end();
            const bool assigned1 = it1 != new_indices_groups.end();
            const bool assigned2 = it2 != new_indices_groups.end();

            std::set<size_t> assigned_groups;
            if (assigned0)
            {
                assigned_groups.insert(it0->second);
            }
            if (assigned1)
            {
                assigned_groups.insert(it1->second);
            }
            if (assigned2)
            {
                assigned_groups.insert(it2->second);
            }

            if (assigned_groups.empty())
            {
                // None of the points are assigned yet, just assign them to a new group
                const size_t new_group_index = max_group_index++;
                new_indices_groups[i0] = new_group_index;
                new_indices_groups[i1] = new_group_index;
                new_indices_groups[i2] = new_group_index;
                new_groups_vertices[new_group_index] = { i0, i1, i2 };
            }
            else
            {
                const size_t target_group = *assigned_groups.begin();
                std::set<size_t>& target_group_vertices = new_groups_vertices[target_group];

                std::set<size_t> source_groups = assigned_groups;
                source_groups.erase(source_groups.begin());

                // First assign vertices that are not assigned yet
                if (! assigned0)
                {
                    new_indices_groups[i0] = target_group;
                    target_group_vertices.insert(i0);
                }
                if (! assigned1)
                {
                    new_indices_groups[i1] = target_group;
                    target_group_vertices.insert(i1);
                }
                if (! assigned2)
                {
                    new_indices_groups[i2] = target_group;
                    target_group_vertices.insert(i2);
                }

                // Now merge source groups to the target group, including actually processed vertices
                for (const size_t source_group : source_groups)
                {
                    auto it_source_group = new_groups_vertices.find(source_group);
                    for (const size_t vertex_from_source_group : it_source_group->second)
                    {
                        size_t& vertex_group = new_indices_groups[vertex_from_source_group];
                        // new_groups_vertices[vertex_group].extract(vertex_from_source_group);
                        vertex_group = target_group;
                        target_group_vertices.insert(vertex_from_source_group);
                    }

                    new_groups_vertices.erase(it_source_group);
                }
            }
        }

        std::map<size_t, std::vector<size_t>> new_faces_groups; // group_index: [face_index]
        for (const size_t face_index : faces_group)
        {
            const Face& face = indices[face_index];
            const size_t i0 = std::get<0>(face);
            new_faces_groups[new_indices_groups[i0]].push_back(face_index);
        }

        for (const std::vector<size_t>& faces_indices : std::views::values(new_faces_groups))
        {
            result.push_back(faces_indices);
        }
    }

    return result;
}

bool unwrap_lscm(
    const std::vector<Vertex>& vertices,
    const std::vector<Face>& indices,
    uint32_t desired_definition,
    std::vector<UVCoord>& uv_coords,
    uint32_t& texture_width,
    uint32_t& texture_height)
{
    std::vector<std::vector<size_t>> grouped_faces = make_faces_groups(vertices, indices, uv_coords);
    // grouped_faces = splitNonAdjacentFacesGroups(grouped_faces, indices);

    // float u_min = 0.0;
    // float v_min = 0.0;
    // for (const UVCoord& unpacked_uv_coords : uv_coords)
    // {
    //     u_min = std::min(std::get<0>(unpacked_uv_coords), u_min);
    //     v_min = std::min(std::get<1>(unpacked_uv_coords), v_min);
    // }
    //
    // if (u_min < 0.0 || v_min < 0.0)
    // {
    //     for (UVCoord& unpacked_uv_coords : uv_coords)
    //     {
    //         std::get<0>(unpacked_uv_coords) -= u_min;
    //         std::get<1>(unpacked_uv_coords) -= v_min;
    //     }
    // }

    printf("\rPacking charts\n");

    xatlas::Atlas* atlas = xatlas::Create();

    xatlas::UvMeshDecl mesh;
    mesh.vertexUvData = uv_coords.data();
    mesh.indexData = indices.data();
    mesh.vertexCount = vertices.size();
    mesh.vertexStride = sizeof(UVCoord);
    mesh.indexCount = indices.size() * 3;
    mesh.indexFormat = xatlas::IndexFormat::UInt32;

    if (xatlas::AddUvMesh(atlas, mesh) != xatlas::AddMeshError::Success)
    {
        xatlas::Destroy(atlas);
        printf("\rError adding mesh\n");
        return false;
    }

    const xatlas::PackOptions pack_options{ .resolution = desired_definition };
    xatlas::SetCharts(atlas, grouped_faces);
    xatlas::PackCharts(atlas, pack_options);

    // For some reason, the width and height need to be inverted to make the coordinates consistent
    texture_width = atlas->height;
    texture_height = atlas->width;

    const xatlas::Mesh& output_mesh = *atlas->meshes;

    const auto width = static_cast<float>(atlas->width);
    const auto height = static_cast<float>(atlas->height);

    for (size_t i = 0; i < output_mesh.vertexCount; ++i)
    {
        const xatlas::Vertex& vertex = output_mesh.vertexArray[i];
        uv_coords[vertex.xref] = std::make_tuple(vertex.uv[0] / width, vertex.uv[1] / height);
    }

    xatlas::Destroy(atlas);
    return true;

    return true;
}

// static void uv_map_mirror(BMFace* efa, const bool* regular, const bool fan, const int cd_loop_uv_offset)
// {
//     /* A heuristic to improve alignment of faces near the seam.
//      * In simple terms, we're looking for faces which span more
//      * than 0.5 units in the *u* coordinate.
//      * If we find such a face, we try and improve the unwrapping
//      * by adding (1.0, 0.0) onto some of the face's UVs.
//      *
//      * Note that this is only a heuristic. The property we're
//      * attempting to maintain is that the winding of the face
//      * in UV space corresponds with the handedness of the face
//      * in 3D space w.r.t to the unwrapping. Even for triangles,
//      * that property is somewhat complicated to evaluate. */
//
//     float right_u = -1.0e30f;
//     BMLoop* l;
//     BMIter liter;
//     blender::Array<float*, BM_DEFAULT_NGON_STACK_SIZE> uvs(efa->len);
//     int j;
//     BM_ITER_ELEM_INDEX(l, &liter, efa, BM_LOOPS_OF_FACE, j)
//     {
//         float* luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
//         uvs[j] = luv;
//         if (luv[0] >= 1.0f)
//         {
//             luv[0] -= 1.0f;
//         }
//         right_u = max_ff(right_u, luv[0]);
//     }
//
//     float left_u = 1.0e30f;
//     BM_ITER_ELEM(l, &liter, efa, BM_LOOPS_OF_FACE)
//     {
//         float* luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
//         if (right_u <= luv[0] + 0.5f)
//         {
//             left_u = min_ff(left_u, luv[0]);
//         }
//     }
//
//     BM_ITER_ELEM(l, &liter, efa, BM_LOOPS_OF_FACE)
//     {
//         float* luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
//         if (luv[0] + 0.5f < right_u)
//         {
//             if (2 * luv[0] + 1.0f < left_u + right_u)
//             {
//                 luv[0] += 1.0f;
//             }
//         }
//     }
//     if (! fan)
//     {
//         return;
//     }
//
//     /* Another heuristic, this time, we attempt to "fan"
//      * the UVs of faces which pass through one of the poles
//      * of the unwrapping. */
//
//     /* Need to recompute min and max. */
//     float minmax_u[2] = { 1.0e30f, -1.0e30f };
//     int pole_count = 0;
//     for (int i = 0; i < efa->len; i++)
//     {
//         if (regular[i])
//         {
//             minmax_u[0] = min_ff(minmax_u[0], uvs[i][0]);
//             minmax_u[1] = max_ff(minmax_u[1], uvs[i][0]);
//         }
//         else
//         {
//             pole_count++;
//         }
//     }
//     if (ELEM(pole_count, 0, efa->len))
//     {
//         return;
//     }
//     for (int i = 0; i < efa->len; i++)
//     {
//         if (regular[i])
//         {
//             continue;
//         }
//         float u = 0.0f;
//         float sum = 0.0f;
//         const int i_plus = (i + 1) % efa->len;
//         const int i_minus = (i + efa->len - 1) % efa->len;
//         if (regular[i_plus])
//         {
//             u += uvs[i_plus][0];
//             sum += 1.0f;
//         }
//         if (regular[i_minus])
//         {
//             u += uvs[i_minus][0];
//             sum += 1.0f;
//         }
//         if (sum == 0)
//         {
//             u += minmax_u[0] + minmax_u[1];
//             sum += 2.0f;
//         }
//         uvs[i][0] = u / sum;
//     }
// }

/**
 * Store a face and it's current branch on the generalized atan2 function.
 *
 * In complex analysis, we can generalize the `arctangent` function
 * into a multi-valued function that is "almost everywhere continuous"
 * in the complex plane.
 *
 * The downside is that we need to keep track of which "branch" of the
 * multi-valued function we are currently on.
 *
 * \note Even though `atan2(a+bi, c+di)` is now (multiply) defined for all
 * complex inputs, we will only evaluate it with `b==0` and `d==0`.
 */
// struct UV_FaceBranch
// {
//     BMFace* efa;
//     float branch;
// };

// void ED_uvedit_add_simple_uvs(Main* bmain, const Scene* scene, Object* ob)
// {
//     Mesh* mesh = static_cast<Mesh*>(ob->data);
//     bool sync_selection = (scene->toolsettings->uv_flag & UV_FLAG_SYNC_SELECT) != 0;
//
//     BMeshCreateParams create_params{};
//     create_params.use_toolflags = false;
//     BMesh* bm = BM_mesh_create(&bm_mesh_allocsize_default, &create_params);
//
//     /* turn sync selection off,
//      * since we are not in edit mode we need to ensure only the uv flags are tested */
//     scene->toolsettings->uv_flag &= ~UV_FLAG_SYNC_SELECT;
//
//     ED_mesh_uv_ensure(mesh, nullptr);
//
//     BMeshFromMeshParams bm_from_me_params{};
//     bm_from_me_params.calc_face_normal = true;
//     bm_from_me_params.calc_vert_normal = true;
//     BM_mesh_bm_from_me(bm, mesh, &bm_from_me_params);
//
//     /* Select all UVs for cube_project. */
//     ED_uvedit_select_all(bm);
//     /* A cube size of 2.0 maps [-1..1] vertex coords to [0.0..1.0] in UV coords. */
//     uvedit_unwrap_cube_project(scene, bm, 2.0, false, false, nullptr);
//
//     /* Pack UVs. */
//     blender::geometry::UVPackIsland_Params params;
//     params.rotate_method = ED_UVPACK_ROTATION_ANY;
//     params.only_selected_uvs = false;
//     params.only_selected_faces = false;
//     params.correct_aspect = false;
//     params.use_seams = true;
//     params.margin_method = ED_UVPACK_MARGIN_SCALED;
//     params.margin = 0.001f;
//
//     uvedit_pack_islands_multi(scene, { ob }, &bm, nullptr, false, true, &params);
//
//     /* Write back from BMesh to Mesh. */
//     BMeshToMeshParams bm_to_me_params{};
//     BM_mesh_bm_to_me(bmain, bm, mesh, &bm_to_me_params);
//     BM_mesh_free(bm);
//
//     if (sync_selection)
//     {
//         scene->toolsettings->uv_flag |= UV_FLAG_SYNC_SELECT;
//     }
// }
