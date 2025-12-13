/**
 * Audio-driven main loop with blocking audio writes.
 *
 * This loop is included by minarch.c when SYNC_MODE_AUDIOCLOCK is defined.
 *
 * Timing approach:
 * - Audio hardware clock is the timing source
 * - Core runs every loop iteration (no frame pacing)
 * - SND_batchSamples() blocks when buffer is full (up to 10ms)
 * - Audio callback drains buffer at hardware audio rate
 * - Natural backpressure from audio blocking rate-limits emulation
 *
 * For devices with unstable vsync (e.g., M17).
 */

static void run_main_loop(void) {
	LOG_info("Using audioclock sync mode (audio-driven timing)\n");

	PWR_warn(1);
	PWR_disableAutosleep();

	LOG_debug("Special_init");
	Special_init(); // after config

	LOG_debug("Entering main loop (audioclock mode)");
	sec_start = SDL_GetTicks();
	while (!quit) {
		GFX_startFrame();

		// Always run core - audio blocking in SND_batchSamples() handles timing.
		// When audio buffer is full, the core will block (up to 10ms) waiting for
		// the audio callback to consume samples. This naturally rate-limits emulation
		// to match the audio hardware clock.

		// Call frame time callback if registered (per libretro spec)
		if (video_state.frame_time_cb) {
			retro_usec_t frame_now = getMicroseconds();
			retro_usec_t delta;
			if (fast_forward) {
				delta = video_state.frame_time_ref;
			} else if (video_state.frame_time_last == 0) {
				delta = video_state.frame_time_ref;
			} else {
				delta = frame_now - video_state.frame_time_last;
			}
			video_state.frame_time_last = frame_now;
			video_state.frame_time_cb(delta);
		}

		// Measure frame execution time for auto CPU scaling
		uint64_t frame_start = getMicroseconds();
		core.run();
		uint64_t frame_time = getMicroseconds() - frame_start;

		// Store frame time for auto CPU scaling analysis
		if (overclock == 3 && !fast_forward && !show_menu) {
			auto_cpu_state
			    .frame_times[auto_cpu_state.frame_time_index % MINARCH_CPU_FRAME_BUFFER_SIZE] =
			    frame_time;
			auto_cpu_state.frame_time_index++;
		}

		// Present frame (may or may not wait for vsync depending on platform)
		GFX_flip(screen);
		frame_ready_for_flip = 0;

		limitFF();
		trackFPS();
		updateAutoCPU();

		if (show_menu) {
			GFX_clearBlit(); // Switch to UI mode for menu rendering
			Menu_loop();
		}

		hdmimon();
	}
}
