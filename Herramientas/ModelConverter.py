import bpy
import math
import mathutils
import re
import struct
from pathlib import Path
if bpy.data.filepath:
    OUTPUT = Path(bpy.data.filepath).resolve().with_suffix(".f3d")
else:
    OUTPUT = Path(bpy.app.tempdir or ".").resolve() / "untitled.f3d"
    print("AVISO: guarda primero el .blend; se usará untitled.f3d.")

scene = bpy.context.scene
original_frame = scene.frame_current
original_subframe = scene.frame_subframe
fps = scene.render.fps / max(scene.render.fps_base, 0.000001)

INTERPOLATION_MODE_IDS = {
    name: index for index, name in enumerate((
        "CONSTANT", "LINEAR", "BEZIER", "SINE", "QUAD", "CUBIC", "QUART",
        "QUINT", "EXPO", "CIRC", "BACK", "BOUNCE", "ELASTIC",
    ))
}

meshes = [obj for obj in scene.objects if obj.type == "MESH"]
armatures = [obj for obj in scene.objects if obj.type == "ARMATURE"]
armature_indices = {obj.as_pointer(): i for i, obj in enumerate(armatures)}

materials = []
material_indices = {}
for obj in meshes:
    for slot in obj.material_slots:
        material = slot.material
        if material is not None and material.as_pointer() not in material_indices:
            material_indices[material.as_pointer()] = len(materials)
            materials.append(material)


def write_string(file_obj, value):
    data = str(value).encode("utf-8")
    file_obj.write(struct.pack("<I", len(data)))
    file_obj.write(data)


def matrix_values(matrix):
    return tuple(float(matrix[r][c]) for r in range(4) for c in range(4))


def write_matrix(file_obj, matrix):
    file_obj.write(struct.pack("<16f", *matrix_values(matrix)))


def set_frame(frame):
    scene.frame_set(frame)
    bpy.context.view_layer.update()


def get_vector(node, socket_name, fallback):
    socket = node.inputs.get(socket_name)
    if socket is None:
        return tuple(fallback)
    try:
        return tuple(float(value) for value in socket.default_value[:3])
    except (TypeError, ValueError, IndexError):
        return tuple(fallback)


def get_id_type(datablock):
    return getattr(datablock, "id_type", "")


def get_mapping_nodes(node_tree):
    if node_tree is None or not getattr(node_tree, "nodes", None):
        return []
    return [node for node in node_tree.nodes if node.type == "MAPPING"]


def curves_for_action(action, slot):
    """Get F-curves from a legacy Action or a specific modern Action Slot."""
    if slot is not None:
        try:
            from bpy_extras import anim_utils
            channelbag = anim_utils.action_get_channelbag_for_slot(action, slot)
            if channelbag is not None:
                return list(channelbag.fcurves)
        except Exception:
            pass
        if getattr(action, "is_action_legacy", False) is False and hasattr(action, "layers"):
            return []

    try:
        return list(action.fcurves)
    except Exception:
        return []


def action_frame_range(action, fcurves):
    """Return an inclusive integer sample range for one Action/Slot."""
    if getattr(action, "use_frame_range", False):
        start = float(getattr(action, "frame_start", 0.0))
        end = float(getattr(action, "frame_end", start))
    else:
        keyed_frames = []
        for curve in fcurves:
            for point in getattr(curve, "keyframe_points", ()):
                keyed_frames.append(float(point.co.x))
        if keyed_frames:
            start, end = min(keyed_frames), max(keyed_frames)
        else:
            try:
                start, end = (float(v) for v in action.frame_range)
            except Exception:
                start, end = float(scene.frame_start), float(scene.frame_end)

    first = math.floor(start)
    last = math.ceil(end)
    if last < first:
        first, last = last, first
    return first, last


def matching_slot_for_owner(action, slot, owner):
    """Return whether a slot appears to belong to a data-block by name."""
    if slot is None:
        return False
    owner_name = owner.name
    slot_display = getattr(slot, "name_display", "") or ""
    slot_identifier = getattr(slot, "identifier", "") or ""
    return (
        slot_display == owner_name
        or slot_identifier == owner_name
        or slot_identifier.endswith(owner_name)
    )


def assigned_to_slot(owner, action, slot):
    anim_data = getattr(owner, "animation_data", None)
    if anim_data is None or anim_data.action != action:
        return False
    if slot is None:
        return True
    current_slot = getattr(anim_data, "action_slot", None)
    return current_slot is not None and getattr(current_slot, "handle", None) == getattr(slot, "handle", None)


