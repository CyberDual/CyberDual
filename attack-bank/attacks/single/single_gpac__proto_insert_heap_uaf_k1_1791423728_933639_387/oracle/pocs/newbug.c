#include <gpac/scenegraph.h>
#include <gpac/scenegraph_vrml.h>
#include <gpac/list.h>

int main(void)
{
	GF_SceneGraph *graph = gf_sg_new();
	GF_Command *command = gf_sg_command_new(graph, GF_SG_PROTO_INSERT);
	GF_Proto *first = gf_sg_proto_new(graph, 1, "First", GF_TRUE);
	GF_Proto *second = gf_sg_proto_new(graph, 2, "Second", GF_TRUE);
	if (!graph || !command || !first || !second) return 1;
	gf_list_add(command->new_proto_list, second);
	gf_list_add(command->new_proto_list, first);
	gf_sg_command_apply(graph, command, 0);
	gf_sg_command_del(command);
	gf_sg_del(graph);
	return 0;
}
