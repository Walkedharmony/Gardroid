package com.zeronovel.gardroid.utils;

import android.app.AlertDialog;
import android.content.Context;
import android.widget.Toast;
import com.zeronovel.gardroid.bridge.NativeLib;
import java.io.File;
import java.util.List;

public class ScnManager {

    private final Context context;
    private final NativeLib nativeLib;

    public interface ScnConfigCallback {
        void onConfigComplete(boolean convertScn);
    }

    public ScnManager(Context context) {
        this.context = context;
        this.nativeLib = new NativeLib();
    }

    public boolean containsScnFiles(List<String> filePaths) {
        for (String path : filePaths) {
            if (path.toLowerCase().endsWith(".scn")) return true;
        }
        return false;
    }

    public void configureScnExtraction(List<String> filePaths, ScnConfigCallback callback) {
        new AlertDialog.Builder(context)
                .setTitle("Scenario Script Detected")
                .setMessage("File .scn ditemukan dalam arsip.\nConvert ke Text (.txt)?")
                .setPositiveButton("Ya, Convert", (dialog, which) -> {
                    callback.onConfigComplete(true);
                })
                .setNegativeButton("Tidak", (dialog, which) -> {
                    callback.onConfigComplete(false);
                })
                .setCancelable(false)
                .show();
    }

    public void processScnFile(String extractedFilePath) {
        if (!extractedFilePath.toLowerCase().endsWith(".scn")) return;

        File inFile = new File(extractedFilePath);
        if (!inFile.exists()) return;

        String outPath = extractedFilePath + ".txt";

        nativeLib.extractScnText(extractedFilePath, outPath);
    }
}