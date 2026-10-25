#include <gpac/filters.h>
#include <gpac/constants.h>
#include <string.h>

typedef struct
{
	GF_FilterPid *opid;
	Bool sent;
} PocCtx;

static GF_Err poc_initialize(GF_Filter *filter)
{
	PocCtx *ctx = gf_filter_get_udta(filter);
	/*6-byte TrueHD DSI: fmt (4 bytes) + peak data rate (15 bits) + reserved bit*/
	u8 dsi[6] = {0x0B, 0xB8, 0x0F, 0x3F, 0x2E, 0xE0};

	ctx->opid = gf_filter_pid_new(filter);
	if (!ctx->opid) return GF_OUT_OF_MEM;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_TRUEHD));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(48000));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(2));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(48000));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA((char *) dsi, sizeof(dsi)));
	return GF_OK;
}

static GF_Err poc_process(GF_Filter *filter)
{
	PocCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	u8 *data;

	if (ctx->sent) return GF_OK;
	ctx->sent = GF_TRUE;

	pck = gf_filter_pck_new_alloc(ctx->opid, 16, &data);
	if (!pck) return GF_OUT_OF_MEM;
	memset(data, 0, 16);
	gf_filter_pck_set_framing(pck, GF_TRUE, GF_TRUE);
	gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
	gf_filter_pck_set_cts(pck, 0);
	gf_filter_pck_send(pck);
	gf_filter_pid_set_eos(ctx->opid);
	return GF_OK;
}

static const GF_FilterCapability PocCaps[] =
{
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_TRUEHD),
};

static const GF_FilterRegister PocRegister = {
	.name = "poc_thd_source",
	GF_FS_SET_DESCRIPTION("PoC TrueHD source")
	.private_size = sizeof(PocCtx),
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	SETCAPS(PocCaps),
	.initialize = poc_initialize,
	.process = poc_process,
};

int main(int argc, char **argv)
{
	GF_FilterSession *fs;
	GF_Err e = GF_OK;
	const char *dst = (argc > 1) ? argv[1] : "poc_out.mp4";

	fs = gf_fs_new_defaults(0);
	if (!fs) return 1;
	gf_fs_add_filter_register(fs, &PocRegister);

	if (!gf_fs_load_destination(fs, dst, NULL, NULL, &e) ) {
		fprintf(stderr, "cannot load destination %s\n", dst);
		gf_fs_del(fs);
		return 1;
	}
	if (!gf_fs_load_filter(fs, "poc_thd_source", &e) ) {
		fprintf(stderr, "cannot load poc_thd_source\n");
		gf_fs_del(fs);
		return 1;
	}
	e = gf_fs_run(fs);
	gf_fs_del(fs);
	return (e == GF_OK) || (e == GF_EOS) ? 0 : 1;
}
