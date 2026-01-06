/**
 * Vsync-driven main loop with frame pacing and audio rate control.
 *
 * This loop is included by player.c when SYNC_MODE_AUDIOCLOCK is not defined.
 *
 * Timing approach:
 * - Display vsync is the timing source (GFX_present blocks until vsync)
 * - Frame pacer uses Bresenham accumulator to decide when to step emulation
 * - Audio rate control maintains buffer at 50% using dual-timescale PI controller
 * - Works with any display Hz / game fps mismatch
 *
 * For devices with stable vsync.
 */

static void run_main_loop(void) {
	// Initialize frame pacer with display Hz
	double display_hz = FramePacer_getDisplayHz();
	FramePacer_init(&frame_pacer, core.fps, display_hz);
	LOG_info("Frame pacer: %.2ffps @ %.2fHz (%s) [Q16: %d/%d]\n", core.fps, display_hz,
	         FramePacer_isDirectMode(&frame_pacer) ? "direct" : "vsync-driven Bresenham",
	         frame_pacer.game_fps_q16, frame_pacer.display_hz_q16);

	// Keep audio rate control ENABLED with frame pacing.
	// The integral term is slow enough (300-frame average) that it won't fight
	// the per-frame step/repeat pattern. It learns the average vsync rate and
	// compensates for hardware drift (e.g., 58.7Hz vs 60Hz nominal).
	// Without rate control, vsync variance causes continuous audio underruns.

	PWR_warn(1);
	PWR_disableAutosleep();

	// force a vsync immediately before loop
	// for better frame pacing?
	GFX_clearAll();
	GFX_present(NULL);

	LOG_debug("Special_init");
	Special_init(); // after config

	LOG_debug("Entering main loop (vsync mode)");
	sec_start = SDL_GetTicks();
	while (!quit) {
		GFX_startFrame();
		input_polled_this_frame = 0; // Reset at start of frame

		// Frame pacing: Bresenham accumulator decides whether to run core this vsync.
		// Vsync (from GFX_present) is the timing source - each loop iteration = one display refresh.
		// Core runs at its natural rate (e.g., 60fps), display refreshes at panel Hz (e.g., 72Hz).
		// When display Hz > game fps: some frames are repeated (re-presented).
		//
		// During fast-forward: run core multiple times per vsync to achieve speedup.
		// max_ff_speed: 0=2x, 1=3x, 2=4x, 3=5x
		int runs_this_vsync = fast_forward ? (max_ff_speed + 2) : 1;

		for (int run = 0; run < runs_this_vsync; run++) {
			// First run uses frame pacer, subsequent FF runs always execute
			// Don't run core while menu is active
			bool should_run_core = !show_menu && ((run == 0) ? (fast_forward || FramePacer_step(&frame_pacer))
			                                                   : fast_forward);

			if (should_run_core) {
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

				// Update audio rate control integral (once per frame)
				// Skip during FF to prevent integral windup (audio is skipped)
				if (!fast_forward) {
					SND_newFrame();
				}

				// Measure frame execution time for auto CPU scaling
				uint64_t frame_start = getMicroseconds();
				GLVideo_bindFBO();
				core.run();
				uint64_t frame_time = getMicroseconds() - frame_start;

				// Store frame time for auto CPU scaling analysis
				if (overclock == 3 && !fast_forward && !show_menu) {
					auto_cpu_state.frame_times[auto_cpu_state.frame_time_index %
					                            CPU_FRAME_BUFFER_SIZE] = frame_time;
					auto_cpu_state.frame_time_index++;
				}
			}
		}

		// Always present for vsync timing - when !should_run_core, re-presents previous frame
		// Skip for HW rendering - frame already presented via GLVideo_present()
		if (!GLVideo_isEnabled()) {
			GFX_present(&renderer);
			frame_ready_for_flip = 0;
		}

		// Track performance (only once per vsync, not per FF run)
		limitFF();
		trackFPS();
		updateAutoCPU();

		// Fallback input poll - ensures MENU button and shortcuts work even when
		// core doesn't call input_poll_callback (e.g., showing error screens).
		// Guard inside callback prevents double execution.
		input_poll_callback();

		if (show_menu) {
			Menu_loop();

			// Rebind FBO for HW cores after menu (menu uses FBO 0)
			if (GLVideo_isEnabled()) {
				GLVideo_bindFBO();
			}
		}

		hdmimon();
	}
}
