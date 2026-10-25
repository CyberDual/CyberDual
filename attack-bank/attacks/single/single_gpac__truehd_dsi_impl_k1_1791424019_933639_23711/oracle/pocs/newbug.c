#include <gpac/filters.h>
#include <gpac/constants.h>
#include <stdio.h>

static GF_FilterPid *out_pid;
static u32 config_size = 6;

static GF_Err source_init(GF_Filter *filter)
{
	static u8 config[] = { 0x08, 0x12, 0x34, 0x56, 0x12, 0x34, 0, 0, 0, 0 };
	out_pid = gf_filter_pid_new(filter);
	if (!out_pid) return GF_OUT_OF_MEM;
	gf_filter_pid_set_property(out_pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO));
	gf_filter_pid_set_property(out_pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_TRUEHD));
	gf_filter_pid_set_property(out_pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(48000));
	gf_filter_pid_set_property(out_pid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(48000));
	gf_filter_pid_set_property(out_pid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(2));
	gf_filter_pid_set_property(out_pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(config, config_size));
	return GF_OK;
}

static GF_Err source_process(GF_Filter *filter)
{
	gf_filter_pid_set_eos(out_pid);
	return GF_EOS;
}

static const GF_FilterCapability source_caps[] = {
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_TRUEHD),
};

static const GF_FilterRegister source_reg = {
	.name = "pocsrc",
	SETCAPS(source_caps),
	.initialize = source_init,
	.process = source_process,
};

int main(int argc, char **argv)
{
	GF_Err err = GF_OK;
	if (argc > 1) config_size = 10;
	GF_FilterSession *session = gf_fs_new(0, GF_FS_SCHEDULER_LOCK_FREE, GF_FS_FLAG_NO_IMPLICIT, NULL);
	if (!session) return 1;
	gf_fs_add_filter_register(session, &source_reg);
	GF_Filter *source = gf_fs_load_filter(session, "pocsrc", &err);
	GF_Filter *mux = gf_fs_load_filter(session, "mp4mx", &err);
	if (!source || !mux || gf_filter_set_source(mux, source, NULL)) return 2;
	GF_Filter *sink = gf_filter_connect_destination(mux, "/tmp/gpac_truehd_poc.mp4", &err);
	if (!sink) return 3;
	err = gf_fs_run(session);
	printf("session result: %d\n", err);
	gf_fs_del(session);
	return (err != GF_OK && err != GF_EOS) ? 4 : 0;
}
