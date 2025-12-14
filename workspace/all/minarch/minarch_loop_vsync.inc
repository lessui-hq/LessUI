/**
 * Vsync-driven main loop with frame pacing and audio rate control.
 *
 * This loop is included by minarch.c when SYNC_MODE_AUDIOCLOCK is not defined.
 *
 * Timing approach:
 * - Display vsync is the timing source (GFX_flip blocks until vsync)
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
	GFX_flip(screen);

	LOG_debug("Special_init");
	Special_init(); // after config

	LOG_debug("Entering main loop (vsync mode)");
	sec_start = SDL_GetTicks();
	while (!quit) {
		GFX_startFrame();
		input_polled_this_frame = 0; // Reset at start of frame

		// Frame pacing: Bresenham accumulator decides whether to run core this vsync.
		// Vsync (from GFX_flip) is the timing source - each loop iteration = one display refresh.
		// Core runs at its natural rate (e.g., 60fps), display refreshes at panel Hz (e.g., 72Hz).
		// When display Hz > game fps: some frames are repeated (re-presented).
		bool should_run_core = fast_forward || FramePacer_step(&frame_pacer);

		if (should_run_core) {
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

			// Report audio buffer status to core for frameskip decisions
			if (core.audio_buffer_status) {
				unsigned occupancy = SND_getBufferOccupancy();
				core.audio_buffer_status(true, occupancy, occupancy < 25);
			}

			// Update audio rate control integral (once per frame)
			SND_newFrame();

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
		}

		// Always flip for vsync timing - when !should_run_core, re-presents previous frame
		GFX_flip(screen);
		frame_ready_for_flip = 0;

		if (should_run_core) {
			limitFF();
			trackFPS();
			updateAutoCPU();
		}

		// Fallback input poll - ensures MENU button and shortcuts work even when
		// core doesn't call input_poll_callback (e.g., showing error screens).
		// Guard inside callback prevents double execution.
		input_poll_callback();

		if (show_menu) {
			LOG_debug("Main loop: show_menu=1, entering Menu_loop");
			GFX_clearBlit(); // Switch to UI mode for menu rendering
			Menu_loop();
			LOG_debug("Main loop: returned from Menu_loop");
		}

		hdmimon();
	}
}
