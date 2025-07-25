package org.vita3k.emulator;


import android.content.Context;
import android.content.Intent;
import android.content.res.AssetFileDescriptor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.Settings;
import android.system.ErrnoException;
import android.system.Os;
import android.view.Surface;
import android.view.ViewGroup;

import androidx.annotation.Keep;
import androidx.core.content.pm.ShortcutInfoCompat;
import androidx.core.content.pm.ShortcutManagerCompat;
import androidx.core.graphics.drawable.IconCompat;
import androidx.documentfile.provider.DocumentFile;

import com.jakewharton.processphoenix.ProcessPhoenix;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;
import org.vita3k.emulator.overlay.InputOverlay;

public class Emulator extends SDLActivity
{
    private String currentGameId = "";
    private EmuSurface mSurface;

    public InputOverlay getmOverlay() {
        return mSurface.getmOverlay();
    }

    @Keep
    public void setCurrentGameId(String gameId){
        currentGameId = gameId;
    }

    /**
     * This method is called by SDL before loading the native shared libraries.
     * It can be overridden to provide names of shared libraries to be loaded.
     * The default implementation returns the defaults. It never returns null.
     * An array returned by a new implementation must at least contain "SDL2".
     * Also keep in mind that the order the libraries are loaded may matter.
     *
     * @return names of shared libraries to be loaded (e.g. "SDL2", "main").
     */
    @Override
    protected String[] getLibraries() {
        return new String[] {
                // "SDL2",
                // "SDL2_audio",
                // "SDL2_image",
                // "SDL2_mixer",
                // "SDL2_net",
                // "SDL2_ttf",
                "Vita3K"
        };
    }

    @Override
    protected SDLSurface createSDLSurface(Context context) {
        // Create the input overlay in the same time
        mSurface = new EmuSurface(context);
        return mSurface;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.v(TAG, "Manufacturer: " + Build.MANUFACTURER);
        Log.v(TAG, "Device: " + Build.DEVICE);
        Log.v(TAG, "Model: " + Build.MODEL);
        Log.v(TAG, "onCreate()");
        super.onCreate(savedInstanceState);


        /* Control activity re-creation */
        if (mSDLMainFinished || mActivityCreated) {
              boolean allow_recreate = SDLActivity.nativeAllowRecreateActivity();
              if (mSDLMainFinished) {
                  Log.v(TAG, "SDL main() finished");
              }
              if (allow_recreate) {
                  Log.v(TAG, "activity re-created");
              } else {
                  Log.v(TAG, "activity finished");
                  System.exit(0);
                  return;
              }
        }

        mActivityCreated = true;

        try {
            Thread.currentThread().setName("SDLActivity");
        } catch (Exception e) {
            Log.v(TAG, "modify thread properties failed " + e.toString());
        }

        // Load shared libraries
        String errorMsgBrokenLib = "";
        try {
            loadLibraries();
            mBrokenLibraries = false; /* success */
        } catch(UnsatisfiedLinkError e) {
            System.err.println(e.getMessage());
            mBrokenLibraries = true;
            errorMsgBrokenLib = e.getMessage();
        } catch(Exception e) {
            System.err.println(e.getMessage());
            mBrokenLibraries = true;
            errorMsgBrokenLib = e.getMessage();
        }

        if (mBrokenLibraries) {
            mSingleton = this;
            AlertDialog.Builder dlgAlert  = new AlertDialog.Builder(this);
            dlgAlert.setMessage("An error occurred while trying to start the application. Please try again and/or reinstall."
                  + System.getProperty("line.separator")
                  + System.getProperty("line.separator")
                  + "Error: " + errorMsgBrokenLib);
            dlgAlert.setTitle("SDL Error");
            dlgAlert.setPositiveButton("Exit",
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog,int id) {
                        // if this button is clicked, close current activity
                        SDLActivity.mSingleton.finish();
                    }
                });
           dlgAlert.setCancelable(false);
           dlgAlert.create().show();

