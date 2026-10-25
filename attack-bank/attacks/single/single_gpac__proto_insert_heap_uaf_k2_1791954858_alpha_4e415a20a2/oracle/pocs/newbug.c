#include <gpac/tools.h>
#include <gpac/scenegraph.h>
#include <gpac/scenegraph_vrml.h>

int main(void)
{
	GF_SceneGraph *sg;
	GF_Proto *oldp, *neu;
	GF_Command *ins;
	MFURL *url;
	SFURL *slot;

	gf_sys_init(GF_MemTrackerNone, NULL);
	sg = gf_sg_new();
	oldp = gf_sg_proto_new(sg, 7, "Old", GF_FALSE);
	url = gf_sg_proto_get_extern_url(oldp);
	slot = NULL;
	gf_sg_vrml_mf_append(url, GF_SG_VRML_MFURL, (void **)&slot);
	slot->url = gf_strdup("extern.bt");

	neu = gf_sg_proto_new(sg, 7, "New", GF_TRUE);
	ins = gf_sg_command_new(sg, GF_SG_PROTO_INSERT);
	gf_list_add(ins->new_proto_list, neu);
	gf_sg_command_apply(sg, ins, 0);
	gf_sg_command_del(ins);
	gf_sg_del(sg);
	gf_sys_close();
	return 0;
}