def has_pose_curves(fcurves):
    return any("pose.bones[" in curve.data_path for curve in fcurves)


def matching_mapping_nodes(node_tree, fcurves):
    """Find Mapping nodes animated by F-curves in this NodeTree."""
    matches = []
    for node in get_mapping_nodes(node_tree):
        try:
            prefix = node.path_from_id()
        except Exception:
            prefix = f'nodes["{node.name}"]'

        for curve in fcurves:
            path = curve.data_path
            if (
                path.startswith(prefix)
                and "inputs[" in path
                and "default_value" in path
            ):
                matches.append(node)
                break
    return matches


def mapping_component_curves(node, fcurves):
    """Return F-curves for Location, Rotation and Scale XYZ components."""
    result = []
    for socket_name in ("Location", "Rotation", "Scale"):
        socket = node.inputs.get(socket_name)
        try:
            data_path = socket.path_from_id("default_value") if socket is not None else None
        except Exception:
            data_path = None

        for axis in range(3):
            curve = next((candidate for candidate in fcurves
                          if candidate.array_index == axis
                          and candidate.data_path == data_path), None)
            result.append(curve)
    return result


def interpolation_mode_at(curve, frame):
    """Return the interpolation used by this F-curve from the sampled frame."""
    if curve is None or not curve.keyframe_points:
        return INTERPOLATION_MODE_IDS["LINEAR"]

    points = curve.keyframe_points
    previous = points[0]
    for point in points:
        if float(point.co.x) > frame + 0.000001:
            break
        previous = point

    mode = getattr(previous, "interpolation", "LINEAR")
    if mode not in INTERPOLATION_MODE_IDS:
        raise ValueError(f"Modo de interpolación Blender no soportado: {mode}")
    return INTERPOLATION_MODE_IDS[mode]


def owner_matches_slot_type(owner, slot):
    if slot is None:
        return True
    slot_type = getattr(slot, "target_id_type", "UNSPECIFIED")
    owner_type = get_id_type(owner)
    return slot_type in ("UNSPECIFIED", "", owner_type)


def choose_owner(candidates, action, slot, fcurves, kind, tree_mapping_map=None):
    """Choose the specific target for one Action/Slot, avoiding guesswork.

    Priority: already assigned target, target named by the slot, then a
    unique path-compatible candidate. Ambiguous clips are skipped safely.
    """
    candidates = [owner for owner in candidates if owner_matches_slot_type(owner, slot)]

    if kind == "SKELETAL":
        def compatible(owner):
            bone_names = {bone.name for bone in owner.data.bones}
            return any(
                f'pose.bones["{bone_name}"]' in curve.data_path
                for bone_name in bone_names
                for curve in fcurves
            )
    else:
        def compatible(owner):
            return bool((tree_mapping_map or {}).get(owner.as_pointer(), []))

    candidates = [owner for owner in candidates if compatible(owner)]
    if not candidates:
        return None

    assigned = [owner for owner in candidates if assigned_to_slot(owner, action, slot)]
    if len(assigned) == 1:
        return assigned[0]
    if len(assigned) > 1:
        candidates = assigned

    named = [owner for owner in candidates if matching_slot_for_owner(action, slot, owner)]
    if len(named) == 1:
        return named[0]
    if len(named) > 1:
        candidates = named

    if len(candidates) == 1:
        return candidates[0]

    names = ", ".join(owner.name for owner in candidates)
    print(
        f"AVISO: se omite el clip ambiguo '{action.name}' "
        f"(slot '{getattr(slot, 'identifier', 'Legacy')}'); "
        f"posibles destinos: {names}. Asigna la acción/slot al destino correcto."
    )
    return None
node_trees = []
seen_tree_ptrs = set()
for material in bpy.data.materials:
    tree = material.node_tree
    if tree is not None and tree.as_pointer() not in seen_tree_ptrs:
        seen_tree_ptrs.add(tree.as_pointer())
        node_trees.append(tree)
for tree in bpy.data.node_groups:
    if tree is not None and tree.as_pointer() not in seen_tree_ptrs:
        seen_tree_ptrs.add(tree.as_pointer())
        node_trees.append(tree)
mesh_armatures = {}
for obj in meshes:
    armature_obj = next(
        (
            modifier.object
            for modifier in obj.modifiers
            if modifier.type == "ARMATURE"
            and modifier.object is not None
            and modifier.object.type == "ARMATURE"
            and modifier.object.as_pointer() in armature_indices
        ),
        None,
    )
    mesh_armatures[obj.as_pointer()] = (
        armature_indices[armature_obj.as_pointer()] if armature_obj else -1
    )
