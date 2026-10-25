#include <gpac/scene_manager.h>
#include <gpac/tools.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
	static const char scene[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<XMT-A xmlns=\"urn:mpeg:mpeg4:xmta:schema:2002\">\n"
		" <Header>\n"
		"  <ObjectDescriptor objectDescriptorID=\"x\" binaryID=\"1000\"/>\n"
		" </Header>\n"
		" <Body>\n"
		"  <Replace>\n"
		"   <Scene>\n"
		"    <OrderedGroup>\n"
		"     <children>\n"
		"      <Inline url=\"od:x#scene\"/>\n"
		"     </children>\n"
		"    </OrderedGroup>\n"
		"   </Scene>\n"
		"  </Replace>\n"
		" </Body>\n"
		"</XMT-A>\n";
	char path[] = "/tmp/gpac-xmt-XXXXXX";
	GF_SceneLoader load;
	GF_SceneGraph *graph;
	GF_SceneManager *ctx;
	GF_Err e;
	FILE *file;
	int fd = mkstemp(path);
	if (fd < 0) return 2;
	file = fdopen(fd, "w");
	if (!file) return 2;
	if (fwrite(scene, 1, sizeof(scene) - 1, file) != sizeof(scene) - 1) return 2;
	if (fclose(file)) return 2;
	if (gf_sys_init(GF_MemTrackerNone, NULL)) return 2;
	graph = gf_sg_new();
	ctx = gf_sm_new(graph);
	memset(&load, 0, sizeof(load));
	load.type = GF_SM_LOAD_XMTA;
	load.fileName = path;
	load.ctx = ctx;
	e = gf_sm_load_init(&load);
	if (!e) e = gf_sm_load_run(&load);
	gf_sm_load_done(&load);
	gf_sm_del(ctx);
	gf_sg_del(graph);
	gf_sys_close();
	unlink(path);
	return e ? 1 : 0;
}
