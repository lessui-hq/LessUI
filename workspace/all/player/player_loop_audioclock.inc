/**
 * Audio-driven main loop with blocking audio writes.
 *
 * This loop is included by player.c when SYNC_MODE_AUDIOCLOCK is defined.
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
	uint32_t last_ff_flip = 0;
	while (!quit) {
		GFX_startFrame();
		input_polled_this_frame = 0; // Reset at start of frame

		// Always run core - audio blocking in SND_batchSamples() handles timing.
		// When audio buffer is full, the core will block (up to 10ms) waiting for
		// the audio callback to consume samples. This naturally rate-limits emulation
		// to match the audio hardware clock.
		//
		// During fast-forward: audio is skipped (no blocking), limitFF() controls speed.

		// Call frame time callback if registered (per libretro spec)
		if (video_state.frame_time_cb) {
			retro_usec_t frame_now = getMicroseconds();
			retro_usec_t delta;
			if (fast_forward) {
				// Use reference time during FF, don't update frame_time_last
				// to avoid timing discontinuity when FF ends
				delta = video_state.frame_time_ref;
			} else {
				if (video_state.frame_time_last == 0) {
					delta = video_state.frame_time_ref;
				} else {
					delta = frame_now - video_state.frame_time_last;
				}
				video_state.frame_time_last = frame_now;
			}
			video_state.frame_time_cb(delta);
		}

		// Report audio buffer status to core for frameskip decisions
		// During FF, report audio inactive (no output during FF)
		if (core.audio_buffer_status) {
			if (fast_forward) {
				core.audio_buffer_status(false, 0, false);
			} else {
				unsigned occupancy = SND_getBufferOccupancy();
				core.audio_buffer_status(true, occupancy, occupancy < 25);
			}
		}

		// Note: SND_newFrame() is not called here because audioclock mode uses
		// blocking audio writes for timing, not the PI rate controller.

		// Measure frame execution time for auto CPU scaling
		uint64_t frame_start = getMicroseconds();
		core.run();
		uint64_t frame_time = getMicroseconds() - frame_start;

		// Store frame time for auto CPU scaling analysis
		if (overclock == 3 && !fast_forward && !show_menu) {
			auto_cpu_state
			    .frame_times[auto_cpu_state.frame_time_index % PLAYER_CPU_FRAME_BUFFER_SIZE] =
			    frame_time;
			auto_cpu_state.frame_time_index++;
		}

		// Present frame
		// During FF: throttle vsync to avoid blocking (limitFF controls speed instead)
		// Normal: present every frame (may or may not wait for vsync depending on platform)
		if (fast_forward) {
			uint32_t now = SDL_GetTicks();
			if (now - last_ff_flip >= 30) { // Visual update every 30ms
				GFX_present(&renderer);
				frame_ready_for_flip = 0;
				last_ff_flip = now;
			}
		} else {
			GFX_present(&renderer);
			frame_ready_for_flip = 0;
		}

		limitFF();
		trackFPS();
		updateAutoCPU();

		// Fallback input poll - ensures MENU button and shortcuts work even when
		// core doesn't call input_poll_callback (e.g., showing error screens).
		// Guard inside callback prevents double execution.
		input_poll_callback();

		if (show_menu) {
			LOG_debug("Main loop: show_menu=1, entering Menu_loop");
			Menu_loop();
			LOG_debug("Main loop: returned from Menu_loop");
		}

		hdmimon();
	}
}