clips = []
for action in bpy.data.actions:
    action_slots = list(getattr(action, "slots", []))
    slots = action_slots if action_slots else [None]

    for slot in slots:
        fcurves = curves_for_action(action, slot)
        if not fcurves:
            continue

        if has_pose_curves(fcurves):
            owner = choose_owner(
                armatures, action, slot, fcurves, "SKELETAL"
            )
            if owner is not None:
                first, last = action_frame_range(action, fcurves)
                clips.append({
                    "kind": "SKELETAL",
                    "code": 1,
                    "name": f"{action.name}::{getattr(slot, 'identifier', 'Legacy')}::{owner.name}",
                    "action": action,
                    "slot": slot,
                    "owner": owner,
                    "fcurves": fcurves,
                    "mapping_nodes": [],
                    "start": first,
                    "end": last,
                })
            continue
        tree_mapping_map = {
            tree.as_pointer(): matching_mapping_nodes(tree, fcurves)
            for tree in node_trees
        }
        if not any(tree_mapping_map.values()):
            continue

        owner = choose_owner(
            node_trees, action, slot, fcurves, "UV",
            tree_mapping_map=tree_mapping_map,
        )
        if owner is not None:
            first, last = action_frame_range(action, fcurves)
            clips.append({
                "kind": "UV",
                "code": 2,
                "name": f"{action.name}::{getattr(slot, 'identifier', 'Legacy')}::{owner.name}",
                "action": action,
                "slot": slot,
                "owner": owner,
                "fcurves": fcurves,
                "mapping_nodes": tree_mapping_map.get(owner.as_pointer(), []),
                "start": first,
                "end": last,
            })
def write_materials(file_obj):
    """Material records deliberately contain NO texture references or bytes."""
    for material in materials:
        write_string(file_obj, material.name)
        color = tuple(float(value) for value in material.diffuse_color)
        file_obj.write(struct.pack(
            "<4f2f",
            *color,
            float(getattr(material, "metallic", 0.0)),
            float(getattr(material, "roughness", 0.5)),
        ))

        mapping_nodes = get_mapping_nodes(material.node_tree)
        file_obj.write(struct.pack("<I", len(mapping_nodes)))
        for node in mapping_nodes:
            write_string(file_obj, node.name)
            write_string(file_obj, getattr(node, "vector_type", "POINT"))
            location = get_vector(node, "Location", (0, 0, 0))
            rotation = get_vector(node, "Rotation", (0, 0, 0))
            scale = get_vector(node, "Scale", (1, 1, 1))
            file_obj.write(struct.pack("<9f", *location, *rotation, *scale))


def get_mesh_color_layer(mesh):
    """Find the active or primary vertex color / color attribute layer."""
    if hasattr(mesh, "color_attributes") and mesh.color_attributes:
        layer = mesh.color_attributes.active_color
        if layer is None:
            layer = getattr(mesh.color_attributes, "render_color", None)
        if layer is None and len(mesh.color_attributes) > 0:
            layer = mesh.color_attributes[0]
        if layer is not None:
            return layer
    if hasattr(mesh, "attributes") and mesh.attributes:
        for attr in mesh.attributes:
            if getattr(attr, "data_type", "") in ("FLOAT_COLOR", "BYTE_COLOR"):
                return attr
    if hasattr(mesh, "vertex_colors") and mesh.vertex_colors:
        return mesh.vertex_colors.active or (mesh.vertex_colors[0] if len(mesh.vertex_colors) > 0 else None)
    return None


def get_loop_color(color_layer, loop):
    """Return an RGBA float tuple for the given loop (default 1.0, 1.0, 1.0, 1.0)."""
    if color_layer is None:
        return (1.0, 1.0, 1.0, 1.0)
    try:
        domain = getattr(color_layer, "domain", "CORNER")
        if domain == "POINT":
            c = color_layer.data[loop.vertex_index].color
        else:
            c = color_layer.data[loop.index].color

        r = float(c[0])
        g = float(c[1])
        b = float(c[2])
        a = float(c[3]) if len(c) > 3 else 1.0
        return (r, g, b, a)
    except Exception:
        return (1.0, 1.0, 1.0, 1.0)


