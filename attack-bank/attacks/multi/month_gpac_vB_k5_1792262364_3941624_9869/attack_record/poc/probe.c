#include <gpac/mpeg4_odf.h>

#include <stdio.h>

int main(void)
{
	static u8 preselection[] = {
		0x00, 0x00, 0x00, 0x16, 'p', 'r', 's', 'l',
		0x00, 0x00, 0x10, 0x00,
		0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x00,
		'A', 0x00
	};
	GF_List *configs = gf_list_new();
	GF_Err err;

	if (!configs)
		return 2;

	err = gf_odf_preselection_cfg_parse(preselection, sizeof(preselection), configs);
	if (err != GF_OK) {
		gf_odf_preselection_cfg_del(configs);
		return 3;
	}

	GF_PreselectionConfig *config = (GF_PreselectionConfig *) gf_list_get(configs, 0);
	if (config && config->preselection_tag)
		printf("%s\n", config->preselection_tag);

	gf_odf_preselection_cfg_del(configs);
	return 0;
}
