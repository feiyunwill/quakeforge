# vim:ts=4:et
# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy, bgl
from bpy.props import BoolProperty, FloatProperty, StringProperty, EnumProperty
from bpy.props import BoolVectorProperty, CollectionProperty, PointerProperty
from bpy.props import FloatVectorProperty, IntProperty
from mathutils import Vector

from .entityclass import EntityClass

def draw_callback(self, context):
    def obj_location(obj):
        ec = None
        if obj.qfentity.classname in entity_classes:
            ec = entity_classes[obj.qfentity.classname]
        if not ec or ec.size:
            return obj.location
        loc = Vector()
        for i in range(8):
            loc += Vector(obj.bound_box[i])
        return obj.location + loc/8.0
    qfmap = context.scene.qfmap
    entity_classes = qfmap.entity_classes
    entity_targets = qfmap.entity_targets
    target_entities = qfmap.target_entities
    bgl.glLineWidth(3)
    ents = 0
    targs = 0
    for obj in target_entities:
        #obj = bpy.data.objects[objname]
        qfentity = obj.qfentity
        if qfentity.classname not in entity_classes:
            continue
        ents += 1
        ec = entity_classes[qfentity.classname]
        target = None
        killtarget = None
        for field in qfentity.fields:
            if field.name == "target" and field.value:
                target = field.value
            if field.name == "killtarget" and field.value:
                killtarget = field.value
        targetlist = [target, killtarget]
        if target == killtarget:
            del targetlist[1]
        for tname in targetlist:
            if tname and tname in entity_targets:
                targets = entity_targets[tname]
                bgl.glColor4f(ec.color[0], ec.color[1], ec.color[2], 1)
                for ton in targets:
                    targs += 1
                    to = bpy.data.objects[ton]
                    bgl.glBegin(bgl.GL_LINE_STRIP)
                    loc = obj_location(obj)
                    bgl.glVertex3f(loc.x, loc.y, loc.z)
                    loc = obj_location(to)
                    bgl.glVertex3f(loc.x, loc.y, loc.z)
                    bgl.glEnd()
    bgl.glLineWidth(1)

