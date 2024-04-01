#include "debug_geometry_container.h"

#ifndef DISABLE_DEBUG_RENDERING
#include "config_3d.h"
#include "config_scope_3d.h"
#include "debug_draw_3d.h"
#include "geometry_generators.h"
#include "stats_3d.h"

#include <array>

GODOT_WARNING_DISABLE()
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/world3d.hpp>
GODOT_WARNING_RESTORE()
using namespace godot;

DebugGeometryContainer::DebugGeometryContainer(class DebugDraw3D *p_root) {
	ZoneScoped;
	owner = p_root;
	RenderingServer *rs = RenderingServer::get_singleton();

	// Create wireframe mesh drawer
	{
		Ref<ArrayMesh> _array_mesh;
		_array_mesh.instantiate();
		RID _immediate_instance = rs->instance_create();

		rs->instance_set_base(_immediate_instance, _array_mesh->get_rid());
		rs->instance_geometry_set_cast_shadows_setting(_immediate_instance, RenderingServer::SHADOW_CASTING_SETTING_OFF);
		rs->instance_geometry_set_flag(_immediate_instance, RenderingServer::INSTANCE_FLAG_USE_DYNAMIC_GI, false);
		rs->instance_geometry_set_flag(_immediate_instance, RenderingServer::INSTANCE_FLAG_USE_BAKED_LIGHT, false);

		Ref<ShaderMaterial> mat = owner->get_wireframe_material();
		rs->instance_geometry_set_material_override(_immediate_instance, mat->get_rid());

		immediate_mesh_storage.instance = _immediate_instance;
		immediate_mesh_storage.material = mat;
		immediate_mesh_storage.mesh = _array_mesh;
	}

	// Generate geometry and create MMI's in RenderingServer
	{
		auto *meshes = owner->get_shared_meshes();

		CreateMMI(InstanceType::CUBE, UsingShaderType::Wireframe, NAMEOF(mmi_cubes), meshes[(int)InstanceType::CUBE]);
		CreateMMI(InstanceType::CUBE_CENTERED, UsingShaderType::Wireframe, NAMEOF(mmi_cubes_centered), meshes[(int)InstanceType::CUBE_CENTERED]);
		CreateMMI(InstanceType::ARROWHEAD, UsingShaderType::Wireframe, NAMEOF(mmi_arrowheads), meshes[(int)InstanceType::ARROWHEAD]);
		CreateMMI(InstanceType::POSITION, UsingShaderType::Wireframe, NAMEOF(mmi_positions), meshes[(int)InstanceType::POSITION]);
		CreateMMI(InstanceType::SPHERE, UsingShaderType::Wireframe, NAMEOF(mmi_spheres), meshes[(int)InstanceType::SPHERE]);
		CreateMMI(InstanceType::SPHERE_HD, UsingShaderType::Wireframe, NAMEOF(mmi_spheres_hd), meshes[(int)InstanceType::SPHERE_HD]);
		CreateMMI(InstanceType::CYLINDER, UsingShaderType::Wireframe, NAMEOF(mmi_cylinders), meshes[(int)InstanceType::CYLINDER]);
		CreateMMI(InstanceType::CYLINDER_AB, UsingShaderType::Wireframe, NAMEOF(mmi_cylinders), meshes[(int)InstanceType::CYLINDER_AB]);

		// VOLUMETRIC

		CreateMMI(InstanceType::LINE_VOLUMETRIC, UsingShaderType::Expandable, NAMEOF(mmi_cubes_volumetric), meshes[(int)InstanceType::LINE_VOLUMETRIC]);
		CreateMMI(InstanceType::CUBE_VOLUMETRIC, UsingShaderType::Expandable, NAMEOF(mmi_cubes_volumetric), meshes[(int)InstanceType::CUBE_VOLUMETRIC]);
		CreateMMI(InstanceType::CUBE_CENTERED_VOLUMETRIC, UsingShaderType::Expandable, NAMEOF(mmi_cubes_centered_volumetric), meshes[(int)InstanceType::CUBE_CENTERED_VOLUMETRIC]);
		CreateMMI(InstanceType::ARROWHEAD_VOLUMETRIC, UsingShaderType::Expandable, NAMEOF(mmi_arrowheads_volumetric), meshes[(int)InstanceType::ARROWHEAD_VOLUMETRIC]);
		CreateMMI(InstanceType::POSITION_VOLUMETRIC, UsingShaderType::Expandable, NAMEOF(mmi_positions_volumetric), meshes[(int)InstanceType::POSITION_VOLUMETRIC]);
		CreateMMI(InstanceType::SPHERE_VOLUMETRIC, UsingShaderType::Expandable, NAMEOF(mmi_spheres_volumetric), meshes[(int)InstanceType::SPHERE_VOLUMETRIC]);
		CreateMMI(InstanceType::SPHERE_HD_VOLUMETRIC, UsingShaderType::Expandable, NAMEOF(mmi_spheres_hd_volumetric), meshes[(int)InstanceType::SPHERE_HD_VOLUMETRIC]);
		CreateMMI(InstanceType::CYLINDER_VOLUMETRIC, UsingShaderType::Expandable, NAMEOF(mmi_cylinders_volumetric), meshes[(int)InstanceType::CYLINDER_VOLUMETRIC]);
		CreateMMI(InstanceType::CYLINDER_AB_VOLUMETRIC, UsingShaderType::Expandable, NAMEOF(mmi_cylinders_volumetric), meshes[(int)InstanceType::CYLINDER_AB_VOLUMETRIC]);

		// SOLID

		CreateMMI(InstanceType::BILLBOARD_SQUARE, UsingShaderType::Billboard, NAMEOF(mmi_billboard_squares), meshes[(int)InstanceType::BILLBOARD_SQUARE]);
		CreateMMI(InstanceType::PLANE, UsingShaderType::Solid, NAMEOF(mmi_planes), meshes[(int)InstanceType::PLANE]);

		set_render_layer_mask(1);
	}
}

