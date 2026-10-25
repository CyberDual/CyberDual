#include <gpac/tools.h>
#include <gpac/list.h>
#include <gpac/mpeg4_odf.h>
#include <stdio.h>
#include <string.h>

static void put_u32(unsigned char *p, unsigned int v)
{
	p[0] = (unsigned char) (v >> 24);
	p[1] = (unsigned char) (v >> 16);
	p[2] = (unsigned char) (v >> 8);
	p[3] = (unsigned char) v;
}

int main(void)
{
	/* prsl container with one labl child. Language is 300 'A' bytes plus NUL,
	   which is a legal null-terminated label language; the label text is "x". */
	enum { LANG = 300 };
	enum { LABL_PAYLOAD = 4 + 2 + (LANG + 1) + 2 };
	enum { LABL_SIZE = 8 + LABL_PAYLOAD };
	enum { PRSL_SIZE = 20 + LABL_SIZE };
	unsigned char buf[PRSL_SIZE];
	unsigned int o = 0;
	GF_List *cfg;
	GF_Err e;

	memset(buf, 0, sizeof(buf));
	put_u32(buf + o, PRSL_SIZE);
	o += 4;
	buf[o++] = 'p'; buf[o++] = 'r'; buf[o++] = 's'; buf[o++] = 'l';
	o += 4; /* version + flags */
	put_u32(buf + o, 1); /* group_id */
	o += 4;
	put_u32(buf + o, 0); /* entity_id_count */
	o += 4;
	put_u32(buf + o, LABL_SIZE);
	o += 4;
	buf[o++] = 'l'; buf[o++] = 'a'; buf[o++] = 'b'; buf[o++] = 'l';
	o += 4; /* version + flags */
	buf[o++] = 0;
	buf[o++] = 1; /* label_id */
	memset(buf + o, 'A', LANG);
	o += LANG;
	buf[o++] = 0;
	buf[o++] = 'x';
	buf[o++] = 0;

	gf_sys_init(GF_MemTrackerNone, NULL);
	cfg = gf_list_new();
	e = gf_odf_preselection_cfg_parse(buf, (u32) sizeof(buf), cfg);
	if (e == GF_OK)
		gf_odf_preselection_cfg_del(cfg);
	else
		gf_list_del(cfg);
	gf_sys_close();
	return (e == GF_OK) ? 0 : 1;
}