class VIEW3D_PT_QFEntityRelations(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Register callback"
    bl_default_closed = True

    initDone = False

    @classmethod
    def poll(cls, context):
        if cls.initDone:
            return False
        if context.area.type == 'VIEW_3D':
            sv3d = bpy.types.SpaceView3D
            sv3d.draw_handler_add (draw_callback, (cls, context), 'WINDOW',
                                   'POST_VIEW')
            cls.initDone = True
        return False
    def draw_header(self, context):
        pass
    def draw(self, context):
        pass

class OBJECT_UL_EntityField_list(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data,
                  active_propname, index):
        layout.label(item.name)
        layout.label(item.value)

def qfentity_items(self, context):
    qfmap = context.scene.qfmap
    entclasses = qfmap.entity_classes
    eclist = list(entclasses.keys())
    eclist.sort()
    enum = (('', "--", ""),)
    enum += tuple(map(lambda ec: (ec, ec, ""), eclist))
    return enum

class QFEntityProp(bpy.types.PropertyGroup):
    value = StringProperty(name="")
    template_list_controls = StringProperty(default="value", options={'HIDDEN'})

class QFEntity(bpy.types.PropertyGroup):
    classname = EnumProperty(items = qfentity_items, name = "Entity Class")
    flags = BoolVectorProperty(size=12)
    fields = CollectionProperty(type=QFEntityProp, name="Fields")
    field_idx = IntProperty()

class QFEntpropAdd(bpy.types.Operator):
    '''Add an entity field/value pair'''
    bl_idname = "object.entprop_add"
    bl_label = "Entprop Add"
    def execute(self, context):
        qfentity = context.active_object.qfentity
        item = qfentity.fields.add()
        item.name = ""
        item.value = ""
        return {'FINISHED'}

class QFEntpropRemove(bpy.types.Operator):
    '''Remove an entity field/value pair'''
    bl_idname = "object.entprop_remove"
    bl_label = "Entprop Remove"
    def execute(self, context):
        qfentity = context.active_object.qfentity
        if qfentity.field_idx >= 0:
            qfentity.fields.remove(qfentity.field_idx)
        return {'FINISHED'}

def reflow_text(text, max_width):
    lines = []
    for text_line in text.split("\n"):
        if not text_line:
            continue
        words = text_line.split(" ")
        flowed_line = ""
        while words:
            if len(flowed_line) + len(words[0]) > max_width:
                lines.append(flowed_line)
                flowed_line = ""
            flowed_line += (" " if flowed_line else "") + words[0]
            del words[0]
        if flowed_line:
            lines.append(flowed_line)
    return lines

class OBJECT_PT_EntityPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = 'object'
    bl_label = 'QF Entity'

    @classmethod
    def poll(cls, context):
        return True

    def draw(self, context):
        layout = self.layout
        qfentity = context.active_object.qfentity
        qfmap = context.scene.qfmap
        if qfentity.classname:
            ec = qfmap.entity_classes[qfentity.classname]
        else:
            ec = EntityClass.null()
        flags = ec.flagnames + ("",) * (8 - len(ec.flagnames))
        flags += ("!easy", "!medium", "!hard", "!dm")
        row = layout.row()
        row.prop(qfentity, "classname")
        box=layout.box()
        lines = reflow_text(ec.comment, 40)
        for l in lines:
            box.label(l)
        row = layout.row()
        for c in range(3):
            col = row.column()
            sub = col.column(align=True)
            for r in range(4):
                idx = c * 4 + r
                sub.prop(qfentity, "flags", text=flags[idx], index=idx)
        row = layout.row()
        col = row.column()
        col.template_list("OBJECT_UL_EntityField_list", "", qfentity, "fields",
                          qfentity, "field_idx", rows=3)
        col = row.column(align=True)
        col.operator("object.entprop_add", icon='ZOOMIN', text="")
        col.operator("object.entprop_remove", icon='ZOOMOUT', text="")
        if len(qfentity.fields) > qfentity.field_idx >= 0:
            row = layout.row()
            field = qfentity.fields[qfentity.field_idx]
            row.prop(field, "name", text="Field Name")
            row.prop(field, "value", text="Value")

def default_brush_entity(entityclass):
    name = entityclass.name
    verts = [(-8, -8, -8),
             ( 8,  8, -8),
             (-8,  8,  8),
             ( 8, -8,  8)]
    faces = [(0, 2, 1),
             (0, 3, 2),
             (0, 1, 3),
             (1, 2, 3)]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    return mesh

def entity_box(entityclass):
    name = entityclass.name
    size = entityclass.size
    color = entityclass.color
    if name in bpy.data.meshes:
        return bpy.data.meshes[name]
    verts = [(size[0][0], size[0][1], size[0][2]),
             (size[0][0], size[0][1], size[1][2]),
             (size[0][0], size[1][1], size[0][2]),
             (size[0][0], size[1][1], size[1][2]),
             (size[1][0], size[0][1], size[0][2]),
             (size[1][0], size[0][1], size[1][2]),
             (size[1][0], size[1][1], size[0][2]),
             (size[1][0], size[1][1], size[1][2])]
    faces = [(0, 1, 3, 2),
             (0, 2, 6, 4),
             (0, 4, 5, 1),
             (4, 6, 7, 5),
             (6, 2, 3, 7),
             (1, 5, 7, 3)]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = color
    mat.use_raytrace = False
    mesh.materials.append(mat)
    return mesh

def set_entity_props(obj, ent):
    qfe = obj.qfentity
    if "classname" in ent.d:
        try:
            qfe.classname = ent.d["classname"]
        except TypeError:
            #FIXME hmm, maybe an enum wasn't the most brilliant idea?
            qfe.classname
    if "spawnflags" in ent.d:
        flags = int(float(ent.d["spawnflags"]))
        for i in range(12):
            qfe.flags[i] = (flags & (1 << i)) and True or False
    if "target" in ent.d or "killtarget" in ent.d:
        bpy.context.scene.qfmap.target_entities.append(obj)
    if "targetname" in ent.d:
        targetname = ent.d["targetname"]
        entity_targets = bpy.context.scene.qfmap.entity_targets
        if targetname not in entity_targets:
            entity_targets[targetname] = []
        entity_targets[targetname].append(obj.name)
    for key in ent.d.keys():
        if key in {"classname", "spawnflags", "origin"}:
            continue
        item = qfe.fields.add()
        item.name = key
        item.value = ent.d[key]

def add_entity(self, context, entclass):
    entity_class = context.scene.qfmap.entity_classes[entclass]
    context.user_preferences.edit.use_global_undo = False
    for obj in bpy.data.objects:
        obj.select = False
    if entity_class.size:
        mesh = entity_box(entity_class)
    else:
        mesh = default_brush_entity(entity_class)
    obj = bpy.data.objects.new(entity_class.name, mesh)
    obj.location = context.scene.cursor_location
    obj.select = True
    obj.qfentity.classname = entclass
    context.scene.objects.link(obj)
    bpy.context.scene.objects.active=obj
    context.user_preferences.edit.use_global_undo = True
    return {'FINISHED'}

def register():
    bpy.types.Object.qfentity = PointerProperty(type=QFEntity)