def write_meshes(file_obj):
    for obj in meshes:
        mesh = obj.data
        mesh.calc_loop_triangles()
        uv_layers = list(mesh.uv_layers)
        triangles = list(mesh.loop_triangles)
        arm_idx = mesh_armatures[obj.as_pointer()]
        color_layer = get_mesh_color_layer(mesh)

        write_string(file_obj, obj.name)
        file_obj.write(struct.pack(
            "<iIIII",
            arm_idx,
            len(obj.material_slots),
            len(mesh.loops),
            len(triangles),
            len(uv_layers),
        ))

        for slot in obj.material_slots:
            material = slot.material
            idx = material_indices.get(material.as_pointer(), -1) if material else -1
            file_obj.write(struct.pack("<i", idx))

        for layer in uv_layers:
            write_string(file_obj, layer.name)

        if arm_idx >= 0:
            arm_obj = armatures[arm_idx]
            matrix = arm_obj.matrix_world.inverted_safe() @ obj.matrix_world
        else:
            matrix = obj.matrix_world.copy()

        # Bake matrix into vertices so objects are positioned and rotated as seen in Blender
        rot_mat = matrix.to_3x3()
        write_matrix(file_obj, mathutils.Matrix.Identity(4))

        bone_indices = {}
        if arm_idx >= 0:
            bone_indices = {
                bone.name: index
                for index, bone in enumerate(armatures[arm_idx].data.bones)
            }
        for loop in mesh.loops:
            vertex = mesh.vertices[loop.vertex_index]
            co = matrix @ vertex.co
            try:
                normal = (rot_mat @ loop.normal).normalized()
            except Exception:
                normal = loop.normal
            color = get_loop_color(color_layer, loop)
            file_obj.write(struct.pack(
                "<3f3f4f",
                co.x, co.y, co.z,
                normal.x, normal.y, normal.z,
                *color,
            ))

            for layer in uv_layers:
                uv = layer.data[loop.index].uv
                file_obj.write(struct.pack("<2f", float(uv.x), float(uv.y)))

            weights = []
            if arm_idx >= 0:
                for assignment in vertex.groups:
                    group = obj.vertex_groups[assignment.group]
                    bone_index = bone_indices.get(group.name)
                    if bone_index is not None and assignment.weight > 0.0:
                        weights.append((bone_index, float(assignment.weight)))

            if len(weights) > 65535:
                raise ValueError(f"Demasiadas influencias de hueso en {obj.name}.")
            file_obj.write(struct.pack("<H", len(weights)))
            for bone_index, weight in weights:
                if bone_index > 65535:
                    raise ValueError(f"Índice de hueso demasiado grande en {obj.name}.")
                file_obj.write(struct.pack("<Hf", bone_index, weight))

        for triangle in triangles:
            file_obj.write(struct.pack(
                "<4I",
                triangle.loops[0],
                triangle.loops[1],
                triangle.loops[2],
                triangle.material_index,
            ))


def write_armatures(file_obj):
    for armature_obj in armatures:
        bones = list(armature_obj.data.bones)
        bone_indices = {bone.name: index for index, bone in enumerate(bones)}

        write_string(file_obj, armature_obj.name)
        file_obj.write(struct.pack("<I", len(bones)))

        for bone in bones:
            write_string(file_obj, bone.name)
            parent_index = (
                bone_indices.get(bone.parent.name, -1)
                if bone.parent is not None else -1
            )
            file_obj.write(struct.pack("<i", parent_index))
            write_matrix(file_obj, bone.matrix_local)


def install_action(owner, action, slot):
    """Temporarily install one Action/Slot and return restoration state."""
    old_anim_data = getattr(owner, "animation_data", None)
    state = {
        "had_anim_data": old_anim_data is not None,
        "action": old_anim_data.action if old_anim_data else None,
        "slot": getattr(old_anim_data, "action_slot", None) if old_anim_data else None,
        "use_nla": getattr(old_anim_data, "use_nla", None) if old_anim_data else None,
        "action_influence": getattr(old_anim_data, "action_influence", None) if old_anim_data else None,
        "action_blend_type": getattr(old_anim_data, "action_blend_type", None) if old_anim_data else None,
    }

    anim_data = old_anim_data or owner.animation_data_create()
    try:
        if hasattr(anim_data, "use_nla"):
            anim_data.use_nla = False
        if hasattr(anim_data, "action_influence"):
            anim_data.action_influence = 1.0
        if hasattr(anim_data, "action_blend_type"):
            anim_data.action_blend_type = "REPLACE"
        anim_data.action = action

        if slot is not None and hasattr(anim_data, "action_slot"):
            compatible_slots = list(getattr(anim_data, "action_suitable_slots", []))
            selected = next(
                (candidate for candidate in compatible_slots
                 if getattr(candidate, "handle", None) == getattr(slot, "handle", None)),
                None,
            )
            if selected is None:
                raise RuntimeError(
                    f"El slot '{getattr(slot, 'identifier', '')}' no es compatible "
                    f"con el destino '{owner.name}'."
                )
            anim_data.action_slot = selected
    except Exception:
        restore_action(owner, state)
        raise

    return state


