package com.zeronovel.gardroid.utils;

import android.app.AlertDialog;
import android.content.Context;
import com.zeronovel.gardroid.bridge.NativeLib;
import java.io.File;
import java.util.List;

public class KsManager {

    private final Context context;
    private final NativeLib nativeLib;

    public interface KsConfigCallback {
        void onConfigComplete(boolean convertKs);
    }

    public KsManager(Context context) {
        this.context = context;
        this.nativeLib = new NativeLib();
    }

    public boolean containsKsFiles(List<String> filePaths) {
        if (!new SettingsConfig(context).isManagerEnabled(SettingsConfig.KEY_KS_MANAGER)) return false;
        for (String path : filePaths) {
            if (path.toLowerCase().endsWith(".ks")) return true;
        }
        return false;
    }

    public void configureKsExtraction(List<String> filePaths, KsConfigCallback callback) {
        if (!new SettingsConfig(context).isManagerEnabled(SettingsConfig.KEY_KS_MANAGER)) {
            callback.onConfigComplete(false);
            return;
        }
        // Jika setting ON, otomatis convert tanpa dialog
        callback.onConfigComplete(true);
    }

    public void processKsFile(String extractedFilePath) {
        if (!extractedFilePath.toLowerCase().endsWith(".ks")) return;

        File inFile = new File(extractedFilePath);
        if (!inFile.exists()) return;

        String outPath = extractedFilePath + ".txt";
        nativeLib.extractKsText(extractedFilePath, outPath);
    }
}