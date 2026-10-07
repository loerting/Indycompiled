package com.indycompiled.game;

import android.os.Bundle;
import android.view.Display;
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
        requestHighestRefreshRate();
    }

    /** Asks for the display's fastest mode at the current resolution (e.g. 120 Hz); Android otherwise runs apps at 60. */
    private void requestHighestRefreshRate() {
        Display display = getWindowManager().getDefaultDisplay();
        Display.Mode current = display.getMode();
        Display.Mode best = current;
        for (Display.Mode mode : display.getSupportedModes()) {
            if (mode.getPhysicalWidth() == current.getPhysicalWidth()
                    && mode.getPhysicalHeight() == current.getPhysicalHeight()
                    && mode.getRefreshRate() > best.getRefreshRate()) {
                best = mode;
            }
        }
        WindowManager.LayoutParams params = getWindow().getAttributes();
        params.preferredDisplayModeId = best.getModeId();
        getWindow().setAttributes(params);
    }
}