DebugGeometryContainer::~DebugGeometryContainer() {
	ZoneScoped;
	LOCK_GUARD(owner->datalock);

	geometry_pool.clear_pool();
}

void DebugGeometryContainer::CreateMMI(InstanceType p_type, UsingShaderType p_shader, const String &p_name, Ref<ArrayMesh> p_mesh) {
	ZoneScoped;
	RenderingServer *rs = RenderingServer::get_singleton();

	RID mmi = rs->instance_create();

	Ref<MultiMesh> new_mm;
	new_mm.instantiate();
	new_mm->set_name(String::num_int64((int)p_type));

	new_mm->set_transform_format(MultiMesh::TransformFormat::TRANSFORM_3D);
	new_mm->set_use_colors(true);
	new_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	new_mm->set_use_custom_data(true);
	new_mm->set_mesh(p_mesh);

	rs->instance_set_base(mmi, new_mm->get_rid());

	Ref<ShaderMaterial> mat;
	switch (p_shader) {
		case UsingShaderType::Wireframe:
			mat = owner->get_wireframe_material();
			break;
		case UsingShaderType::Billboard:
			mat = owner->get_billboard_material();
			break;
		case UsingShaderType::Solid:
			mat = owner->get_plane_material();
			break;
		case UsingShaderType::Expandable:
			mat = owner->get_extendable_material();
			break;
	}

	p_mesh->surface_set_material(0, mat);

	rs->instance_geometry_set_cast_shadows_setting(mmi, RenderingServer::SHADOW_CASTING_SETTING_OFF);
	rs->instance_geometry_set_flag(mmi, RenderingServer::INSTANCE_FLAG_USE_DYNAMIC_GI, false);
	rs->instance_geometry_set_flag(mmi, RenderingServer::INSTANCE_FLAG_USE_BAKED_LIGHT, false);

	multi_mesh_storage[(int)p_type].instance = mmi;
	multi_mesh_storage[(int)p_type].mesh = new_mm;
}

void DebugGeometryContainer::set_world(Ref<World3D> p_new_world) {
	ZoneScoped;
	if (p_new_world == base_world_viewport) {
		return;
	}

	base_world_viewport = p_new_world;
	RenderingServer *rs = RenderingServer::get_singleton();
	RID scenario = base_world_viewport.is_valid() ? base_world_viewport->get_scenario() : RID();

	for (auto &s : multi_mesh_storage) {
		rs->instance_set_scenario(s.instance, scenario);
	}
	rs->instance_set_scenario(immediate_mesh_storage.instance, scenario);
}

Ref<World3D> DebugGeometryContainer::get_world() {
	return base_world_viewport;
}

