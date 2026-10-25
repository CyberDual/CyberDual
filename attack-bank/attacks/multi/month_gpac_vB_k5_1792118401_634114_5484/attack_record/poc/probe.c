#include <stdio.h>
#include <gpac/tools.h>
#include <gpac/list.h>
#include <gpac/mpeg4_odf.h>
#include <gpac/isomedia.h>

static void try_preselection(void)
{
	u8 data[] = {
		0x00, 0x00, 0x00, 0x19,
		'p', 'r', 's', 'l',
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x02,
		0x11, 0x22, 0x33, 0x44, 0x55
	};
	GF_List *cfg = gf_list_new();
	if (!cfg)
		return;
	gf_odf_preselection_cfg_parse(data, (u32) sizeof(data), cfg);
	gf_odf_preselection_cfg_del(cfg);
}

static void try_group(void)
{
	static const u8 file[] = {
		0x00, 0x00, 0x00, 0x2d, 'm', 'e', 't', 'a',
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x21, 'g', 'r', 'p', 'l',
		0x00, 0x00, 0x00, 0x19, 'a', 'l', 't', 'r',
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x02,
		0x11, 0x22, 0x33, 0x44, 0x55
	};
	const char *path = "/tmp/gpac-group.mp4";
	FILE *out;
	GF_ISOFile *mov;

	out = fopen(path, "wb");
	if (!out)
		return;
	if (fwrite(file, 1, sizeof(file), out) != sizeof(file)) {
		fclose(out);
		return;
	}
	fclose(out);
	mov = gf_isom_open(path, GF_ISOM_OPEN_READ, NULL);
	if (mov)
		gf_isom_delete(mov);
}

int main(void)
{
	gf_sys_init(GF_MemTrackerNone, NULL);
	try_preselection();
	try_group();
	gf_sys_close();
	return 0;
}