           return;
        }


        /* Control activity re-creation */
        /* Robustness: check that the native code is run for the first time.
         * (Maybe Activity was reset, but not the native code.) */
        {
            int run_count = SDLActivity.nativeCheckSDLThreadCounter(); /* get and increment a native counter */
            if (run_count != 0) {
                boolean allow_recreate = SDLActivity.nativeAllowRecreateActivity();
                if (allow_recreate) {
                    Log.v(TAG, "activity re-created // run_count: " + run_count);
                } else {
                    Log.v(TAG, "activity finished // run_count: " + run_count);
                    System.exit(0);
                    return;
                }
            }
        }

        // Set up JNI
        SDL.setupJNI();

        // Initialize state
        SDL.initialize();

        // So we can call stuff from static callbacks
        mSingleton = this;
        SDL.setContext(this);

        mClipboardHandler = new SDLClipboardHandler();

        mHIDDeviceManager = HIDDeviceManager.acquire(this);

        // Set up the surface
        mSurface = createSDLSurface(this);

        mLayout = new RelativeLayout(this);
        mLayout.addView(mSurface);

        // Get our current screen orientation and pass it down.
        SDLActivity.nativeSetNaturalOrientation(SDLActivity.getNaturalOrientation());
        mCurrentRotation = SDLActivity.getCurrentRotation();
        SDLActivity.onNativeRotationChanged(mCurrentRotation);

        mCurrentLocale = getContext().getResources().getConfiguration().getLocales().get(0);
            
        switch (getContext().getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK) {
        case Configuration.UI_MODE_NIGHT_NO:
            SDLActivity.onNativeDarkModeChanged(false);
            break;
        case Configuration.UI_MODE_NIGHT_YES:
            SDLActivity.onNativeDarkModeChanged(true);
            break;
        }

        setContentView(mLayout);

        setWindowStyle(false);

        getWindow().getDecorView().setOnSystemUiVisibilityChangeListener(this);

        }
    }
    
    private final String APP_RESTART_PARAMETERS = "AppStartParameters";

    @Override
    protected String[] getArguments() {
        Intent intent = getIntent();

        String[] args = intent.getStringArrayExtra(APP_RESTART_PARAMETERS);
        if(args == null)
            args = new String[]{};

        return args;
    }

    @Override
    protected void onNewIntent(Intent intent){
        super.onNewIntent(intent);

        // if we start the app from a shortcut and are in the main menu
        // or in a different game, start the new game
        if(intent.getAction().startsWith("LAUNCH_")){
            String game_id = intent.getAction().substring(7);
            if(!game_id.equals(currentGameId))
                ProcessPhoenix.triggerRebirth(getContext(), intent);
        }
    }

    @Keep
    public void restartApp(String app_path, String exec_path, String exec_args){
        ArrayList<String> args = new ArrayList<>();

        // first build the args given to Vita3K when it restarts
        // this is similar to run_execv in main.cpp
        args.add("-a");
        args.add("true");
        if(!app_path.isEmpty()){
            args.add("-r");
            args.add(app_path);

            if(!exec_path.isEmpty()){
                args.add("--self");
                args.add(exec_path);

                if(!exec_args.isEmpty()){
                    args.add("--app-args");
                    args.add(exec_args);
                }
            }
        }

        Intent restart_intent = new Intent(getContext(), Emulator.class);
        restart_intent.putExtra(APP_RESTART_PARAMETERS, args.toArray(new String[]{}));
        ProcessPhoenix.triggerRebirth(getContext(), restart_intent);
    }

    final int FILE_DIALOG_CODE = 545;
    final int FOLDER_DIALOG_CODE = 546;
    final int STORAGE_MANAGER_DIALOG_CODE = 547;

    @Keep
    public void showFileDialog() {
        Intent intent = new Intent()
                .setType("*/*")
                .setAction(Intent.ACTION_GET_CONTENT)
                .putExtra(Intent.EXTRA_LOCAL_ONLY, true);

        intent = Intent.createChooser(intent, "Choose a file");
        startActivityForResult(intent, FILE_DIALOG_CODE);
    }

    private boolean isStorageManagerEnabled(){
        return (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) && Environment.isExternalStorageManager();
    }

    @Keep
    public void showFolderDialog() {
        // If running Android 10-, SDL should have already asked for read and write permissions
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R || isStorageManagerEnabled()) {
            Intent intent = new Intent()
                    .setAction(Intent.ACTION_OPEN_DOCUMENT_TREE)
                    .putExtra(Intent.EXTRA_LOCAL_ONLY, true);

            intent = Intent.createChooser(intent, "Choose a folder");
            startActivityForResult(intent, FOLDER_DIALOG_CODE);
        } else {
            Intent intent = new Intent()
                    .setAction(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION)
                    .setData(Uri.parse("package:" + BuildConfig.APPLICATION_ID));

            startActivityForResult(intent, STORAGE_MANAGER_DIALOG_CODE);
        }
    }

    private File getFileFromUri(Uri uri){
        try {
            InputStream inputStream = getContentResolver().openInputStream(uri);
            File tempFile = File.createTempFile("vita3ktemp", ".bin");
            tempFile.deleteOnExit();

            FileOutputStream outStream = new FileOutputStream(tempFile);
            byte[] buffer = new byte[1024 * 1024];
            int bytesRead;
            while ((bytesRead = inputStream.read(buffer)) != -1) {
                outStream.write(buffer, 0, bytesRead);
            }
            outStream.close();
            inputStream.close();

            return tempFile;
        } catch (Exception e) {
            return null;
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if(requestCode == FILE_DIALOG_CODE){
            String result_path = "";
            int result_fd = -1;
            if(resultCode == RESULT_OK){
                Uri result_uri = data.getData();
                result_path = result_uri.toString();
                java.nio.file.Path normalized =
                    java.nio.file.FileSystems.getDefault().getPath(result_path).normalize();
                if (normalized.startsWith("/data"))
                   throw new SecurityException();
                
                try (AssetFileDescriptor asset_fd = getContentResolver().openAssetFileDescriptor(result_uri, "r")){
                    // if the file is less than 4 KB, make a temporary copy
                    if(asset_fd.getLength() >= 4*1024) {
                        try (ParcelFileDescriptor file_descr = getContentResolver().openFileDescriptor(result_uri, "r")) {
                            result_fd = file_descr.detachFd();
                            // in case the last call returns a ErrnoException
                            result_path = result_uri.toString();
                            result_path = Os.readlink("/proc/self/fd/" + result_fd);
                        }
                    } else {
                        File f = getFileFromUri(result_uri);
                        result_path = f.getAbsolutePath();
                    }
                } catch (ErrnoException | IOException e) {
                }
            }
            filedialogReturn(result_path, result_fd);
        } else if(requestCode == FOLDER_DIALOG_CODE){
            String result_path = "";
            if(resultCode == RESULT_OK){
                Uri result_uri = data.getData();
                DocumentFile tree = DocumentFile.fromTreeUri(getApplicationContext(), result_uri);
                try(ParcelFileDescriptor file_descr = getContentResolver().openFileDescriptor(tree.getUri(), "r")) {
                    int result_fd = file_descr.getFd();

                    result_path = Os.readlink("/proc/self/fd/" + result_fd);
                    // replace /mnt/user/{id} with /storage
                    if(result_path.startsWith("/mnt/user/")){
                        result_path = result_path.substring("/mnt/user/".length());
                        result_path = "/storage" + result_path.substring(result_path.indexOf('/'));
                    }
                } catch (ErrnoException | IOException e) {
                }
            }
            filedialogReturn(result_path, 0);
        } else if (requestCode == STORAGE_MANAGER_DIALOG_CODE) {
            if (isStorageManagerEnabled()) {
                showFolderDialog();
            } else {
                filedialogReturn("", -1);
            }
        }
    }

    @Keep
    public void setControllerOverlayState(int overlay_mask, boolean edit, boolean reset, boolean portrait){
        getmOverlay().setState(overlay_mask);
        getmOverlay().setIsInEditMode(edit);

        if(reset)
            getmOverlay().resetButtonPlacement(portrait);
    }

    @Keep
    public void setControllerOverlayScale(float scale, float scale_joystick){
        getmOverlay().setScale(scale, scale_joystick);
    }

    @Keep
    public void setControllerOverlayOpacity(int opacity){
        getmOverlay().setOpacity(opacity);
    }

    @Keep
    public boolean createShortcut(String game_path, String game_id, String game_name){
        if(!ShortcutManagerCompat.isRequestPinShortcutSupported(getContext()))
            return false;

        // first look at the icon, its location should always be the same
        File src_icon = new File(game_path + "/ux0/app/" + game_id + "/sce_sys/icon0.png");
        Bitmap icon;
        if(src_icon.exists())
            icon = BitmapFactory.decodeFile(src_icon.getPath());
        else
            icon = BitmapFactory.decodeResource(getResources(), R.mipmap.ic_launcher);

        // intent to directly start the game
        Intent game_intent = new Intent(getContext(), Emulator.class);
        ArrayList<String> args = new ArrayList<String>();
        args.add("-r");
        args.add(game_id);
        game_intent.putExtra(APP_RESTART_PARAMETERS, args.toArray(new String[]{}));
        game_intent.setAction("LAUNCH_" + game_id);

        // now create the pinned shortcut
        ShortcutInfoCompat shortcut = new ShortcutInfoCompat.Builder(getContext(), game_id)
                .setShortLabel(game_name)
                .setLongLabel(game_name)
                .setIcon(IconCompat.createWithBitmap(icon))
                .setIntent(game_intent)
                .build();
        ShortcutManagerCompat.requestPinShortcut(getContext(), shortcut, null);

        return true;
    }

    @Keep
    public boolean isDefaultOrientationLandscape() {
        // we know the current device orientation is landscape
        // so the default one is also landscape if and only if the rotation is 0 or 180
        int rotation = getWindowManager().getDefaultDisplay().getRotation();
        return rotation == Surface.ROTATION_0 || rotation == Surface.ROTATION_180;
    }

@Override
public int writeReport(byte[] report, boolean feature) {
    // no need
    return 0;
}

@Override
public int readReport(byte[] report, boolean feature) {
    // no need
    return 0;
}

@Override
public int getFeatureReport(byte[] report) {
    // no need
    return 0;
}
    
    public native void filedialogReturn(String result_path, int result_fd);
}