void DebugGeometryContainer::update_geometry(double p_delta) {
	ZoneScoped;
	LOCK_GUARD(owner->datalock);

	// cleanup and get available viewports
	std::vector<Viewport *> available_viewports = geometry_pool.get_and_validate_viewports();

	// accumulate a time delta to delete objects in any case after their timers expire.
	geometry_pool.update_expiration_delta(p_delta, ProcessType::PROCESS);

	// Do not update geometry if frozen
	if (owner->get_config()->is_freeze_3d_render())
		return;

	if (immediate_mesh_storage.mesh->get_surface_count()) {
		ZoneScopedN("Clear lines");
		immediate_mesh_storage.mesh->clear_surfaces();
	}

	// Return if nothing to do
	if (!owner->is_debug_enabled()) {
		ZoneScopedN("Reset instances");
		for (auto &item : multi_mesh_storage) {
			if (item.mesh->get_visible_instance_count())
				item.mesh->set_visible_instance_count(0);
		}
		geometry_pool.reset_counter(p_delta);
		geometry_pool.reset_visible_objects();
		return;
	}

	// Update render layers
	if (render_layers != owner->get_config()->get_geometry_render_layers()) {
		set_render_layer_mask(owner->get_config()->get_geometry_render_layers());
	}

	std::unordered_map<Viewport *, std::shared_ptr<GeometryPoolCullingData> > culling_data;
	{
		ZoneScopedN("Get frustums");

		for (const auto &vp_p : available_viewports) {
			std::vector<std::array<Plane, 6> > frustum_planes;
			std::vector<AABBMinMax> frustum_boxes;

			std::vector<std::pair<Array, Camera3D *> > frustum_arrays;
			frustum_arrays.reserve(1);

#ifdef DEBUG_ENABLED
			auto custom_editor_viewports = owner->get_custom_editor_viewports();
			bool is_editor_vp = std::find_if(
										custom_editor_viewports.cbegin(),
										custom_editor_viewports.cend(),
										[&vp_p](const auto &it) { return it == vp_p; }) != custom_editor_viewports.cend();

			if (IS_EDITOR_HINT() && is_editor_vp) {
				Camera3D *cam = nullptr;
				Node *root = SCENE_TREE()->get_edited_scene_root();
				if (root) {
					cam = root->get_viewport()->get_camera_3d();
				}

				if (owner->config->is_force_use_camera_from_scene() && cam) {
					frustum_arrays.push_back({ cam->get_frustum(), cam });
				} else if (custom_editor_viewports.size() > 0) {
					for (const auto &evp : custom_editor_viewports) {
						if (evp->get_update_mode() == SubViewport::UpdateMode::UPDATE_ALWAYS) {
							Camera3D *cam = evp->get_camera_3d();
							if (cam) {
								frustum_arrays.push_back({ cam->get_frustum(), cam });
							}
						}
					}
				}
			} else {
#endif
				Camera3D *vp_cam = vp_p->get_camera_3d();
				if (vp_cam) {
					frustum_arrays.push_back({ vp_cam->get_frustum(), vp_cam });
				}
#ifdef DEBUG_ENABLED
			}
#endif

			// Convert Array to vector
			if (frustum_arrays.size()) {
				for (auto &pair : frustum_arrays) {
					Array &arr = pair.first;
					if (arr.size() == 6) {
						std::array<Plane, 6> a;
						for (int i = 0; i < arr.size(); i++)
							a[i] = (Plane)arr[i];

						MathUtils::scale_frustum_far_plane_distance(a, pair.second->get_global_transform(), owner->get_config()->get_frustum_length_scale());

						if (owner->get_config()->is_use_frustum_culling())
							frustum_planes.push_back(a);

						auto cube = MathUtils::get_frustum_cube(a);
						AABB aabb = MathUtils::calculate_vertex_bounds(cube.data(), cube.size());
						frustum_boxes.push_back(aabb);

#if false
						// Debug camera bounds
						{
							SphereBounds sb = aabb;
							auto cfg = owner->new_scoped_config()->set_thickness(0.1f)->set_hd_sphere(true); //->set_viewport(vp_p);
							owner->draw_sphere(sb.position, sb.radius, Colors::crimson);
							owner->draw_aabb(aabb, Colors::yellow);
						}
#endif
					}
				}
			}

			culling_data[vp_p] = std::make_shared<GeometryPoolCullingData>(frustum_planes, frustum_boxes);
		}
	}

	// Debug bounds of instances and lines
	if (owner->get_config()->is_visible_instance_bounds()) {
		ZoneScopedN("Debug bounds");
		struct sphereDebug {
			Vector3 pos;
			real_t radius;
		};

		Viewport *vp;
		if (available_viewports.size()) {
			vp = *available_viewports.begin();
			auto cfg = std::make_shared<DebugDraw3DScopeConfig::Data>(owner->scoped_config()->data);
			cfg->thickness = 0;

			std::vector<AABBMinMax> new_instances;
			geometry_pool.for_each_instance([&new_instances](DelayedRendererInstance *o) {
				if (!o->is_visible || o->is_expired())
					return;
				new_instances.push_back(o->bounds);
			});

			// Draw custom sphere for 1 frame
			for (auto &i : new_instances) {
				cfg->viewport = vp;
				Vector3 diag = i.max - i.min;
				Vector3 center = i.center;
				real_t radius = i.radius;

				geometry_pool.add_or_update_instance(
						cfg,
						InstanceType::SPHERE,
						0,
						ProcessType::PROCESS,
						Transform3D(Basis().scaled(VEC3_ONE(radius) * 2), center),
						Colors::debug_sphere_bounds,
						SphereBounds(center, radius));

				geometry_pool.add_or_update_instance(
						cfg,
						InstanceType::CUBE_CENTERED,
						0,
						ProcessType::PROCESS,
						Transform3D(Basis().scaled(diag), center),
						Colors::debug_rough_box_bounds,
						SphereBounds(center, radius));
			}

			geometry_pool.for_each_line([this, &cfg, &vp](DelayedRendererLine *o) {
				if (!o->is_visible || o->is_expired())
					return;

				Vector3 diag = o->bounds.max - o->bounds.min;
				Vector3 center = o->bounds.center;
				real_t radius = o->bounds.radius;

				cfg->viewport = vp;
				geometry_pool.add_or_update_instance(
						cfg,
						InstanceType::CUBE_CENTERED,
						0,
						ProcessType::PROCESS,
						Transform3D(Basis().scaled(diag), center),
						Colors::debug_box_bounds,
						SphereBounds(center, radius),
						&Colors::empty_color);

				geometry_pool.add_or_update_instance(
						cfg,
						InstanceType::SPHERE,
						0,
						ProcessType::PROCESS,
						Transform3D(Basis().scaled(VEC3_ONE(radius) * 2), center),
						Colors::debug_sphere_bounds,
						SphereBounds(center, radius));
			});

			for (const auto &culling_data : culling_data) {
				for (const auto &frustum : culling_data.second->m_frustums) {
					size_t s = GeometryGenerator::CubeIndexes.size();
					std::unique_ptr<Vector3[]> l(new Vector3[s]);
					GeometryGenerator::CreateCameraFrustumLinesWireframe(frustum, l.get());

					geometry_pool.add_or_update_line(
							cfg,
							0,
							ProcessType::PROCESS,
							std::move(l),
							s,
							Colors::red);
				}
			}
		}
	}

	std::vector<Ref<MultiMesh> *> meshes((int)InstanceType::MAX);
	for (int i = 0; i < (int)InstanceType::MAX; i++) {
		meshes[i] = &multi_mesh_storage[i].mesh;
	}

	geometry_pool.reset_visible_objects();
	geometry_pool.fill_mesh_data(meshes, immediate_mesh_storage.mesh, culling_data);

	geometry_pool.reset_counter(p_delta, ProcessType::PROCESS);

	is_frame_rendered = true;
}

