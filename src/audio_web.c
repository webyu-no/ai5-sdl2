/* Browser-native Web Audio backend for the Web YU-NO build. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <emscripten.h>

#include "nulib.h"
#include "ai5/arc.h"
#include "asset.h"
#include "audio.h"
#include "vm.h"

struct channel { unsigned id; char *file_name; bool loop; int volume; };

EM_JS(void, browser_audio_init, (), { Module.webYuno.audio.initialize(Module); });
EM_JS(void, browser_audio_stop, (unsigned id), { Module.webYuno.audio.stop(id); });
EM_JS(int, browser_audio_play_cached_music, (const char *name, int loop, int preserve_playing,
		int volume), {
	return Module.webYuno.audio.playCachedMusic(UTF8ToString(name), Boolean(loop),
		Boolean(preserve_playing), volume) ? 1 : 0;
});
EM_ASYNC_JS(void, browser_audio_play, (unsigned id, const char *name,
		const uint8_t *data, size_t size, int loop), {
	const bytes = HEAPU8.slice(data, data + size);
	await Module.webYuno.audio.play(id, UTF8ToString(name), bytes, Boolean(loop));
});
EM_JS(void, browser_audio_play_voice, (unsigned id, const char *name,
		const uint8_t *data, size_t size), {
	const bytes = HEAPU8.slice(data, data + size);
	Module.webYuno.audio.play(id, UTF8ToString(name), bytes, false).catch(console.warn);
});
EM_JS(void, browser_audio_play_effect, (unsigned id, const char *name,
		const uint8_t *data, size_t size), {
	const bytes = HEAPU8.slice(data, data + size);
	Module.webYuno.audio.play(id, UTF8ToString(name), bytes, false).catch(console.warn);
});
EM_JS(void, browser_audio_load_voice, (unsigned id, const char *name), {
	Module.webYuno.playVoice(id, UTF8ToString(name)).catch(console.warn);
});
EM_JS(void, browser_audio_volume, (unsigned id, int percent), { Module.webYuno.audio.setVolume(id, percent); });
EM_JS(void, browser_audio_fade, (unsigned id, int percent, unsigned ms, int stop), {
	Module.webYuno.audio.fade(id, percent, ms, Boolean(stop));
});
EM_JS(int, browser_audio_playing, (unsigned id), { return Module.webYuno.audio.isPlaying(id); });
EM_JS(int, browser_audio_fading, (unsigned id), { return Module.webYuno.audio.isFading(id); });

static struct channel channels[] = {
	[AUDIO_CH_BGM] = { .id = 0, .loop = true, .volume = 100 },
	[AUDIO_CH_SE0] = { .id = 1, .volume = 100 },
	[AUDIO_CH_SE1] = { .id = 2, .volume = 100 },
	[AUDIO_CH_SE2] = { .id = 3, .volume = 100 },
	[AUDIO_CH_VOICE0] = { .id = 4, .volume = 100 },
	[AUDIO_CH_VOICE1] = { .id = 5, .volume = 100 },
};

static int get_linear_volume(int vol)
{
	vol = clamp(-5000, 0, vol);
	return vol >= 0 ? 100 : floorf(powf(10.f, (float)vol / 2000.f) * 100 + 0.5f);
}

static void channel_stop(struct channel *ch)
{
	browser_audio_stop(ch->id);
	free(ch->file_name);
	ch->file_name = NULL;
}

void audio_fini(void)
{
	for (unsigned i = 0; i < ARRAY_SIZE(channels); i++) channel_stop(&channels[i]);
}

void audio_init(void) { browser_audio_init(); atexit(audio_fini); }

static void channel_play(struct channel *ch, struct archive_data *file, bool check_playing)
{
	if (check_playing && ch->file_name && !strcmp(ch->file_name, file->name)
			&& browser_audio_playing(ch->id)) return;
	channel_stop(ch);
	browser_audio_volume(ch->id, ch->volume);
	if (ch->id >= AUDIO_CH_VOICE0)
		browser_audio_play_voice(ch->id, file->name, file->data, file->size);
	else if (ch->id >= AUDIO_CH_SE0)
		browser_audio_play_effect(ch->id, file->name, file->data, file->size);
	else
		browser_audio_play(ch->id, file->name, file->data, file->size, ch->loop);
	ch->file_name = strdup(file->name);
}

static void channel_play_voice_name(struct channel *ch, const char *name)
{
	channel_stop(ch);
	browser_audio_volume(ch->id, ch->volume);
	browser_audio_load_voice(ch->id, name);
	ch->file_name = strdup(name);
}

static bool channel_play_cached_name(struct channel *ch, const char *name, bool check_playing)
{
	if (!browser_audio_play_cached_music(name, ch->loop, check_playing, ch->volume))
		return false;
	free(ch->file_name);
	ch->file_name = strdup(name);
	return true;
}

static void channel_set_volume(struct channel *ch, int vol)
{
	ch->volume = get_linear_volume(vol);
	browser_audio_volume(ch->id, ch->volume);
}

static void channel_fade(struct channel *ch, int vol, int t, bool stop, bool sync)
{
	browser_audio_fade(ch->id, get_linear_volume(vol), t, stop);
	if (sync) while (browser_audio_fading(ch->id)) { vm_peek(); vm_delay(8); }
}

static void channel_mixer_fade(struct channel *ch, int vol, int t, bool stop, bool sync)
{
	// Unlike a per-stream fade, mixer gain persists when the next file starts.
	// Keep that logical state on the C side because a browser source has to be
	// recreated to cancel Web Audio's uncancellable scheduled stop.
	ch->volume = get_linear_volume(vol);
	browser_audio_fade(ch->id, ch->volume, t, stop);
	if (sync) while (browser_audio_fading(ch->id)) { vm_peek(); vm_delay(8); }
}

static bool channel_is_playing(struct channel *ch) { return browser_audio_playing(ch->id); }
static bool channel_is_fading(struct channel *ch) { return browser_audio_fading(ch->id); }
void audio_update(void) {}

#include "audio_interface.c"
