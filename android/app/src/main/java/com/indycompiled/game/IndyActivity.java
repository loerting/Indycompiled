package com.indycompiled.game;

import android.os.Bundle;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

/**
 * Indycompiled's activity: SDL3's SDLActivity (window, GLES surface, input, audio, lifecycle),
 * which loads libSDL3.so and libmain.so and runs SDL_main() from libmain.so on its own thread.
 */
public class IndyActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Game: never dim or lock the screen while the activity is in front
        // (SDL also does this while its screensaver is disabled, which is its default).
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }
}