void DebugGeometryContainer::update_geometry_physics_start(double p_delta) {
	if (is_frame_rendered) {
		geometry_pool.reset_counter(p_delta, ProcessType::PHYSICS_PROCESS);
		is_frame_rendered = false;
	}
}

void DebugGeometryContainer::update_geometry_physics_end(double p_delta) {
	geometry_pool.update_expiration_delta(p_delta, ProcessType::PHYSICS_PROCESS);
}

void DebugGeometryContainer::get_render_stats(Ref<DebugDraw3DStats> &p_stats) {
	ZoneScoped;
	LOCK_GUARD(owner->datalock);
	return geometry_pool.set_stats(p_stats);
}

void DebugGeometryContainer::set_render_layer_mask(int32_t p_layers) {
	ZoneScoped;
	LOCK_GUARD(owner->datalock);
	if (render_layers != p_layers) {
		RenderingServer *rs = RenderingServer::get_singleton();
		for (auto &mmi : multi_mesh_storage)
			rs->instance_set_layer_mask(mmi.instance, p_layers);

		rs->instance_set_layer_mask(immediate_mesh_storage.instance, p_layers);
		render_layers = p_layers;
	}
}

int32_t DebugGeometryContainer::get_render_layer_mask() const {
	return render_layers;
}

void DebugGeometryContainer::clear_3d_objects() {
	ZoneScoped;
	LOCK_GUARD(owner->datalock);
	for (auto &s : multi_mesh_storage) {
		s.mesh->set_instance_count(0);
	}
	immediate_mesh_storage.mesh->clear_surfaces();

	geometry_pool.clear_pool();
}

#endif