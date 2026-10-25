#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gpac/filters.h>
#include <gpac/isomedia.h>

static u8 *g_dsi;
static u32 g_dsi_size;

typedef struct {
	GF_FilterPid *pid;
	Bool sent;
} THDSrc;

static GF_Err thd_src_initialize(GF_Filter *filter)
{
	THDSrc *ctx = gf_filter_get_udta(filter);
	ctx->pid = gf_filter_pid_new(filter);
	if (!ctx->pid) return GF_OUT_OF_MEM;
	gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO));
	gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_TRUEHD));
	gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(48000));
	gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(48000));
	gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(2));
	gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_FALSE));
	gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(g_dsi, g_dsi_size));
	return GF_OK;
}

static GF_Err thd_src_process(GF_Filter *filter)
{
	u8 *data;
	GF_FilterPacket *pck;
	THDSrc *ctx = gf_filter_get_udta(filter);
	if (ctx->sent) return GF_EOS;
	pck = gf_filter_pck_new_alloc(ctx->pid, 16, &data);
	if (!pck) return GF_OUT_OF_MEM;
	memset(data, 0, 16);
	gf_filter_pck_set_dts(pck, 0);
	gf_filter_pck_set_cts(pck, 0);
	gf_filter_pck_set_duration(pck, 40);
	gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
	gf_filter_pck_send(pck);
	gf_filter_pid_set_eos(ctx->pid);
	ctx->sent = GF_TRUE;
	return GF_EOS;
}

static const GF_FilterCapability THDSrcCaps[] =
{
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_TRUEHD),
	{0},
};

static const GF_FilterRegister THDSrcReg =
{
	.name = "thdsrc",
	GF_FS_SET_DESCRIPTION("TrueHD elementary source")
	.private_size = sizeof(THDSrc),
	.flags = GF_FS_REG_EXPLICIT_ONLY | GF_FS_REG_ACT_AS_SOURCE,
	SETCAPS(THDSrcCaps),
	.initialize = thd_src_initialize,
	.process = thd_src_process,
};

static int run_mux(const char *out)
{
	GF_Err e = GF_OK;
	GF_Filter *src, *mx, *dst;
	GF_FilterSession *fs;
	char mx_args[64];
	char dst_args[512];

	fs = gf_fs_new_defaults(0);
	if (!fs) return 2;
	gf_fs_add_filter_register(fs, &THDSrcReg);
	src = gf_fs_load_filter(fs, "thdsrc", &e);
	if (!src) {
		fprintf(stderr, "src load %s\n", gf_error_to_string(e));
		gf_fs_del(fs);
		return 2;
	}
	memcpy(mx_args, "mp4mx:store=flat", 17);
	mx = gf_fs_load_filter(fs, mx_args, &e);
	if (!mx) {
		fprintf(stderr, "mx load %s\n", gf_error_to_string(e));
		gf_fs_del(fs);
		return 2;
	}
	snprintf(dst_args, sizeof(dst_args), "fout:dst=%s", out);
	dst = gf_fs_load_filter(fs, dst_args, &e);
	if (!dst) {
		fprintf(stderr, "dst load %s\n", gf_error_to_string(e));
		gf_fs_del(fs);
		return 2;
	}
	gf_filter_set_source(mx, src, NULL);
	gf_filter_set_source(dst, mx, NULL);
	e = gf_fs_run(fs);
	gf_fs_del(fs);
	if (e && (e != GF_EOS)) {
		fprintf(stderr, "run %s\n", gf_error_to_string(e));
		return 3;
	}
	return 0;
}

int main(int argc, char **argv)
{
	const char *mode = (argc > 1) ? argv[1] : "bug";
	const char *out = (argc > 2) ? argv[2] : "/tmp/thd_out.mp4";
	u8 raw[16];
	int rc;
	GF_ISOFile *iso;
	u32 fmt = 0, prate = 0;
	GF_Err ge;

	gf_sys_init(GF_MemTrackerNone, NULL);
	memset(raw, 0, sizeof(raw));
	raw[0] = 0x00; raw[1] = 0x01; raw[2] = 0x02; raw[3] = 0x03;
	raw[4] = 0x00; raw[5] = 0x20;

	if (!strcmp(mode, "bug")) {
		g_dsi_size = 7;
	} else if (!strcmp(mode, "ok6")) {
		g_dsi_size = 6;
	} else if (!strcmp(mode, "ok10")) {
		g_dsi_size = 10;
	} else if (!strcmp(mode, "short")) {
		g_dsi_size = 5;
	} else {
		fprintf(stderr, "usage: %s bug|ok6|ok10|short [out.mp4]\n", argv[0]);
		return 2;
	}
	g_dsi = malloc(g_dsi_size);
	if (!g_dsi) return 2;
	memcpy(g_dsi, raw, g_dsi_size);

	rc = run_mux(out);
	if (rc) {
		gf_sys_close();
		return rc;
	}
	iso = gf_isom_open(out, GF_ISOM_OPEN_READ, NULL);
	if (!iso) {
		fprintf(stderr, "open failed\n");
		gf_sys_close();
		return 4;
	}
	ge = gf_isom_truehd_config_get(iso, 1, 1, &fmt, &prate);
	printf("mode=%s get=%s fmt=%08x prate=%u\n", mode, gf_error_to_string(ge), fmt, prate);
	gf_isom_close(iso);
	gf_sys_close();
	return (ge == GF_OK) ? 0 : 5;
}