def restore_action(owner, state):
    if not state["had_anim_data"]:
        owner.animation_data_clear()
        return

    anim_data = owner.animation_data
    if anim_data is None:
        anim_data = owner.animation_data_create()

    anim_data.action = state["action"]
    if hasattr(anim_data, "action_slot"):
        try:
            anim_data.action_slot = state["slot"] if state["action"] else None
        except Exception:
            pass
    if state["use_nla"] is not None and hasattr(anim_data, "use_nla"):
        anim_data.use_nla = state["use_nla"]
    if state["action_influence"] is not None and hasattr(anim_data, "action_influence"):
        anim_data.action_influence = state["action_influence"]
    if state["action_blend_type"] is not None and hasattr(anim_data, "action_blend_type"):
        anim_data.action_blend_type = state["action_blend_type"]


def write_clips(file_obj):
    for clip in clips:
        owner = clip["owner"]
        action = clip["action"]
        slot = clip["slot"]
        first = clip["start"]
        last = clip["end"]
        sample_frames = list(range(first, last + 1))

        write_string(file_obj, clip["name"])
        file_obj.write(struct.pack("<B", clip["code"]))  
        write_string(file_obj, owner.name)
        write_string(file_obj, action.name)
        write_string(file_obj, getattr(slot, "identifier", "Legacy"))
        file_obj.write(struct.pack("<iiI", first, last, len(sample_frames)))

        state = None
        try:
            state = install_action(owner, action, slot)

            if clip["kind"] == "SKELETAL":
                bones = list(owner.data.bones)
                file_obj.write(struct.pack("<I", len(bones)))

                for frame in sample_frames:
                    set_frame(frame)
                    file_obj.write(struct.pack(
                        "<i16f", frame, *matrix_values(owner.matrix_world)
                    ))
                    for bone in bones:
                        pose_bone = owner.pose.bones.get(bone.name)
                        matrix = pose_bone.matrix if pose_bone else bone.matrix_local
                        write_matrix(file_obj, matrix)

            else:
                mapping_nodes = clip["mapping_nodes"]
                mapping_curves = [mapping_component_curves(node, clip["fcurves"])
                                  for node in mapping_nodes]
                file_obj.write(struct.pack("<I", len(mapping_nodes)))
                for node in mapping_nodes:
                    write_string(file_obj, node.name)

                for frame in sample_frames:
                    set_frame(frame)
                    file_obj.write(struct.pack("<i", frame))
                    for node_index, node in enumerate(mapping_nodes):
                        location = get_vector(node, "Location", (0, 0, 0))
                        rotation = get_vector(node, "Rotation", (0, 0, 0))
                        scale = get_vector(node, "Scale", (1, 1, 1))
                        file_obj.write(struct.pack(
                            "<9f", *location, *rotation, *scale
                        ))
                        modes = [interpolation_mode_at(curve, frame)
                                 for curve in mapping_curves[node_index]]
                        file_obj.write(struct.pack("<9B", *modes))

        finally:
            if state is not None:
                restore_action(owner, state)
            scene.frame_set(original_frame, subframe=original_subframe)
temp_output = OUTPUT.with_suffix(OUTPUT.suffix + ".tmp")
try:
    with open(temp_output, "wb") as file_obj:
        file_obj.write(struct.pack(
            "<4sHHIIIIf",
            b"FC3D",
            4,
            0,
            len(meshes),
            len(materials),
            len(armatures),
            len(clips),
            float(fps),
        ))
        write_materials(file_obj)
        write_meshes(file_obj)
        write_armatures(file_obj)
        write_clips(file_obj)

    temp_output.replace(OUTPUT)
except Exception:
    try:
        temp_output.unlink(missing_ok=True)
    except OSError:
        pass
    raise
finally:
    scene.frame_set(original_frame, subframe=original_subframe)

skeletal_count = sum(1 for clip in clips if clip["kind"] == "SKELETAL")
uv_count = sum(1 for clip in clips if clip["kind"] == "UV")
print(f"Exportación FC3D v4 completada: {OUTPUT}")
print(f"Mallas: {len(meshes)} | Materiales: {len(materials)} | Esqueletos: {len(armatures)}")
print(f"Clips esqueléticos: {skeletal_count} | Clips UV: {uv_count}")
print("No se exportaron bytes, rutas ni archivos de texturas.")
